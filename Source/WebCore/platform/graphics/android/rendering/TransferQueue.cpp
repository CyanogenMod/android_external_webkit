/*
 * Copyright 2011, The Android Open Source Project
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

#define LOG_TAG "TransferQueue"
#define LOG_NDEBUG 1

#include "config.h"
#include "TransferQueue.h"

#if USE(ACCELERATED_COMPOSITING)

#include "AndroidLog.h"
#include "BaseRenderer.h"
#include "DrawQuadData.h"
#include "GLUtils.h"
#include "Tile.h"
#include "TileTexture.h"
#include "TilesManager.h"
#include <android/native_window.h>
#include <gui/SurfaceTexture.h>
#include <gui/SurfaceTextureClient.h>

// For simple webView usage, MINIMAL_SIZE is recommended for memory saving.
// In browser case, EFFICIENT_SIZE is preferred.
#define MINIMAL_SIZE 1
#define EFFICIENT_SIZE 6

// Set this to 1 if we would like to take the new GpuUpload approach which
// relied on the glCopyTexSubImage2D instead of a glDraw call
#define GPU_UPLOAD_WITHOUT_DRAW 1

namespace WebCore {

TransferQueue::TransferQueue(bool useMinimalMem)
    : m_eglSurface(EGL_NO_SURFACE)
    , m_transferQueueIndex(0)
    , m_fboID(0)
    , m_sharedSurfaceTextureId(0)
    , m_hasGLContext(true)
    , m_currentDisplay(EGL_NO_DISPLAY)
    , m_currentUploadType(DEFAULT_UPLOAD_TYPE)
{
    memset(&m_GLStateBeforeBlit, 0, sizeof(m_GLStateBeforeBlit));
    m_transferQueueSize = useMinimalMem ? MINIMAL_SIZE : EFFICIENT_SIZE;
    m_emptyItemCount = m_transferQueueSize;
    m_transferQueue = new TileTransferData[m_transferQueueSize];
}

TransferQueue::~TransferQueue()
{
    android::Mutex::Autolock lock(m_transferQueueItemLocks);
    cleanupGLResources();
    delete[] m_transferQueue;
}

// Set the queue to be totally empty, abandon the Surface Texture. This should
// be called only when we hit a wrong EGL Context in an error situation.
void TransferQueue::resetQueue()
{
    android::Mutex::Autolock lock(m_transferQueueItemLocks);
    emptyAndAbandonQueue();
    m_sharedSurfaceTextureId = 0;
}

// This should be called within the m_transferQueueItemLocks.
// Now only called by emptyQueue() and destructor.
void TransferQueue::cleanupGLResources()
{
    if (m_fboID) {
        glDeleteFramebuffers(1, &m_fboID);
        m_fboID = 0;
    }
    if (m_sharedSurfaceTextureId) {
        glDeleteTextures(1, &m_sharedSurfaceTextureId);
        m_sharedSurfaceTextureId = 0;
    }
}

void TransferQueue::initGLResources(int width, int height)
{
    android::Mutex::Autolock lock(m_transferQueueItemLocks);
    if (!m_sharedSurfaceTextureId) {
        glGenTextures(1, &m_sharedSurfaceTextureId);
        sp<BufferQueue> bufferQueue(new BufferQueue(true));
        m_sharedSurfaceTexture =
#if GPU_UPLOAD_WITHOUT_DRAW
            new android::SurfaceTexture(m_sharedSurfaceTextureId, true,
                                        GL_TEXTURE_2D, true, bufferQueue);
#else
            new android::SurfaceTexture(m_sharedSurfaceTextureId, true,
                                        GL_TEXTURE_EXTERNAL_OES, true,
                                        bufferQueue);
#endif
        m_ANW = new android::SurfaceTextureClient(m_sharedSurfaceTexture);
        m_sharedSurfaceTexture->setSynchronousMode(true);

        int extraBuffersNeeded = 0;
        int extraHackyBuffersNeeded = 0;
        if (m_transferQueueSize == EFFICIENT_SIZE)
            extraHackyBuffersNeeded = 13;
        m_ANW->query(m_ANW.get(), NATIVE_WINDOW_MIN_UNDEQUEUED_BUFFERS,
                     &extraBuffersNeeded);
        bufferQueue->setBufferCount(m_transferQueueSize + extraBuffersNeeded +
                extraHackyBuffersNeeded);

        int result = native_window_set_buffers_geometry(m_ANW.get(),
                width, height, HAL_PIXEL_FORMAT_RGBA_8888);
        GLUtils::checkSurfaceTextureError("native_window_set_buffers_geometry", result);
        result = native_window_set_usage(m_ANW.get(),
                GRALLOC_USAGE_SW_READ_OFTEN | GRALLOC_USAGE_SW_WRITE_OFTEN);
        GLUtils::checkSurfaceTextureError("native_window_set_usage", result);
    }

    if (!m_fboID)
        glGenFramebuffers(1, &m_fboID);
}

// When bliting, if the item from the transfer queue is mismatching b/t the
// Tile and the content, then the item is considered as obsolete, and
// the content is discarded.
bool TransferQueue::checkObsolete(const TileTransferData* data)
{
    Tile* baseTilePtr = data->savedTilePtr;
    if (!baseTilePtr) {
        ALOGV("Invalid savedTilePtr , such that the tile is obsolete");
        return true;
    }

    TileTexture* baseTileTexture = baseTilePtr->backTexture();
    if (!baseTileTexture || baseTileTexture != data->savedTileTexturePtr) {
        ALOGV("Invalid baseTileTexture %p (vs expected %p), such that the tile is obsolete",
              baseTileTexture, data->savedTileTexturePtr);
        return true;
    }

    return false;
}

void TransferQueue::blitTileFromQueue(GLuint fboID, TileTexture* destTex,
                                      GLuint srcTexId, GLenum srcTexTarget,
                                      int index)
{
#if GPU_UPLOAD_WITHOUT_DRAW
    glBindFramebuffer(GL_FRAMEBUFFER, fboID);
    glBindTexture(GL_TEXTURE_2D, destTex->m_ownTextureId);

    int textureWidth = destTex->getSize().width();
    int textureHeight = destTex->getSize().height();

    glFramebufferTexture2D(GL_FRAMEBUFFER,
                           GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D,
                           srcTexId,
                           0);

    glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0,
                        textureWidth, textureHeight);
    if (GLUtils::checkGlError("At the end of blitTileFromQueue()")) {
#ifndef DEBUG
        if (GLUtils::allowGLLog())
#endif
        ALOGE("blitTileFromQueue ERROR: fboId %d, destTexId %d, srcTexId %d,"
              " textureWidth %d, textureHeight %d", fboID, destTex->m_ownTextureId,
              srcTexId, textureWidth, textureHeight);
    }
#else
    // Then set up the FBO and copy the SurfTex content in.
    glBindFramebuffer(GL_FRAMEBUFFER, fboID);
    glFramebufferTexture2D(GL_FRAMEBUFFER,
                           GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D,
                           destTex->m_ownTextureId,
                           0);
    setGLStateForCopy(destTex->getSize().width(),
                      destTex->getSize().height());
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        ALOGV("Error: glCheckFramebufferStatus failed");
        return;
    }

    // Use empty rect to set up the special matrix to draw.
    SkRect rect  = SkRect::MakeEmpty();

    TextureQuadData data(srcTexId, GL_NEAREST, srcTexTarget, Blit, 0, 0, 1.0, false);
    TilesManager::instance()->shader()->drawQuad(&data);
#endif
}

// This function must be called inside the m_transferQueueItemLocks, for the
// wait and getHasGLContext().
// Only called by updateQueueWithBitmap() for now.
bool TransferQueue::readyForUpdate()
{
    if (!getHasGLContext())
        return false;
    // Don't use a while loop since when the WebView tear down, the emptyCount
    // will still be 0, and we bailed out b/c of GL context lost.
    if (!m_emptyItemCount)
        m_transferQueueItemCond.wait(m_transferQueueItemLocks);

    if (!getHasGLContext())
        return false;

    return true;
}

// Both getHasGLContext and setHasGLContext should be called within the lock.
bool TransferQueue::getHasGLContext()
{
    return m_hasGLContext;
}

void TransferQueue::setHasGLContext(bool hasContext)
{
    m_hasGLContext = hasContext;
}

// Call within a m_transferQueueItemLocks, now called by resetQueue() and
// cleanupGLResoucesAndQueue()
void TransferQueue::emptyAndAbandonQueue()
{
    for (int i = 0 ; i < m_transferQueueSize; i++)
        clearItemInTranferQueue(i);
    m_emptyItemCount = m_transferQueueSize;
    clearPureColorQueue();

    if (m_sharedSurfaceTexture.get()) {
        m_sharedSurfaceTexture->abandon();
        m_sharedSurfaceTexture.clear();
    }
    // This can prevent the tex gen thread to produce, until next incoming draw.
    setHasGLContext(false);
}

void TransferQueue::cleanupGLResourcesAndQueue()
{
    android::Mutex::Autolock lock(m_transferQueueItemLocks);
    emptyAndAbandonQueue();
    cleanupGLResources();
}

// Set all the content in the queue to pendingDiscard, after this, there will
// be nothing added to the queue, and this can be called in any thread.
// However, in order to discard the content in the Surface Texture using
// updateTexImage, cleanupPendingDiscard need to be called on the UI thread.
// Must be called within a m_transferQueueItemLocks.
void TransferQueue::setPendingDiscard()
{
    for (int i = 0 ; i < m_transferQueueSize; i++)
        if (m_transferQueue[i].status == pendingBlit)
            m_transferQueue[i].status = pendingDiscard;

    clearPureColorQueue();

    bool GLContextExisted = getHasGLContext();
    // Unblock the Tex Gen thread first before Tile Page deletion.
    // Otherwise, there will be a deadlock while removing operations.
    setHasGLContext(false);

    // Only signal once when GL context lost.
    if (GLContextExisted)
        m_transferQueueItemCond.signal();
}

void TransferQueue::clearPureColorQueue()
{
    for (unsigned int i = 0 ; i < m_pureColorTileQueue.size(); i++) {
        SkSafeUnref(m_pureColorTileQueue[i].savedTilePainter);
        m_pureColorTileQueue[i].savedTilePainter = 0;
    }
    m_pureColorTileQueue.clear();
}

void TransferQueue::updatePureColorTiles()
{
    for (unsigned int i = 0 ; i < m_pureColorTileQueue.size(); i++) {
        TileTransferData* data = &m_pureColorTileQueue[i];
        if (data->status == pendingBlit) {
            TileTexture* destTexture = 0;
            bool obsoleteTile = checkObsolete(data);
            if (!obsoleteTile) {
                destTexture = data->savedTilePtr->backTexture();
                destTexture->setPureColor(data->pureColor);
                destTexture->transferComplete();
            }
        } else if (data->status == emptyItem || data->status == pendingDiscard) {
            // The queue should be clear instead of setting to different status.
            ALOGV("Warning: Don't expect an emptyItem here.");
        }
    }
    clearPureColorQueue();
}

// Call on UI thread to copy from the shared Surface Texture to the Tile's texture.
void TransferQueue::updateDirtyTiles()
{
    android::Mutex::Autolock lock(m_transferQueueItemLocks);

    cleanupPendingDiscard();
    if (!getHasGLContext())
        setHasGLContext(true);

    // Check the pure color tile first, since it is simpler.
    updatePureColorTiles();

    // Start from the oldest item, we call the updateTexImage to retrive
    // the texture and blit that into each Tile's texture.
    const int nextItemIndex = getNextTransferQueueIndex();
    int index = nextItemIndex;
    bool usedFboForUpload = false;
    for (int k = 0; k < m_transferQueueSize ; k++) {
        if (m_transferQueue[index].status == pendingBlit) {
            bool obsoleteTile = checkObsolete(&m_transferQueue[index]);
            // Save the needed info, update the Surf Tex, clean up the item in
            // the queue. Then either move on to next item or copy the content.
            TileTexture* destTexture = 0;
            if (!obsoleteTile)
                destTexture = m_transferQueue[index].savedTilePtr->backTexture();

            if (m_transferQueue[index].uploadType == GpuUpload) {
                status_t result = m_sharedSurfaceTexture->updateTexImage();
                if (result != OK)
                    ALOGE("unexpected error: updateTexImage return %d", result);
            }

            if (obsoleteTile) {
                ALOGV("Warning: the texture is obsolete for this baseTile");
                clearItemInTranferQueue(index);
                index = (index + 1) % m_transferQueueSize;
                continue;
            }

            // guarantee that we have a texture to blit into
            destTexture->requireGLTexture();
            GLUtils::checkGlError("before blitTileFromQueue");
            if (m_transferQueue[index].uploadType == CpuUpload) {
                // Here we just need to upload the bitmap content to the GL Texture
                GLUtils::updateTextureWithBitmap(destTexture->m_ownTextureId,
                                                 *m_transferQueue[index].bitmap);
            } else {
                if (!usedFboForUpload) {
                    saveGLState();
                    usedFboForUpload = true;
                }
                blitTileFromQueue(m_fboID, destTexture, m_sharedSurfaceTextureId,
                                  m_sharedSurfaceTexture->getCurrentTextureTarget(),
                                  index);
            }

            destTexture->setPure(false);
            destTexture->transferComplete();
            clearItemInTranferQueue(index);
            ALOGV("Blit tile x, y %d %d with dest texture %p to destTexture->m_ownTextureId %d",
                  m_transferQueue[index].savedTilePtr,
                  destTexture,
                  destTexture->m_ownTextureId);
        }
        index = (index + 1) % m_transferQueueSize;
    }

    // Clean up FBO setup. Doing this for both CPU/GPU upload can make the
    // dynamic switch possible. Moving this out from the loop can save some
    // milli-seconds.
    if (usedFboForUpload) {
        restoreGLState();
        GLUtils::checkGlError("updateDirtyTiles");
    }

    m_emptyItemCount = m_transferQueueSize;
    m_transferQueueItemCond.signal();
}

void TransferQueue::updateQueueWithBitmap(const TileRenderInfo* renderInfo,
                                          SkBitmap& bitmap)
{
    TRACE_METHOD();
    if (!tryUpdateQueueWithBitmap(renderInfo, bitmap)) {
        // failed placing bitmap in queue, discard tile's texture so it will be
        // re-enqueued (and repainted)
        Tile* tile = renderInfo->baseTile;
        if (tile)
            tile->backTextureTransferFail();
    }
}

bool TransferQueue::tryUpdateQueueWithBitmap(const TileRenderInfo* renderInfo,
                                             SkBitmap& bitmap)
{
    // This lock need to cover the full update since it is possible that queue
    // will be cleaned up in the middle of this update without the lock.
    // The Surface Texture will not block us since the readyForUpdate will check
    // availability of the slots in the queue first.
    android::Mutex::Autolock lock(m_transferQueueItemLocks);
    bool ready = readyForUpdate();
    TextureUploadType currentUploadType = m_currentUploadType;
    if (!ready) {
        ALOGV("Quit bitmap update: not ready! for tile x y %d %d",
              renderInfo->x, renderInfo->y);
        return false;
    }
    if (currentUploadType == GpuUpload) {
        // a) Dequeue the Surface Texture and write into the buffer
        if (!m_ANW.get()) {
            ALOGV("ERROR: ANW is null");
            return false;
        }

        if (!GLUtils::updateSharedSurfaceTextureWithBitmap(m_ANW.get(), bitmap))
            return false;
    }

    // b) After update the Surface Texture, now udpate the transfer queue info.
    addItemInTransferQueue(renderInfo, currentUploadType, bitmap);

    ALOGV("Bitmap updated x, y %d %d, baseTile %p",
          renderInfo->x, renderInfo->y, renderInfo->baseTile);
    return true;
}

void TransferQueue::addItemInPureColorQueue(const TileRenderInfo* renderInfo)
{
    // The pure color tiles' queue will be read from UI thread and written in
    // Tex Gen thread, thus we need to have a lock here.
    android::Mutex::Autolock lock(m_transferQueueItemLocks);
    TileTransferData data;
    addItemCommon(renderInfo, GpuUpload, &data);
    data.pureColor = renderInfo->pureColor;
    m_pureColorTileQueue.append(data);
}

void TransferQueue::clearItemInTranferQueue(int index)
{
    m_transferQueue[index].savedTilePtr = 0;
    SkSafeUnref(m_transferQueue[index].savedTilePainter);
    m_transferQueue[index].savedTilePainter = 0;
    m_transferQueue[index].status = emptyItem;
}

// Translates the info from TileRenderInfo and others to TileTransferData.
// This is used by pure color tiles and normal tiles.
void TransferQueue::addItemCommon(const TileRenderInfo* renderInfo,
                                  TextureUploadType type,
                                  TileTransferData* data)
{
    data->savedTileTexturePtr = renderInfo->baseTile->backTexture();
    data->savedTilePainter = renderInfo->tilePainter;
    SkSafeRef(data->savedTilePainter);
    data->savedTilePtr = renderInfo->baseTile;
    data->status = pendingBlit;
    data->uploadType = type;

    IntRect inval(0, 0, 0, 0);
}

// Note that there should be lock/unlock around this function call.
// Currently only called by GLUtils::updateSharedSurfaceTextureWithBitmap.
void TransferQueue::addItemInTransferQueue(const TileRenderInfo* renderInfo,
                                           TextureUploadType type,
                                           SkBitmap& bitmap)
{
    m_transferQueueIndex = (m_transferQueueIndex + 1) % m_transferQueueSize;

    int index = m_transferQueueIndex;
    if (m_transferQueue[index].savedTilePtr
        || m_transferQueue[index].status != emptyItem) {
        ALOGV("ERROR update a tile which is dirty already @ index %d", index);
    }

    TileTransferData* data = &m_transferQueue[index];
    addItemCommon(renderInfo, type, data);
    if (type == CpuUpload) {
        // Lazily create the bitmap
        if (!m_transferQueue[index].bitmap) {
            m_transferQueue[index].bitmap = new SkBitmap();
            int w = bitmap.width();
            int h = bitmap.height();
            m_transferQueue[index].bitmap->setConfig(bitmap.config(), w, h);
            m_transferQueue[index].bitmap->allocPixels();
        }
        SkBitmap temp = (*m_transferQueue[index].bitmap);
        (*m_transferQueue[index].bitmap) = bitmap;
        bitmap = temp;
    }

    m_emptyItemCount--;
}

void TransferQueue::setTextureUploadType(TextureUploadType type)
{
    android::Mutex::Autolock lock(m_transferQueueItemLocks);
    if (m_currentUploadType == type)
        return;

    setPendingDiscard();

#ifdef FORCE_CPU_UPLOAD
    m_currentUploadType = CpuUpload; // force to cpu upload mode for now until gpu upload mode is fixed
#else
    m_currentUploadType = type;
#endif
    ALOGD("Now we set the upload to %s", m_currentUploadType == GpuUpload ? "GpuUpload" : "CpuUpload");
}

// Note: this need to be called within the lock and on the UI thread.
// Only called by updateDirtyTiles() and emptyQueue() for now
void TransferQueue::cleanupPendingDiscard()
{
    int index = getNextTransferQueueIndex();

    for (int i = 0 ; i < m_transferQueueSize; i++) {
        if (m_transferQueue[index].status == pendingDiscard) {
            // No matter what the current upload type is, as long as there has
            // been a Surf Tex enqueue operation, this updateTexImage need to
            // be called to keep things in sync.
            if (m_transferQueue[index].uploadType == GpuUpload) {
                status_t result = m_sharedSurfaceTexture->updateTexImage();
                if (result != OK)
                    ALOGE("unexpected error: updateTexImage return %d", result);
            }

            // since tiles in the queue may be from another webview, remove
            // their textures so that they will be repainted / retransferred
            Tile* tile = m_transferQueue[index].savedTilePtr;
            TileTexture* texture = m_transferQueue[index].savedTileTexturePtr;
            if (tile && texture && texture->owner() == tile) {
                // since tile destruction removes textures on the UI thread, the
                // texture->owner ptr guarantees the tile is valid
                tile->discardBackTexture();
                ALOGV("transfer queue discarded tile %p, removed texture", tile);
            }
            clearItemInTranferQueue(index);
        }
        index = (index + 1) % m_transferQueueSize;
    }
}

void TransferQueue::saveGLState()
{
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, m_GLStateBeforeBlit.bufferId);
    glGetIntegerv(GL_VIEWPORT, m_GLStateBeforeBlit.viewport);
    glGetBooleanv(GL_SCISSOR_TEST, m_GLStateBeforeBlit.scissor);
    glGetBooleanv(GL_DEPTH_TEST, m_GLStateBeforeBlit.depth);
#ifdef DEBUG
    glGetFloatv(GL_COLOR_CLEAR_VALUE, m_GLStateBeforeBlit.clearColor);
#endif
}

void TransferQueue::setGLStateForCopy(int width, int height)
{
    // Need to match the texture size.
    glViewport(0, 0, width, height);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_DEPTH_TEST);
    // Clear the content is only for debug purpose.
#ifdef DEBUG
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT);
#endif
}

void TransferQueue::restoreGLState()
{
    glBindFramebuffer(GL_FRAMEBUFFER, m_GLStateBeforeBlit.bufferId[0]);
    glViewport(m_GLStateBeforeBlit.viewport[0],
               m_GLStateBeforeBlit.viewport[1],
               m_GLStateBeforeBlit.viewport[2],
               m_GLStateBeforeBlit.viewport[3]);

    if (m_GLStateBeforeBlit.scissor[0])
        glEnable(GL_SCISSOR_TEST);

    if (m_GLStateBeforeBlit.depth[0])
        glEnable(GL_DEPTH_TEST);
#ifdef DEBUG
    glClearColor(m_GLStateBeforeBlit.clearColor[0],
                 m_GLStateBeforeBlit.clearColor[1],
                 m_GLStateBeforeBlit.clearColor[2],
                 m_GLStateBeforeBlit.clearColor[3]);
#endif
}

int TransferQueue::getNextTransferQueueIndex()
{
    return (m_transferQueueIndex + 1) % m_transferQueueSize;
}

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING
