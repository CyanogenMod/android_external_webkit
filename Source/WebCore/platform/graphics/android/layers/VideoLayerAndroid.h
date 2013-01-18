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

#ifndef VideoLayerAndroid_h
#define VideoLayerAndroid_h

#if USE(ACCELERATED_COMPOSITING)

#include "GLUtils.h"
#include "LayerAndroid.h"
#include "VideoLayerManager.h"
#include <jni.h>

namespace android {
class GLConsumer;
}

namespace WebCore {

// This instance is shared between the WebCore and UI copies of VideoLayerAndroid
class FrameCaptureMutex : public SkRefCnt {
public:
    android::Condition& condition() { return m_condition; }
    android::Mutex& mutex() { return m_mutex; }
private:
    android::Condition m_condition;
    android::Mutex m_mutex;
};

class VideoLayerAndroid : public LayerAndroid {

public:
    VideoLayerAndroid();
    explicit VideoLayerAndroid(const VideoLayerAndroid& layer);

    virtual bool isVideo() const { return true; }

    virtual LayerAndroid* copy() const { return new VideoLayerAndroid(*this); }

    // The following functions are called in UI thread only.
    virtual bool drawGL(bool layerTilesDisabled);
    void setSurfaceTexture(sp<GLConsumer> texture, int textureName, PlayerState playerState);
    virtual bool needsIsolatedSurface() { return true; }
    bool copyToBitmap(SkBitmapRef*& bitmapRef);
private:
    void showPreparingAnimation(const SkRect& rect,
                                const SkRect innerRect);
    SkRect calVideoRect(const SkRect& rect);
    void serviceFrameCapture();
    // Surface texture for showing the video is actually allocated in Java side
    // and passed into this native code.
    sp<android::GLConsumer> m_surfaceTexture;

    static double m_rotateDegree;

    static const int ROTATESTEP = 12;

    // Used for signalling between rendering and UI thread for
    // video frame capture case
    SkRefPtr<FrameCaptureMutex> m_frameCaptureMutex;
};

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)

#endif // VideoLayerAndroid_h
