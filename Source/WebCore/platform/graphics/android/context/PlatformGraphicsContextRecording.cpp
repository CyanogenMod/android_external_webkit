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
    mGraphicsOperationCollection->append(text);
    mPicture = 0;
}


//**************************************
// State management
//**************************************

void PlatformGraphicsContextRecording::beginTransparencyLayer(float opacity)
{
    mGraphicsOperationCollection->append(new GraphicsOperation::BeginTransparencyLayer(opacity));
}

void PlatformGraphicsContextRecording::endTransparencyLayer()
{
    mGraphicsOperationCollection->append(new GraphicsOperation::EndTransparencyLayer());
}

void PlatformGraphicsContextRecording::save()
{
    PlatformGraphicsContext::save();
    mGraphicsOperationCollection->append(new GraphicsOperation::Save());
}

void PlatformGraphicsContextRecording::restore()
{
    PlatformGraphicsContext::restore();
    mGraphicsOperationCollection->append(new GraphicsOperation::Restore());
}

//**************************************
// State setters
//**************************************

void PlatformGraphicsContextRecording::setAlpha(float alpha)
{
    PlatformGraphicsContext::setAlpha(alpha);
    mGraphicsOperationCollection->append(new GraphicsOperation::SetAlpha(alpha));
}

void PlatformGraphicsContextRecording::setCompositeOperation(CompositeOperator op)
{
    PlatformGraphicsContext::setCompositeOperation(op);
    mGraphicsOperationCollection->append(new GraphicsOperation::SetCompositeOperation(op));
}

void PlatformGraphicsContextRecording::setFillColor(const Color& c)
{
    PlatformGraphicsContext::setFillColor(c);
    mGraphicsOperationCollection->append(new GraphicsOperation::SetFillColor(c));
}

void PlatformGraphicsContextRecording::setFillShader(SkShader* fillShader)
{
    PlatformGraphicsContext::setFillShader(fillShader);
    mGraphicsOperationCollection->append(new GraphicsOperation::SetFillShader(fillShader));
}

void PlatformGraphicsContextRecording::setLineCap(LineCap cap)
{
    PlatformGraphicsContext::setLineCap(cap);
    mGraphicsOperationCollection->append(new GraphicsOperation::SetLineCap(cap));
}

void PlatformGraphicsContextRecording::setLineDash(const DashArray& dashes, float dashOffset)
{
    PlatformGraphicsContext::setLineDash(dashes, dashOffset);
    mGraphicsOperationCollection->append(new GraphicsOperation::SetLineDash(dashes, dashOffset));
}

void PlatformGraphicsContextRecording::setLineJoin(LineJoin join)
{
    PlatformGraphicsContext::setLineJoin(join);
    mGraphicsOperationCollection->append(new GraphicsOperation::SetLineJoin(join));
}

void PlatformGraphicsContextRecording::setMiterLimit(float limit)
{
    PlatformGraphicsContext::setMiterLimit(limit);
    mGraphicsOperationCollection->append(new GraphicsOperation::SetMiterLimit(limit));
}

void PlatformGraphicsContextRecording::setShadow(int radius, int dx, int dy, SkColor c)
{
    PlatformGraphicsContext::setShadow(radius, dx, dy, c);
    mGraphicsOperationCollection->append(new GraphicsOperation::SetShadow(radius, dx, dy, c));
}

void PlatformGraphicsContextRecording::setShouldAntialias(bool useAA)
{
    m_state->useAA = useAA;
    PlatformGraphicsContext::setShouldAntialias(useAA);
    mGraphicsOperationCollection->append(new GraphicsOperation::SetShouldAntialias(useAA));
}

void PlatformGraphicsContextRecording::setStrokeColor(const Color& c)
{
    PlatformGraphicsContext::setStrokeColor(c);
    mGraphicsOperationCollection->append(new GraphicsOperation::SetStrokeColor(c));
}

void PlatformGraphicsContextRecording::setStrokeShader(SkShader* strokeShader)
{
    PlatformGraphicsContext::setStrokeShader(strokeShader);
    mGraphicsOperationCollection->append(new GraphicsOperation::SetStrokeShader(strokeShader));
}

void PlatformGraphicsContextRecording::setStrokeStyle(StrokeStyle style)
{
    PlatformGraphicsContext::setStrokeStyle(style);
    mGraphicsOperationCollection->append(new GraphicsOperation::SetStrokeStyle(style));
}

void PlatformGraphicsContextRecording::setStrokeThickness(float f)
{
    PlatformGraphicsContext::setStrokeThickness(f);
    mGraphicsOperationCollection->append(new GraphicsOperation::SetStrokeThickness(f));
}

//**************************************
// Matrix operations
//**************************************

void PlatformGraphicsContextRecording::concatCTM(const AffineTransform& affine)
{
    mCurrentMatrix.preConcat(affine);
    mGraphicsOperationCollection->append(new GraphicsOperation::ConcatCTM(affine));
}

void PlatformGraphicsContextRecording::rotate(float angleInRadians)
{
    float value = angleInRadians * (180.0f / 3.14159265f);
    mCurrentMatrix.preRotate(SkFloatToScalar(value));
    mGraphicsOperationCollection->append(new GraphicsOperation::Rotate(angleInRadians));
}

void PlatformGraphicsContextRecording::scale(const FloatSize& size)
{
    mCurrentMatrix.preScale(SkFloatToScalar(size.width()), SkFloatToScalar(size.height()));
    mGraphicsOperationCollection->append(new GraphicsOperation::Scale(size));
}

void PlatformGraphicsContextRecording::translate(float x, float y)
{
    mCurrentMatrix.preTranslate(SkFloatToScalar(x), SkFloatToScalar(y));
    mGraphicsOperationCollection->append(new GraphicsOperation::Translate(x, y));
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
    mGraphicsOperationCollection->append(new GraphicsOperation::InnerRoundedRectClip(rect, thickness));
}

void PlatformGraphicsContextRecording::canvasClip(const Path& path)
{
    clip(path);
}

void PlatformGraphicsContextRecording::clip(const FloatRect& rect)
{
    mGraphicsOperationCollection->append(new GraphicsOperation::Clip(rect));
}

void PlatformGraphicsContextRecording::clip(const Path& path)
{
    mGraphicsOperationCollection->append(new GraphicsOperation::ClipPath(path));
}

void PlatformGraphicsContextRecording::clipConvexPolygon(size_t numPoints,
                                                const FloatPoint*, bool antialias)
{
    // TODO
}

void PlatformGraphicsContextRecording::clipOut(const IntRect& r)
{
    mGraphicsOperationCollection->append(new GraphicsOperation::ClipOut(r));
}

void PlatformGraphicsContextRecording::clipOut(const Path& path)
{
    mGraphicsOperationCollection->append(new GraphicsOperation::ClipPath(path, true));
}

void PlatformGraphicsContextRecording::clipPath(const Path& pathToClip, WindRule clipRule)
{
    GraphicsOperation::ClipPath* operation = new GraphicsOperation::ClipPath(pathToClip);
    operation->setWindRule(clipRule);
    mGraphicsOperationCollection->append(operation);
}

void PlatformGraphicsContextRecording::clearRect(const FloatRect& rect)
{
    mGraphicsOperationCollection->append(new GraphicsOperation::ClearRect(rect));
}

//**************************************
// Drawing
//**************************************

void PlatformGraphicsContextRecording::drawBitmapPattern(
        const SkBitmap& bitmap, const SkMatrix& matrix,
        CompositeOperator compositeOp, const FloatRect& destRect)
{
    mGraphicsOperationCollection->append(new GraphicsOperation::DrawBitmapPattern(bitmap, matrix, compositeOp, destRect));
}

void PlatformGraphicsContextRecording::drawBitmapRect(const SkBitmap& bitmap,
                                   const SkIRect* src, const SkRect& dst,
                                   CompositeOperator op)
{
    mGraphicsOperationCollection->append(new GraphicsOperation::DrawBitmapRect(bitmap, *src, dst, op));
}

void PlatformGraphicsContextRecording::drawConvexPolygon(size_t numPoints,
                                                const FloatPoint* points,
                                                bool shouldAntialias)
{
    // TODO
}

void PlatformGraphicsContextRecording::drawEllipse(const IntRect& rect)
{
    mGraphicsOperationCollection->append(new GraphicsOperation::DrawEllipse(rect));
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
    mGraphicsOperationCollection->append(new GraphicsOperation::DrawLine(point1, point2));
}

void PlatformGraphicsContextRecording::drawLineForText(const FloatPoint& pt, float width)
{
    mGraphicsOperationCollection->append(new GraphicsOperation::DrawLineForText(pt, width));
}

void PlatformGraphicsContextRecording::drawLineForTextChecking(const FloatPoint& pt,
        float width, GraphicsContext::TextCheckingLineStyle lineStyle)
{
    mGraphicsOperationCollection->append(new GraphicsOperation::DrawLineForTextChecking(pt, width, lineStyle));
}

void PlatformGraphicsContextRecording::drawRect(const IntRect& rect)
{
    mGraphicsOperationCollection->append(new GraphicsOperation::DrawRect(rect));
}

void PlatformGraphicsContextRecording::fillPath(const Path& pathToFill, WindRule fillRule)
{
    mGraphicsOperationCollection->append(new GraphicsOperation::FillPath(pathToFill, fillRule));
}

void PlatformGraphicsContextRecording::fillRect(const FloatRect& rect)
{
    mGraphicsOperationCollection->append(new GraphicsOperation::FillRect(rect));
}

void PlatformGraphicsContextRecording::fillRect(const FloatRect& rect,
                                       const Color& color)
{
    GraphicsOperation::FillRect* operation = new GraphicsOperation::FillRect(rect);
    operation->setColor(color);
    mGraphicsOperationCollection->append(operation);
}

void PlatformGraphicsContextRecording::fillRoundedRect(
        const IntRect& rect, const IntSize& topLeft, const IntSize& topRight,
        const IntSize& bottomLeft, const IntSize& bottomRight,
        const Color& color)
{
    mGraphicsOperationCollection->append(new GraphicsOperation::FillRoundedRect(rect, topLeft,
                 topRight, bottomLeft, bottomRight, color));
}

void PlatformGraphicsContextRecording::strokeArc(const IntRect& r, int startAngle,
                              int angleSpan)
{
    mGraphicsOperationCollection->append(new GraphicsOperation::StrokeArc(r, startAngle, angleSpan));
}

void PlatformGraphicsContextRecording::strokePath(const Path& pathToStroke)
{
    mGraphicsOperationCollection->append(new GraphicsOperation::StrokePath(pathToStroke));
}

void PlatformGraphicsContextRecording::strokeRect(const FloatRect& rect, float lineWidth)
{
    mGraphicsOperationCollection->append(new GraphicsOperation::StrokeRect(rect, lineWidth));
}


}   // WebCore
