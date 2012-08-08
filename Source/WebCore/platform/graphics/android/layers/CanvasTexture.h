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

#ifndef CanvasTexture_h
#define CanvasTexture_h

#if USE(ACCELERATED_COMPOSITING)

#include "CanvasLayer.h"

#include <wtf/RefPtr.h>
#include <utils/threads.h>

namespace android {
class GLConsumer;
class Surface;
}

namespace WebCore {

class CanvasTexture : public ThreadSafeRefCounted<CanvasTexture> {

public:
    /********************************************
     * Called by both threads
     ********************************************/
    static PassRefPtr<CanvasTexture> getCanvasTexture(CanvasLayer* layer);
    bool setHwAccelerated(bool hwAccelerated);

    /********************************************
     * Called by WebKit thread
     ********************************************/
    void setSize(const IntSize& size);
    android::Surface* nativeWindow();
    bool uploadImageBuffer(ImageBuffer* imageBuffer);
    bool uploadImageBitmap(SkBitmap* bitmap);
    bool hasValidTexture() { return m_hasValidTexture; }

    /********************************************
     * Called by UI thread WITH GL context
     ********************************************/
    virtual ~CanvasTexture();
    void requireTexture();
    GLuint texture() { requireTexture(); return m_texture; }
    bool updateTexImage();

private:
    /********************************************
     * Called by both threads
     ********************************************/
    void destroySurfaceTextureLocked();

    /********************************************
     * Called by WebKit thread
     ********************************************/
    CanvasTexture(int layerId);
    bool useSurfaceTexture();

    IntSize m_size;
    int m_layerId;
    GLuint m_texture;
    android::Mutex m_surfaceLock;
    sp<android::GLConsumer> m_surfaceTexture;
    sp<android::Surface> m_ANW;
    bool m_hasValidTexture;
    bool m_useHwAcceleration;

};

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)

#endif // CanvasTexture_h
