/*
 * Copyright 2006, The Android Open Source Project
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

#ifndef platform_graphics_context_h
#define platform_graphics_context_h

#include "IntRect.h"
#include "GraphicsContext.h"
#include "RenderSkinAndroid.h"
#include "SkCanvas.h"
#include "SkPicture.h"
#include "SkTDArray.h"
#include <wtf/Vector.h>

class SkCanvas;

namespace WebCore {
    
class PlatformGraphicsContext {
public:
    PlatformGraphicsContext(SkCanvas* canvas, bool takeCanvasOwnership = false);
    ~PlatformGraphicsContext();

    void setGraphicsContext(GraphicsContext* gc) { m_gc = gc; }

    // FIXME: Make mCanvas private
    SkCanvas*                   mCanvas;
    // FIXME: This is used by ImageBufferAndroid, which should really be
    //        managing the canvas lifecycle itself
    bool deleteUs() const { return m_deleteCanvas; }

    // State management
    void beginTransparencyLayer(float opacity);
    void endTransparencyLayer();
    void save();
    void restore();

    // State values
    void setAlpha(float alpha);
    void setCompositeOperation(CompositeOperator op);
    void setFillColor(const Color& c);
    void setFillShader(SkShader* fillShader);
    void setLineCap(LineCap cap);
    void setLineDash(const DashArray& dashes, float dashOffset);
    void setLineJoin(LineJoin join);
    void setMiterLimit(float limit);
    void setShadow(int radius, int dx, int dy, SkColor c);
    void setShouldAntialias(bool useAA);
    void setStrokeColor(const Color& c);
    void setStrokeShader(SkShader* strokeShader);
    void setStrokeStyle(StrokeStyle style);
    void setStrokeThickness(float f);

    // FIXME: These setupPaint* should be private, but
    //        they are used by FontAndroid currently
    void setupPaintFill(SkPaint* paint) const;
    bool setupPaintShadow(SkPaint* paint, SkPoint* offset) const;
    // Sets up the paint for stroking. Returns true if the style is really
    // just a dash of squares (the size of the paint's stroke-width.
    bool setupPaintStroke(SkPaint* paint, SkRect* rect, bool isHLine = false);

    // Matrix operations
    void concatCTM(const AffineTransform& affine);
    void rotate(float angleInRadians);
    void scale(const FloatSize& size);
    void translate(float x, float y);
    const SkMatrix& getTotalMatrix() { return mCanvas->getTotalMatrix(); }

    // Clipping
    void addInnerRoundedRectClip(const IntRect& rect, int thickness);
    void canvasClip(const Path& path);
    void clip(const FloatRect& rect);
    void clip(const Path& path);
    void clipConvexPolygon(size_t numPoints, const FloatPoint*, bool antialias);
    void clipOut(const IntRect& r);
    void clipOut(const Path& p);
    void clipPath(const Path& pathToClip, WindRule clipRule);

    // Drawing
    void clearRect(const FloatRect& rect);
    void drawBitmapPattern(const SkBitmap& bitmap, const SkMatrix& matrix,
                           CompositeOperator compositeOp, const FloatRect& destRect);
    void drawBitmapRect(const SkBitmap& bitmap, const SkIRect* src,
                        const SkRect& dst, CompositeOperator op);
    void drawConvexPolygon(size_t numPoints, const FloatPoint* points,
                           bool shouldAntialias);
    void drawEllipse(const IntRect& rect);
    void drawFocusRing(const Vector<IntRect>& rects, int /* width */,
                       int /* offset */, const Color& color);
    void drawHighlightForText(const Font& font, const TextRun& run,
                              const FloatPoint& point, int h,
                              const Color& backgroundColor, ColorSpace colorSpace,
                              int from, int to, bool isActive);
    void drawLine(const IntPoint& point1, const IntPoint& point2);
    void drawLineForText(const FloatPoint& pt, float width);
    void drawLineForTextChecking(const FloatPoint& pt, float width,
                                 GraphicsContext::TextCheckingLineStyle);
    void drawRect(const IntRect& rect);
    void fillPath(const Path& pathToFill, WindRule fillRule);
    void fillRect(const FloatRect& rect);
    void fillRect(const FloatRect& rect, const Color& color, ColorSpace);
    void fillRoundedRect(const IntRect& rect, const IntSize& topLeft,
                         const IntSize& topRight, const IntSize& bottomLeft,
                         const IntSize& bottomRight, const Color& color,
                         ColorSpace);
    void strokeArc(const IntRect& r, int startAngle, int angleSpan);
    void strokePath(const Path& pathToStroke);
    void strokeRect(const FloatRect& rect, float lineWidth);

private:

    // shadowsIgnoreTransforms is only true for canvas's ImageBuffer, which will
    // have a GraphicsContext
    bool shadowsIgnoreTransforms() const {
        return m_gc && m_gc->shadowsIgnoreTransforms();
    }

    void setupPaintCommon(SkPaint* paint) const;

    bool m_deleteCanvas;
    struct State;
    WTF::Vector<State> m_stateStack;
    State* m_state;
    GraphicsContext* m_gc; // Back-ptr to our parent
};

}
#endif
