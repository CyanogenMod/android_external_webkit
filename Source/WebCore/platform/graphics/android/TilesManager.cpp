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
#include "BaseTile.h"
#include "SkCanvas.h"
#include "SkDevice.h"
#include "SkPaint.h"
#include <android/native_window.h>
#include <cutils/atomic.h>
#include <gui/SurfaceTexture.h>
#include <gui/SurfaceTextureClient.h>
#include <wtf/CurrentTime.h>

// Important: We need at least twice as many textures as is needed to cover
// one viewport, otherwise the allocation may stall.
// We need n textures for one TiledPage, and another n textures for the
// second page used when scaling.
// In our case, we use 256*256 textures. On the tablet, this equates to
// at least 60 textures, or 112 with expanded tile boundaries.
// 112(tiles)*256*256*4(bpp)*2(pages) = 56MB
// It turns out the viewport dependent value m_maxTextureCount is a reasonable
// number to cap the layer tile texturs, it worked on both phones and tablets.
// TODO: after merge the pool of base tiles and layer tiles, we should revisit
// the logic of allocation management.
#define MAX_TEXTURE_ALLOCATION ((6+TILE_PREFETCH_DISTANCE*2)*(5+TILE_PREFETCH_DISTANCE*2)*4)
#define TILE_WIDTH 256
#define TILE_HEIGHT 256
#define LAYER_TILE_WIDTH 256
#define LAYER_TILE_HEIGHT 256

#define BYTES_PER_PIXEL 4 // 8888 config

#define LAYER_TEXTURES_DESTROY_TIMEOUT 60 // If we do not need layers for 60 seconds, free the textures

namespace WebCore {

GLint TilesManager::getMaxTextureSize()
{
    static GLint maxTextureSize = 0;
    if (!maxTextureSize)
        glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTextureSize);
    return maxTextureSize;
}

int TilesManager::getMaxTextureAllocation()
{
    return MAX_TEXTURE_ALLOCATION;
}

TilesManager::TilesManager()
    : m_layerTexturesRemain(true)
    , m_highEndGfx(false)
    , m_maxTextureCount(0)
    , m_maxLayerTextureCount(0)
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
{
    ALOGV("TilesManager ctor");
    m_textures.reserveCapacity(MAX_TEXTURE_ALLOCATION);
    m_availableTextures.reserveCapacity(MAX_TEXTURE_ALLOCATION);
    m_tilesTextures.reserveCapacity(MAX_TEXTURE_ALLOCATION);
    m_availableTilesTextures.reserveCapacity(MAX_TEXTURE_ALLOCATION);
    m_pixmapsGenerationThread = new TexturesGenerator();
    m_pixmapsGenerationThread->run("TexturesGenerator");
}

void TilesManager::allocateTiles()
{
    int nbTexturesToAllocate = m_maxTextureCount - m_textures.size();
    ALOGV("%d tiles to allocate (%d textures planned)", nbTexturesToAllocate, m_maxTextureCount);
    int nbTexturesAllocated = 0;
    for (int i = 0; i < nbTexturesToAllocate; i++) {
        BaseTileTexture* texture = new BaseTileTexture(
            tileWidth(), tileHeight());
        // the atomic load ensures that the texture has been fully initialized
        // before we pass a pointer for other threads to operate on
        BaseTileTexture* loadedTexture =
            reinterpret_cast<BaseTileTexture*>(
            android_atomic_acquire_load(reinterpret_cast<int32_t*>(&texture)));
        m_textures.append(loadedTexture);
        nbTexturesAllocated++;
    }

    int nbLayersTexturesToAllocate = m_maxLayerTextureCount - m_tilesTextures.size();
    ALOGV("%d layers tiles to allocate (%d textures planned)",
          nbLayersTexturesToAllocate, m_maxLayerTextureCount);
    int nbLayersTexturesAllocated = 0;
    for (int i = 0; i < nbLayersTexturesToAllocate; i++) {
        BaseTileTexture* texture = new BaseTileTexture(
            layerTileWidth(), layerTileHeight());
        // the atomic load ensures that the texture has been fully initialized
        // before we pass a pointer for other threads to operate on
        BaseTileTexture* loadedTexture =
            reinterpret_cast<BaseTileTexture*>(
            android_atomic_acquire_load(reinterpret_cast<int32_t*>(&texture)));
        m_tilesTextures.append(loadedTexture);
        nbLayersTexturesAllocated++;
    }
    ALOGV("allocated %d textures for base (total: %d, %d Mb), %d textures for layers (total: %d, %d Mb)",
          nbTexturesAllocated, m_textures.size(),
          m_textures.size() * TILE_WIDTH * TILE_HEIGHT * 4 / 1024 / 1024,
          nbLayersTexturesAllocated, m_tilesTextures.size(),
          m_tilesTextures.size() * LAYER_TILE_WIDTH * LAYER_TILE_HEIGHT * 4 / 1024 / 1024);
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

void TilesManager::discardTexturesVector(unsigned long long sparedDrawCount,
                                         WTF::Vector<BaseTileTexture*>& textures,
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
                static_cast<BaseTile*>(owner)->discardTextures();
            }
            dealloc++;
        }
    }

    bool base = textures == m_textures;
    // Clean up the vector of BaseTileTextures and reset the max texture count.
    if (discardedIndex.size()) {
        android::Mutex::Autolock lock(m_texturesLock);
        for (int i = discardedIndex.size() - 1; i >= 0; i--)
            textures.remove(discardedIndex[i]);

        int remainedTextureNumber = textures.size();
        int* countPtr = base ? &m_maxTextureCount : &m_maxLayerTextureCount;
        if (remainedTextureNumber < *countPtr) {
            ALOGV("reset maxTextureCount for %s tiles from %d to %d",
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
        BaseTileTexture* texture = m_textures[i];
        if (texture->m_ownTextureId)
            *nbAllocatedTextures += 1;
    }
    *nbLayerTextures = m_tilesTextures.size();
    for (unsigned int i = 0; i < m_tilesTextures.size(); i++) {
        BaseTileTexture* texture = m_tilesTextures[i];
        if (texture->m_ownTextureId)
            *nbAllocatedLayerTextures += 1;
    }
}

void TilesManager::printTextures()
{
#ifdef DEBUG
    ALOGV("++++++");
    for (unsigned int i = 0; i < m_textures.size(); i++) {
        BaseTileTexture* texture = m_textures[i];
        BaseTile* o = 0;
        if (texture->owner())
            o = (BaseTile*) texture->owner();
        int x = -1;
        int y = -1;
        if (o) {
            x = o->x();
            y = o->y();
        }
        ALOGV("[%d] texture %x owner: %x (%d, %d) page: %x scale: %.2f",
              i, texture,
              o, x, y, o ? o->page() : 0, o ? o->scale() : 0);
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

BaseTileTexture* TilesManager::getAvailableTexture(BaseTile* owner)
{
    android::Mutex::Autolock lock(m_texturesLock);

    // Sanity check that the tile does not already own a texture
    if (owner->backTexture() && owner->backTexture()->owner() == owner) {
        ALOGV("same owner (%d, %d), getAvailableBackTexture(%x) => texture %x",
              owner->x(), owner->y(), owner, owner->backTexture());
        if (owner->isLayerTile())
            m_availableTilesTextures.remove(m_availableTilesTextures.find(owner->backTexture()));
        else
            m_availableTextures.remove(m_availableTextures.find(owner->backTexture()));
        return owner->backTexture();
    }

    WTF::Vector<BaseTileTexture*>* availableTexturePool;
    if (owner->isLayerTile()) {
        availableTexturePool = &m_availableTilesTextures;
    } else {
        availableTexturePool = &m_availableTextures;
    }

    // The heuristic for selecting a texture is as follows:
    //  1. Skip textures currently being painted, they can't be painted while
    //         busy anyway
    //  2. If a tile isn't owned, break with that one
    //  3. Don't let tiles acquire their front textures
    //  4. Otherwise, use the least recently prepared tile, but ignoring tiles
    //         drawn in the last frame to avoid flickering

    BaseTileTexture* farthestTexture = 0;
    unsigned long long oldestDrawCount = getDrawGLCount() - 1;
    const unsigned int max = availableTexturePool->size();
    for (unsigned int i = 0; i < max; i++) {
        BaseTileTexture* texture = (*availableTexturePool)[i];
        BaseTile* currentOwner = static_cast<BaseTile*>(texture->owner());
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
        BaseTile* previousOwner = static_cast<BaseTile*>(farthestTexture->owner());
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

int TilesManager::maxTextureCount()
{
    android::Mutex::Autolock lock(m_texturesLock);
    return m_maxTextureCount;
}

int TilesManager::maxLayerTextureCount()
{
    android::Mutex::Autolock lock(m_texturesLock);
    return m_maxLayerTextureCount;
}

void TilesManager::setMaxTextureCount(int max)
{
    ALOGV("setMaxTextureCount: %d (current: %d, total:%d)",
         max, m_maxTextureCount, MAX_TEXTURE_ALLOCATION);
    if (m_maxTextureCount == MAX_TEXTURE_ALLOCATION ||
         max <= m_maxTextureCount)
        return;

    android::Mutex::Autolock lock(m_texturesLock);

    if (max < MAX_TEXTURE_ALLOCATION)
        m_maxTextureCount = max;
    else
        m_maxTextureCount = MAX_TEXTURE_ALLOCATION;

    allocateTiles();
}

void TilesManager::setMaxLayerTextureCount(int max)
{
    ALOGV("setMaxLayerTextureCount: %d (current: %d, total:%d)",
         max, m_maxLayerTextureCount, MAX_TEXTURE_ALLOCATION);
    if (!max && m_hasLayerTextures) {
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
    if (m_maxLayerTextureCount == MAX_TEXTURE_ALLOCATION ||
         max <= m_maxLayerTextureCount)
        return;

    android::Mutex::Autolock lock(m_texturesLock);

    if (max < MAX_TEXTURE_ALLOCATION)
        m_maxLayerTextureCount = max;
    else
        m_maxLayerTextureCount = MAX_TEXTURE_ALLOCATION;

    allocateTiles();
    m_hasLayerTextures = true;
}

TransferQueue* TilesManager::transferQueue()
{
    // To minimize the memory usage, transfer queue can be set to minimal size
    // if required.
    if (!m_queue)
        m_queue = new TransferQueue(m_useMinimalMemory);
    return m_queue;
}

float TilesManager::tileWidth()
{
    return TILE_WIDTH;
}

float TilesManager::tileHeight()
{
    return TILE_HEIGHT;
}

float TilesManager::layerTileWidth()
{
    return LAYER_TILE_WIDTH;
}

float TilesManager::layerTileHeight()
{
    return LAYER_TILE_HEIGHT;
}

TilesManager* TilesManager::instance()
{
    if (!gInstance) {
        gInstance = new TilesManager();
        ALOGV("instance(), new gInstance is %x", gInstance);
        ALOGV("Waiting for the generator...");
        gInstance->waitForGenerator();
        ALOGV("Generator ready!");
    }
    return gInstance;
}

TilesManager* TilesManager::gInstance = 0;

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
