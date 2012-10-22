/*
 * Copyright 2010, The Android Open Source Project
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#define LOG_TAG "TilesManager"
#define LOG_NDEBUG 1

#include "config.h"
#include "TilesManager.h"

#if USE(ACCELERATED_COMPOSITING)

#include "AndroidLog.h"
#include "GLWebViewState.h"
#include "SkCanvas.h"
#include "SkDevice.h"
#include "SkPaint.h"
#include "Tile.h"
#include "TileTexture.h"
#include "TransferQueue.h"

#include <android/native_window.h>
#include <cutils/atomic.h>
#include <gui/SurfaceTexture.h>
#include <gui/SurfaceTextureClient.h>
#include <wtf/CurrentTime.h>

// Important: We need at least twice as many textures as is needed to cover
// one viewport, otherwise the allocation may stall.
// We need n textures for one TiledPage, and another n textures for the
// second page used when scaling.
// In our case, we use 256*256 textures. Both base and layers can use up to
// MAX_TEXTURE_ALLOCATION textures, which is 224MB GPU memory in total.
// For low end graphics systems, we cut this upper limit to half.
// We've found the viewport dependent value m_currentTextureCount is a reasonable
// number to cap the layer tile texturs, it worked on both phones and tablets.
// TODO: after merge the pool of base tiles and layer tiles, we should revisit
// the logic of allocation management.
#define MAX_TEXTURE_ALLOCATION ((10+TILE_PREFETCH_DISTANCE*2)*(7+TILE_PREFETCH_DISTANCE*2)*4)
#define TILE_WIDTH 256
#define TILE_HEIGHT 256

#define BYTES_PER_PIXEL 4 // 8888 config

#define LAYER_TEXTURES_DESTROY_TIMEOUT 60 // If we do not need layers for 60 seconds, free the textures

// Eventually this should be dynamically be determined, and smart scheduling
// between the generators should be implemented
#define NUM_TEXTURES_GENERATORS 1

namespace WebCore {

int TilesManager::getMaxTextureAllocation()
{
    if (m_maxTextureAllocation == -1) {
        GLint glMaxTextureSize = 0;
        glGetIntegerv(GL_MAX_TEXTURE_SIZE, &glMaxTextureSize);
        GLUtils::checkGlError("TilesManager::getMaxTextureAllocation");
        // Half of glMaxTextureSize can be used for base, the other half for layers.
        m_maxTextureAllocation = std::min(MAX_TEXTURE_ALLOCATION, glMaxTextureSize / 2);
        if (!m_highEndGfx)
            m_maxTextureAllocation = m_maxTextureAllocation / 2;
    }
    return m_maxTextureAllocation;
}

TilesManager::TilesManager()
    : m_layerTexturesRemain(true)
    , m_highEndGfx(false)
    , m_currentTextureCount(0)
    , m_currentLayerTextureCount(0)
    , m_maxTextureAllocation(-1)
    , m_generatorReady(false)
    , m_showVisualIndicator(false)
    , m_invertedScreen(false)
    , m_useMinimalMemory(true)
    , m_useDoubleBuffering(true)
    , m_contentUpdates(0)
    , m_webkitContentUpdates(0)
    , m_queue(0)
    , m_drawGLCount(1)
    , m_lastTimeLayersUsed(0)
    , m_hasLayerTextures(false)
    , m_eglContext(EGL_NO_CONTEXT)
{
    ALOGV("TilesManager ctor");
    m_textures.reserveCapacity(MAX_TEXTURE_ALLOCATION / 2);
    m_availableTextures.reserveCapacity(MAX_TEXTURE_ALLOCATION / 2);
    m_tilesTextures.reserveCapacity(MAX_TEXTURE_ALLOCATION / 2);
    m_availableTilesTextures.reserveCapacity(MAX_TEXTURE_ALLOCATION / 2);

    m_textureGenerators = new sp<TexturesGenerator>[NUM_TEXTURES_GENERATORS];
    for (int i = 0; i < NUM_TEXTURES_GENERATORS; i++) {
        m_textureGenerators[i] = new TexturesGenerator(this);
        ALOGD("Starting TG #%d, %p", i, m_textureGenerators[i].get());
        m_textureGenerators[i]->run("TexturesGenerator");
    }
}

TilesManager::~TilesManager()
{
    delete[] m_textureGenerators;
}


void TilesManager::allocateTextures()
{
    int nbTexturesToAllocate = m_currentTextureCount - m_textures.size();
    ALOGV("%d tiles to allocate (%d textures planned)", nbTexturesToAllocate, m_currentTextureCount);
    int nbTexturesAllocated = 0;
    for (int i = 0; i < nbTexturesToAllocate; i++) {
        TileTexture* texture = new TileTexture(
            tileWidth(), tileHeight());
        // the atomic load ensures that the texture has been fully initialized
        // before we pass a pointer for other threads to operate on
        TileTexture* loadedTexture =
            reinterpret_cast<TileTexture*>(
            android_atomic_acquire_load(reinterpret_cast<int32_t*>(&texture)));
        m_textures.append(loadedTexture);
        nbTexturesAllocated++;
    }

    int nbLayersTexturesToAllocate = m_currentLayerTextureCount - m_tilesTextures.size();
    ALOGV("%d layers tiles to allocate (%d textures planned)",
          nbLayersTexturesToAllocate, m_currentLayerTextureCount);
    int nbLayersTexturesAllocated = 0;
    for (int i = 0; i < nbLayersTexturesToAllocate; i++) {
        TileTexture* texture = new TileTexture(
            tileWidth(), tileHeight());
        // the atomic load ensures that the texture has been fully initialized
        // before we pass a pointer for other threads to operate on
        TileTexture* loadedTexture =
            reinterpret_cast<TileTexture*>(
            android_atomic_acquire_load(reinterpret_cast<int32_t*>(&texture)));
        m_tilesTextures.append(loadedTexture);
        nbLayersTexturesAllocated++;
    }
    ALOGV("allocated %d textures for base (total: %d, %d Mb), %d textures for layers (total: %d, %d Mb)",
          nbTexturesAllocated, m_textures.size(),
          m_textures.size() * TILE_WIDTH * TILE_HEIGHT * 4 / 1024 / 1024,
          nbLayersTexturesAllocated, m_tilesTextures.size(),
          m_tilesTextures.size() * tileWidth() * tileHeight() * 4 / 1024 / 1024);
}

void TilesManager::discardTextures(bool allTextures, bool glTextures)
{
    const unsigned int max = m_textures.size();

    unsigned long long sparedDrawCount = ~0; // by default, spare no textures
    if (!allTextures) {
        // if we're not deallocating all textures, spare those with max drawcount
        sparedDrawCount = 0;
        for (unsigned int i = 0; i < max; i++) {
            TextureOwner* owner = m_textures[i]->owner();
            if (owner)
                sparedDrawCount = std::max(sparedDrawCount, owner->drawCount());
        }
    }
    discardTexturesVector(sparedDrawCount, m_textures, glTextures);
    discardTexturesVector(sparedDrawCount, m_tilesTextures, glTextures);
}

void TilesManager::markAllGLTexturesZero()
{
    for (unsigned int i = 0; i < m_textures.size(); i++)
        m_textures[i]->m_ownTextureId = 0;
    for (unsigned int i = 0; i < m_tilesTextures.size(); i++)
        m_tilesTextures[i]->m_ownTextureId = 0;
}

void TilesManager::discardTexturesVector(unsigned long long sparedDrawCount,
                                         WTF::Vector<TileTexture*>& textures,
                                         bool deallocateGLTextures)
{
    const unsigned int max = textures.size();
    int dealloc = 0;
    WTF::Vector<int> discardedIndex;
    for (unsigned int i = 0; i < max; i++) {
        TextureOwner* owner = textures[i]->owner();
        if (!owner || owner->drawCount() < sparedDrawCount) {
            if (deallocateGLTextures) {
                // deallocate textures' gl memory
                textures[i]->discardGLTexture();
                discardedIndex.append(i);
            } else if (owner) {
                // simply detach textures from owner
                static_cast<Tile*>(owner)->discardTextures();
            }
            dealloc++;
        }
    }

    bool base = textures == m_textures;
    // Clean up the vector of TileTextures and reset the max texture count.
    if (discardedIndex.size()) {
        android::Mutex::Autolock lock(m_texturesLock);
        for (int i = discardedIndex.size() - 1; i >= 0; i--)
            textures.remove(discardedIndex[i]);

        int remainedTextureNumber = textures.size();
        int* countPtr = base ? &m_currentTextureCount : &m_currentLayerTextureCount;
        if (remainedTextureNumber < *countPtr) {
            ALOGV("reset currentTextureCount for %s tiles from %d to %d",
                  base ? "base" : "layer", *countPtr, remainedTextureNumber);
            *countPtr = remainedTextureNumber;
        }

    }

    ALOGV("Discarded %d %s textures (out of %d %s tiles)",
          dealloc, (deallocateGLTextures ? "gl" : ""),
          max, base ? "base" : "layer");
}

void TilesManager::gatherTexturesNumbers(int* nbTextures, int* nbAllocatedTextures,
                                        int* nbLayerTextures, int* nbAllocatedLayerTextures)
{
    *nbTextures = m_textures.size();
    for (unsigned int i = 0; i < m_textures.size(); i++) {
        TileTexture* texture = m_textures[i];
        if (texture->m_ownTextureId)
            *nbAllocatedTextures += 1;
    }
    *nbLayerTextures = m_tilesTextures.size();
    for (unsigned int i = 0; i < m_tilesTextures.size(); i++) {
        TileTexture* texture = m_tilesTextures[i];
        if (texture->m_ownTextureId)
            *nbAllocatedLayerTextures += 1;
    }
}

void TilesManager::dirtyTexturesVector(WTF::Vector<TileTexture*>& textures)
{
    for (unsigned int i = 0; i < textures.size(); i++) {
        Tile* currentOwner = static_cast<Tile*>(textures[i]->owner());
        if (currentOwner)
            currentOwner->markAsDirty();
    }
}

void TilesManager::dirtyAllTiles()
{
    dirtyTexturesVector(m_textures);
    dirtyTexturesVector(m_tilesTextures);
}

void TilesManager::printTextures()
{
#ifdef DEBUG
    ALOGV("++++++");
    for (unsigned int i = 0; i < m_textures.size(); i++) {
        TileTexture* texture = m_textures[i];
        Tile* o = 0;
        if (texture->owner())
            o = (Tile*) texture->owner();
        int x = -1;
        int y = -1;
        if (o) {
            x = o->x();
            y = o->y();
        }
        ALOGV("[%d] texture %x owner: %x (%d, %d) scale: %.2f",
              i, texture, o, x, y, o ? o->scale() : 0);
    }
    ALOGV("------");
#endif // DEBUG
}

void TilesManager::gatherTextures()
{
    android::Mutex::Autolock lock(m_texturesLock);
    m_availableTextures = m_textures;
    m_availableTilesTextures = m_tilesTextures;
    m_layerTexturesRemain = true;
}

TileTexture* TilesManager::getAvailableTexture(Tile* owner)
{
    android::Mutex::Autolock lock(m_texturesLock);

    WTF::Vector<TileTexture*>* availableTexturePool;
    if (owner->isLayerTile())
        availableTexturePool = &m_availableTilesTextures;
    else
        availableTexturePool = &m_availableTextures;

    // Sanity check that the tile does not already own a texture
    if (owner->backTexture() && owner->backTexture()->owner() == owner) {
        int removeIndex = availableTexturePool->find(owner->backTexture());

        // TODO: investigate why texture isn't found
        if (removeIndex >= 0)
            availableTexturePool->remove(removeIndex);
        return owner->backTexture();
    }

    // The heuristic for selecting a texture is as follows:
    //  1. Skip textures currently being painted, they can't be painted while
    //         busy anyway
    //  2. If a tile isn't owned, break with that one
    //  3. Don't let tiles acquire their front textures
    //  4. Otherwise, use the least recently prepared tile, but ignoring tiles
    //         drawn in the last frame to avoid flickering

    TileTexture* farthestTexture = 0;
    unsigned long long oldestDrawCount = getDrawGLCount() - 1;
    const unsigned int max = availableTexturePool->size();
    for (unsigned int i = 0; i < max; i++) {
        TileTexture* texture = (*availableTexturePool)[i];
        Tile* currentOwner = static_cast<Tile*>(texture->owner());
        if (!currentOwner) {
            // unused texture! take it!
            farthestTexture = texture;
            break;
        }

        if (currentOwner == owner) {
            // Don't let a tile acquire its own front texture, as the
            // acquisition logic doesn't handle that
            continue;
        }

        unsigned long long textureDrawCount = currentOwner->drawCount();
        if (oldestDrawCount > textureDrawCount) {
            farthestTexture = texture;
            oldestDrawCount = textureDrawCount;
        }
    }

    if (farthestTexture) {
        Tile* previousOwner = static_cast<Tile*>(farthestTexture->owner());
        if (farthestTexture->acquire(owner)) {
            if (previousOwner) {
                previousOwner->removeTexture(farthestTexture);

                ALOGV("%s texture %p stolen from tile %d, %d for %d, %d, drawCount was %llu (now %llu)",
                      owner->isLayerTile() ? "LAYER" : "BASE",
                      farthestTexture, previousOwner->x(), previousOwner->y(),
                      owner->x(), owner->y(),
                      oldestDrawCount, getDrawGLCount());
            }

            availableTexturePool->remove(availableTexturePool->find(farthestTexture));
            return farthestTexture;
        }
    } else {
        if (owner->isLayerTile()) {
            // couldn't find a tile for a layer, layers shouldn't request redraw
            // TODO: once we do layer prefetching, don't set this for those
            // tiles
            m_layerTexturesRemain = false;
        }
    }

    ALOGV("Couldn't find an available texture for %s tile %x (%d, %d) out of %d available",
          owner->isLayerTile() ? "LAYER" : "BASE",
          owner, owner->x(), owner->y(), max);
#ifdef DEBUG
    printTextures();
#endif // DEBUG
    return 0;
}

void TilesManager::setHighEndGfx(bool highEnd)
{
    m_highEndGfx = highEnd;
}

bool TilesManager::highEndGfx()
{
    return m_highEndGfx;
}

int TilesManager::currentTextureCount()
{
    android::Mutex::Autolock lock(m_texturesLock);
    return m_currentTextureCount;
}

int TilesManager::currentLayerTextureCount()
{
    android::Mutex::Autolock lock(m_texturesLock);
    return m_currentLayerTextureCount;
}

void TilesManager::setCurrentTextureCount(int newTextureCount)
{
    int maxTextureAllocation = getMaxTextureAllocation();
    ALOGV("setCurrentTextureCount: %d (current: %d, max:%d)",
         newTextureCount, m_currentTextureCount, maxTextureAllocation);
    if (m_currentTextureCount == maxTextureAllocation ||
        newTextureCount <= m_currentTextureCount)
        return;

    android::Mutex::Autolock lock(m_texturesLock);
    m_currentTextureCount = std::min(newTextureCount, maxTextureAllocation);

    allocateTextures();
}

void TilesManager::setCurrentLayerTextureCount(int newTextureCount)
{
    int maxTextureAllocation = getMaxTextureAllocation();
    ALOGV("setCurrentLayerTextureCount: %d (current: %d, max:%d)",
         newTextureCount, m_currentLayerTextureCount, maxTextureAllocation);
    if (!newTextureCount && m_hasLayerTextures) {
        double secondsSinceLayersUsed = WTF::currentTime() - m_lastTimeLayersUsed;
        if (secondsSinceLayersUsed > LAYER_TEXTURES_DESTROY_TIMEOUT) {
            unsigned long long sparedDrawCount = ~0; // by default, spare no textures
            bool deleteGLTextures = true;
            discardTexturesVector(sparedDrawCount, m_tilesTextures, deleteGLTextures);
            m_hasLayerTextures = false;
        }
        return;
    }
    m_lastTimeLayersUsed = WTF::currentTime();
    if (m_currentLayerTextureCount == maxTextureAllocation ||
        newTextureCount <= m_currentLayerTextureCount)
        return;

    android::Mutex::Autolock lock(m_texturesLock);
    m_currentLayerTextureCount = std::min(newTextureCount, maxTextureAllocation);

    allocateTextures();
    m_hasLayerTextures = true;
}

TransferQueue* TilesManager::transferQueue()
{
    // m_queue will be created on the UI thread, although it may
    // be accessed from the TexturesGenerator. However, that can only happen after
    // a previous transferQueue() call due to a prepare.
    if (!m_queue)
        m_queue = new TransferQueue(m_useMinimalMemory && !m_highEndGfx);
    return m_queue;
}

// When GL context changed or we get a low memory signal, we want to cleanup all
// the GPU memory webview is using.
// The recreation will be on the next incoming draw call at the drawGL of
// GLWebViewState or the VideoLayerAndroid
void TilesManager::cleanupGLResources()
{
    transferQueue()->cleanupGLResourcesAndQueue();
    shader()->cleanupGLResources();
    videoLayerManager()->cleanupGLResources();
    m_eglContext = EGL_NO_CONTEXT;
    GLUtils::checkGlError("TilesManager::cleanupGLResources");
}

void TilesManager::updateTilesIfContextVerified()
{
    EGLContext ctx = eglGetCurrentContext();
    GLUtils::checkEglError("contextChanged");
    if (ctx != m_eglContext) {
        if (m_eglContext != EGL_NO_CONTEXT) {
            // A change in EGL context is an unexpected error, but we don't want to
            // crash or ANR. Therefore, abandon the Surface Texture and GL resources;
            // they'll be recreated later in setupDrawing. (We can't delete them
            // since the context is gone)
            ALOGE("Unexpected : EGLContext changed! current %x , expected %x",
                  ctx, m_eglContext);
            transferQueue()->resetQueue();
            shader()->forceNeedsInit();
            videoLayerManager()->forceNeedsInit();
            markAllGLTexturesZero();
        } else {
            // This is the first time we went into this new EGL context.
            // We will have the GL resources to be re-inited and we can't update
            // dirty tiles yet.
            ALOGD("new EGLContext from framework: %x ", ctx);
        }
    } else {
        // Here before we draw, update the Tile which has updated content.
        // Inside this function, just do GPU blits from the transfer queue into
        // the Tiles' texture.
        transferQueue()->updateDirtyTiles();
        // Clean up GL textures for video layer.
        videoLayerManager()->deleteUnusedTextures();
    }
    m_eglContext = ctx;
    return;
}

void TilesManager::removeOperationsForFilter(OperationFilter* filter)
{
    for (int i = 0; i < NUM_TEXTURES_GENERATORS; i++)
        m_textureGenerators[i]->removeOperationsForFilter(filter);
    delete filter;
}

bool TilesManager::tryUpdateOperationWithPainter(Tile* tile, TilePainter* painter)
{
    for (int i = 0; i < NUM_TEXTURES_GENERATORS; i++) {
        if (m_textureGenerators[i]->tryUpdateOperationWithPainter(tile, painter))
            return true;
    }
    return false;
}

void TilesManager::scheduleOperation(QueuedOperation* operation)
{
    // TODO: painter awareness, store prefer awareness, store preferred thread into painter
    m_scheduleThread = (m_scheduleThread + 1) % NUM_TEXTURES_GENERATORS;
    m_textureGenerators[m_scheduleThread]->scheduleOperation(operation);
}

int TilesManager::tileWidth()
{
    return TILE_WIDTH;
}

int TilesManager::tileHeight()
{
    return TILE_HEIGHT;
}

TilesManager* TilesManager::instance()
{
    if (!gInstance) {
        gInstance = new TilesManager();
        ALOGV("instance(), new gInstance is %x", gInstance);
    }
    return gInstance;
}

TilesManager* TilesManager::gInstance = 0;

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
