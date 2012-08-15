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

#ifndef GraphicsOperation_h
#define GraphicsOperation_h

#include "Color.h"
#include "FloatRect.h"
#include "GlyphBuffer.h"
#include "Font.h"
#include "IntRect.h"
#include "PlatformGraphicsContext.h"
#include "PlatformGraphicsContextSkia.h"
#include "SkCanvas.h"
#include "SkShader.h"
#include "SkRefCnt.h"

#include <utils/threads.h>
#include <wtf/text/CString.h>

#define TYPE_CASE(type) case type: return #type;

#define DEBUG_GRAPHICS_OPERATIONS false

#if DEBUG_GRAPHICS_OPERATIONS
#define TYPE(x) virtual OperationType type() { return x; }
#else
#define TYPE(x)
#endif

namespace WebCore {

class CanvasState;
class LinearAllocator;

namespace GraphicsOperation {

class Operation {
public:
    Operation()
        : m_state(0)
        , m_canvasState(0)
    {}

    void* operator new(size_t size, LinearAllocator* allocator);

    // Purposely not implemented - use a LinearAllocator please
    void* operator new(size_t size);
    void operator delete(void* ptr);

    // This m_state is applied by ourselves
    PlatformGraphicsContext::State* m_state;
    // This is the canvas state that this operation needs
    // Only used for drawing operations, state operations will be undefined
    CanvasState* m_canvasState;

    bool apply(PlatformGraphicsContext* context) {
        if (m_state)
            context->setRawState(m_state);
        return applyImpl(context);
    }
    virtual bool applyImpl(PlatformGraphicsContext* context) = 0;
    virtual ~Operation() {}

    virtual const IntRect*  opaqueRect() { return 0; }
    virtual bool isOpaque() { return false; }
    virtual void setOpaqueRect(const IntRect& bounds) {}

#if DEBUG_GRAPHICS_OPERATIONS
    typedef enum { UndefinedOperation
                  // Matrix operations
                  , ConcatCTMOperation
                  , ScaleOperation
                  , RotateOperation
                  , TranslateOperation
                  // Clipping
                  , InnerRoundedRectClipOperation
                  , ClipOperation
                  , ClipPathOperation
                  , ClipOutOperation
                  , ClearRectOperation
                  // Drawing
                  , DrawBitmapPatternOperation
                  , DrawBitmapRectOperation
                  , DrawConvexPolygonQuadOperation
                  , DrawEllipseOperation
                  , DrawFocusRingOperation
                  , DrawLineOperation
                  , DrawLineForTextOperation
                  , DrawLineForTextCheckingOperation
                  , DrawRectOperation
                  , FillPathOperation
                  , FillRectOperation
                  , FillRoundedRectOperation
                  , StrokeArcOperation
                  , StrokePathOperation
                  , StrokeRectOperation
                  , DrawMediaButtonOperation
                  // Text
                  , DrawPosTextOperation
    } OperationType;

    const char* name()
    {
        switch (type()) {
            TYPE_CASE(UndefinedOperation)
            // Matrix operations
            TYPE_CASE(ConcatCTMOperation)
            TYPE_CASE(ScaleOperation)
            TYPE_CASE(RotateOperation)
            TYPE_CASE(TranslateOperation)
            // Clipping
            TYPE_CASE(InnerRoundedRectClipOperation)
            TYPE_CASE(ClipOperation)
            TYPE_CASE(ClipPathOperation)
            TYPE_CASE(ClipOutOperation)
            TYPE_CASE(ClearRectOperation)
            // Drawing
            TYPE_CASE(DrawBitmapPatternOperation)
            TYPE_CASE(DrawBitmapRectOperation)
            TYPE_CASE(DrawConvexPolygonQuadOperation)
            TYPE_CASE(DrawEllipseOperation)
            TYPE_CASE(DrawFocusRingOperation)
            TYPE_CASE(DrawLineOperation)
            TYPE_CASE(DrawLineForTextOperation)
            TYPE_CASE(DrawLineForTextCheckingOperation)
            TYPE_CASE(DrawRectOperation)
            TYPE_CASE(FillPathOperation)
            TYPE_CASE(FillRectOperation)
            TYPE_CASE(FillRoundedRectOperation)
            TYPE_CASE(StrokeArcOperation)
            TYPE_CASE(StrokePathOperation)
            TYPE_CASE(StrokeRectOperation)
            TYPE_CASE(DrawMediaButtonOperation)
            // Text
            TYPE_CASE(DrawPosTextOperation)
        }
        return "Undefined";
    }
#endif
    TYPE(UndefinedOperation)
};

class PossiblyOpaqueOperation : public Operation {
public:
    virtual const IntRect* opaqueRect() { return &m_absoluteOpaqueRect; }
    virtual void setOpaqueRect(const IntRect& bounds) { m_absoluteOpaqueRect = bounds; }

private:
    IntRect m_absoluteOpaqueRect;
};

//**************************************
// Matrix operations
//**************************************

class ConcatCTM : public Operation {
public:
    ConcatCTM(const AffineTransform& affine) : m_matrix(affine) {}
    virtual bool applyImpl(PlatformGraphicsContext* context) {
        context->concatCTM(m_matrix);
        return true;
    }
    TYPE(ConcatCTMOperation)
private:
    AffineTransform m_matrix;
};

class Rotate : public Operation {
public:
    Rotate(float angleInRadians) : m_angle(angleInRadians) {}
    virtual bool applyImpl(PlatformGraphicsContext* context) {
        context->rotate(m_angle);
        return true;
    }
    TYPE(RotateOperation)
private:
    float m_angle;
};

class Scale : public Operation {
public:
    Scale(const FloatSize& size) : m_scale(size) {}
    virtual bool applyImpl(PlatformGraphicsContext* context) {
        context->scale(m_scale);
        return true;
    }
    TYPE(ScaleOperation)
private:
    FloatSize m_scale;
};

class Translate : public Operation {
public:
    Translate(float x, float y) : m_x(x), m_y(y) {}
    virtual bool applyImpl(PlatformGraphicsContext* context) {
        context->translate(m_x, m_y);
        return true;
    }
    TYPE(TranslateOperation)
private:
    float m_x;
    float m_y;
};

//**************************************
// Clipping
//**************************************

class InnerRoundedRectClip : public Operation {
public:
    InnerRoundedRectClip(const IntRect& rect, int thickness)
        : m_rect(rect), m_thickness(thickness) {}
    virtual bool applyImpl(PlatformGraphicsContext* context) {
        context->addInnerRoundedRectClip(m_rect, m_thickness);
        return true;
    }
    TYPE(InnerRoundedRectClipOperation)
private:
    IntRect m_rect;
    int m_thickness;
};

class Clip : public Operation {
public:
    Clip(const FloatRect& rect) : m_rect(rect) {}
    virtual bool applyImpl(PlatformGraphicsContext* context) {
        return context->clip(m_rect);
    }
    TYPE(ClipOperation)
private:
    const FloatRect m_rect;
};

class ClipPath : public Operation {
public:
    ClipPath(const Path& path, bool clipout = false)
        : m_path(path), m_clipOut(clipout), m_hasWindRule(false) {}
    void setWindRule(WindRule rule) { m_windRule = rule; m_hasWindRule = true; }
    virtual bool applyImpl(PlatformGraphicsContext* context) {
        if (m_hasWindRule) {
            return context->clipPath(m_path, m_windRule);
        }
        if (m_clipOut)
            return context->clipOut(m_path);
        else
            return context->clip(m_path);
    }
    TYPE(ClipPathOperation)
private:
    const Path m_path;
    bool m_clipOut;
    WindRule m_windRule;
    bool m_hasWindRule;
};

class ClipOut : public Operation {
public:
    ClipOut(const IntRect& rect) : m_rect(rect) {}
    virtual bool applyImpl(PlatformGraphicsContext* context) {
        return context->clipOut(m_rect);
    }
    TYPE(ClipOutOperation)
private:
    const IntRect m_rect;
};

class ClearRect : public Operation {
public:
    ClearRect(const FloatRect& rect) : m_rect(rect) {}
    virtual bool applyImpl(PlatformGraphicsContext* context) {
        context->clearRect(m_rect);
        return true;
    }
    TYPE(ClearRectOperation)
private:
    FloatRect m_rect;
};

//**************************************
// Drawing
//**************************************

class DrawBitmapPattern : public PossiblyOpaqueOperation {
public:
    DrawBitmapPattern(const SkBitmap& bitmap, const SkMatrix& matrix,
                      CompositeOperator op, const FloatRect& destRect)
        : m_bitmap(bitmap), m_matrix(matrix), m_operator(op), m_destRect(destRect) {}
    virtual bool applyImpl(PlatformGraphicsContext* context) {
        context->drawBitmapPattern(m_bitmap, m_matrix, m_operator, m_destRect);
        return true;
    }
    virtual bool isOpaque() { return m_bitmap.isOpaque(); }
    TYPE(DrawBitmapPatternOperation)

private:
    SkBitmap m_bitmap;
    SkMatrix m_matrix;
    CompositeOperator m_operator;
    FloatRect m_destRect;
};

class DrawBitmapRect : public PossiblyOpaqueOperation {
public:
    DrawBitmapRect(const SkBitmap& bitmap, const SkIRect& srcR,
                   const SkRect& dstR, CompositeOperator op)
        : m_bitmap(bitmap), m_srcR(srcR), m_dstR(dstR), m_operator(op) {}
    virtual bool applyImpl(PlatformGraphicsContext* context) {
        context->drawBitmapRect(m_bitmap, &m_srcR, m_dstR, m_operator);
        return true;
    }
    virtual bool isOpaque() { return m_bitmap.isOpaque(); }
    TYPE(DrawBitmapRectOperation)
private:
    SkBitmap m_bitmap;
    SkIRect m_srcR;
    SkRect m_dstR;
    CompositeOperator m_operator;
};

class DrawConvexPolygonQuad : public Operation {
public:
    DrawConvexPolygonQuad(const FloatPoint* points, bool shouldAntiAlias)
        : m_shouldAntiAlias(shouldAntiAlias)
    {
        memcpy(m_points, points, 4 * sizeof(FloatPoint));
    }
    virtual bool applyImpl(PlatformGraphicsContext* context) {
        context->drawConvexPolygon(4, m_points, m_shouldAntiAlias);
        return true;
    }
    TYPE(DrawConvexPolygonQuadOperation)
private:
    bool m_shouldAntiAlias;
    FloatPoint m_points[4];
};

class DrawEllipse : public Operation {
public:
    DrawEllipse(const IntRect& rect) : m_rect(rect) {}
    virtual bool applyImpl(PlatformGraphicsContext* context) {
        context->drawEllipse(m_rect);
        return true;
    }
    TYPE(DrawEllipseOperation)
private:
    IntRect m_rect;
};

class DrawFocusRing : public Operation {
public:
    DrawFocusRing(const Vector<IntRect>& rects, int width, int offset, const Color& color)
        : m_rects(rects)
        , m_width(width)
        , m_offset(offset)
        , m_color(color)
    {}
    virtual bool applyImpl(PlatformGraphicsContext* context) {
        context->drawFocusRing(m_rects, m_width, m_offset, m_color);
        return true;
    }
    TYPE(DrawFocusRingOperation)
private:
    Vector<IntRect> m_rects;
    int m_width;
    int m_offset;
    Color m_color;
};

class DrawLine : public Operation {
public:
    DrawLine(const IntPoint& point1, const IntPoint& point2)
        : m_point1(point1), m_point2(point2) {}
    virtual bool applyImpl(PlatformGraphicsContext* context) {
        context->drawLine(m_point1, m_point2);
        return true;
    }
    TYPE(DrawLineOperation)
private:
    IntPoint m_point1;
    IntPoint m_point2;
};

class DrawLineForText : public Operation {
public:
    DrawLineForText(const FloatPoint& pt, float width)
        : m_point(pt), m_width(width) {}
    virtual bool applyImpl(PlatformGraphicsContext* context) {
        context->drawLineForText(m_point, m_width);
        return true;
    }
    TYPE(DrawLineForTextOperation)
private:
    FloatPoint m_point;
    float m_width;
};

class DrawLineForTextChecking : public Operation {
public:
    DrawLineForTextChecking(const FloatPoint& pt, float width,
                            GraphicsContext::TextCheckingLineStyle lineStyle)
        : m_point(pt), m_width(width), m_lineStyle(lineStyle) {}
    virtual bool applyImpl(PlatformGraphicsContext* context) {
        context->drawLineForTextChecking(m_point, m_width, m_lineStyle);
        return true;
    }
    TYPE(DrawLineForTextCheckingOperation)
private:
    FloatPoint m_point;
    float m_width;
    GraphicsContext::TextCheckingLineStyle m_lineStyle;
};

class DrawRect : public Operation {
public:
    DrawRect(const IntRect& rect) : m_rect(rect) {}
    virtual bool applyImpl(PlatformGraphicsContext* context) {
        context->drawRect(m_rect);
        return true;
    }
    TYPE(DrawRectOperation)
private:
    IntRect m_rect;
};

class FillPath : public Operation {
public:
    FillPath(const Path& pathToFill, WindRule fillRule)
        : m_path(pathToFill), m_fillRule(fillRule) {}
    virtual bool applyImpl(PlatformGraphicsContext* context) {
        context->fillPath(m_path, m_fillRule);
        return true;
    }
    TYPE(FillPathOperation)
private:
    Path m_path;
    WindRule m_fillRule;
};

class FillRect : public PossiblyOpaqueOperation {
public:
    FillRect(const FloatRect& rect) : m_rect(rect), m_hasColor(false) {}
    void setColor(Color c) { m_color = c; m_hasColor = true; }
    virtual bool applyImpl(PlatformGraphicsContext* context) {
        if (m_hasColor)
             context->fillRect(m_rect, m_color);
        else
             context->fillRect(m_rect);
        return true;
    }
    virtual bool isOpaque() { return (m_hasColor && !m_color.hasAlpha())
            || (!m_hasColor && SkColorGetA(m_state->fillColor) == 0xFF); }
    TYPE(FillRectOperation)
private:
    FloatRect m_rect;
    Color m_color;
    bool m_hasColor;
};

class FillRoundedRect : public Operation {
public:
    FillRoundedRect(const IntRect& rect,
                    const IntSize& topLeft,
                    const IntSize& topRight,
                    const IntSize& bottomLeft,
                    const IntSize& bottomRight,
                    const Color& color)
        : m_rect(rect)
        , m_topLeft(topLeft)
        , m_topRight(topRight)
        , m_bottomLeft(bottomLeft)
        , m_bottomRight(bottomRight)
        , m_color(color)
    {}
    virtual bool applyImpl(PlatformGraphicsContext* context) {
        context->fillRoundedRect(m_rect, m_topLeft, m_topRight,
                                 m_bottomLeft, m_bottomRight,
                                 m_color);
        return true;
    }
    TYPE(FillRoundedRectOperation)
private:
    IntRect m_rect;
    IntSize m_topLeft;
    IntSize m_topRight;
    IntSize m_bottomLeft;
    IntSize m_bottomRight;
    Color m_color;
};

class StrokeArc : public Operation {
public:
    StrokeArc(const IntRect& r, int startAngle, int angleSpan)
        : m_rect(r)
        , m_startAngle(startAngle)
        , m_angleSpan(angleSpan)
    {}
    virtual bool applyImpl(PlatformGraphicsContext* context) {
        context->strokeArc(m_rect, m_startAngle, m_angleSpan);
        return true;
    }
    TYPE(StrokeArcOperation)
private:
    IntRect m_rect;
    int m_startAngle;
    int m_angleSpan;
};

class StrokePath : public Operation {
public:
    StrokePath(const Path& path) : m_path(path) {}
    virtual bool applyImpl(PlatformGraphicsContext* context) {
        context->strokePath(m_path);
        return true;
    }
    TYPE(StrokePathOperation)
private:
    Path m_path;
};


class StrokeRect : public Operation {
public:
    StrokeRect(const FloatRect& rect, float lineWidth)
        : m_rect(rect), m_lineWidth(lineWidth) {}
    virtual bool applyImpl(PlatformGraphicsContext* context) {
        context->strokeRect(m_rect, m_lineWidth);
        return true;
    }
    TYPE(StrokeRectOperation)
private:
    FloatRect m_rect;
    float m_lineWidth;
};

class DrawMediaButton : public Operation {
public:
    DrawMediaButton(const IntRect& rect, RenderSkinMediaButton::MediaButton buttonType,
                    bool translucent, bool drawBackground,
                    const IntRect& thumb)
        : m_rect(rect)
        , m_thumb(thumb)
        , m_buttonType(buttonType)
        , m_translucent(translucent)
        , m_drawBackground(drawBackground)
    {}
    virtual bool applyImpl(PlatformGraphicsContext* context) {
        context->drawMediaButton(m_rect, m_buttonType, m_translucent, m_drawBackground, m_thumb);
        return true;
    }
    TYPE(DrawMediaButtonOperation)
private:
    IntRect m_rect;
    IntRect m_thumb;
    RenderSkinMediaButton::MediaButton m_buttonType;
    bool m_translucent : 1;
    bool m_drawBackground : 1;
};

//**************************************
// Text
//**************************************

class DrawPosText : public Operation {
public:
    DrawPosText(const void* text, size_t byteLength,
                const SkPoint pos[], const SkPaint* paint)
        : m_text(text)
        , m_byteLength(byteLength)
        , m_pos(pos)
        , m_paint(paint)
    {}
    virtual bool applyImpl(PlatformGraphicsContext* context) {
        context->drawPosText(m_text, m_byteLength, m_pos, *m_paint);
        return true;
    }
    TYPE(DrawPosTextOperation)
private:
    const void* m_text;
    size_t m_byteLength;
    const SkPoint* m_pos;
    const SkPaint* m_paint;
};

}

}

#endif // GraphicsOperation_h
