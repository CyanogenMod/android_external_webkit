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

#ifndef IFrameContentLayerAndroid_h
#define IFrameContentLayerAndroid_h

#if USE(ACCELERATED_COMPOSITING)

#include "ScrollableLayerAndroid.h"

namespace WebCore {

class IFrameContentLayerAndroid : public ScrollableLayerAndroid {

public:
    IFrameContentLayerAndroid(RenderLayer* owner)
        : ScrollableLayerAndroid(owner) {}
    IFrameContentLayerAndroid(const ScrollableLayerAndroid& layer)
        : ScrollableLayerAndroid(layer) {}
    IFrameContentLayerAndroid(const LayerAndroid& layer)
        : ScrollableLayerAndroid(layer) {}
    IFrameContentLayerAndroid(const IFrameContentLayerAndroid& layer)
        : ScrollableLayerAndroid(layer)
        , m_iframeScrollOffset(layer.m_iframeScrollOffset) {}

    virtual ~IFrameContentLayerAndroid() {};

    // isIFrame() return true for compatibility reason (see ViewStateSerializer)
    virtual bool isIFrame() const { return true; }
    virtual bool isIFrameContent() const { return true; }

    virtual LayerAndroid* copy() const { return new IFrameContentLayerAndroid(*this); }
    virtual SubclassType subclassType() const { return LayerAndroid::IFrameContentLayer; }

    // Scrolls to the given position in the layer.
    // Returns whether or not any scrolling was required.
    virtual bool scrollTo(int x, int y);

    // Fills the rect with the current scroll offset and the maximum scroll offset.
    // fLeft   = scrollX
    // fTop    = scrollY
    // fRight  = maxScrollX
    // fBottom = maxScrollY
    virtual void getScrollRect(SkIRect*) const;

    void setIFrameScrollOffset(IntPoint offset) { m_iframeScrollOffset = offset; }

private:
    IntPoint m_iframeScrollOffset;
};

}

#endif // USE(ACCELERATED_COMPOSITING)

#endif // IFrameContentLayerAndroid_h
