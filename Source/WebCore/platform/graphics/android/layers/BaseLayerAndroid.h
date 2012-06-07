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

class Image;
class RenderLayerCompositor;
class RenderStyle;
class BackgroundImagePositioning;

class BaseLayerAndroid : public LayerAndroid {
public:
    BaseLayerAndroid(LayerContent* content);
    virtual SubclassType subclassType() const { return LayerAndroid::BaseLayer; }
    virtual void getLocalTransform(SkMatrix* matrix) const;
    virtual const TransformationMatrix* drawTransform() const { return 0; }
    virtual bool needsTexture() { return content(); }
    virtual IFrameLayerAndroid* updatePosition(SkRect viewport,
                                               IFrameLayerAndroid* parentIframeLayer);
    void updatePositionsRecursive(const SkRect& visibleContentRect);
    void setBackgroundColor(Color& color) { m_color = color; }
    Color getBackgroundColor() { return m_color; }

private:
    // TODO: move to SurfaceCollection.
    Color m_color;
    bool m_positionsCalculated;
};

class ForegroundBaseLayerAndroid : public LayerAndroid {
public:
    ForegroundBaseLayerAndroid(LayerContent* content);
    virtual SubclassType subclassType() const { return LayerAndroid::ForegroundBaseLayer; }

    virtual bool needsTexture() { return false; }
};

class FixedBackgroundImageLayerAndroid : public LayerAndroid {
public:
    FixedBackgroundImageLayerAndroid(PassRefPtr<RenderStyle> style, int w, int h);
    FixedBackgroundImageLayerAndroid(const FixedBackgroundImageLayerAndroid& layer);
    virtual ~FixedBackgroundImageLayerAndroid();
    virtual LayerAndroid* copy() const { return new FixedBackgroundImageLayerAndroid(*this); }
    virtual bool needsTexture() { return true; }
    virtual SubclassType subclassType() const { return LayerAndroid::FixedBackgroundImageLayer; }
    virtual bool drawGL(bool layerTilesDisabled);
    static Image* GetCachedImage(PassRefPtr<RenderStyle> style);

private:
    bool drawSimpleQuad(ImageTexture* imageTexture,
                        BackgroundImagePositioning* position,
                        const IntPoint& repeatTimes, const FloatPoint& startPoint,
                        const FloatPoint& origin, const Color& backgroundColor);
    void drawRepeatedGrid(ImageTexture* imageTexture,
                          BackgroundImagePositioning* position,
                          const IntPoint& repeatTimes, const FloatPoint& startPoint,
                          const FloatPoint& origin, const Color& backgroundColor);
    int m_width;
    int m_height;
};

} // namespace WebCore

#endif //BaseLayerAndroid_h
