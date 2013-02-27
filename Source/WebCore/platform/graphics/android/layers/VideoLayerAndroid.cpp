/*
 * Copyright 2011 The Android Open Source Project
 * Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
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

#define LOG_TAG "VideoLayerAndroid"
#define LOG_NDEBUG 1

#include "config.h"
#include "VideoLayerAndroid.h"

#include "AndroidLog.h"
#include "DrawQuadData.h"
#include "ShaderProgram.h"
#include "TilesManager.h"
#include <GLES2/gl2.h>
#include <gui/GLConsumer.h>
#include "SkBitmapRef.h"

#if USE(ACCELERATED_COMPOSITING)

namespace WebCore {

double VideoLayerAndroid::m_rotateDegree = 0;

VideoLayerAndroid::VideoLayerAndroid()
    : LayerAndroid((RenderLayer*)0)
    // Create mutex and condition variables in WebCoreLayer type instances only.
    // Instances created with type UILayer will share the same mutex and
    // condition variables as the source VideoLayerAndroid.
    , m_frameCaptureMutex(new FrameCaptureMutex())
{
    // New FrameCaptureMutex with assignment during initialization results
    // in double ref counting. Call unref to correct it.
    m_frameCaptureMutex->unref();
}

VideoLayerAndroid::VideoLayerAndroid(const VideoLayerAndroid& layer)
    : LayerAndroid(layer)
    // Copy constructor sets the same mutex variables as the source
    // VideoLayerAndroid. This will increment the ref count.
    , m_frameCaptureMutex(layer.m_frameCaptureMutex)
{
}

// We can use this function to set the Layer to point to surface texture.
void VideoLayerAndroid::setSurfaceTexture(sp<GLConsumer> texture,
                                          int textureName, PlayerState playerState)
{
    m_surfaceTexture = texture;
    TilesManager::instance()->videoLayerManager()->registerTexture(uniqueId(), textureName);
    TilesManager::instance()->videoLayerManager()->updatePlayerState(uniqueId(), playerState);
}

void VideoLayerAndroid::showPreparingAnimation(const SkRect& rect,
                                               const SkRect innerRect)
{
    ShaderProgram* shader = TilesManager::instance()->shader();
    VideoLayerManager* manager = TilesManager::instance()->videoLayerManager();
    // Paint the video content's background.
    PureColorQuadData backGroundQuadData(Color(128, 128, 128, 255), LayerQuad,
                                         &m_drawTransform, &rect);
    shader->drawQuad(&backGroundQuadData);

    TransformationMatrix addReverseRotation;
    TransformationMatrix addRotation = m_drawTransform;
    addRotation.translate(innerRect.fLeft, innerRect.fTop);
    double halfButtonSize = manager->getButtonSize() / 2;
    addRotation.translate(halfButtonSize, halfButtonSize);
    addReverseRotation = addRotation;
    addRotation.rotate(m_rotateDegree);
    addRotation.translate(-halfButtonSize, -halfButtonSize);

    SkRect size = SkRect::MakeWH(innerRect.width(), innerRect.height());

    TextureQuadData spinnerQuadData(manager->getSpinnerOuterTextureId(),
                                    GL_TEXTURE_2D, GL_LINEAR,
                                    LayerQuad, &addRotation, &size);
    shader->drawQuad(&spinnerQuadData);

    addReverseRotation.rotate(-m_rotateDegree);
    addReverseRotation.translate(-halfButtonSize, -halfButtonSize);

    spinnerQuadData.updateTextureId(manager->getSpinnerInnerTextureId());
    spinnerQuadData.updateDrawMatrix(&addReverseRotation);
    shader->drawQuad(&spinnerQuadData);

    m_rotateDegree += ROTATESTEP;
}

SkRect VideoLayerAndroid::calVideoRect(const SkRect& rect)
{
    SkRect videoRect = rect;
    VideoLayerManager* manager = TilesManager::instance()->videoLayerManager();
    float aspectRatio = manager->getAspectRatio(uniqueId());
    float deltaY = rect.height() - rect.width() / aspectRatio;
    if (deltaY >= 0)
        videoRect.inset(0, deltaY / 2);
    else {
        float deltaX = rect.width() - rect.height() * aspectRatio;
        if (deltaX >= 0)
            videoRect.inset(deltaX / 2, 0);
    }
    return videoRect;
}

bool VideoLayerAndroid::drawGL(bool layerTilesDisabled)
{
    // Lazily allocated the textures.
    TilesManager* tilesManager = TilesManager::instance();
    VideoLayerManager* manager = tilesManager->videoLayerManager();
    manager->initGLResourcesIfNeeded();

    ShaderProgram* shader = tilesManager->shader();

    SkRect rect = SkRect::MakeSize(getSize());
    GLfloat surfaceMatrix[16];

    // Calculate the video rect based on the aspect ratio and the element rect.
    SkRect videoRect = calVideoRect(rect);
    PureColorQuadData pureColorQuadData(Color(0, 0, 0, 255), LayerQuad,
                                        &m_drawTransform, &rect);

    if (videoRect != rect) {
        // Paint the whole video element with black color when video content
        // can't cover the whole area.
        shader->drawQuad(&pureColorQuadData);
    }

    // Inner rect is for the progressing / play / pause animation.
    SkRect innerRect = SkRect::MakeWH(manager->getButtonSize(),
                                      manager->getButtonSize());
    if (innerRect.contains(videoRect))
        innerRect = videoRect;
    double buttonSize = manager->getButtonSize();
    innerRect.offset(videoRect.fLeft + (videoRect.width() - buttonSize) / 2,
                     videoRect.fTop + (videoRect.height() - buttonSize) / 2);

    // When we are drawing the animation of the play/pause button in the
    // middle of the video, we need to ask for redraw.
    bool needRedraw = false;
    TextureQuadData iconQuadData(0, GL_TEXTURE_2D, GL_LINEAR, LayerQuad,
                                 &m_drawTransform, &innerRect);
    // Draw the poster image, the progressing image or the Video depending
    // on the player's state.
    PlayerState playerState = manager->getPlayerState(uniqueId());

    if (playerState == PREPARING) {
        // Show the progressing animation, with two rotating circles
        showPreparingAnimation(videoRect, innerRect);
        needRedraw = true;
    } else if (playerState == PLAYING && m_surfaceTexture.get()) {
        // Show the real video.
        m_surfaceTexture->updateTexImage();
        m_surfaceTexture->getTransformMatrix(surfaceMatrix);
        GLuint textureId = manager->getTextureId(uniqueId());
        shader->drawVideoLayerQuad(m_drawTransform, surfaceMatrix,
                                   videoRect, textureId);
        manager->updateMatrix(uniqueId(), surfaceMatrix);

        // Use the scale to control the fading the sizing during animation
        double scale = manager->drawIcon(uniqueId(), PlayIcon);
        if (scale) {
            innerRect.inset(manager->getButtonSize() / 4 * scale,
                            manager->getButtonSize() / 4 * scale);
            iconQuadData.updateTextureId(manager->getPlayTextureId());
            iconQuadData.updateOpacity(scale);
            shader->drawQuad(&iconQuadData);
            needRedraw = true;
        }
    } else {
        GLuint textureId = manager->getTextureId(uniqueId());
        GLfloat* matrix = manager->getMatrix(uniqueId());
        if (textureId && matrix) {
            // Show the screen shot for each video.
            shader->drawVideoLayerQuad(m_drawTransform, matrix,
                                       videoRect, textureId);
        } else {
            // Show the static poster b/c there is no screen shot available.
            pureColorQuadData.updateColor(Color(128, 128, 128, 255));
            shader->drawQuad(&pureColorQuadData);

            iconQuadData.updateTextureId(manager->getPosterTextureId());
            iconQuadData.updateOpacity(1.0);
            shader->drawQuad(&iconQuadData);
        }

        // Use the scale to control the fading and the sizing during animation.
        double scale = manager->drawIcon(uniqueId(), PauseIcon);
        if (scale) {
            innerRect.inset(manager->getButtonSize() / 4 * scale,
                            manager->getButtonSize() / 4 * scale);
            iconQuadData.updateTextureId(manager->getPauseTextureId());
            iconQuadData.updateOpacity(scale);
            shader->drawQuad(&iconQuadData);
            needRedraw = true;
        }
    }
    // Check if there is a pending request to capture video frame
    if (manager->serviceFrameCapture(uniqueId())) {
        serviceFrameCapture();
    }
    return needRedraw;
}

void VideoLayerAndroid::serviceFrameCapture() {
    // Should not arrive here without the mutex and condition variable already created
    ASSERT(m_frameCaptureMutex != NULL);
    VideoLayerManager* manager = TilesManager::instance()->videoLayerManager();
    GLuint textureId = manager->getTextureId(uniqueId());
    GLfloat* matrix = manager->getMatrix(uniqueId());
    IntSize videoSize = manager->getVideoNaturalSize(uniqueId());
    PlayerState playerState = manager->getPlayerState(uniqueId());
    SkBitmap videoFrame;
    // Use ARGB format for video frame capture bitmap
    videoFrame.setConfig(SkBitmap::kARGB_8888_Config, videoSize.width(),
            videoSize.height());
    // Allocate SkBitmap pixels using the default allocator
    videoFrame.allocPixels(NULL, 0);
    videoFrame.eraseColor(SK_ColorBLACK);
    SkRect rect = SkRect::MakeWH(SkIntToScalar(videoSize.width()),
            SkIntToScalar(videoSize.height()));
    ShaderProgram* shader = TilesManager::instance()->shader();
    if ((playerState != PREPARED && playerState != PLAYING) ||
            !m_surfaceTexture.get() || !textureId || !matrix)
        ALOGE("serviceFrameCapture() called in bad state");
    else
        shader->drawVideoLayerToBitmap(matrix, rect, textureId, videoFrame);

    SkBitmapRef* bitmapRef = new SkBitmapRef(videoFrame);
    manager->pushBitmap(uniqueId(), bitmapRef);
    SkSafeUnref(bitmapRef);
    // Signal the frame capture client
    android::AutoMutex lock(m_frameCaptureMutex->mutex());
    m_frameCaptureMutex->condition().signal();
}

// This is called from the webcore thread
bool VideoLayerAndroid::copyToBitmap(SkBitmapRef*& bitmapRef) {
    VideoLayerManager* manager = TilesManager::instance()->videoLayerManager();
    if (m_frameCaptureMutex == NULL) {
        ALOGE("Video frame capture error bad state");
        return false;
    }

    m_frameCaptureMutex->mutex().lock();
    manager->requestFrameCapture(uniqueId());

    // Timeout waiting for the video frame capture in the UI thread.
    // Due to context switching, this could take up to a few hundred milliseconds.
    // Set this to a value long enough that we won't get a premature timeout for
    // a non-error situation.
    static const nsecs_t DRAW_VIDEO_FRAME_TIMEOUT = 1LL*1000*1000*1000; // one second
    status_t sts = m_frameCaptureMutex->condition().waitRelative(
            m_frameCaptureMutex->mutex(), DRAW_VIDEO_FRAME_TIMEOUT);
    m_frameCaptureMutex->mutex().unlock();

    if (sts == android::TIMED_OUT) {
        ALOGE("Video frame capture timed out");
        return false;
    }
    if (sts != android::OK) {
        ALOGE("Video frame capture wait condition error %d", sts);
        return false;
    }
    bitmapRef = manager->popBitmap(uniqueId());
    return true;
}

}
#endif // USE(ACCELERATED_COMPOSITING)
