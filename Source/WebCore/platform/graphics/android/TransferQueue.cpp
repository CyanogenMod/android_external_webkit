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

#include "config.h"
#include "TransferQueue.h"

#if USE(ACCELERATED_COMPOSITING)

#include "BaseTile.h"
#include "PaintedSurface.h"
#include <android/native_window.h>
#include <gui/SurfaceTexture.h>
#include <gui/SurfaceTextureClient.h>

#include <cutils/log.h>
#include <wtf/text/CString.h>
#define XLOGC(...) android_printLog(ANDROID_LOG_DEBUG, "TransferQueue", __VA_ARGS__)

#ifdef DEBUG

#undef XLOG
#define XLOG(...) android_printLog(ANDROID_LOG_DEBUG, "TransferQueue", __VA_ARGS__)

#else

#undef XLOG
#define XLOG(...)

#endif // DEBUG

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
    , m_interruptedByRemovingOp(false)
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

// This should be called within the m_transferQueueItemLocks.
// Now only called by emptyQueue() and destructor.
void TransferQueue::cleanupGLResources()
{
    if (m_sharedSurfaceTexture.get()) {
        m_sharedSurfaceTexture->abandon();
        m_sharedSurfaceTexture.clear();
    }
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
        m_sharedSurfaceTexture =
#if GPU_UPLOAD_WITHOUT_DRAW
            new android::SurfaceTexture(m_sharedSurfaceTextureId, true,
                                        GL_TEXTURE_2D, false);
#else
            new android::SurfaceTexture(m_sharedSurfaceTextureId);
#endif
        m_ANW = new android::SurfaceTextureClient(m_sharedSurfaceTexture);
        m_sharedSurfaceTexture->setSynchronousMode(true);

        int extraBuffersNeeded = 0;
        m_ANW->query(m_ANW.get(), NATIVE_WINDOW_MIN_UNDEQUEUED_BUFFERS,
                     &extraBuffersNeeded);
        m_sharedSurfaceTexture->setBufferCount(m_transferQueueSize + extraBuffersNeeded);

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
// BaseTile and the content, then the item is considered as obsolete, and
// the content is discarded.
bool TransferQueue::checkObsolete(const TileTransferData* data)
{
    BaseTile* baseTilePtr = data->savedBaseTilePtr;
    if (!baseTilePtr) {
        XLOG("Invalid savedBaseTilePtr , such that the tile is obsolete");
        return true;
    }

    BaseTileTexture* baseTileTexture = baseTilePtr->backTexture();
    if (!baseTileTexture) {
        XLOG("Invalid baseTileTexture , such that the tile is obsolete");
        return true;
    }

    const TextureTileInfo* tileInfo = &(data->tileInfo);

    if (tileInfo->m_x != baseTilePtr->x()
        || tileInfo->m_y != baseTilePtr->y()
        || tileInfo->m_scale != baseTilePtr->scale()
        || tileInfo->m_painter != baseTilePtr->painter()) {
        XLOG("Mismatching x, y, scale or painter , such that the tile is obsolete");
        return true;
    }

    return false;
}

void TransferQueue::blitTileFromQueue(GLuint fboID, BaseTileTexture* destTex,
                                      BaseTileTexture* frontTex,
                                      GLuint srcTexId, GLenum srcTexTarget,
                                      int index)
{
#if GPU_UPLOAD_WITHOUT_DRAW
    glBindFramebuffer(GL_FRAMEBUFFER, fboID);
    glBindTexture(GL_TEXTURE_2D, destTex->m_ownTextureId);

    int textureWidth = destTex->getSize().width();
    int textureHeight = destTex->getSize().height();

    IntRect inval = m_transferQueue[index].tileInfo.m_inval;
    bool partialInval = !inval.isEmpty();

    if (partialInval && frontTex) {
        // recopy the previous texture to the new one, as
        // the partial update will not cover the entire texture
        glFramebufferTexture2D(GL_FRAMEBUFFER,
                               GL_COLOR_ATTACHMENT0,
                               GL_TEXTURE_2D,
                               frontTex->m_ownTextureId,
                               0);
        glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0,
                            textureWidth, textureHeight);
    }

    glFramebufferTexture2D(GL_FRAMEBUFFER,
                           GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D,
                           srcTexId,
                           0);

    if (!partialInval) {
        glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0,
                            textureWidth, textureHeight);
    } else {
        glCopyTexSubImage2D(GL_TEXTURE_2D, 0, inval.x(), inval.y(), 0, 0,
                            inval.width(), inval.height());
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
        XLOG("Error: glCheckFramebufferStatus failed");
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return;
    }

    // Use empty rect to set up the special matrix to draw.
    SkRect rect  = SkRect::MakeEmpty();
    TilesManager::instance()->shader()->drawQuad(rect, srcTexId, 1.0,
                       srcTexTarget, GL_NEAREST);

    // To workaround a sync issue on some platforms, we should insert the sync
    // here while in the current FBO.
    // This will essentially kick off the GPU command buffer, and the Tex Gen
    // thread will then have to wait for this buffer to finish before writing
    // into the same memory.
    EGLDisplay dpy = eglGetCurrentDisplay();
    if (m_currentDisplay != dpy)
        m_currentDisplay = dpy;
    if (m_currentDisplay != EGL_NO_DISPLAY) {
        if (m_transferQueue[index].m_syncKHR != EGL_NO_SYNC_KHR)
            eglDestroySyncKHR(m_currentDisplay, m_transferQueue[index].m_syncKHR);
        m_transferQueue[index].m_syncKHR = eglCreateSyncKHR(m_currentDisplay,
                                                            EGL_SYNC_FENCE_KHR,
                                                            0);
    }
    GLUtils::checkEglError("CreateSyncKHR");
#endif
}

void TransferQueue::interruptTransferQueue(bool interrupt)
{
    m_transferQueueItemLocks.lock();
    m_interruptedByRemovingOp = interrupt;
    if (m_interruptedByRemovingOp)
        m_transferQueueItemCond.signal();
    m_transferQueueItemLocks.unlock();
}

// This function must be called inside the m_transferQueueItemLocks, for the
// wait, m_interruptedByRemovingOp and getHasGLContext().
// Only called by updateQueueWithBitmap() for now.
bool TransferQueue::readyForUpdate()
{
    if (!getHasGLContext())
        return false;
    // Don't use a while loop since when the WebView tear down, the emptyCount
    // will still be 0, and we bailed out b/c of GL context lost.
    if (!m_emptyItemCount) {
        if (m_interruptedByRemovingOp)
            return false;
        m_transferQueueItemCond.wait(m_transferQueueItemLocks);
        if (m_interruptedByRemovingOp)
            return false;
    }

    if (!getHasGLContext())
        return false;

    // Disable this wait until we figure out why this didn't work on some
    // drivers b/5332112.
#if 0
    if (m_currentUploadType == GpuUpload
        && m_currentDisplay != EGL_NO_DISPLAY) {
        // Check the GPU fence
        EGLSyncKHR syncKHR = m_transferQueue[getNextTransferQueueIndex()].m_syncKHR;
        if (syncKHR != EGL_NO_SYNC_KHR)
            eglClientWaitSyncKHR(m_currentDisplay,
                                 syncKHR,
                                 EGL_SYNC_FLUSH_COMMANDS_BIT_KHR,
                                 EGL_FOREVER_KHR);
    }
    GLUtils::checkEglError("WaitSyncKHR");
#endif

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

void TransferQueue::setPendingDiscardWithLock()
{
    android::Mutex::Autolock lock(m_transferQueueItemLocks);
    setPendingDiscard();
}

void TransferQueue::emptyQueue()
{
    android::Mutex::Autolock lock(m_transferQueueItemLocks);
    setPendingDiscard();
    cleanupPendingDiscard();
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

    m_pureColorTileQueue.clear();

    bool GLContextExisted = getHasGLContext();
    // Unblock the Tex Gen thread first before Tile Page deletion.
    // Otherwise, there will be a deadlock while removing operations.
    setHasGLContext(false);

    // Only signal once when GL context lost.
    if (GLContextExisted)
        m_transferQueueItemCond.signal();
}

void TransferQueue::updatePureColorTiles()
{
    for (unsigned int i = 0 ; i < m_pureColorTileQueue.size(); i++) {
        TileTransferData* data = &m_pureColorTileQueue[i];
        if (data->status == pendingBlit) {
            BaseTileTexture* destTexture = 0;
            bool obsoleteBaseTile = checkObsolete(data);
            if (!obsoleteBaseTile) {
                destTexture = data->savedBaseTilePtr->backTexture();
                destTexture->setPureColor(data->pureColor);
                destTexture->setOwnTextureTileInfoFromQueue(&data->tileInfo);
            }
        } else if (data->status == emptyItem || data->status == pendingDiscard) {
            // The queue should be clear instead of setting to different status.
            XLOG("Warning: Don't expect an emptyItem here.");
        }
    }
    m_pureColorTileQueue.clear();
}

// Call on UI thread to copy from the shared Surface Texture to the BaseTile's texture.
void TransferQueue::updateDirtyBaseTiles()
{
    android::Mutex::Autolock lock(m_transferQueueItemLocks);

    cleanupPendingDiscard();
    if (!getHasGLContext())
        setHasGLContext(true);

    // Check the pure color tile first, since it is simpler.
    updatePureColorTiles();

    // Start from the oldest item, we call the updateTexImage to retrive
    // the texture and blit that into each BaseTile's texture.
    const int nextItemIndex = getNextTransferQueueIndex();
    int index = nextItemIndex;
    bool usedFboForUpload = false;
    for (int k = 0; k < m_transferQueueSize ; k++) {
        if (m_transferQueue[index].status == pendingBlit) {
            bool obsoleteBaseTile = checkObsolete(&m_transferQueue[index]);
            // Save the needed info, update the Surf Tex, clean up the item in
            // the queue. Then either move on to next item or copy the content.
            BaseTileTexture* destTexture = 0;
            BaseTileTexture* frontTexture = 0;
            if (!obsoleteBaseTile) {
                destTexture = m_transferQueue[index].savedBaseTilePtr->backTexture();
                // while destTexture is guaranteed to not be null, frontTexture
                // might be (first transfer)
                frontTexture = m_transferQueue[index].savedBaseTilePtr->frontTexture();
            }

            if (m_transferQueue[index].uploadType == GpuUpload) {
                status_t result = m_sharedSurfaceTexture->updateTexImage();
                if (result != OK)
                    XLOGC("unexpected error: updateTexImage return %d", result);
            }
            m_transferQueue[index].savedBaseTilePtr = 0;
            m_transferQueue[index].status = emptyItem;
            if (obsoleteBaseTile) {
                XLOG("Warning: the texture is obsolete for this baseTile");
                index = (index + 1) % m_transferQueueSize;
                continue;
            }

            // guarantee that we have a texture to blit into
            destTexture->requireGLTexture();

            if (m_transferQueue[index].uploadType == CpuUpload) {
                // Here we just need to upload the bitmap content to the GL Texture
                GLUtils::updateTextureWithBitmap(destTexture->m_ownTextureId,
                                                 *m_transferQueue[index].bitmap,
                                                 m_transferQueue[index].tileInfo.m_inval);
            } else {
                if (!usedFboForUpload) {
                    saveGLState();
                    usedFboForUpload = true;
                }
                blitTileFromQueue(m_fboID, destTexture, frontTexture,
                                  m_sharedSurfaceTextureId,
                                  m_sharedSurfaceTexture->getCurrentTextureTarget(),
                                  index);
            }

            // After the base tile copied into the GL texture, we need to
            // update the texture's info such that at draw time, readyFor
            // will find the latest texture's info
            // We don't need a map any more, each texture contains its own
            // texturesTileInfo.
            destTexture->setPure(false);
            destTexture->setOwnTextureTileInfoFromQueue(&m_transferQueue[index].tileInfo);

            XLOG("Blit tile x, y %d %d with dest texture %p to destTexture->m_ownTextureId %d",
                 m_transferQueue[index].tileInfo.m_x,
                 m_transferQueue[index].tileInfo.m_y,
                 destTexture,
                 destTexture->m_ownTextureId);
        }
        index = (index + 1) % m_transferQueueSize;
    }

    // Clean up FBO setup. Doing this for both CPU/GPU upload can make the
    // dynamic switch possible. Moving this out from the loop can save some
    // milli-seconds.
    if (usedFboForUpload) {
        glBindFramebuffer(GL_FRAMEBUFFER, 0); // rebind the standard FBO
        restoreGLState();
        GLUtils::checkGlError("updateDirtyBaseTiles");
    }

    m_emptyItemCount = m_transferQueueSize;
    m_transferQueueItemCond.signal();
}

void TransferQueue::updateQueueWithBitmap(const TileRenderInfo* renderInfo,
                                          const SkBitmap& bitmap)
{
    if (!tryUpdateQueueWithBitmap(renderInfo, bitmap)) {
        // failed placing bitmap in queue, discard tile's texture so it will be
        // re-enqueued (and repainted)
        BaseTile* tile = renderInfo->baseTile;
        if (tile)
            tile->backTextureTransferFail();
    }
}

bool TransferQueue::tryUpdateQueueWithBitmap(const TileRenderInfo* renderInfo,
                                             const SkBitmap& bitmap)
{
    // This lock need to cover the full update since it is possible that queue
    // will be cleaned up in the middle of this update without the lock.
    // The Surface Texture will not block us since the readyForUpdate will check
    // availability of the slots in the queue first.
    android::Mutex::Autolock lock(m_transferQueueItemLocks);
    bool ready = readyForUpdate();
    TextureUploadType currentUploadType = m_currentUploadType;
    if (!ready) {
        XLOG("Quit bitmap update: not ready! for tile x y %d %d",
             renderInfo->x, renderInfo->y);
        return false;
    }
    if (currentUploadType == GpuUpload) {
        // a) Dequeue the Surface Texture and write into the buffer
        if (!m_ANW.get()) {
            XLOG("ERROR: ANW is null");
            return false;
        }

        ANativeWindow_Buffer buffer;
        if (ANativeWindow_lock(m_ANW.get(), &buffer, 0))
            return false;

        uint8_t* img = (uint8_t*)buffer.bits;
        int row, col;
        int bpp = 4; // Now we only deal with RGBA8888 format.
        int width = TilesManager::instance()->tileWidth();
        int height = TilesManager::instance()->tileHeight();
        if (bitmap.width() == width && bitmap.height() == height) {
            bitmap.lockPixels();
            uint8_t* bitmapOrigin = static_cast<uint8_t*>(bitmap.getPixels());

            if (buffer.stride != bitmap.width())
                // Copied line by line since we need to handle the offsets and stride.
                for (row = 0 ; row < bitmap.height(); row ++) {
                    uint8_t* dst = &(img[buffer.stride * row * bpp]);
                    uint8_t* src = &(bitmapOrigin[bitmap.width() * row * bpp]);
                    memcpy(dst, src, bpp * bitmap.width());
                }
            else
                memcpy(img, bitmapOrigin, bpp * bitmap.width() * bitmap.height());

            bitmap.unlockPixels();
        }

        ANativeWindow_unlockAndPost(m_ANW.get());
    }

    // b) After update the Surface Texture, now udpate the transfer queue info.
    addItemInTransferQueue(renderInfo, currentUploadType, &bitmap);

    XLOG("Bitmap updated x, y %d %d, baseTile %p",
         renderInfo->x, renderInfo->y, renderInfo->baseTile);
    return true;
}

void TransferQueue::addItemInPureColorQueue(const TileRenderInfo* renderInfo, Color color)
{
    // The pure color tiles' queue will be read from UI thread and written in
    // Tex Gen thread, thus we need to have a lock here.
    android::Mutex::Autolock lock(m_transferQueueItemLocks);
    TileTransferData data;
    addItemCommon(renderInfo, GpuUpload, &data);
    data.pureColor = color;
    m_pureColorTileQueue.append(data);
}

// Translates the info from TileRenderInfo and others to TileTransferData.
// This is used by pure color tiles and normal tiles.
void TransferQueue::addItemCommon(const TileRenderInfo* renderInfo,
                                  TextureUploadType type,
                                  TileTransferData* data)
{
    data->savedBaseTileTexturePtr = renderInfo->baseTile->backTexture();
    data->savedBaseTilePtr = renderInfo->baseTile;
    data->status = pendingBlit;
    data->uploadType = type;

    // Now fill the tileInfo.
    TextureTileInfo* textureInfo = &(data->tileInfo);

    IntRect inval(0, 0, 0, 0);
    if (renderInfo->invalRect) {
        inval.setX(renderInfo->invalRect->fLeft);
        inval.setY(renderInfo->invalRect->fTop);
        inval.setWidth(renderInfo->invalRect->width());
        inval.setHeight(renderInfo->invalRect->height());
    }
    textureInfo->m_inval = inval;

    textureInfo->m_x = renderInfo->x;
    textureInfo->m_y = renderInfo->y;
    textureInfo->m_scale = renderInfo->scale;
    textureInfo->m_painter = renderInfo->tilePainter;

    textureInfo->m_picture = renderInfo->textureInfo->m_pictureCount;
}

// Note that there should be lock/unlock around this function call.
// Currently only called by GLUtils::updateSharedSurfaceTextureWithBitmap.
void TransferQueue::addItemInTransferQueue(const TileRenderInfo* renderInfo,
                                           TextureUploadType type,
                                           const SkBitmap* bitmap)
{
    m_transferQueueIndex = (m_transferQueueIndex + 1) % m_transferQueueSize;

    int index = m_transferQueueIndex;
    if (m_transferQueue[index].savedBaseTilePtr
        || m_transferQueue[index].status != emptyItem) {
        XLOG("ERROR update a tile which is dirty already @ index %d", index);
    }

    TileTransferData* data = &m_transferQueue[index];
    addItemCommon(renderInfo, type, data);
    if (type == CpuUpload && bitmap) {
        // Lazily create the bitmap
        if (!m_transferQueue[index].bitmap) {
            m_transferQueue[index].bitmap = new SkBitmap();
            int w = bitmap->width();
            int h = bitmap->height();
            m_transferQueue[index].bitmap->setConfig(bitmap->config(), w, h);
        }
        bitmap->copyTo(m_transferQueue[index].bitmap, bitmap->config());
    }

    m_emptyItemCount--;
}

void TransferQueue::setTextureUploadType(TextureUploadType type)
{
    android::Mutex::Autolock lock(m_transferQueueItemLocks);
    if (m_currentUploadType == type)
        return;

    setPendingDiscard();

    m_currentUploadType = type;
    XLOGC("Now we set the upload to %s", m_currentUploadType == GpuUpload ? "GpuUpload" : "CpuUpload");
}

// Note: this need to be called within the lock and on the UI thread.
// Only called by updateDirtyBaseTiles() and emptyQueue() for now
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
                    XLOGC("unexpected error: updateTexImage return %d", result);
            }

            // since tiles in the queue may be from another webview, remove
            // their textures so that they will be repainted / retransferred
            BaseTile* tile = m_transferQueue[index].savedBaseTilePtr;
            BaseTileTexture* texture = m_transferQueue[index].savedBaseTileTexturePtr;
            if (tile && texture && texture->owner() == tile) {
                // since tile destruction removes textures on the UI thread, the
                // texture->owner ptr guarantees the tile is valid
                tile->discardBackTexture();
                XLOG("transfer queue discarded tile %p, removed texture", tile);
            }

            m_transferQueue[index].savedBaseTilePtr = 0;
            m_transferQueue[index].savedBaseTileTexturePtr = 0;
            m_transferQueue[index].status = emptyItem;
        }
        index = (index + 1) % m_transferQueueSize;
    }
}

void TransferQueue::saveGLState()
{
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
