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
#include "PlatformGraphicsContext.h"

#include "AffineTransform.h"
#include "Font.h"
#include "Gradient.h"
#include "GraphicsContext.h"
#include "Node.h"
#include "NotImplemented.h"
#include "Path.h"
#include "Pattern.h"
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
#include "android_graphics.h"

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

static int RoundToInt(float x)
{
    return (int)roundf(x);
}

template <typename T> T* deepCopyPtr(const T* src)
{
    return src ? new T(*src) : 0;
}

// Set a bitmap shader that mimics dashing by width-on, width-off.
// Returns false if it could not succeed (e.g. there was an existing shader)
static bool setBitmapDash(SkPaint* paint, int width) {
    if (width <= 0 || paint->getShader())
        return false;

    SkColor c = paint->getColor();

    SkBitmap bm;
    bm.setConfig(SkBitmap::kARGB_8888_Config, 2, 1);
    bm.allocPixels();
    bm.lockPixels();

    // set the ON pixel
    *bm.getAddr32(0, 0) = SkPreMultiplyARGB(0xFF, SkColorGetR(c),
                                            SkColorGetG(c), SkColorGetB(c));
    // set the OFF pixel
    *bm.getAddr32(1, 0) = 0;
    bm.unlockPixels();

    SkMatrix matrix;
    matrix.setScale(SkIntToScalar(width), SK_Scalar1);

    SkShader* s = SkShader::CreateBitmapShader(bm, SkShader::kRepeat_TileMode,
                                               SkShader::kClamp_TileMode);
    s->setLocalMatrix(matrix);

    paint->setShader(s)->unref();
    return true;
}

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
// State structs
//**************************************

struct ShadowRec {
    SkScalar blur;
    SkScalar dx;
    SkScalar dy;
    SkColor color;  // alpha>0 means valid shadow
    ShadowRec(SkScalar b = 0,
              SkScalar x = 0,
              SkScalar y = 0,
              SkColor c = 0) // by default, alpha=0, so no shadow
            : blur(b), dx(x), dy(y), color(c)
        {};
};

struct PlatformGraphicsContext::State {
    SkPathEffect* pathEffect;
    float miterLimit;
    float alpha;
    float strokeThickness;
    SkPaint::Cap lineCap;
    SkPaint::Join lineJoin;
    SkXfermode::Mode mode;
    int dashRatio; // Ratio of the length of a dash to its width
    ShadowRec shadow;
    SkColor fillColor;
    SkShader* fillShader;
    SkColor strokeColor;
    SkShader* strokeShader;
    bool useAA;
    StrokeStyle strokeStyle;

    State()
        : pathEffect(0)
        , miterLimit(4)
        , alpha(1)
        , strokeThickness(0) // Same as default in GraphicsContextPrivate.h
        , lineCap(SkPaint::kDefault_Cap)
        , lineJoin(SkPaint::kDefault_Join)
        , mode(SkXfermode::kSrcOver_Mode)
        , dashRatio(3)
        , fillColor(SK_ColorBLACK)
        , fillShader(0)
        , strokeColor(SK_ColorBLACK)
        , strokeShader(0)
        , useAA(true)
        , strokeStyle(SolidStroke)
    {
    }

    State(const State& other)
        : pathEffect(other.pathEffect)
        , miterLimit(other.miterLimit)
        , alpha(other.alpha)
        , strokeThickness(other.strokeThickness)
        , lineCap(other.lineCap)
        , lineJoin(other.lineJoin)
        , mode(other.mode)
        , dashRatio(other.dashRatio)
        , shadow(other.shadow)
        , fillColor(other.fillColor)
        , fillShader(other.fillShader)
        , strokeColor(other.strokeColor)
        , strokeShader(other.strokeShader)
        , useAA(other.useAA)
        , strokeStyle(other.strokeStyle)
    {
        SkSafeRef(pathEffect);
        SkSafeRef(fillShader);
        SkSafeRef(strokeShader);
    }

    ~State()
    {
        SkSafeUnref(pathEffect);
        SkSafeUnref(fillShader);
        SkSafeUnref(strokeShader);
    }

    void setShadow(int radius, int dx, int dy, SkColor c)
    {
        // Cut the radius in half, to visually match the effect seen in
        // safari browser
        shadow.blur = SkScalarHalf(SkIntToScalar(radius));
        shadow.dx = SkIntToScalar(dx);
        shadow.dy = SkIntToScalar(dy);
        shadow.color = c;
    }

    bool setupShadowPaint(SkPaint* paint, SkPoint* offset,
                          bool shadowsIgnoreTransforms)
    {
        paint->setAntiAlias(true);
        paint->setDither(true);
        paint->setXfermodeMode(mode);
        paint->setColor(shadow.color);
        offset->set(shadow.dx, shadow.dy);

        // Currently, only GraphicsContexts associated with the
        // HTMLCanvasElement have shadows ignore transforms set.  This
        // allows us to distinguish between CSS and Canvas shadows which
        // have different rendering specifications.
        uint32_t flags = SkBlurMaskFilter::kHighQuality_BlurFlag;
        if (shadowsIgnoreTransforms) {
            offset->fY = -offset->fY;
            flags |= SkBlurMaskFilter::kIgnoreTransform_BlurFlag;
        }

        if (shadow.blur > 0) {
            paint->setMaskFilter(SkBlurMaskFilter::Create(shadow.blur,
                                 SkBlurMaskFilter::kNormal_BlurStyle))->unref();
        }
        return SkColorGetA(shadow.color) && (shadow.blur || shadow.dx || shadow.dy);
    }

    SkColor applyAlpha(SkColor c) const
    {
        int s = RoundToInt(alpha * 256);
        if (s >= 256)
            return c;
        if (s < 0)
            return 0;

        int a = SkAlphaMul(SkColorGetA(c), s);
        return (c & 0x00FFFFFF) | (a << 24);
    }

    PlatformGraphicsContext::State cloneInheritedProperties();
private:
    // Not supported.
    void operator=(const State&);
};

// Returns a new State with all of this object's inherited properties copied.
PlatformGraphicsContext::State PlatformGraphicsContext::State::cloneInheritedProperties()
{
    return PlatformGraphicsContext::State(*this);
}

//**************************************
// PlatformGraphicsContext
//**************************************

PlatformGraphicsContext::PlatformGraphicsContext(SkCanvas* canvas,
                                                 bool takeCanvasOwnership)
        : mCanvas(canvas)
        , m_deleteCanvas(takeCanvasOwnership)
        , m_stateStack(sizeof(State))
        , m_gc(0)
{
    m_stateStack.append(State());
    m_state = &m_stateStack.last();
}

PlatformGraphicsContext::~PlatformGraphicsContext()
{
    if (m_deleteCanvas)
        delete mCanvas;
}

//**************************************
// State management
//**************************************

void PlatformGraphicsContext::beginTransparencyLayer(float opacity)
{
    SkCanvas* canvas = mCanvas;
    canvas->saveLayerAlpha(0, (int)(opacity * 255), TRANSPARENCY_SAVEFLAGS);
}

void PlatformGraphicsContext::endTransparencyLayer()
{
    mCanvas->restore();
}

void PlatformGraphicsContext::save()
{
    m_stateStack.append(m_state->cloneInheritedProperties());
    m_state = &m_stateStack.last();

    // Save our native canvas.
    mCanvas->save();
}

void PlatformGraphicsContext::restore()
{
    m_stateStack.removeLast();
    m_state = &m_stateStack.last();

    // Restore our native canvas.
    mCanvas->restore();
}

//**************************************
// State setters
//**************************************

void PlatformGraphicsContext::setAlpha(float alpha)
{
    m_state->alpha = alpha;
}

void PlatformGraphicsContext::setCompositeOperation(CompositeOperator op)
{
    m_state->mode = WebCoreCompositeToSkiaComposite(op);
}

void PlatformGraphicsContext::setFillColor(const Color& c)
{
    m_state->fillColor = c.rgb();
    setFillShader(0);
}

void PlatformGraphicsContext::setFillShader(SkShader* fillShader)
{
    if (fillShader)
        m_state->fillColor = Color::black;

    if (fillShader != m_state->fillShader) {
        SkSafeUnref(m_state->fillShader);
        m_state->fillShader = fillShader;
        SkSafeRef(m_state->fillShader);
    }
}

void PlatformGraphicsContext::setLineCap(LineCap cap)
{
    switch (cap) {
    case ButtCap:
        m_state->lineCap = SkPaint::kButt_Cap;
        break;
    case RoundCap:
        m_state->lineCap = SkPaint::kRound_Cap;
        break;
    case SquareCap:
        m_state->lineCap = SkPaint::kSquare_Cap;
        break;
    default:
        SkDEBUGF(("PlatformGraphicsContext::setLineCap: unknown LineCap %d\n", cap));
        break;
    }
}

void PlatformGraphicsContext::setLineDash(const DashArray& dashes, float dashOffset)
{
    size_t dashLength = dashes.size();
    if (!dashLength)
        return;

    size_t count = !(dashLength % 2) ? dashLength : dashLength * 2;
    SkScalar* intervals = new SkScalar[count];

    for (unsigned int i = 0; i < count; i++)
        intervals[i] = SkFloatToScalar(dashes[i % dashLength]);
    SkPathEffect **effectPtr = &m_state->pathEffect;
    SkSafeUnref(*effectPtr);
    *effectPtr = new SkDashPathEffect(intervals, count, SkFloatToScalar(dashOffset));

    delete[] intervals;
}

void PlatformGraphicsContext::setLineJoin(LineJoin join)
{
    switch (join) {
    case MiterJoin:
        m_state->lineJoin = SkPaint::kMiter_Join;
        break;
    case RoundJoin:
        m_state->lineJoin = SkPaint::kRound_Join;
        break;
    case BevelJoin:
        m_state->lineJoin = SkPaint::kBevel_Join;
        break;
    default:
        SkDEBUGF(("PlatformGraphicsContext::setLineJoin: unknown LineJoin %d\n", join));
        break;
    }
}

void PlatformGraphicsContext::setMiterLimit(float limit)
{
    m_state->miterLimit = limit;
}

void PlatformGraphicsContext::setShadow(int radius, int dx, int dy, SkColor c)
{
    m_state->setShadow(radius, dx, dy, c);
}

void PlatformGraphicsContext::setShouldAntialias(bool useAA)
{
    m_state->useAA = useAA;
}

void PlatformGraphicsContext::setStrokeColor(const Color& c)
{
    m_state->strokeColor = c.rgb();
    setStrokeShader(0);
}

void PlatformGraphicsContext::setStrokeShader(SkShader* strokeShader)
{
    if (strokeShader)
        m_state->strokeColor = Color::black;

    if (strokeShader != m_state->strokeShader) {
        SkSafeUnref(m_state->strokeShader);
        m_state->strokeShader = strokeShader;
        SkSafeRef(m_state->strokeShader);
    }
}

void PlatformGraphicsContext::setStrokeStyle(StrokeStyle style)
{
    m_state->strokeStyle = style;
}

void PlatformGraphicsContext::setStrokeThickness(float f)
{
    m_state->strokeThickness = f;
}

//**************************************
// Paint setup
//**************************************

void PlatformGraphicsContext::setupPaintCommon(SkPaint* paint) const
{
    paint->setAntiAlias(m_state->useAA);
    paint->setDither(true);
    paint->setXfermodeMode(m_state->mode);
    if (SkColorGetA(m_state->shadow.color) > 0) {

        // Currently, only GraphicsContexts associated with the
        // HTMLCanvasElement have shadows ignore transforms set.  This
        // allows us to distinguish between CSS and Canvas shadows which
        // have different rendering specifications.
        SkScalar dy = m_state->shadow.dy;
        uint32_t flags = SkBlurDrawLooper::kHighQuality_BlurFlag;
        if (shadowsIgnoreTransforms()) {
            dy = -dy;
            flags |= SkBlurDrawLooper::kIgnoreTransform_BlurFlag;
            flags |= SkBlurDrawLooper::kOverrideColor_BlurFlag;
        }

        SkDrawLooper* looper = new SkBlurDrawLooper(m_state->shadow.blur,
                                                    m_state->shadow.dx,
                                                    dy,
                                                    m_state->shadow.color,
                                                    flags);
        paint->setLooper(looper)->unref();
    }
    paint->setFilterBitmap(true);
}

void PlatformGraphicsContext::setupPaintFill(SkPaint* paint) const
{
    this->setupPaintCommon(paint);
    paint->setColor(m_state->applyAlpha(m_state->fillColor));
    paint->setShader(m_state->fillShader);
}

bool PlatformGraphicsContext::setupPaintShadow(SkPaint* paint, SkPoint* offset) const
{
    return m_state->setupShadowPaint(paint, offset, shadowsIgnoreTransforms());
}

bool PlatformGraphicsContext::setupPaintStroke(SkPaint* paint, SkRect* rect,
                                               bool isHLine)
{
    this->setupPaintCommon(paint);
    paint->setColor(m_state->applyAlpha(m_state->strokeColor));
    paint->setShader(m_state->strokeShader);

    float width = m_state->strokeThickness;

    // This allows dashing and dotting to work properly for hairline strokes
    // FIXME: Should we only do this for dashed and dotted strokes?
    if (!width)
        width = 1;

    paint->setStyle(SkPaint::kStroke_Style);
    paint->setStrokeWidth(SkFloatToScalar(width));
    paint->setStrokeCap(m_state->lineCap);
    paint->setStrokeJoin(m_state->lineJoin);
    paint->setStrokeMiter(SkFloatToScalar(m_state->miterLimit));

    if (rect && (RoundToInt(width) & 1))
        rect->inset(-SK_ScalarHalf, -SK_ScalarHalf);

    SkPathEffect* pe = m_state->pathEffect;
    if (pe) {
        paint->setPathEffect(pe);
        return false;
    }
    switch (m_state->strokeStyle) {
    case NoStroke:
    case SolidStroke:
        width = 0;
        break;
    case DashedStroke:
        width = m_state->dashRatio * width;
        break;
        // No break
    case DottedStroke:
        break;
    }

    if (width > 0) {
        // Return true if we're basically a dotted dash of squares
        bool justSqrs = RoundToInt(width) == RoundToInt(paint->getStrokeWidth());

        if (justSqrs || !isHLine || !setBitmapDash(paint, width)) {
#if 0
            // this is slow enough that we just skip it for now
            // see http://b/issue?id=4163023
            SkScalar intervals[] = { width, width };
            pe = new SkDashPathEffect(intervals, 2, 0);
            paint->setPathEffect(pe)->unref();
#endif
        }
        return justSqrs;
    }
    return false;
}

//**************************************
// Matrix operations
//**************************************

void PlatformGraphicsContext::concatCTM(const AffineTransform& affine)
{
    mCanvas->concat(affine);
}

void PlatformGraphicsContext::rotate(float angleInRadians)
{
    mCanvas->rotate(SkFloatToScalar(angleInRadians * (180.0f / 3.14159265f)));
}

void PlatformGraphicsContext::scale(const FloatSize& size)
{
    mCanvas->scale(SkFloatToScalar(size.width()), SkFloatToScalar(size.height()));
}

void PlatformGraphicsContext::translate(float x, float y)
{
    mCanvas->translate(SkFloatToScalar(x), SkFloatToScalar(y));
}

//**************************************
// Clipping
//**************************************

void PlatformGraphicsContext::addInnerRoundedRectClip(const IntRect& rect,
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

void PlatformGraphicsContext::canvasClip(const Path& path)
{
    clip(path);
}

void PlatformGraphicsContext::clip(const FloatRect& rect)
{
    mCanvas->clipRect(rect);
}

void PlatformGraphicsContext::clip(const Path& path)
{
    mCanvas->clipPath(*path.platformPath(), SkRegion::kIntersect_Op, true);
}

void PlatformGraphicsContext::clipConvexPolygon(size_t numPoints,
                                                const FloatPoint*, bool antialias)
{
    if (numPoints <= 1)
        return;

    // This is only used if HAVE_PATH_BASED_BORDER_RADIUS_DRAWING is defined
    // in RenderObject.h which it isn't for us. TODO: Support that :)
}

void PlatformGraphicsContext::clipOut(const IntRect& r)
{
    mCanvas->clipRect(r, SkRegion::kDifference_Op);
}

void PlatformGraphicsContext::clipOut(const Path& p)
{
    mCanvas->clipPath(*p.platformPath(), SkRegion::kDifference_Op);
}

void PlatformGraphicsContext::clipPath(const Path& pathToClip, WindRule clipRule)
{
    SkPath path = *pathToClip.platformPath();
    path.setFillType(clipRule == RULE_EVENODD
            ? SkPath::kEvenOdd_FillType : SkPath::kWinding_FillType);
    mCanvas->clipPath(path);
}
void PlatformGraphicsContext::clearRect(const FloatRect& rect)
{
    SkPaint paint;

    setupPaintFill(&paint);
    paint.setXfermodeMode(SkXfermode::kClear_Mode);
    mCanvas->drawRect(rect, paint);
}

//**************************************
// Drawing
//**************************************

void PlatformGraphicsContext::drawBitmapPattern(
        const SkBitmap& bitmap, const SkMatrix& matrix,
        CompositeOperator compositeOp, const FloatRect& destRect)
{
    SkShader* shader = SkShader::CreateBitmapShader(bitmap,
                                                    SkShader::kRepeat_TileMode,
                                                    SkShader::kRepeat_TileMode);
    shader->setLocalMatrix(matrix);
    SkPaint paint;
    setupPaintFill(&paint);
    paint.setShader(shader);
    paint.setXfermodeMode(WebCoreCompositeToSkiaComposite(compositeOp));
    fixPaintForBitmapsThatMaySeam(&paint);
    mCanvas->drawRect(destRect, paint);
}

void PlatformGraphicsContext::drawBitmapRect(const SkBitmap& bitmap,
                                             const SkIRect* src, const SkRect& dst,
                                             CompositeOperator op)
{
    SkPaint paint;
    setupPaintFill(&paint);
    paint.setXfermodeMode(WebCoreCompositeToSkiaComposite(op));
    fixPaintForBitmapsThatMaySeam(&paint);
    mCanvas->drawBitmapRect(bitmap, src, dst, &paint);
}

void PlatformGraphicsContext::drawConvexPolygon(size_t numPoints,
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

void PlatformGraphicsContext::drawEllipse(const IntRect& rect)
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

void PlatformGraphicsContext::drawFocusRing(const Vector<IntRect>& rects,
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

void PlatformGraphicsContext::drawHighlightForText(
        const Font& font, const TextRun& run, const FloatPoint& point, int h,
        const Color& backgroundColor, ColorSpace colorSpace, int from,
        int to, bool isActive)
{
    IntRect rect = (IntRect)font.selectionRectForText(run, point, h, from, to);
    if (isActive)
        fillRect(rect, backgroundColor, colorSpace);
    else {
        int x = rect.x(), y = rect.y(), w = rect.width(), h = rect.height();
        const int t = 3, t2 = t * 2;

        fillRect(IntRect(x, y, w, t), backgroundColor, colorSpace);
        fillRect(IntRect(x, y+h-t, w, t), backgroundColor, colorSpace);
        fillRect(IntRect(x, y+t, t, h-t2), backgroundColor, colorSpace);
        fillRect(IntRect(x+w-t, y+t, t, h-t2), backgroundColor, colorSpace);
    }
}

void PlatformGraphicsContext::drawLine(const IntPoint& point1,
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

void PlatformGraphicsContext::drawLineForText(const FloatPoint& pt, float width)
{
    SkRect r;
    setrectForUnderline(&r, m_state->strokeThickness, pt, 0, width);

    SkPaint paint;
    paint.setAntiAlias(true);
    paint.setColor(m_state->strokeColor);

    mCanvas->drawRect(r, paint);
}

void PlatformGraphicsContext::drawLineForTextChecking(const FloatPoint& pt,
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

void PlatformGraphicsContext::drawRect(const IntRect& rect)
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

void PlatformGraphicsContext::fillPath(const Path& pathToFill, WindRule fillRule)
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

void PlatformGraphicsContext::fillRect(const FloatRect& rect)
{
    SkPaint paint;
    setupPaintFill(&paint);
    mCanvas->drawRect(rect, paint);
}

void PlatformGraphicsContext::fillRect(const FloatRect& rect,
                                       const Color& color, ColorSpace)
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

void PlatformGraphicsContext::fillRoundedRect(
        const IntRect& rect, const IntSize& topLeft, const IntSize& topRight,
        const IntSize& bottomLeft, const IntSize& bottomRight,
        const Color& color, ColorSpace)
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

void PlatformGraphicsContext::strokeArc(const IntRect& r, int startAngle,
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

void PlatformGraphicsContext::strokePath(const Path& pathToStroke)
{
    const SkPath* path = pathToStroke.platformPath();
    if (!path)
        return;

    SkPaint paint;
    setupPaintStroke(&paint, 0);

    mCanvas->drawPath(*path, paint);
}

void PlatformGraphicsContext::strokeRect(const FloatRect& rect, float lineWidth)
{
    SkPaint paint;

    setupPaintStroke(&paint, 0);
    paint.setStrokeWidth(SkFloatToScalar(lineWidth));
    mCanvas->drawRect(rect, paint);
}

}   // WebCore
