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

#define LOG_TAG "PlatformGraphicsContext"
#define LOG_NDEBUG 1

#include "config.h"
#include "PlatformGraphicsContext.h"

#include "AndroidLog.h"
#include "SkBlurDrawLooper.h"
#include "SkBlurMaskFilter.h"
#include "SkColorPriv.h"
#include "SkDashPathEffect.h"
#include "SkPaint.h"
#include "SkShader.h"
#include "SkiaUtils.h"

namespace WebCore {

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

//**************************************
// State implementation
//**************************************

PlatformGraphicsContext::State::State()
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

PlatformGraphicsContext::State::State(const State& other)
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

PlatformGraphicsContext::State::~State()
{
    SkSafeUnref(pathEffect);
    SkSafeUnref(fillShader);
    SkSafeUnref(strokeShader);
}

void PlatformGraphicsContext::State::setShadow(int radius, int dx, int dy, SkColor c)
{
    // Cut the radius in half, to visually match the effect seen in
    // safari browser
    shadow.blur = SkScalarHalf(SkIntToScalar(radius));
    shadow.dx = SkIntToScalar(dx);
    shadow.dy = SkIntToScalar(dy);
    shadow.color = c;
}

bool PlatformGraphicsContext::State::setupShadowPaint(SkPaint* paint, SkPoint* offset,
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

SkColor PlatformGraphicsContext::State::applyAlpha(SkColor c) const
{
    int s = RoundToInt(alpha * 256);
    if (s >= 256)
        return c;
    if (s < 0)
        return 0;

    int a = SkAlphaMul(SkColorGetA(c), s);
    return (c & 0x00FFFFFF) | (a << 24);
}

// Returns a new State with all of this object's inherited properties copied.
PlatformGraphicsContext::State PlatformGraphicsContext::State::cloneInheritedProperties()
{
    return PlatformGraphicsContext::State(*this);
}

//**************************************
// PlatformGraphicsContext
//**************************************

PlatformGraphicsContext::PlatformGraphicsContext()
{
    m_stateStack.append(State());
    m_state = &m_stateStack.last();
}

PlatformGraphicsContext::~PlatformGraphicsContext()
{
}

//**************************************
// State management
//**************************************

void PlatformGraphicsContext::save()
{
    m_stateStack.append(m_state->cloneInheritedProperties());
    m_state = &m_stateStack.last();
}

void PlatformGraphicsContext::restore()
{
    m_stateStack.removeLast();
    m_state = &m_stateStack.last();
}

//**************************************
// State setters
//**************************************

void PlatformGraphicsContext::setAlpha(float alpha)
{
    m_state->alpha = alpha;
}

int PlatformGraphicsContext::getNormalizedAlpha() const
{
    int alpha = roundf(m_state->alpha * 256);
    if (alpha > 255)
        alpha = 255;
    else if (alpha < 0)
        alpha = 0;
    return alpha;
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
        ALOGD("PlatformGraphicsContextSkia::setLineCap: unknown LineCap %d\n", cap);
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
        ALOGD("PlatformGraphicsContextSkia::setLineJoin: unknown LineJoin %d\n", join);
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

}   // WebCore
