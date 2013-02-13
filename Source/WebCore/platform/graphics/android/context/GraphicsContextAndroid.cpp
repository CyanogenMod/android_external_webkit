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
#include "config.h"
#include "GraphicsContext.h"

#include "AffineTransform.h"
#include "Font.h"
#include "Gradient.h"
#include "NotImplemented.h"
#include "Path.h"
#include "Pattern.h"
#include "PlatformGraphicsContext.h"
#include "PlatformGraphicsContextSkia.h"
#include "SkBitmapRef.h"
#include "SkBlurDrawLooper.h"
#include "SkBlurMaskFilter.h"
#include "SkCanvas.h"
#include "SkColorPriv.h"
#include "SkCornerPathEffect.h"
#include "SkDashPathEffect.h"
#include "SkDevice.h"
#include "SkGradientShader.h"
#include "SkPaint.h"
#include "SkString.h"
#include "SkiaUtils.h"
#include "TransformationMatrix.h"

using namespace std;

namespace WebCore {

// This class just holds onto a PlatformContextSkia for GraphicsContext.
class GraphicsContextPlatformPrivate {
    WTF_MAKE_NONCOPYABLE(GraphicsContextPlatformPrivate);
public:
    GraphicsContextPlatformPrivate(PlatformGraphicsContext* platformContext)
        : m_context(platformContext) { }
    ~GraphicsContextPlatformPrivate()
    {
        if (m_context && m_context->deleteUs())
            delete m_context;
    }


    PlatformGraphicsContext* context() { return m_context; }

private:
    // Non-owning pointer to the PlatformContext.
    PlatformGraphicsContext* m_context;
};

static void syncPlatformContext(GraphicsContext* gc)
{
    // Stroke and fill sometimes reference each other, so always
    // sync them both to make sure our state is consistent.

    PlatformGraphicsContext* pgc = gc->platformContext();
    Gradient* grad = gc->state().fillGradient.get();
    Pattern* pat = gc->state().fillPattern.get();

    if (grad)
        pgc->setFillShader(grad->platformGradient());
    else if (pat)
        pgc->setFillShader(pat->platformPattern(AffineTransform()));
    else
        pgc->setFillColor(gc->state().fillColor);

    grad = gc->state().strokeGradient.get();
    pat = gc->state().strokePattern.get();

    if (grad)
        pgc->setStrokeShader(grad->platformGradient());
    else if (pat)
        pgc->setStrokeShader(pat->platformPattern(AffineTransform()));
    else
        pgc->setStrokeColor(gc->state().strokeColor);
}

////////////////////////////////////////////////////////////////////////////////////////////////

GraphicsContext* GraphicsContext::createOffscreenContext(int width, int height)
{
    PlatformGraphicsContextSkia* pgc = new PlatformGraphicsContextSkia(new SkCanvas, true);

    SkBitmap bitmap;

    bitmap.setConfig(SkBitmap::kARGB_8888_Config, width, height);
    bitmap.allocPixels();
    bitmap.eraseColor(0);
    pgc->canvas()->setBitmapDevice(bitmap);

    GraphicsContext* ctx = new GraphicsContext(pgc);
    return ctx;
}

////////////////////////////////////////////////////////////////////////////////////////////////

void GraphicsContext::platformInit(PlatformGraphicsContext* gc)
{
    if (gc)
        gc->setGraphicsContext(this);
    m_data = new GraphicsContextPlatformPrivate(gc);
    setPaintingDisabled(!gc || gc->isPaintingDisabled());
}

void GraphicsContext::platformDestroy()
{
    delete m_data;
}

void GraphicsContext::savePlatformState()
{
    if (paintingDisabled())
        return;
    platformContext()->save();
}

void GraphicsContext::restorePlatformState()
{
    if (paintingDisabled())
        return;
    platformContext()->restore();
}

bool GraphicsContext::willFill() const
{
    return m_state.fillColor.rgb();
}

bool GraphicsContext::willStroke() const
{
    return m_state.strokeColor.rgb();
}

// Draws a filled rectangle with a stroked border.
void GraphicsContext::drawRect(const IntRect& rect)
{
    if (paintingDisabled())
        return;

    syncPlatformContext(this);
    platformContext()->drawRect(rect);
}

// This is only used to draw borders.
void GraphicsContext::drawLine(const IntPoint& point1, const IntPoint& point2)
{
    if (paintingDisabled())
        return;

    syncPlatformContext(this);
    platformContext()->drawLine(point1, point2);
}

void GraphicsContext::drawLineForText(const FloatPoint& pt, float width, bool /* printing */)
{
    if (paintingDisabled())
        return;

    syncPlatformContext(this);
    platformContext()->drawLineForText(pt, width);
}

void GraphicsContext::drawLineForTextChecking(const FloatPoint& pt, float width,
                                              TextCheckingLineStyle style)
{
    if (paintingDisabled())
        return;

    syncPlatformContext(this);
    platformContext()->drawLineForTextChecking(pt, width, style);
}

// This method is only used to draw the little circles used in lists.
void GraphicsContext::drawEllipse(const IntRect& rect)
{
    if (paintingDisabled())
        return;

    syncPlatformContext(this);
    platformContext()->drawEllipse(rect);
}

void GraphicsContext::strokeArc(const IntRect& r, int startAngle, int angleSpan)
{
    if (paintingDisabled())
        return;

    syncPlatformContext(this);
    platformContext()->strokeArc(r, startAngle, angleSpan);
}

void GraphicsContext::drawConvexPolygon(size_t numPoints, const FloatPoint* points,
                                        bool shouldAntialias)
{
    if (paintingDisabled())
        return;

    syncPlatformContext(this);
    platformContext()->drawConvexPolygon(numPoints, points, shouldAntialias);
}

void GraphicsContext::fillRoundedRect(const IntRect& rect, const IntSize& topLeft, const IntSize& topRight,
                                      const IntSize& bottomLeft, const IntSize& bottomRight,
                                      const Color& color, ColorSpace colorSpace)
{
    if (paintingDisabled())
        return;

    syncPlatformContext(this);
    platformContext()->fillRoundedRect(rect, topLeft, topRight,
            bottomLeft, bottomRight, color, colorSpace);
}

void GraphicsContext::fillRect(const FloatRect& rect)
{
    if (paintingDisabled())
        return;

    syncPlatformContext(this);
    platformContext()->fillRect(rect);
}

void GraphicsContext::fillRect(const FloatRect& rect, const Color& color, ColorSpace colorSpace)
{
    if (paintingDisabled())
        return;

    syncPlatformContext(this);
    platformContext()->fillRect(rect, color, colorSpace);
}

void GraphicsContext::clip(const FloatRect& rect)
{
    if (paintingDisabled())
        return;

    platformContext()->clip(rect);
}

void GraphicsContext::clip(const Path& path)
{
    if (paintingDisabled())
        return;

    platformContext()->clip(path);
}

void GraphicsContext::addInnerRoundedRectClip(const IntRect& rect, int thickness)
{
    if (paintingDisabled())
        return;

    platformContext()->addInnerRoundedRectClip(rect, thickness);
}

void GraphicsContext::canvasClip(const Path& path)
{
    if (paintingDisabled())
        return;

    platformContext()->canvasClip(path);
}

void GraphicsContext::clipOut(const IntRect& r)
{
    if (paintingDisabled())
        return;

    platformContext()->clipOut(r);
}

#if ENABLE(SVG)
void GraphicsContext::clipPath(const Path& pathToClip, WindRule clipRule)
{
    if (paintingDisabled())
        return;

    platformContext()->clipPath(pathToClip, clipRule);
}
#endif

void GraphicsContext::clipOut(const Path& p)
{
    if (paintingDisabled())
        return;

    platformContext()->clipOut(p);
}

//////////////////////////////////////////////////////////////////////////////////////////////////

#if SVG_SUPPORT
KRenderingDeviceContext* GraphicsContext::createRenderingDeviceContext()
{
    return new KRenderingDeviceContextQuartz(platformContext());
}
#endif

void GraphicsContext::beginTransparencyLayer(float opacity)
{
    if (paintingDisabled())
        return;

    platformContext()->beginTransparencyLayer(opacity);
}

void GraphicsContext::endTransparencyLayer()
{
    if (paintingDisabled())
        return;

    platformContext()->endTransparencyLayer();
}

///////////////////////////////////////////////////////////////////////////

void GraphicsContext::setupFillPaint(SkPaint* paint)
{
    if (paintingDisabled())
        return;
    syncPlatformContext(this);
    platformContext()->setupPaintFill(paint);
}

void GraphicsContext::setupStrokePaint(SkPaint* paint)
{
    if (paintingDisabled())
        return;
    syncPlatformContext(this);
    platformContext()->setupPaintStroke(paint, 0);
}

bool GraphicsContext::setupShadowPaint(SkPaint* paint, SkPoint* offset)
{
    if (paintingDisabled())
        return false;
    syncPlatformContext(this);
    return platformContext()->setupPaintShadow(paint, offset);
}

void GraphicsContext::setPlatformStrokeColor(const Color& c, ColorSpace)
{
}

void GraphicsContext::setPlatformStrokeThickness(float f)
{
    if (paintingDisabled())
        return;
    platformContext()->setStrokeThickness(f);
}

void GraphicsContext::setPlatformStrokeStyle(StrokeStyle style)
{
    if (paintingDisabled())
        return;
    platformContext()->setStrokeStyle(style);
}

void GraphicsContext::setPlatformFillColor(const Color& c, ColorSpace)
{
}

void GraphicsContext::setPlatformShadow(const FloatSize& size, float blur, const Color& color, ColorSpace)
{
    if (paintingDisabled())
        return;

    if (blur <= 0)
        this->clearPlatformShadow();

    SkColor c;
    if (color.isValid())
        c = color.rgb();
    else
        c = SkColorSetARGB(0xFF / 3, 0, 0, 0); // "std" Apple shadow color
    platformContext()->setShadow(blur, size.width(), size.height(), c);
}

void GraphicsContext::clearPlatformShadow()
{
    if (paintingDisabled())
        return;

    platformContext()->setShadow(0, 0, 0, 0);
}

///////////////////////////////////////////////////////////////////////////////

void GraphicsContext::drawFocusRing(const Vector<IntRect>& rects, int width, int offset, const Color& color)
{
    if (paintingDisabled())
        return;

    syncPlatformContext(this);
    platformContext()->drawFocusRing(rects, width, offset, color);
}

void GraphicsContext::drawFocusRing(const Path&, int, int, const Color&)
{
    // Do nothing, since we draw the focus ring independently.
}

PlatformGraphicsContext* GraphicsContext::platformContext() const
{
    ASSERT(!paintingDisabled());
    return m_data->context();
}

void GraphicsContext::setMiterLimit(float limit)
{
    if (paintingDisabled())
        return;
    platformContext()->setMiterLimit(limit);
}

void GraphicsContext::setAlpha(float alpha)
{
    if (paintingDisabled())
        return;
    platformContext()->setAlpha(alpha);
}

void GraphicsContext::setPlatformCompositeOperation(CompositeOperator op)
{
    if (paintingDisabled())
        return;
    platformContext()->setCompositeOperation(op);
}

void GraphicsContext::clearRect(const FloatRect& rect)
{
    if (paintingDisabled())
        return;

    syncPlatformContext(this);
    platformContext()->clearRect(rect);
}

void GraphicsContext::strokeRect(const FloatRect& rect, float lineWidth)
{
    if (paintingDisabled())
        return;

    syncPlatformContext(this);
    platformContext()->strokeRect(rect, lineWidth);
}

void GraphicsContext::setLineCap(LineCap cap)
{
    if (paintingDisabled())
        return;
    platformContext()->setLineCap(cap);
}

#if ENABLE(SVG)
void GraphicsContext::setLineDash(const DashArray& dashes, float dashOffset)
{
    if (paintingDisabled())
        return;

    platformContext()->setLineDash(dashes, dashOffset);
}
#endif

void GraphicsContext::setLineJoin(LineJoin join)
{
    if (paintingDisabled())
        return;
    platformContext()->setLineJoin(join);
}

void GraphicsContext::scale(const FloatSize& size)
{
    if (paintingDisabled())
        return;
    platformContext()->scale(size);
}

void GraphicsContext::rotate(float angleInRadians)
{
    if (paintingDisabled())
        return;
    platformContext()->rotate(angleInRadians);
}

void GraphicsContext::translate(float x, float y)
{
    if (paintingDisabled())
        return;
    if (!x && !y)
        return;
    platformContext()->translate(x, y);
}

void GraphicsContext::concatCTM(const AffineTransform& affine)
{
    if (paintingDisabled())
        return;
    platformContext()->concatCTM(affine);
}

// This is intended to round the rect to device pixels (through the CTM)
// and then invert the result back into source space, with the hope that when
// it is drawn (through the matrix), it will land in the "right" place (i.e.
// on pixel boundaries).

// For android, we record this geometry once and then draw it though various
// scale factors as the user zooms, without re-recording. Thus this routine
// should just leave the original geometry alone.

// If we instead draw into bitmap tiles, we should then perform this
// transform -> round -> inverse step.

FloatRect GraphicsContext::roundToDevicePixels(const FloatRect& rect, RoundingMode)
{
    return rect;
}

//////////////////////////////////////////////////////////////////////////////////////////////////

void GraphicsContext::setURLForRect(const KURL& link, const IntRect& destRect)
{
// Appears to be PDF specific, so we ignore it
}

void GraphicsContext::setPlatformShouldAntialias(bool useAA)
{
    if (paintingDisabled())
        return;
    platformContext()->setShouldAntialias(useAA);
}

void GraphicsContext::setPlatformFillGradient(Gradient* fillGradient)
{
}

void GraphicsContext::setPlatformFillPattern(Pattern* fillPattern)
{
}

void GraphicsContext::setPlatformStrokeGradient(Gradient* strokeGradient)
{
}

void GraphicsContext::setPlatformStrokePattern(Pattern* strokePattern)
{
}

AffineTransform GraphicsContext::getCTM() const
{
    if (paintingDisabled())
        return AffineTransform();
    const SkMatrix& m = platformContext()->getTotalMatrix();
    return AffineTransform(SkScalarToDouble(m.getScaleX()), // a
                           SkScalarToDouble(m.getSkewY()), // b
                           SkScalarToDouble(m.getSkewX()), // c
                           SkScalarToDouble(m.getScaleY()), // d
                           SkScalarToDouble(m.getTranslateX()), // e
                           SkScalarToDouble(m.getTranslateY())); // f
}

void GraphicsContext::setCTM(const AffineTransform& transform)
{
    // The SkPicture mode of Skia does not support SkCanvas::setMatrix(), so we
    // can not simply use that method here. We could calculate the transform
    // required to achieve the desired matrix and use SkCanvas::concat(), but
    // there's currently no need for this.
    ASSERT_NOT_REACHED();
}

///////////////////////////////////////////////////////////////////////////////

void GraphicsContext::fillPath(const Path& pathToFill)
{
    if (paintingDisabled())
        return;

    syncPlatformContext(this);
    platformContext()->fillPath(pathToFill, fillRule());
}

void GraphicsContext::strokePath(const Path& pathToStroke)
{
    if (paintingDisabled())
        return;

    syncPlatformContext(this);
    platformContext()->strokePath(pathToStroke);
}

InterpolationQuality GraphicsContext::imageInterpolationQuality() const
{
    notImplemented();
    return InterpolationDefault;
}

void GraphicsContext::setImageInterpolationQuality(InterpolationQuality mode)
{
#if 0
    enum InterpolationQuality {
        InterpolationDefault,
        InterpolationNone,
        InterpolationLow,
        InterpolationMedium,
        InterpolationHigh
    };
#endif
    // TODO: record this, so we can know when to use bitmap-filtering when we draw
    // ... not sure how meaningful this will be given our playback model.

    // Certainly safe to do nothing for the present.
}

void GraphicsContext::clipConvexPolygon(size_t numPoints, const FloatPoint*,
                                        bool antialias)
{
    if (paintingDisabled())
        return;

    if (numPoints <= 1)
        return;

    // FIXME: IMPLEMENT!
}

void GraphicsContext::drawHighlightForText(const Font& font, const TextRun& run,
                                           const FloatPoint& point, int h,
                                           const Color& backgroundColor,
                                           ColorSpace colorSpace, int from,
                                           int to, bool isActive)
{
    if (paintingDisabled())
        return;

    syncPlatformContext(this);
    platformContext()->drawHighlightForText(font, run, point, h, backgroundColor,
            colorSpace, from, to, isActive);
}

} // namespace WebCore
