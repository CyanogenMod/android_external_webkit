/*
 * Copyright 2011 The Android Open Source Project
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

#define LOG_TAG "VideoLayerManager"
#define LOG_NDEBUG 1

#include "config.h"
#include "VideoLayerManager.h"

#include "AndroidLog.h"
#include "RenderSkinMediaButton.h"
#include "SkCanvas.h"
#include <wtf/CurrentTime.h>

#if USE(ACCELERATED_COMPOSITING)

// The animation of the play/pause icon will last for PLAY_PAUSE_ICON_SHOW_TIME
// seconds.
#define PLAY_PAUSE_ICON_SHOW_TIME 1

// Define the max sum of all the video's sizes.
// Note that video_size = width * height. If there is no compression, then the
// maximum memory consumption could be 4 * video_size.
// Setting this to 2M, means that maximum memory consumption of all the
// screenshots would not be above 8M.
#define MAX_VIDEOSIZE_SUM 2097152

// We don't preload the video data, so we don't have the exact size yet.
// Assuming 16:9 by default, this will be corrected after video prepared.
#define DEFAULT_VIDEO_ASPECT_RATIO 1.78

#define VIDEO_TEXTURE_NUMBER 5
#define VIDEO_BUTTON_SIZE 64

namespace WebCore {

VideoLayerManager::VideoLayerManager()
    : m_currentTimeStamp(0)
    , m_createdTexture(false)
    , m_posterTextureId(0)
    , m_spinnerOuterTextureId(0)
    , m_spinnerInnerTextureId(0)
    , m_playTextureId(0)
    , m_pauseTextureId(0)
    , m_buttonRect(0, 0, VIDEO_BUTTON_SIZE, VIDEO_BUTTON_SIZE)
{
}

int VideoLayerManager::getButtonSize()
{
    return VIDEO_BUTTON_SIZE;
}

GLuint VideoLayerManager::createTextureFromImage(RenderSkinMediaButton::MediaButton buttonType)
{
    SkRect rect = SkRect(m_buttonRect);
    SkBitmap bitmap;
    bitmap.setConfig(SkBitmap::kARGB_8888_Config, rect.width(), rect.height());
    bitmap.allocPixels();
    bitmap.eraseColor(0);

    SkCanvas canvas(bitmap);
    canvas.drawARGB(0, 0, 0, 0, SkXfermode::kClear_Mode);
    RenderSkinMediaButton::Draw(&canvas, m_buttonRect, buttonType, true, false);

    GLuint texture;
    glGenTextures(1, &texture);

    GLUtils::createTextureWithBitmap(texture, bitmap);
    bitmap.reset();
    return texture;
}

// Should be called at the VideoLayerAndroid::drawGL to make sure we allocate
// the GL resources lazily.
void VideoLayerManager::initGLResourcesIfNeeded()
{
    if (!m_createdTexture) {
        ALOGD("Reinit GLResource for VideoLayer");
        initGLResources();
    }
}

void VideoLayerManager::initGLResources()
{
    GLUtils::checkGlError("before initGLResources()");
    m_spinnerOuterTextureId =
        createTextureFromImage(RenderSkinMediaButton::SPINNER_OUTER);
    m_spinnerInnerTextureId =
        createTextureFromImage(RenderSkinMediaButton::SPINNER_INNER);
    m_posterTextureId =
        createTextureFromImage(RenderSkinMediaButton::VIDEO);
    m_playTextureId = createTextureFromImage(RenderSkinMediaButton::PLAY);
    m_pauseTextureId = createTextureFromImage(RenderSkinMediaButton::PAUSE);

    m_createdTexture = !GLUtils::checkGlError("initGLResources()");
    return;
}

void VideoLayerManager::cleanupGLResources()
{
    if (m_createdTexture) {
        GLuint videoTextures[VIDEO_TEXTURE_NUMBER] = { m_spinnerOuterTextureId,
            m_spinnerInnerTextureId, m_posterTextureId, m_playTextureId,
            m_pauseTextureId };

        glDeleteTextures(VIDEO_TEXTURE_NUMBER, videoTextures);
        m_createdTexture = false;
    }
    // Delete the texture in retired mode, but have not hit draw call to be
    // removed.
    deleteUnusedTextures();

    // Go over the registered GL textures (screen shot textures) and delete them.
    android::Mutex::Autolock lock(m_videoLayerInfoMapLock);
    InfoIterator end = m_videoLayerInfoMap.end();
    for (InfoIterator it = m_videoLayerInfoMap.begin(); it != end; ++it) {
        // The map include every video has been played, so their textureId can
        // be deleted already, like hitting onTrimMemory multiple times.
        if (it->second->textureId) {
            ALOGV("delete texture from the map %d", it->second->textureId);
            glDeleteTextures(1, &it->second->textureId);
            // Set the textureID to 0 to show the video icon.
            it->second->textureId = 0;
        }
    }

    GLUtils::checkGlError("cleanupGLResources()");
    return;
}

// Getting TextureId for GL draw call, in the UI thread.
GLuint VideoLayerManager::getTextureId(const int layerId)
{
    android::Mutex::Autolock lock(m_videoLayerInfoMapLock);
    GLuint result = 0;
    if (m_videoLayerInfoMap.contains(layerId))
        result = m_videoLayerInfoMap.get(layerId)->textureId;
    return result;
}

// Getting the aspect ratio for GL draw call, in the UI thread.
float VideoLayerManager::getAspectRatio(const int layerId)
{
    android::Mutex::Autolock lock(m_videoLayerInfoMapLock);
    float result = 0;
    if (m_videoLayerInfoMap.contains(layerId))
        result = m_videoLayerInfoMap.get(layerId)->aspectRatio;
    return result;
}

// Getting matrix for GL draw call, in the UI thread.
GLfloat* VideoLayerManager::getMatrix(const int layerId)
{
    android::Mutex::Autolock lock(m_videoLayerInfoMapLock);
    GLfloat* result = 0;
    if (m_videoLayerInfoMap.contains(layerId))
        result = m_videoLayerInfoMap.get(layerId)->surfaceMatrix;
    return result;
}

int VideoLayerManager::getTotalMemUsage()
{
    int sum = 0;
    InfoIterator end = m_videoLayerInfoMap.end();
    for (InfoIterator it = m_videoLayerInfoMap.begin(); it != end; ++it)
        sum += it->second->videoSize;
    return sum;
}

// When the video start, we know its texture info, so we register when we
// recieve the setSurfaceTexture call, this happens on UI thread.
void VideoLayerManager::registerTexture(const int layerId, const GLuint textureId)
{
    android::Mutex::Autolock lock(m_videoLayerInfoMapLock);
    // If the texture has been registered, then early return.
    if (m_videoLayerInfoMap.get(layerId)) {
        GLuint oldTextureId = m_videoLayerInfoMap.get(layerId)->textureId;
        if (oldTextureId != textureId)
            removeLayerInternal(layerId);
        else
            return;
    }
    // The old info is deleted and now complete the new info and store it.
    VideoLayerInfo* pInfo = new VideoLayerInfo();
    pInfo->textureId = textureId;
    memset(pInfo->surfaceMatrix, 0, sizeof(pInfo->surfaceMatrix));
    pInfo->videoSize = 0;
    pInfo->aspectRatio = DEFAULT_VIDEO_ASPECT_RATIO;
    m_currentTimeStamp++;
    pInfo->timeStamp = m_currentTimeStamp;
    pInfo->lastIconShownTime = 0;
    pInfo->iconState = Registered;

    m_videoLayerInfoMap.add(layerId, pInfo);
    ALOGV("GL texture %d regisered for layerId %d", textureId, layerId);

    return;
}

// Only when the video is prepared, we got the video size. So we should update
// the size for the video accordingly.
// This is called from webcore thread, from MediaPlayerPrivateAndroid.
void VideoLayerManager::updateVideoLayerSize(const int layerId, const int size,
                                             const float ratio)
{
    android::Mutex::Autolock lock(m_videoLayerInfoMapLock);
    if (m_videoLayerInfoMap.contains(layerId)) {
        VideoLayerInfo* pInfo = m_videoLayerInfoMap.get(layerId);
        if (pInfo) {
            pInfo->videoSize = size;
            pInfo->aspectRatio = ratio;
        }
    }

    // If the memory usage is out of bound, then just delete the oldest ones.
    // Because we only recycle the texture before the current timestamp, the
    // current video's texture will not be deleted.
    while (getTotalMemUsage() > MAX_VIDEOSIZE_SUM)
        if (!recycleTextureMem())
            break;
    return;
}

// This is called only from UI thread, at drawGL time.
void VideoLayerManager::updateMatrix(const int layerId, const GLfloat* matrix)
{
    android::Mutex::Autolock lock(m_videoLayerInfoMapLock);
    if (m_videoLayerInfoMap.contains(layerId)) {
        // If the existing video layer's matrix is matching the incoming one,
        // then skip the update.
        VideoLayerInfo* pInfo = m_videoLayerInfoMap.get(layerId);
        ASSERT(matrix);
        if (pInfo && !memcmp(matrix, pInfo->surfaceMatrix, sizeof(pInfo->surfaceMatrix)))
            return;
        memcpy(pInfo->surfaceMatrix, matrix, sizeof(pInfo->surfaceMatrix));
    } else {
        ALOGV("Error: should not reach here, the layerId %d should exist!", layerId);
        ASSERT(false);
    }
    return;
}

// This is called on the webcore thread, save the GL texture for recycle in
// the retired queue. They will be deleted in deleteUnusedTextures() in the UI
// thread.
// Return true when we found one texture to retire.
bool VideoLayerManager::recycleTextureMem()
{
    // Find the oldest texture int the m_videoLayerInfoMap, put it in m_retiredTextures
    int oldestTimeStamp = m_currentTimeStamp;
    int oldestLayerId = -1;

    InfoIterator end = m_videoLayerInfoMap.end();
#ifdef DEBUG
    ALOGV("VideoLayerManager::recycleTextureMem m_videoLayerInfoMap contains");
    for (InfoIterator it = m_videoLayerInfoMap.begin(); it != end; ++it)
        ALOGV("  layerId %d, textureId %d, videoSize %d, timeStamp %d ",
              it->first, it->second->textureId, it->second->videoSize, it->second->timeStamp);
#endif
    for (InfoIterator it = m_videoLayerInfoMap.begin(); it != end; ++it) {
        if (it->second->timeStamp < oldestTimeStamp) {
            oldestTimeStamp = it->second->timeStamp;
            oldestLayerId = it->first;
        }
    }

    bool foundTextureToRetire = (oldestLayerId != -1);
    if (foundTextureToRetire)
        removeLayerInternal(oldestLayerId);

    return foundTextureToRetire;
}

// This is only called in the UI thread, b/c glDeleteTextures need to be called
// on the right context.
void VideoLayerManager::deleteUnusedTextures()
{
    m_retiredTexturesLock.lock();
    int size = m_retiredTextures.size();
    if (size > 0) {
        GLuint* textureNames = new GLuint[size];
        int index = 0;
        Vector<GLuint>::const_iterator end = m_retiredTextures.end();
        for (Vector<GLuint>::const_iterator it = m_retiredTextures.begin();
             it != end; ++it) {
            GLuint textureName = *it;
            if (textureName) {
                textureNames[index] = textureName;
                index++;
                ALOGV("GL texture %d will be deleted", textureName);
            }
        }
        glDeleteTextures(size, textureNames);
        delete textureNames;
        m_retiredTextures.clear();
    }
    m_retiredTexturesLock.unlock();
    GLUtils::checkGlError("deleteUnusedTextures");
    return;
}

// This can be called in the webcore thread in the media player's dtor.
void VideoLayerManager::removeLayer(const int layerId)
{
    android::Mutex::Autolock lock(m_videoLayerInfoMapLock);
    removeLayerInternal(layerId);
}

// This can be called on both UI and webcore thread. Since this is a private
// function, it is up to the public function to handle the lock for
// m_videoLayerInfoMap.
void VideoLayerManager::removeLayerInternal(const int layerId)
{
    // Delete the layerInfo corresponding to this layerId and remove from the map.
    if (m_videoLayerInfoMap.contains(layerId)) {
        GLuint textureId = m_videoLayerInfoMap.get(layerId)->textureId;
        if (textureId) {
            // Buffer up the retired textures in either UI or webcore thread,
            // will be purged at deleteUnusedTextures in the UI thread.
            m_retiredTexturesLock.lock();
            m_retiredTextures.append(textureId);
            m_retiredTexturesLock.unlock();
        }
        delete m_videoLayerInfoMap.get(layerId);
        m_videoLayerInfoMap.remove(layerId);
    }
    return;
}

double VideoLayerManager::drawIcon(const int layerId, IconType type)
{
    // When ratio 0 is returned, the Icon should not be drawn.
    double ratio = 0;

    android::Mutex::Autolock lock(m_videoLayerInfoMapLock);
    if (m_videoLayerInfoMap.contains(layerId)) {
        VideoLayerInfo* pInfo = m_videoLayerInfoMap.get(layerId);
        // If this is state switching moment, reset the time and state
        if ((type == PlayIcon && pInfo->iconState != PlayIconShown)
            || (type == PauseIcon && pInfo->iconState != PauseIconShown)) {
            pInfo->lastIconShownTime = WTF::currentTime();
            pInfo->iconState = (type == PlayIcon) ? PlayIconShown : PauseIconShown;
        }

        // After switching the state, we calculate the ratio depending on the
        // time interval.
        if ((type == PlayIcon && pInfo->iconState == PlayIconShown)
            || (type == PauseIcon && pInfo->iconState == PauseIconShown)) {
            double delta = WTF::currentTime() - pInfo->lastIconShownTime;
            ratio = 1.0 - (delta / PLAY_PAUSE_ICON_SHOW_TIME);
        }
    }

    if (ratio > 1 || ratio < 0)
        ratio = 0;
    return ratio;
}

}
#endif // USE(ACCELERATED_COMPOSITING)
