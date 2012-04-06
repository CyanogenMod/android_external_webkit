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

#if USE(ACCELERATED_COMPOSITING)

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

namespace WebCore {

namespace GraphicsOperation {

class Operation : public SkRefCnt {
public:
    typedef enum { UndefinedOperation
                  // State management
                  , BeginTransparencyLayerOperation
                  , EndTransparencyLayerOperation
                  , SaveOperation
                  , RestoreOperation
                  // State setters
                  , SetAlphaOperation
                  , SetCompositeOpOperation
                  , SetFillColorOperation
                  , SetFillShaderOperation
                  , SetLineCapOperation
                  , SetLineDashOperation
                  , SetLineJoinOperation
                  , SetMiterLimitOperation
                  , SetShadowOperation
                  , SetShouldAntialiasOperation
                  , SetStrokeColorOperation
                  , SetStrokeShaderOperation
                  , SetStrokeStyleOperation
                  , SetStrokeThicknessOperation
                  // Paint setup
                  , SetupPaintFillOperation
                  , SetupPaintShadowOperation
                  , SetupPaintStrokeOperation
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
                  , DrawEllipseOperation
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
                  // Text
                  , DrawComplexTextOperation
                  , DrawTextOperation
    } OperationType;

    virtual void apply(PlatformGraphicsContext* context) = 0;
    virtual ~Operation() {}
    virtual OperationType type() { return UndefinedOperation; }
    virtual String parameters() { return ""; }
    String name()
    {
        switch (type()) {
            TYPE_CASE(UndefinedOperation)
            // State management
            TYPE_CASE(BeginTransparencyLayerOperation)
            TYPE_CASE(EndTransparencyLayerOperation)
            TYPE_CASE(SaveOperation)
            TYPE_CASE(RestoreOperation)
            // State setters
            TYPE_CASE(SetAlphaOperation)
            TYPE_CASE(SetCompositeOpOperation)
            TYPE_CASE(SetFillColorOperation)
            TYPE_CASE(SetFillShaderOperation)
            TYPE_CASE(SetLineCapOperation)
            TYPE_CASE(SetLineDashOperation)
            TYPE_CASE(SetLineJoinOperation)
            TYPE_CASE(SetMiterLimitOperation)
            TYPE_CASE(SetShadowOperation)
            TYPE_CASE(SetShouldAntialiasOperation)
            TYPE_CASE(SetStrokeColorOperation)
            TYPE_CASE(SetStrokeShaderOperation)
            TYPE_CASE(SetStrokeStyleOperation)
            TYPE_CASE(SetStrokeThicknessOperation)
            // Paint setup
            TYPE_CASE(SetupPaintFillOperation)
            TYPE_CASE(SetupPaintShadowOperation)
            TYPE_CASE(SetupPaintStrokeOperation)
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
            TYPE_CASE(DrawEllipseOperation)
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
            // Text
            TYPE_CASE(DrawComplexTextOperation)
            TYPE_CASE(DrawTextOperation)
        }
        return "Undefined";
    }
};

//**************************************
// State management
//**************************************

class BeginTransparencyLayer : public Operation {
public:
    BeginTransparencyLayer(const float opacity) : m_opacity(opacity) {}
    virtual void apply(PlatformGraphicsContext* context) { context->beginTransparencyLayer(m_opacity); }
    virtual OperationType type() { return BeginTransparencyLayerOperation; }
private:
    float m_opacity;
};
class EndTransparencyLayer : public Operation {
public:
    EndTransparencyLayer() {}
    virtual void apply(PlatformGraphicsContext* context) { context->endTransparencyLayer(); }
    virtual OperationType type() { return EndTransparencyLayerOperation; }
};
class Save : public Operation {
public:
    virtual void apply(PlatformGraphicsContext* context) { context->save(); }
    virtual OperationType type() { return SaveOperation; }
};
class Restore : public Operation {
public:
    virtual void apply(PlatformGraphicsContext* context) { context->restore(); }
    virtual OperationType type() { return RestoreOperation; }
};

//**************************************
// State setters
//**************************************

class SetAlpha : public Operation {
public:
    SetAlpha(const float alpha) : m_alpha(alpha) {}
    virtual void apply(PlatformGraphicsContext* context) { context->setAlpha(m_alpha); }
    virtual OperationType type() { return SetAlphaOperation; }
private:
    float m_alpha;
};

class SetCompositeOperation : public Operation {
public:
    SetCompositeOperation(CompositeOperator op) : m_operator(op) {}
    virtual void apply(PlatformGraphicsContext* context) {
        context->setCompositeOperation(m_operator);
    }
    virtual OperationType type() { return SetCompositeOpOperation; }
private:
    CompositeOperator m_operator;
};

class SetFillColor : public Operation {
public:
    SetFillColor(Color color) : m_color(color) {}
    virtual void apply(PlatformGraphicsContext* context) {
        context->setFillColor(m_color);
    }
    virtual OperationType type() { return SetFillColorOperation; }
    virtual String parameters() {
        return String::format("r: %d g: %d b: %d a: %d",
                              m_color.red(),
                              m_color.green(),
                              m_color.blue(),
                              m_color.alpha());
    }
private:
    Color m_color;
};

class SetFillShader : public Operation {
public:
    SetFillShader(SkShader* shader) : m_shader(shader) {
        SkSafeRef(m_shader);
    }
    ~SetFillShader() { SkSafeUnref(m_shader); }
    virtual void apply(PlatformGraphicsContext* context) {
        context->setFillShader(m_shader);
    }
    virtual OperationType type() { return SetFillShaderOperation; }
private:
    SkShader* m_shader;
};

class SetLineCap : public Operation {
public:
    SetLineCap(LineCap cap) : m_cap(cap) {}
    virtual void apply(PlatformGraphicsContext* context) {
        context->setLineCap(m_cap);
    }
    virtual OperationType type() { return SetLineCapOperation; }
private:
    LineCap m_cap;
};

class SetLineDash : public Operation {
public:
    SetLineDash(const DashArray& dashes, float dashOffset)
        : m_dashes(dashes), m_dashOffset(dashOffset) {}
    virtual void apply(PlatformGraphicsContext* context) {
        context->setLineDash(m_dashes, m_dashOffset);
    }
    virtual OperationType type() { return SetLineDashOperation; }
private:
    DashArray m_dashes;
    float m_dashOffset;
};

class SetLineJoin : public Operation {
public:
    SetLineJoin(LineJoin join) : m_join(join) {}
    virtual void apply(PlatformGraphicsContext* context) {
        context->setLineJoin(m_join);
    }
    virtual OperationType type() { return SetLineJoinOperation; }
private:
    LineJoin m_join;
};

class SetMiterLimit : public Operation {
public:
    SetMiterLimit(float limit) : m_limit(limit) {}
    virtual void apply(PlatformGraphicsContext* context) {
        context->setMiterLimit(m_limit);
    }
    virtual OperationType type() { return SetMiterLimitOperation; }
private:
    float m_limit;
};

class SetShadow : public Operation {
public:
    SetShadow(int radius, int dx, int dy, SkColor c)
        : m_radius(radius), m_dx(dx), m_dy(dy), m_color(c) {}
    virtual void apply(PlatformGraphicsContext* context) {
        context->setShadow(m_radius, m_dx, m_dy, m_color);
    }
    virtual OperationType type() { return SetShadowOperation; }
private:
    int m_radius;
    int m_dx;
    int m_dy;
    SkColor m_color;
};

class SetShouldAntialias : public Operation {
public:
    SetShouldAntialias(bool useAA) : m_useAA(useAA) {}
    virtual void apply(PlatformGraphicsContext* context) {
        context->setShouldAntialias(m_useAA);
    }
    virtual OperationType type() { return SetShouldAntialiasOperation; }
private:
    bool m_useAA;
};

class SetStrokeColor : public Operation {
public:
    SetStrokeColor(const Color& c) : m_color(c) {}
    virtual void apply(PlatformGraphicsContext* context) {
        context->setStrokeColor(m_color);
    }
    virtual OperationType type() { return SetStrokeColorOperation; }
private:
    Color m_color;
};

class SetStrokeShader : public Operation {
public:
    SetStrokeShader(SkShader* strokeShader) : m_shader(strokeShader) {
        SkSafeRef(m_shader);
    }
    ~SetStrokeShader() { SkSafeUnref(m_shader); }
    virtual void apply(PlatformGraphicsContext* context) {
        context->setStrokeShader(m_shader);
    }
    virtual OperationType type() { return SetStrokeShaderOperation; }
private:
    SkShader* m_shader;
};

class SetStrokeStyle : public Operation {
public:
    SetStrokeStyle(StrokeStyle style) : m_style(style) {}
    virtual void apply(PlatformGraphicsContext* context) {
        context->setStrokeStyle(m_style);
    }
    virtual OperationType type() { return SetStrokeStyleOperation; }
private:
    StrokeStyle m_style;
};

class SetStrokeThickness : public Operation {
public:
    SetStrokeThickness(float thickness) : m_thickness(thickness) {}
    virtual void apply(PlatformGraphicsContext* context) {
        context->setStrokeThickness(m_thickness);
    }
    virtual OperationType type() { return SetStrokeThicknessOperation; }
private:
    float m_thickness;
};

//**************************************
// Paint setup
//**************************************

class SetupPaintFill : public Operation {
public:
    SetupPaintFill(SkPaint* paint) : m_paint(*paint) {}
    virtual void apply(PlatformGraphicsContext* context) {
        context->setupPaintFill(&m_paint);
    }
    virtual OperationType type() { return SetupPaintFillOperation; }
private:
    SkPaint m_paint;
};

class SetupPaintShadow : public Operation {
public:
    SetupPaintShadow(SkPaint* paint, SkPoint* offset)
        : m_paint(*paint), m_offset(*offset) {}
    virtual void apply(PlatformGraphicsContext* context) {
        context->setupPaintShadow(&m_paint, &m_offset);
    }
    virtual OperationType type() { return SetupPaintShadowOperation; }
private:
    SkPaint m_paint;
    SkPoint m_offset;
};

class SetupPaintStroke : public Operation {
public:
    SetupPaintStroke(SkPaint* paint, SkRect* rect, bool isHLine)
        : m_paint(*paint), m_rect(*rect), m_isHLine(isHLine) {}
    virtual void apply(PlatformGraphicsContext* context) {
        context->setupPaintStroke(&m_paint, &m_rect, m_isHLine);
    }
    virtual OperationType type() { return SetupPaintStrokeOperation; }
private:
    SkPaint m_paint;
    SkRect m_rect;
    bool m_isHLine;
};

//**************************************
// Matrix operations
//**************************************

class ConcatCTM : public Operation {
public:
    ConcatCTM(const AffineTransform& affine) : m_matrix(affine) {}
    virtual void apply(PlatformGraphicsContext* context) {
        context->concatCTM(m_matrix);
    }
    virtual OperationType type() { return ConcatCTMOperation; }
private:
    AffineTransform m_matrix;
};

class Rotate : public Operation {
public:
    Rotate(float angleInRadians) : m_angle(angleInRadians) {}
    virtual void apply(PlatformGraphicsContext* context) {
        context->rotate(m_angle);
    }
    virtual OperationType type() { return RotateOperation; }
private:
    float m_angle;
};

class Scale : public Operation {
public:
    Scale(const FloatSize& size) : m_scale(size) {}
    virtual void apply(PlatformGraphicsContext* context) {
        context->scale(m_scale);
    }
    virtual OperationType type() { return ScaleOperation; }
private:
    FloatSize m_scale;
};

class Translate : public Operation {
public:
    Translate(float x, float y) : m_x(x), m_y(y) {}
    virtual void apply(PlatformGraphicsContext* context) {
        context->translate(m_x, m_y);
    }
    virtual OperationType type() { return TranslateOperation; }
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
    virtual void apply(PlatformGraphicsContext* context) {
        context->addInnerRoundedRectClip(m_rect, m_thickness);
    }
    virtual OperationType type() { return InnerRoundedRectClipOperation; }
private:
    IntRect m_rect;
    int m_thickness;
};

class Clip : public Operation {
public:
    Clip(const FloatRect& rect) : m_rect(rect) {}
    virtual void apply(PlatformGraphicsContext* context) {
        context->clip(m_rect);
    }
    virtual OperationType type() { return ClipOperation; }
private:
    const FloatRect m_rect;
};

class ClipPath : public Operation {
public:
    ClipPath(const Path& path, bool clipout = false)
        : m_path(path), m_clipOut(clipout), m_hasWindRule(false) {}
    void setWindRule(WindRule rule) { m_windRule = rule; m_hasWindRule = true; }
    virtual void apply(PlatformGraphicsContext* context) {
        if (m_hasWindRule) {
            context->clipPath(m_path, m_windRule);
            return;
        }
        if (m_clipOut)
            context->clipOut(m_path);
        else
            context->clip(m_path);
    }
    virtual OperationType type() { return ClipPathOperation; }
private:
    const Path m_path;
    bool m_clipOut;
    WindRule m_windRule;
    bool m_hasWindRule;
};

class ClipOut : public Operation {
public:
    ClipOut(const IntRect& rect) : m_rect(rect) {}
    virtual void apply(PlatformGraphicsContext* context) {
        context->clipOut(m_rect);
    }
    virtual OperationType type() { return ClipOutOperation; }
private:
    const IntRect m_rect;
};

class ClearRect : public Operation {
public:
    ClearRect(const FloatRect& rect) : m_rect(rect) {}
    virtual void apply(PlatformGraphicsContext* context) {
        context->clearRect(m_rect);
    }
    virtual OperationType type() { return ClearRectOperation; }
private:
    FloatRect m_rect;
};

//**************************************
// Drawing
//**************************************

class DrawBitmapPattern : public Operation {
public:
    DrawBitmapPattern(const SkBitmap& bitmap, const SkMatrix& matrix,
                      CompositeOperator op, const FloatRect& destRect)
        : m_bitmap(bitmap), m_matrix(matrix), m_operator(op), m_destRect(destRect) {}
    virtual void apply(PlatformGraphicsContext* context) {
        context->drawBitmapPattern(m_bitmap, m_matrix, m_operator, m_destRect);
    }
    virtual OperationType type() { return DrawBitmapPatternOperation; }
private:
    // TODO: use refcounted bitmap
    const SkBitmap m_bitmap;
    SkMatrix m_matrix;
    CompositeOperator m_operator;
    FloatRect m_destRect;
};

class DrawBitmapRect : public Operation {
public:
    DrawBitmapRect(const SkBitmap& bitmap, const SkIRect& srcR,
                   const SkRect& dstR, CompositeOperator op)
        : m_bitmap(bitmap), m_srcR(srcR), m_dstR(dstR), m_operator(op) {}
    virtual void apply(PlatformGraphicsContext* context) {
        context->drawBitmapRect(m_bitmap, &m_srcR, m_dstR, m_operator);
    }
    virtual OperationType type() { return DrawBitmapRectOperation; }
    virtual String parameters() {
        return String::format("%.2f, %.2f - %.2f x %.2f",
                 m_dstR.fLeft, m_dstR.fTop,
                 m_dstR.width(), m_dstR.height());
    }
private:
    const SkBitmap& m_bitmap;
    SkIRect m_srcR;
    SkRect m_dstR;
    CompositeOperator m_operator;
};

class DrawEllipse : public Operation {
public:
    DrawEllipse(const IntRect& rect) : m_rect(rect) {}
    virtual void apply(PlatformGraphicsContext* context) {
        context->drawEllipse(m_rect);
    }
    virtual OperationType type() { return DrawEllipseOperation; }
private:
    IntRect m_rect;
};

class DrawLine : public Operation {
public:
    DrawLine(const IntPoint& point1, const IntPoint& point2)
        : m_point1(point1), m_point2(point2) {}
    virtual void apply(PlatformGraphicsContext* context) {
        context->drawLine(m_point1, m_point2);
    }
    virtual OperationType type() { return DrawLineOperation; }
private:
    IntPoint m_point1;
    IntPoint m_point2;
};

class DrawLineForText : public Operation {
public:
    DrawLineForText(const FloatPoint& pt, float width)
        : m_point(pt), m_width(width) {}
    virtual void apply(PlatformGraphicsContext* context) {
        context->drawLineForText(m_point, m_width);
    }
    virtual OperationType type() { return DrawLineForTextOperation; }
private:
    FloatPoint m_point;
    float m_width;
};

class DrawLineForTextChecking : public Operation {
public:
    DrawLineForTextChecking(const FloatPoint& pt, float width,
                            GraphicsContext::TextCheckingLineStyle lineStyle)
        : m_point(pt), m_width(width), m_lineStyle(lineStyle) {}
    virtual void apply(PlatformGraphicsContext* context) {
        context->drawLineForTextChecking(m_point, m_width, m_lineStyle);
    }
    virtual OperationType type() { return DrawLineForTextCheckingOperation; }
private:
    FloatPoint m_point;
    float m_width;
    GraphicsContext::TextCheckingLineStyle m_lineStyle;
};

class DrawRect : public Operation {
public:
    DrawRect(const IntRect& rect) : m_rect(rect) {}
    virtual void apply(PlatformGraphicsContext* context) {
        context->drawRect(m_rect);
    }
    virtual OperationType type() { return DrawRectOperation; }
private:
    IntRect m_rect;
};

class FillPath : public Operation {
public:
    FillPath(const Path& pathToFill, WindRule fillRule)
        : m_path(pathToFill), m_fillRule(fillRule) {}
    virtual void apply(PlatformGraphicsContext* context) {
        context->fillPath(m_path, m_fillRule);
    }
    virtual OperationType type() { return FillPathOperation; }
private:
    Path m_path;
    WindRule m_fillRule;
};

class FillRect : public Operation {
public:
    FillRect(const FloatRect& rect) : m_rect(rect), m_hasColor(false) {}
    void setColor(Color c) { m_color = c; m_hasColor = true; }
    virtual void apply(PlatformGraphicsContext* context) {
        if (m_hasColor)
             context->fillRect(m_rect, m_color);
        else
             context->fillRect(m_rect);
    }
    virtual OperationType type() { return FillRectOperation; }
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
    virtual void apply(PlatformGraphicsContext* context) {
        context->fillRoundedRect(m_rect, m_topLeft, m_topRight,
                                 m_bottomLeft, m_bottomRight,
                                 m_color);
    }
    virtual OperationType type() { return FillRoundedRectOperation; }
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
    virtual void apply(PlatformGraphicsContext* context) {
        context->strokeArc(m_rect, m_startAngle, m_angleSpan);
    }
    virtual OperationType type() { return StrokeArcOperation; }
private:
    IntRect m_rect;
    int m_startAngle;
    int m_angleSpan;
};

class StrokePath : public Operation {
public:
    StrokePath(const Path& path) : m_path(path) {}
    virtual void apply(PlatformGraphicsContext* context) {
        context->strokePath(m_path);
    }
    virtual OperationType type() { return StrokePathOperation; }
private:
    Path m_path;
};


class StrokeRect : public Operation {
public:
    StrokeRect(const FloatRect& rect, float lineWidth)
        : m_rect(rect), m_lineWidth(lineWidth) {}
    virtual void apply(PlatformGraphicsContext* context) {
        context->strokeRect(m_rect, m_lineWidth);
    }
    virtual OperationType type() { return StrokeRectOperation; }
private:
    FloatRect m_rect;
    float m_lineWidth;
};

//**************************************
// Text
//**************************************

class DrawComplexText : public Operation {
public:
    DrawComplexText(SkPicture* picture) : m_picture(picture) {
        SkSafeRef(m_picture);
    }
    ~DrawComplexText() { SkSafeUnref(m_picture); }
    virtual void apply(PlatformGraphicsContext* context) {
        if (!context->getCanvas())
            return;
        context->getCanvas()->drawPicture(*m_picture);
    }
    virtual OperationType type() { return DrawComplexTextOperation; }
private:
    SkPicture* m_picture;
};

class DrawText : public Operation {
public:
    DrawText(const Font* font, const SimpleFontData* simpleFont,
             const GlyphBuffer& glyphBuffer,
             int from, int numGlyphs, const FloatPoint& point)
        : m_font(font), m_simpleFont(simpleFont)
        , m_glyphBuffer(glyphBuffer), m_from(from)
        , m_numGlyphs(numGlyphs), m_point(point) {

        SkPicture* picture = new SkPicture();
        SkCanvas* canvas = picture->beginRecording(0, 0, 0);
        PlatformGraphicsContextSkia platformContext(canvas);
        GraphicsContext graphicsContext(&platformContext);
        m_font->drawGlyphs(&graphicsContext, m_simpleFont,
                           m_glyphBuffer, m_from, m_numGlyphs, m_point);
        picture->endRecording();
        m_picture = picture;
    }
    ~DrawText() { SkSafeUnref(m_picture); }
    virtual void apply(PlatformGraphicsContext* context) {
        if (!context->getCanvas())
            return;
        context->getCanvas()->drawPicture(*m_picture);
    }
    virtual OperationType type() { return DrawTextOperation; }
private:
    SkPicture* m_picture;
    const Font* m_font;
    const SimpleFontData* m_simpleFont;
    const GlyphBuffer m_glyphBuffer;
    int m_from;
    int m_numGlyphs;
    const FloatPoint m_point;
};

}

}

#endif // USE(ACCELERATED_COMPOSITING)

#endif // GraphicsOperation_h
