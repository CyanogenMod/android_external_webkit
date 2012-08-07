#define LOG_TAG "PlatformGraphicsContextSkia"
#define LOG_NDEBUG 1

#include "config.h"
#include "PlatformGraphicsContextSkia.h"

#include "AndroidLog.h"
#include "Font.h"
#include "GraphicsContext.h"
#include "SkCanvas.h"
#include "SkCornerPathEffect.h"
#include "SkPaint.h"
#include "SkShader.h"
#include "SkiaUtils.h"

namespace WebCore {

// These are the flags we need when we call saveLayer for transparency.
// Since it does not appear that webkit intends this to also save/restore
// the matrix or clip, I do not give those flags (for performance)
#define TRANSPARENCY_SAVEFLAGS                                  \
    (SkCanvas::SaveFlags)(SkCanvas::kHasAlphaLayer_SaveFlag |   \
                          SkCanvas::kFullColorLayer_SaveFlag)

//**************************************
// Helper functions
//**************************************

static void setrectForUnderline(SkRect* r, float lineThickness,
                                const FloatPoint& point, int yOffset, float width)
{
#if 0
    if (lineThickness < 1) // Do we really need/want this?
        lineThickness = 1;
#endif
    r->fLeft    = point.x();
    r->fTop     = point.y() + yOffset;
    r->fRight   = r->fLeft + width;
    r->fBottom  = r->fTop + lineThickness;
}

static inline int fastMod(int value, int max)
{
    int sign = SkExtractSign(value);

    value = SkApplySign(value, sign);
    if (value >= max)
        value %= max;
    return SkApplySign(value, sign);
}

static inline void fixPaintForBitmapsThatMaySeam(SkPaint* paint) {
    /*  Bitmaps may be drawn to seem next to other images. If we are drawn
        zoomed, or at fractional coordinates, we may see cracks/edges if
        we antialias, because that will cause us to draw the same pixels
        more than once (e.g. from the left and right bitmaps that share
        an edge).

        Disabling antialiasing fixes this, and since so far we are never
        rotated at non-multiple-of-90 angles, this seems to do no harm
     */
    paint->setAntiAlias(false);
}

//**************************************
// PlatformGraphicsContextSkia
//**************************************

PlatformGraphicsContextSkia::PlatformGraphicsContextSkia(SkCanvas* canvas,
                                                         bool takeCanvasOwnership)
    : PlatformGraphicsContext()
    , mCanvas(canvas)
    , m_deleteCanvas(takeCanvasOwnership)
{
    m_gc = 0;
}

PlatformGraphicsContextSkia::~PlatformGraphicsContextSkia()
{
    if (m_deleteCanvas)
        delete mCanvas;
}

bool PlatformGraphicsContextSkia::isPaintingDisabled()
{
    return !mCanvas;
}

//**************************************
// State management
//**************************************

void PlatformGraphicsContextSkia::beginTransparencyLayer(float opacity)
{
    SkCanvas* canvas = mCanvas;
    canvas->saveLayerAlpha(0, (int)(opacity * 255), TRANSPARENCY_SAVEFLAGS);
}

void PlatformGraphicsContextSkia::endTransparencyLayer()
{
    if (!mCanvas)
        return;
    mCanvas->restore();
}

void PlatformGraphicsContextSkia::save()
{
    PlatformGraphicsContext::save();
    // Save our native canvas.
    mCanvas->save();
}

void PlatformGraphicsContextSkia::restore()
{
    PlatformGraphicsContext::restore();
    // Restore our native canvas.
    mCanvas->restore();
}

//**************************************
// Matrix operations
//**************************************

void PlatformGraphicsContextSkia::concatCTM(const AffineTransform& affine)
{
    mCanvas->concat(affine);
}

void PlatformGraphicsContextSkia::rotate(float angleInRadians)
{
    float value = angleInRadians * (180.0f / 3.14159265f);
    mCanvas->rotate(SkFloatToScalar(value));
}

void PlatformGraphicsContextSkia::scale(const FloatSize& size)
{
    mCanvas->scale(SkFloatToScalar(size.width()), SkFloatToScalar(size.height()));
}

void PlatformGraphicsContextSkia::translate(float x, float y)
{
    mCanvas->translate(SkFloatToScalar(x), SkFloatToScalar(y));
}

const SkMatrix& PlatformGraphicsContextSkia::getTotalMatrix()
{
    return mCanvas->getTotalMatrix();
}

//**************************************
// Clipping
//**************************************

void PlatformGraphicsContextSkia::addInnerRoundedRectClip(const IntRect& rect,
                                                      int thickness)
{
    SkPath path;
    SkRect r(rect);

    path.addOval(r, SkPath::kCW_Direction);
    // Only perform the inset if we won't invert r
    if (2 * thickness < rect.width() && 2 * thickness < rect.height()) {
        // Adding one to the thickness doesn't make the border too thick as
        // it's painted over afterwards. But without this adjustment the
        // border appears a little anemic after anti-aliasing.
        r.inset(SkIntToScalar(thickness + 1), SkIntToScalar(thickness + 1));
        path.addOval(r, SkPath::kCCW_Direction);
    }
    mCanvas->clipPath(path, SkRegion::kIntersect_Op, true);
}

void PlatformGraphicsContextSkia::canvasClip(const Path& path)
{
    clip(path);
}

bool PlatformGraphicsContextSkia::clip(const FloatRect& rect)
{
    return mCanvas->clipRect(rect);
}

bool PlatformGraphicsContextSkia::clip(const Path& path)
{
    return mCanvas->clipPath(*path.platformPath(), SkRegion::kIntersect_Op, true);
}

bool PlatformGraphicsContextSkia::clipConvexPolygon(size_t numPoints,
                                                const FloatPoint*, bool antialias)
{
    if (numPoints <= 1)
        return true;

    // This is only used if HAVE_PATH_BASED_BORDER_RADIUS_DRAWING is defined
    // in RenderObject.h which it isn't for us. TODO: Support that :)
    return true;
}

bool PlatformGraphicsContextSkia::clipOut(const IntRect& r)
{
    return mCanvas->clipRect(r, SkRegion::kDifference_Op);
}

bool PlatformGraphicsContextSkia::clipOut(const Path& path)
{
    return mCanvas->clipPath(*path.platformPath(), SkRegion::kDifference_Op);
}

bool PlatformGraphicsContextSkia::clipPath(const Path& pathToClip, WindRule clipRule)
{
    SkPath path = *pathToClip.platformPath();
    path.setFillType(clipRule == RULE_EVENODD
            ? SkPath::kEvenOdd_FillType : SkPath::kWinding_FillType);
    return mCanvas->clipPath(path);
}

void PlatformGraphicsContextSkia::clearRect(const FloatRect& rect)
{
    SkPaint paint;

    setupPaintFill(&paint);
    paint.setXfermodeMode(SkXfermode::kClear_Mode);

    mCanvas->drawRect(rect, paint);
}

//**************************************
// Drawing
//**************************************

void PlatformGraphicsContextSkia::drawBitmapPattern(
        const SkBitmap& bitmap, const SkMatrix& matrix,
        CompositeOperator compositeOp, const FloatRect& destRect)
{
    SkShader* shader = SkShader::CreateBitmapShader(bitmap,
                                                    SkShader::kRepeat_TileMode,
                                                    SkShader::kRepeat_TileMode);
    shader->setLocalMatrix(matrix);
    SkPaint paint;
    setupPaintCommon(&paint);
    paint.setAlpha(getNormalizedAlpha());
    paint.setShader(shader);
    paint.setXfermodeMode(WebCoreCompositeToSkiaComposite(compositeOp));
    fixPaintForBitmapsThatMaySeam(&paint);
    mCanvas->drawRect(destRect, paint);
}

void PlatformGraphicsContextSkia::drawBitmapRect(const SkBitmap& bitmap,
                                             const SkIRect* src, const SkRect& dst,
                                             CompositeOperator op)
{
    SkPaint paint;
    setupPaintCommon(&paint);
    paint.setAlpha(getNormalizedAlpha());
    paint.setXfermodeMode(WebCoreCompositeToSkiaComposite(op));
    fixPaintForBitmapsThatMaySeam(&paint);

    mCanvas->drawBitmapRect(bitmap, src, dst, &paint);
}

void PlatformGraphicsContextSkia::drawConvexPolygon(size_t numPoints,
                                                const FloatPoint* points,
                                                bool shouldAntialias)
{
    if (numPoints <= 1)
        return;

    SkPaint paint;
    SkPath path;

    path.incReserve(numPoints);
    path.moveTo(SkFloatToScalar(points[0].x()), SkFloatToScalar(points[0].y()));
    for (size_t i = 1; i < numPoints; i++)
        path.lineTo(SkFloatToScalar(points[i].x()), SkFloatToScalar(points[i].y()));

    if (mCanvas->quickReject(path, shouldAntialias ?
            SkCanvas::kAA_EdgeType : SkCanvas::kBW_EdgeType)) {
        return;
    }

    if (m_state->fillColor & 0xFF000000) {
        setupPaintFill(&paint);
        paint.setAntiAlias(shouldAntialias);
        mCanvas->drawPath(path, paint);
    }

    if (m_state->strokeStyle != NoStroke) {
        paint.reset();
        setupPaintStroke(&paint, 0);
        paint.setAntiAlias(shouldAntialias);
        mCanvas->drawPath(path, paint);
    }
}

void PlatformGraphicsContextSkia::drawEllipse(const IntRect& rect)
{
    SkPaint paint;
    SkRect oval(rect);

    if (m_state->fillColor & 0xFF000000) {
        setupPaintFill(&paint);
        mCanvas->drawOval(oval, paint);
    }
    if (m_state->strokeStyle != NoStroke) {
        paint.reset();
        setupPaintStroke(&paint, &oval);
        mCanvas->drawOval(oval, paint);
    }
}

void PlatformGraphicsContextSkia::drawFocusRing(const Vector<IntRect>& rects,
                                            int /* width */, int /* offset */,
                                            const Color& color)
{
    unsigned rectCount = rects.size();
    if (!rectCount)
        return;

    SkRegion focusRingRegion;
    const SkScalar focusRingOutset = WebCoreFloatToSkScalar(0.8);
    for (unsigned i = 0; i < rectCount; i++) {
        SkIRect r = rects[i];
        r.inset(-focusRingOutset, -focusRingOutset);
        focusRingRegion.op(r, SkRegion::kUnion_Op);
    }

    SkPath path;
    SkPaint paint;
    paint.setAntiAlias(true);
    paint.setStyle(SkPaint::kStroke_Style);

    paint.setColor(color.rgb());
    paint.setStrokeWidth(focusRingOutset * 2);
    paint.setPathEffect(new SkCornerPathEffect(focusRingOutset * 2))->unref();
    focusRingRegion.getBoundaryPath(&path);
    mCanvas->drawPath(path, paint);
}

void PlatformGraphicsContextSkia::drawHighlightForText(
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

void PlatformGraphicsContextSkia::drawLine(const IntPoint& point1,
                                       const IntPoint& point2)
{
    StrokeStyle style = m_state->strokeStyle;
    if (style == NoStroke)
        return;

    SkPaint paint;
    SkCanvas* canvas = mCanvas;
    const int idx = SkAbs32(point2.x() - point1.x());
    const int idy = SkAbs32(point2.y() - point1.y());

    // Special-case horizontal and vertical lines that are really just dots
    if (setupPaintStroke(&paint, 0, !idy) && (!idx || !idy)) {
        const SkScalar diameter = paint.getStrokeWidth();
        const SkScalar radius = SkScalarHalf(diameter);
        SkScalar x = SkIntToScalar(SkMin32(point1.x(), point2.x()));
        SkScalar y = SkIntToScalar(SkMin32(point1.y(), point2.y()));
        SkScalar dx, dy;
        int count;
        SkRect bounds;

        if (!idy) { // Horizontal
            bounds.set(x, y - radius, x + SkIntToScalar(idx), y + radius);
            x += radius;
            dx = diameter * 2;
            dy = 0;
            count = idx;
        } else { // Vertical
            bounds.set(x - radius, y, x + radius, y + SkIntToScalar(idy));
            y += radius;
            dx = 0;
            dy = diameter * 2;
            count = idy;
        }

        // The actual count is the number of ONs we hit alternating
        // ON(diameter), OFF(diameter), ...
        {
            SkScalar width = SkScalarDiv(SkIntToScalar(count), diameter);
            // Now compute the number of cells (ON and OFF)
            count = SkScalarRound(width);
            // Now compute the number of ONs
            count = (count + 1) >> 1;
        }

        SkAutoMalloc storage(count * sizeof(SkPoint));
        SkPoint* verts = (SkPoint*)storage.get();
        // Now build the array of vertices to past to drawPoints
        for (int i = 0; i < count; i++) {
            verts[i].set(x, y);
            x += dx;
            y += dy;
        }

        paint.setStyle(SkPaint::kFill_Style);
        paint.setPathEffect(0);

        // Clipping to bounds is not required for correctness, but it does
        // allow us to reject the entire array of points if we are completely
        // offscreen. This is common in a webpage for android, where most of
        // the content is clipped out. If drawPoints took an (optional) bounds
        // parameter, that might even be better, as we would *just* use it for
        // culling, and not both wacking the canvas' save/restore stack.
        canvas->save(SkCanvas::kClip_SaveFlag);
        canvas->clipRect(bounds);
        canvas->drawPoints(SkCanvas::kPoints_PointMode, count, verts, paint);
        canvas->restore();
    } else {
        SkPoint pts[2] = { point1, point2 };
        canvas->drawLine(pts[0].fX, pts[0].fY, pts[1].fX, pts[1].fY, paint);
    }
}

void PlatformGraphicsContextSkia::drawLineForText(const FloatPoint& pt, float width)
{
    SkRect r;
    setrectForUnderline(&r, m_state->strokeThickness, pt, 0, width);

    SkPaint paint;
    paint.setAntiAlias(true);
    paint.setColor(m_state->strokeColor);

    mCanvas->drawRect(r, paint);
}

void PlatformGraphicsContextSkia::drawLineForTextChecking(const FloatPoint& pt,
        float width, GraphicsContext::TextCheckingLineStyle)
{
    // TODO: Should we draw different based on TextCheckingLineStyle?
    SkRect r;
    setrectForUnderline(&r, m_state->strokeThickness, pt, 0, width);

    SkPaint paint;
    paint.setAntiAlias(true);
    paint.setColor(SK_ColorRED); // Is this specified somewhere?

    mCanvas->drawRect(r, paint);
}

void PlatformGraphicsContextSkia::drawRect(const IntRect& rect)
{
    SkPaint paint;
    SkRect r(rect);

    if (m_state->fillColor & 0xFF000000) {
        setupPaintFill(&paint);
        mCanvas->drawRect(r, paint);
    }

    // According to GraphicsContext.h, stroking inside drawRect always means
    // a stroke of 1 inside the rect.
    if (m_state->strokeStyle != NoStroke && (m_state->strokeColor & 0xFF000000)) {
        paint.reset();
        setupPaintStroke(&paint, &r);
        paint.setPathEffect(0); // No dashing please
        paint.setStrokeWidth(SK_Scalar1); // Always just 1.0 width
        r.inset(SK_ScalarHalf, SK_ScalarHalf); // Ensure we're "inside"
        mCanvas->drawRect(r, paint);
    }
}

void PlatformGraphicsContextSkia::fillPath(const Path& pathToFill, WindRule fillRule)
{
    SkPath* path = pathToFill.platformPath();
    if (!path)
        return;

    switch (fillRule) {
    case RULE_NONZERO:
        path->setFillType(SkPath::kWinding_FillType);
        break;
    case RULE_EVENODD:
        path->setFillType(SkPath::kEvenOdd_FillType);
        break;
    }

    SkPaint paint;
    setupPaintFill(&paint);

    mCanvas->drawPath(*path, paint);
}

void PlatformGraphicsContextSkia::fillRect(const FloatRect& rect)
{
    SkPaint paint;
    setupPaintFill(&paint);
    mCanvas->drawRect(rect, paint);
}

void PlatformGraphicsContextSkia::fillRect(const FloatRect& rect,
                                       const Color& color)
{
    if (color.rgb() & 0xFF000000) {
        SkPaint paint;

        setupPaintCommon(&paint);
        paint.setColor(color.rgb()); // Punch in the specified color
        paint.setShader(0); // In case we had one set

        // Sometimes we record and draw portions of the page, using clips
        // for each portion. The problem with this is that webkit, sometimes,
        // sees that we're only recording a portion, and they adjust some of
        // their rectangle coordinates accordingly (e.g.
        // RenderBoxModelObject::paintFillLayerExtended() which calls
        // rect.intersect(paintInfo.rect) and then draws the bg with that
        // rect. The result is that we end up drawing rects that are meant to
        // seam together (one for each portion), but if the rects have
        // fractional coordinates (e.g. we are zoomed by a fractional amount)
        // we will double-draw those edges, resulting in visual cracks or
        // artifacts.

        // The fix seems to be to just turn off antialasing for rects (this
        // entry-point in GraphicsContext seems to have been sufficient,
        // though perhaps we'll find we need to do this as well in fillRect(r)
        // as well.) Currently setupPaintCommon() enables antialiasing.

        // Since we never show the page rotated at a funny angle, disabling
        // antialiasing seems to have no real down-side, and it does fix the
        // bug when we're zoomed (and drawing portions that need to seam).
        paint.setAntiAlias(false);

        mCanvas->drawRect(rect, paint);
    }
}

void PlatformGraphicsContextSkia::fillRoundedRect(
        const IntRect& rect, const IntSize& topLeft, const IntSize& topRight,
        const IntSize& bottomLeft, const IntSize& bottomRight,
        const Color& color)
{
    SkPaint paint;
    SkPath path;
    SkScalar radii[8];

    radii[0] = SkIntToScalar(topLeft.width());
    radii[1] = SkIntToScalar(topLeft.height());
    radii[2] = SkIntToScalar(topRight.width());
    radii[3] = SkIntToScalar(topRight.height());
    radii[4] = SkIntToScalar(bottomRight.width());
    radii[5] = SkIntToScalar(bottomRight.height());
    radii[6] = SkIntToScalar(bottomLeft.width());
    radii[7] = SkIntToScalar(bottomLeft.height());
    path.addRoundRect(rect, radii);

    setupPaintFill(&paint);
    paint.setColor(color.rgb());
    mCanvas->drawPath(path, paint);
}

void PlatformGraphicsContextSkia::strokeArc(const IntRect& r, int startAngle,
                                        int angleSpan)
{
    SkPath path;
    SkPaint paint;
    SkRect oval(r);

    if (m_state->strokeStyle == NoStroke) {
        setupPaintFill(&paint); // We want the fill color
        paint.setStyle(SkPaint::kStroke_Style);
        paint.setStrokeWidth(SkFloatToScalar(m_state->strokeThickness));
    } else
        setupPaintStroke(&paint, 0);

    // We do this before converting to scalar, so we don't overflow SkFixed
    startAngle = fastMod(startAngle, 360);
    angleSpan = fastMod(angleSpan, 360);

    path.addArc(oval, SkIntToScalar(-startAngle), SkIntToScalar(-angleSpan));
    mCanvas->drawPath(path, paint);
}

void PlatformGraphicsContextSkia::strokePath(const Path& pathToStroke)
{
    const SkPath* path = pathToStroke.platformPath();
    if (!path)
        return;

    SkPaint paint;
    setupPaintStroke(&paint, 0);

    mCanvas->drawPath(*path, paint);
}

void PlatformGraphicsContextSkia::strokeRect(const FloatRect& rect, float lineWidth)
{
    SkPaint paint;

    setupPaintStroke(&paint, 0);
    paint.setStrokeWidth(SkFloatToScalar(lineWidth));
    mCanvas->drawRect(rect, paint);
}

void PlatformGraphicsContextSkia::drawPosText(const void* text, size_t byteLength,
                                              const SkPoint pos[], const SkPaint& paint)
{
    mCanvas->drawPosText(text, byteLength, pos, paint);
}

void PlatformGraphicsContextSkia::drawMediaButton(const IntRect& rect, RenderSkinMediaButton::MediaButton buttonType,
                                                  bool translucent, bool drawBackground,
                                                  const IntRect& thumb)
{
    RenderSkinMediaButton::Draw(mCanvas, rect, buttonType, translucent, drawBackground, thumb);
}

}   // WebCore
