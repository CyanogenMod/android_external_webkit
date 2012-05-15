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

#ifndef IFrameLayerAndroid_h
#define IFrameLayerAndroid_h

#if USE(ACCELERATED_COMPOSITING)

#include "LayerAndroid.h"

namespace WebCore {

class IFrameLayerAndroid : public LayerAndroid {

public:
    IFrameLayerAndroid(RenderLayer* owner)
        : LayerAndroid(owner) {}
    IFrameLayerAndroid(const LayerAndroid& layer)
        : LayerAndroid(layer) {}
    IFrameLayerAndroid(const IFrameLayerAndroid& layer)
        : LayerAndroid(layer)
        , m_iframeOffset(layer.m_iframeOffset) {}

    virtual ~IFrameLayerAndroid() {};

    virtual bool isIFrame() const { return true; }

    virtual LayerAndroid* copy() const { return new IFrameLayerAndroid(*this); }
    virtual SubclassType subclassType() const { return LayerAndroid::IFrameLayer; }

    virtual IFrameLayerAndroid* updatePosition(SkRect viewport,
                                               IFrameLayerAndroid* parentIframeLayer);

    virtual void dumpLayer(FILE*, int indentLevel) const;

    const IntPoint& iframeOffset() const { return m_iframeOffset; }

private:
    IntPoint m_iframeOffset;
};

}

#endif // USE(ACCELERATED_COMPOSITING)

#endif // IFrameLayerAndroid_h
