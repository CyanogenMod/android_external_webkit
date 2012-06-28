#define LOG_TAG "PlatformGraphicsContextRecording"
#define LOG_NDEBUG 1

#include "config.h"
#include "PlatformGraphicsContextRecording.h"

#include "AndroidLog.h"
#include "Font.h"
#include "GraphicsContext.h"
#include "GraphicsOperationCollection.h"
#include "GraphicsOperation.h"

namespace WebCore {

//**************************************
// PlatformGraphicsContextRecording
//**************************************

PlatformGraphicsContextRecording::PlatformGraphicsContextRecording(GraphicsOperationCollection* picture)
    : PlatformGraphicsContext()
    , mGraphicsOperationCollection(picture)
    , mPicture(0)
{
}

bool PlatformGraphicsContextRecording::isPaintingDisabled()
{
    return !mGraphicsOperationCollection;
}

SkCanvas* PlatformGraphicsContextRecording::recordingCanvas()
{
    SkSafeUnref(mPicture);
    mPicture = new SkPicture();
    return mPicture->beginRecording(0, 0, 0);
}

void PlatformGraphicsContextRecording::endRecording(int type)
{
    if (!mPicture)
        return;
    mPicture->endRecording();
    GraphicsOperation::DrawComplexText* text = new GraphicsOperation::DrawComplexText(mPicture);
    mGraphicsOperationCollection->adoptAndAppend(text);
    mPicture = 0;
}


//**************************************
// State management
//**************************************

void PlatformGraphicsContextRecording::beginTransparencyLayer(float opacity)
{
    mGraphicsOperationCollection->adoptAndAppend(new GraphicsOperation::BeginTransparencyLayer(opacity));
}

void PlatformGraphicsContextRecording::endTransparencyLayer()
{
    mGraphicsOperationCollection->adoptAndAppend(new GraphicsOperation::EndTransparencyLayer());
}

void PlatformGraphicsContextRecording::save()
{
    PlatformGraphicsContext::save();
    mGraphicsOperationCollection->adoptAndAppend(new GraphicsOperation::Save());
}

void PlatformGraphicsContextRecording::restore()
{
    PlatformGraphicsContext::restore();
    mGraphicsOperationCollection->adoptAndAppend(new GraphicsOperation::Restore());
}

//**************************************
// State setters
//**************************************

void PlatformGraphicsContextRecording::setAlpha(float alpha)
{
    PlatformGraphicsContext::setAlpha(alpha);
    mGraphicsOperationCollection->adoptAndAppend(new GraphicsOperation::SetAlpha(alpha));
}

void PlatformGraphicsContextRecording::setCompositeOperation(CompositeOperator op)
{
    PlatformGraphicsContext::setCompositeOperation(op);
    mGraphicsOperationCollection->adoptAndAppend(new GraphicsOperation::SetCompositeOperation(op));
}

bool PlatformGraphicsContextRecording::setFillColor(const Color& c)
{
    if (PlatformGraphicsContext::setFillColor(c)) {
        mGraphicsOperationCollection->adoptAndAppend(new GraphicsOperation::SetFillColor(c));
        return true;
    }
    return false;
}

bool PlatformGraphicsContextRecording::setFillShader(SkShader* fillShader)
{
    if (PlatformGraphicsContext::setFillShader(fillShader)) {
        mGraphicsOperationCollection->adoptAndAppend(new GraphicsOperation::SetFillShader(fillShader));
        return true;
    }
    return false;
}

void PlatformGraphicsContextRecording::setLineCap(LineCap cap)
{
    PlatformGraphicsContext::setLineCap(cap);
    mGraphicsOperationCollection->adoptAndAppend(new GraphicsOperation::SetLineCap(cap));
}

void PlatformGraphicsContextRecording::setLineDash(const DashArray& dashes, float dashOffset)
{
    PlatformGraphicsContext::setLineDash(dashes, dashOffset);
    mGraphicsOperationCollection->adoptAndAppend(new GraphicsOperation::SetLineDash(dashes, dashOffset));
}

void PlatformGraphicsContextRecording::setLineJoin(LineJoin join)
{
    PlatformGraphicsContext::setLineJoin(join);
    mGraphicsOperationCollection->adoptAndAppend(new GraphicsOperation::SetLineJoin(join));
}

void PlatformGraphicsContextRecording::setMiterLimit(float limit)
{
    PlatformGraphicsContext::setMiterLimit(limit);
    mGraphicsOperationCollection->adoptAndAppend(new GraphicsOperation::SetMiterLimit(limit));
}

void PlatformGraphicsContextRecording::setShadow(int radius, int dx, int dy, SkColor c)
{
    PlatformGraphicsContext::setShadow(radius, dx, dy, c);
    mGraphicsOperationCollection->adoptAndAppend(new GraphicsOperation::SetShadow(radius, dx, dy, c));
}

void PlatformGraphicsContextRecording::setShouldAntialias(bool useAA)
{
    m_state->useAA = useAA;
    PlatformGraphicsContext::setShouldAntialias(useAA);
    mGraphicsOperationCollection->adoptAndAppend(new GraphicsOperation::SetShouldAntialias(useAA));
}

bool PlatformGraphicsContextRecording::setStrokeColor(const Color& c)
{
    if (PlatformGraphicsContext::setStrokeColor(c)) {
        mGraphicsOperationCollection->adoptAndAppend(new GraphicsOperation::SetStrokeColor(c));
        return true;
    }
    return false;
}

bool PlatformGraphicsContextRecording::setStrokeShader(SkShader* strokeShader)
{
    if (PlatformGraphicsContext::setStrokeShader(strokeShader)) {
        mGraphicsOperationCollection->adoptAndAppend(new GraphicsOperation::SetStrokeShader(strokeShader));
        return true;
    }
    return false;
}

void PlatformGraphicsContextRecording::setStrokeStyle(StrokeStyle style)
{
    PlatformGraphicsContext::setStrokeStyle(style);
    mGraphicsOperationCollection->adoptAndAppend(new GraphicsOperation::SetStrokeStyle(style));
}

void PlatformGraphicsContextRecording::setStrokeThickness(float f)
{
    PlatformGraphicsContext::setStrokeThickness(f);
    mGraphicsOperationCollection->adoptAndAppend(new GraphicsOperation::SetStrokeThickness(f));
}

//**************************************
// Matrix operations
//**************************************

void PlatformGraphicsContextRecording::concatCTM(const AffineTransform& affine)
{
    mCurrentMatrix.preConcat(affine);
    mGraphicsOperationCollection->adoptAndAppend(new GraphicsOperation::ConcatCTM(affine));
}

void PlatformGraphicsContextRecording::rotate(float angleInRadians)
{
    float value = angleInRadians * (180.0f / 3.14159265f);
    mCurrentMatrix.preRotate(SkFloatToScalar(value));
    mGraphicsOperationCollection->adoptAndAppend(new GraphicsOperation::Rotate(angleInRadians));
}

void PlatformGraphicsContextRecording::scale(const FloatSize& size)
{
    mCurrentMatrix.preScale(SkFloatToScalar(size.width()), SkFloatToScalar(size.height()));
    mGraphicsOperationCollection->adoptAndAppend(new GraphicsOperation::Scale(size));
}

void PlatformGraphicsContextRecording::translate(float x, float y)
{
    mCurrentMatrix.preTranslate(SkFloatToScalar(x), SkFloatToScalar(y));
    mGraphicsOperationCollection->adoptAndAppend(new GraphicsOperation::Translate(x, y));
}

const SkMatrix& PlatformGraphicsContextRecording::getTotalMatrix()
{
    return mCurrentMatrix;
}

//**************************************
// Clipping
//**************************************

void PlatformGraphicsContextRecording::addInnerRoundedRectClip(const IntRect& rect,
                                                      int thickness)
{
    mGraphicsOperationCollection->adoptAndAppend(new GraphicsOperation::InnerRoundedRectClip(rect, thickness));
}

void PlatformGraphicsContextRecording::canvasClip(const Path& path)
{
    clip(path);
}

void PlatformGraphicsContextRecording::clip(const FloatRect& rect)
{
    mGraphicsOperationCollection->adoptAndAppend(new GraphicsOperation::Clip(rect));
}

void PlatformGraphicsContextRecording::clip(const Path& path)
{
    mGraphicsOperationCollection->adoptAndAppend(new GraphicsOperation::ClipPath(path));
}

void PlatformGraphicsContextRecording::clipConvexPolygon(size_t numPoints,
                                                const FloatPoint*, bool antialias)
{
    // TODO
}

void PlatformGraphicsContextRecording::clipOut(const IntRect& r)
{
    mGraphicsOperationCollection->adoptAndAppend(new GraphicsOperation::ClipOut(r));
}

void PlatformGraphicsContextRecording::clipOut(const Path& path)
{
    mGraphicsOperationCollection->adoptAndAppend(new GraphicsOperation::ClipPath(path, true));
}

void PlatformGraphicsContextRecording::clipPath(const Path& pathToClip, WindRule clipRule)
{
    GraphicsOperation::ClipPath* operation = new GraphicsOperation::ClipPath(pathToClip);
    operation->setWindRule(clipRule);
    mGraphicsOperationCollection->adoptAndAppend(operation);
}

void PlatformGraphicsContextRecording::clearRect(const FloatRect& rect)
{
    mGraphicsOperationCollection->adoptAndAppend(new GraphicsOperation::ClearRect(rect));
}

//**************************************
// Drawing
//**************************************

void PlatformGraphicsContextRecording::drawBitmapPattern(
        const SkBitmap& bitmap, const SkMatrix& matrix,
        CompositeOperator compositeOp, const FloatRect& destRect)
{
    mGraphicsOperationCollection->adoptAndAppend(new GraphicsOperation::DrawBitmapPattern(bitmap, matrix, compositeOp, destRect));
}

void PlatformGraphicsContextRecording::drawBitmapRect(const SkBitmap& bitmap,
                                   const SkIRect* src, const SkRect& dst,
                                   CompositeOperator op)
{
    mGraphicsOperationCollection->adoptAndAppend(new GraphicsOperation::DrawBitmapRect(bitmap, *src, dst, op));
}

void PlatformGraphicsContextRecording::drawConvexPolygon(size_t numPoints,
                                                const FloatPoint* points,
                                                bool shouldAntialias)
{
    // TODO
}

void PlatformGraphicsContextRecording::drawEllipse(const IntRect& rect)
{
    mGraphicsOperationCollection->adoptAndAppend(new GraphicsOperation::DrawEllipse(rect));
}

void PlatformGraphicsContextRecording::drawFocusRing(const Vector<IntRect>& rects,
                                            int /* width */, int /* offset */,
                                            const Color& color)
{
    // TODO
}

void PlatformGraphicsContextRecording::drawHighlightForText(
        const Font& font, const TextRun& run, const FloatPoint& point, int h,
        const Color& backgroundColor, ColorSpace colorSpace, int from,
        int to, bool isActive)
{
    IntRect rect = (IntRect)font.selectionRectForText(run, point, h, from, to);
    if (isActive)
        fillRect(rect, backgroundColor);
    else {
        int x = rect.x(), y = rect.y(), w = rect.width(), h = rect.height();
        const int t = 3, t2 = t * 2;

        fillRect(IntRect(x, y, w, t), backgroundColor);
        fillRect(IntRect(x, y+h-t, w, t), backgroundColor);
        fillRect(IntRect(x, y+t, t, h-t2), backgroundColor);
        fillRect(IntRect(x+w-t, y+t, t, h-t2), backgroundColor);
    }
}

void PlatformGraphicsContextRecording::drawLine(const IntPoint& point1,
                             const IntPoint& point2)
{
    mGraphicsOperationCollection->adoptAndAppend(new GraphicsOperation::DrawLine(point1, point2));
}

void PlatformGraphicsContextRecording::drawLineForText(const FloatPoint& pt, float width)
{
    mGraphicsOperationCollection->adoptAndAppend(new GraphicsOperation::DrawLineForText(pt, width));
}

void PlatformGraphicsContextRecording::drawLineForTextChecking(const FloatPoint& pt,
        float width, GraphicsContext::TextCheckingLineStyle lineStyle)
{
    mGraphicsOperationCollection->adoptAndAppend(new GraphicsOperation::DrawLineForTextChecking(pt, width, lineStyle));
}

void PlatformGraphicsContextRecording::drawRect(const IntRect& rect)
{
    mGraphicsOperationCollection->adoptAndAppend(new GraphicsOperation::DrawRect(rect));
}

void PlatformGraphicsContextRecording::fillPath(const Path& pathToFill, WindRule fillRule)
{
    mGraphicsOperationCollection->adoptAndAppend(new GraphicsOperation::FillPath(pathToFill, fillRule));
}

void PlatformGraphicsContextRecording::fillRect(const FloatRect& rect)
{
    mGraphicsOperationCollection->adoptAndAppend(new GraphicsOperation::FillRect(rect));
}

void PlatformGraphicsContextRecording::fillRect(const FloatRect& rect,
                                       const Color& color)
{
    GraphicsOperation::FillRect* operation = new GraphicsOperation::FillRect(rect);
    operation->setColor(color);
    mGraphicsOperationCollection->adoptAndAppend(operation);
}

void PlatformGraphicsContextRecording::fillRoundedRect(
        const IntRect& rect, const IntSize& topLeft, const IntSize& topRight,
        const IntSize& bottomLeft, const IntSize& bottomRight,
        const Color& color)
{
    mGraphicsOperationCollection->adoptAndAppend(new GraphicsOperation::FillRoundedRect(rect, topLeft,
                 topRight, bottomLeft, bottomRight, color));
}

void PlatformGraphicsContextRecording::strokeArc(const IntRect& r, int startAngle,
                              int angleSpan)
{
    mGraphicsOperationCollection->adoptAndAppend(new GraphicsOperation::StrokeArc(r, startAngle, angleSpan));
}

void PlatformGraphicsContextRecording::strokePath(const Path& pathToStroke)
{
    mGraphicsOperationCollection->adoptAndAppend(new GraphicsOperation::StrokePath(pathToStroke));
}

void PlatformGraphicsContextRecording::strokeRect(const FloatRect& rect, float lineWidth)
{
    mGraphicsOperationCollection->adoptAndAppend(new GraphicsOperation::StrokeRect(rect, lineWidth));
}


}   // WebCore
