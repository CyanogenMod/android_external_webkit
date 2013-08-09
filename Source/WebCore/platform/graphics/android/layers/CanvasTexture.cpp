/*
 * Copyright 2012, The Android Open Source Project
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

#define LOG_TAG "CanvasTexture"
#define LOG_NDEBUG 1

#include "config.h"
#include "CanvasTexture.h"

#if USE(ACCELERATED_COMPOSITING)

#include "AndroidLog.h"
#include "GLUtils.h"
#include "Image.h"
#include "ImageBuffer.h"
#include "SkBitmap.h"
#include "SkBitmapRef.h"
#include "SkDevice.h"
#include "SkPixelRef.h"

#include <android/native_window.h>
#include <gui/GLConsumer.h>
#include <gui/Surface.h>

namespace WebCore {

static int s_maxTextureSize = 0;
static HashMap<int, CanvasTexture*> s_textures;
static android::Mutex s_texturesLock;

/********************************************
 * Called by both threads
 ********************************************/

PassRefPtr<CanvasTexture> CanvasTexture::getCanvasTexture(CanvasLayer* layer)
{
    android::Mutex::Autolock lock(s_texturesLock);
    RefPtr<CanvasTexture> texture = s_textures.get(layer->uniqueId());
    if (texture.get())
        return texture.release();
    return adoptRef(new CanvasTexture(layer->uniqueId()));
}

bool CanvasTexture::setHwAccelerated(bool hwAccelerated)
{
    android::Mutex::Autolock lock(m_surfaceLock);
    if (m_useHwAcceleration == hwAccelerated)
        return false;
    m_useHwAcceleration = hwAccelerated;
    if (!m_ANW.get())
        return false;
    destroySurfaceTextureLocked();
    return true;
}

/********************************************
 * Called by WebKit thread
 ********************************************/

void CanvasTexture::setSize(const IntSize& size)
{
    android::Mutex::Autolock lock(m_surfaceLock);
    if (m_size == size)
        return;
    m_size = size;
    if (m_ANW.get()) {
        if (useSurfaceTexture()) {
            int result = native_window_set_buffers_dimensions(m_ANW.get(),
                    m_size.width(), m_size.height());
            GLUtils::checkSurfaceTextureError("native_window_set_buffers_dimensions", result);
            if (result != NO_ERROR)
                m_useHwAcceleration = false; // On error, drop out of HWA
        }
        if (!useSurfaceTexture())
            destroySurfaceTextureLocked();
    }
}

android::Surface* CanvasTexture::nativeWindow()
{
    android::Mutex::Autolock lock(m_surfaceLock);
    if (m_ANW.get())
        return m_ANW.get();
    if (!m_texture)
        return 0;
    if (!useSurfaceTexture())
        return 0;
    m_surfaceTexture = new android::GLConsumer(m_texture, false);
    m_ANW = new android::Surface(m_surfaceTexture->getBufferQueue());
    int result = native_window_set_buffers_format(m_ANW.get(), HAL_PIXEL_FORMAT_RGBA_8888);
    GLUtils::checkSurfaceTextureError("native_window_set_buffers_format", result);
    if (result == NO_ERROR) {
        result = native_window_set_buffers_dimensions(m_ANW.get(),
                m_size.width(), m_size.height());
        GLUtils::checkSurfaceTextureError("native_window_set_buffers_dimensions", result);
    }
    if (result != NO_ERROR) {
        m_useHwAcceleration = false;
        destroySurfaceTextureLocked();
        return 0;
    }
    return m_ANW.get();
}

bool CanvasTexture::uploadImageBitmap(SkBitmap* bitmap)
{
    m_hasValidTexture = false;
    android::Surface* anw = nativeWindow();
    if (!anw)
        return false;
    // Size mismatch, early abort (will fall back to software)
    if (bitmap->width() != m_size.width() ||
            bitmap->height() != m_size.height())
        return false;

    if (!GLUtils::updateSharedSurfaceTextureWithBitmap(anw, *bitmap))
        return false;
    m_hasValidTexture = true;
    return true;
}

bool CanvasTexture::uploadImageBuffer(ImageBuffer* imageBuffer)
{
    m_hasValidTexture = false;
    if (!imageBuffer)
        return false;
    android::Surface* anw = nativeWindow();
    if (!anw)
        return false;
    // Size mismatch, early abort (will fall back to software)
    if (imageBuffer->size() != m_size)
        return false;
    SkCanvas* canvas = imageBufferCanvas(imageBuffer);
    if (!canvas)
        return false;
    const SkBitmap& bitmap = canvas->getDevice()->accessBitmap(false);
    if (!GLUtils::updateSharedSurfaceTextureWithBitmap(anw, bitmap))
        return false;
    m_hasValidTexture = true;
    return true;
}

/********************************************
 * Called by UI thread WITH GL context
 ********************************************/

CanvasTexture::~CanvasTexture()
{
    if (m_layerId) {
        s_texturesLock.lock();
        s_textures.remove(m_layerId);
        s_texturesLock.unlock();
    }
    if (m_texture)
        GLUtils::deleteTexture(&m_texture);
}

void CanvasTexture::requireTexture()
{
    android::Mutex::Autolock lock(m_surfaceLock);
    if (!m_texture)
        glGenTextures(1, &m_texture);
    if (!s_maxTextureSize)
        glGetIntegerv(GL_MAX_TEXTURE_SIZE, &s_maxTextureSize);
}

bool CanvasTexture::updateTexImage()
{
    android::Mutex::Autolock lock(m_surfaceLock);
    if (!m_surfaceTexture.get())
        return false;
    status_t result = m_surfaceTexture->updateTexImage();
    if (result != OK) {
        ALOGE("unexpected error: updateTexImage return %d", result);
        return false;
    }
    return true;
}

/********************************************
 * Called by both threads
 ********************************************/

void CanvasTexture::destroySurfaceTextureLocked()
{
    if (m_ANW.get()) {
        m_ANW.clear();
        m_surfaceTexture->abandon();
        m_surfaceTexture.clear();
    }
}

/********************************************
 * Called by WebKit thread
 ********************************************/

CanvasTexture::CanvasTexture(int layerId)
    : m_size()
    , m_layerId(layerId)
    , m_texture(0)
    , m_surfaceTexture(0)
    , m_ANW(0)
    , m_hasValidTexture(false)
    , m_useHwAcceleration(true)
{
    s_textures.add(m_layerId, this);
}

// TODO: Have a global limit as well as a way to react to low memory situations
bool CanvasTexture::useSurfaceTexture()
{
    if (!m_useHwAcceleration)
        return false;
    if (m_size.isEmpty())
        return false;
    return (m_size.width() < s_maxTextureSize) && (m_size.height() < s_maxTextureSize);
}

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
