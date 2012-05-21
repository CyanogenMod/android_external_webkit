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

#ifndef CanvasLayer_h
#define CanvasLayer_h

#if USE(ACCELERATED_COMPOSITING)

#include "HTMLCanvasElement.h"
#include "ImageData.h"
#include "LayerAndroid.h"
#include "RenderLayer.h"

#include <wtf/RefPtr.h>

namespace WebCore {

class CanvasTexture;

class CanvasLayer : public LayerAndroid, private CanvasObserver {
public:
    CanvasLayer(RenderLayer* owner, HTMLCanvasElement* canvas);
    CanvasLayer(const CanvasLayer& layer);
    virtual ~CanvasLayer();

    virtual LayerAndroid* copy() const { return new CanvasLayer(*this); }
    virtual SubclassType subclassType() const { return LayerAndroid::CanvasLayer; }
    virtual void clearDirtyRegion();

    virtual bool drawGL(bool layerTilesDisabled);
    virtual void contentDraw(SkCanvas* canvas, PaintStyle style);
    virtual bool needsTexture();
    virtual bool needsIsolatedSurface() { return true; }

protected:
    virtual InvalidateFlags onSetHwAccelerated(bool hwAccelerated);

private:
    virtual void canvasChanged(HTMLCanvasElement*, const FloatRect& changedRect);
    virtual void canvasResized(HTMLCanvasElement*);
    virtual void canvasDestroyed(HTMLCanvasElement*);

    void init();
    SkBitmapRef* bitmap() const;
    IntRect visibleContentRect() const;
    IntSize offsetFromRenderer() const;

    HTMLCanvasElement* m_canvas;
    IntRect m_visibleContentRect;
    IntSize m_offsetFromRenderer;
    SkRegion m_dirtyCanvas;
    SkBitmapRef* m_bitmap;
    RefPtr<CanvasTexture> m_texture;
};

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)

#endif // CanvasLayer_h
