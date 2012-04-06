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

#ifndef platform_graphics_context_skia_h
#define platform_graphics_context_skia_h

#include "PlatformGraphicsContext.h"

namespace WebCore {

class PlatformGraphicsContextSkia : public PlatformGraphicsContext {
public:
    PlatformGraphicsContextSkia(SkCanvas* canvas, bool takeCanvasOwnership = false);
    virtual ~PlatformGraphicsContextSkia();
    virtual bool isPaintingDisabled();
    virtual SkCanvas* getCanvas() { return mCanvas; }

    virtual ContextType type() { return PaintingContext; }
    virtual SkCanvas* recordingCanvas() { return mCanvas; }
    virtual void endRecording(int type = 0) {}

    // FIXME: This is used by ImageBufferAndroid, which should really be
    //        managing the canvas lifecycle itself

    virtual bool deleteUs() const { return m_deleteCanvas; }

    // State management
    virtual void beginTransparencyLayer(float opacity);
    virtual void endTransparencyLayer();
    virtual void save();
    virtual void restore();

    // Matrix operations
    virtual void concatCTM(const AffineTransform& affine);
    virtual void rotate(float angleInRadians);
    virtual void scale(const FloatSize& size);
    virtual void translate(float x, float y);
    virtual const SkMatrix& getTotalMatrix();

    // Clipping
    virtual void addInnerRoundedRectClip(const IntRect& rect, int thickness);
    virtual void canvasClip(const Path& path);
    virtual void clip(const FloatRect& rect);
    virtual void clip(const Path& path);
    virtual void clipConvexPolygon(size_t numPoints, const FloatPoint*, bool antialias);
    virtual void clipOut(const IntRect& r);
    virtual void clipOut(const Path& p);
    virtual void clipPath(const Path& pathToClip, WindRule clipRule);

    // Drawing
    virtual void clearRect(const FloatRect& rect);
    virtual void drawBitmapPattern(const SkBitmap& bitmap, const SkMatrix& matrix,
                           CompositeOperator compositeOp, const FloatRect& destRect);
    virtual void drawBitmapRect(const SkBitmap& bitmap, const SkIRect* src,
                        const SkRect& dst, CompositeOperator op);
    virtual void drawConvexPolygon(size_t numPoints, const FloatPoint* points,
                           bool shouldAntialias);
    virtual void drawEllipse(const IntRect& rect);
    virtual void drawFocusRing(const Vector<IntRect>& rects, int /* width */,
                       int /* offset */, const Color& color);
    virtual void drawHighlightForText(const Font& font, const TextRun& run,
                              const FloatPoint& point, int h,
                              const Color& backgroundColor, ColorSpace colorSpace,
                              int from, int to, bool isActive);
    virtual void drawLine(const IntPoint& point1, const IntPoint& point2);
    virtual void drawLineForText(const FloatPoint& pt, float width);
    virtual void drawLineForTextChecking(const FloatPoint& pt, float width,
                                 GraphicsContext::TextCheckingLineStyle);
    virtual void drawRect(const IntRect& rect);
    virtual void fillPath(const Path& pathToFill, WindRule fillRule);
    virtual void fillRect(const FloatRect& rect);
    virtual void fillRect(const FloatRect& rect, const Color& color);
    virtual void fillRoundedRect(const IntRect& rect, const IntSize& topLeft,
                         const IntSize& topRight, const IntSize& bottomLeft,
                         const IntSize& bottomRight, const Color& color);
    virtual void strokeArc(const IntRect& r, int startAngle, int angleSpan);
    virtual void strokePath(const Path& pathToStroke);
    virtual void strokeRect(const FloatRect& rect, float lineWidth);

private:

    // shadowsIgnoreTransforms is only true for canvas's ImageBuffer, which will
    // have a GraphicsContext
    virtual bool shadowsIgnoreTransforms() const {
        return m_gc && m_gc->shadowsIgnoreTransforms();
    }

    SkCanvas* mCanvas;
    bool m_deleteCanvas;
};

}
#endif
