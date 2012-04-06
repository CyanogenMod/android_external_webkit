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

#ifndef BaseLayerAndroid_h
#define BaseLayerAndroid_h

#include "Color.h"
#include "LayerAndroid.h"

namespace WebCore {

class RenderLayerCompositor;

class BaseLayerAndroid : public LayerAndroid {
public:
    BaseLayerAndroid(LayerContent* content);
    virtual SubclassType subclassType() { return LayerAndroid::BaseLayer; }
    virtual void getLocalTransform(SkMatrix* matrix) const;
    virtual const TransformationMatrix* drawTransform() const { return 0; }
    virtual bool needsTexture() { return content(); }
    virtual IFrameLayerAndroid* updatePosition(SkRect viewport,
                                               IFrameLayerAndroid* parentIframeLayer);
    void setBackgroundColor(Color& color) { m_color = color; }
    Color getBackgroundColor() { return m_color; }

private:
    // TODO: move to SurfaceCollection.
    Color m_color;
};

class ForegroundBaseLayerAndroid : public LayerAndroid {
public:
    ForegroundBaseLayerAndroid(LayerContent* content);
    virtual SubclassType subclassType() { return LayerAndroid::ForegroundBaseLayer; }

    virtual bool needsTexture() { return false; }
};

class FixedBackgroundBaseLayerAndroid : public LayerAndroid {
public:
    FixedBackgroundBaseLayerAndroid(LayerContent* content);
    virtual bool needsTexture() { return true; }
    virtual SubclassType subclassType() { return LayerAndroid::FixedBackgroundBaseLayer; }
};

} // namespace WebCore

#endif //BaseLayerAndroid_h
