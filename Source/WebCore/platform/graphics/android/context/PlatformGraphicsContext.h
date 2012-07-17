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
#include "RenderSkinMediaButton.h"
#include "SkCanvas.h"
#include "SkPicture.h"
#include "SkTDArray.h"
#include <wtf/Vector.h>

class SkCanvas;

namespace WebCore {

class PlatformGraphicsContext {
public:
    class State;

    PlatformGraphicsContext();
    virtual ~PlatformGraphicsContext();
    virtual bool isPaintingDisabled() = 0;

    void setGraphicsContext(GraphicsContext* gc) { m_gc = gc; }
    virtual bool deleteUs() const { return false; }

    typedef enum { PaintingContext, RecordingContext } ContextType;
    virtual ContextType type() = 0;

    // State management
    virtual void beginTransparencyLayer(float opacity) = 0;
    virtual void endTransparencyLayer() = 0;
    virtual void save();
    virtual void restore();

    // State values
    virtual void setAlpha(float alpha);
    int getNormalizedAlpha() const;
    virtual void setCompositeOperation(CompositeOperator op);
    virtual bool setFillColor(const Color& c);
    virtual bool setFillShader(SkShader* fillShader);
    virtual void setLineCap(LineCap cap);
    virtual void setLineDash(const DashArray& dashes, float dashOffset);
    virtual void setLineJoin(LineJoin join);
    virtual void setMiterLimit(float limit);
    virtual void setShadow(int radius, int dx, int dy, SkColor c);
    virtual void setShouldAntialias(bool useAA);
    virtual bool setStrokeColor(const Color& c);
    virtual bool setStrokeShader(SkShader* strokeShader);
    virtual void setStrokeStyle(StrokeStyle style);
    virtual void setStrokeThickness(float f);

    // FIXME: These setupPaint* should be private, but
    //        they are used by FontAndroid currently
    virtual void setupPaintFill(SkPaint* paint) const;
    virtual bool setupPaintShadow(SkPaint* paint, SkPoint* offset) const;
    // Sets up the paint for stroking. Returns true if the style is really
    // just a dash of squares (the size of the paint's stroke-width.
    virtual bool setupPaintStroke(SkPaint* paint, SkRect* rect, bool isHLine = false);

    // Matrix operations
    virtual void concatCTM(const AffineTransform& affine) = 0;
    virtual void rotate(float angleInRadians) = 0;
    virtual void scale(const FloatSize& size) = 0;
    virtual void translate(float x, float y) = 0;
    virtual const SkMatrix& getTotalMatrix() = 0;

    // Clipping
    virtual void addInnerRoundedRectClip(const IntRect& rect, int thickness) = 0;
    virtual void canvasClip(const Path& path) = 0;
    virtual bool clip(const FloatRect& rect) = 0;
    virtual bool clip(const Path& path) = 0;
    virtual bool clipConvexPolygon(size_t numPoints, const FloatPoint*, bool antialias) = 0;
    virtual bool clipOut(const IntRect& r) = 0;
    virtual bool clipOut(const Path& p) = 0;
    virtual bool clipPath(const Path& pathToClip, WindRule clipRule) = 0;
    virtual SkIRect getTotalClipBounds() = 0;

    // Drawing
    virtual void clearRect(const FloatRect& rect) = 0;
    virtual void drawBitmapPattern(const SkBitmap& bitmap, const SkMatrix& matrix,
                           CompositeOperator compositeOp, const FloatRect& destRect) = 0;
    virtual void drawBitmapRect(const SkBitmap& bitmap, const SkIRect* src,
                        const SkRect& dst, CompositeOperator op = CompositeSourceOver) = 0;
    virtual void drawConvexPolygon(size_t numPoints, const FloatPoint* points,
                           bool shouldAntialias) = 0;
    virtual void drawEllipse(const IntRect& rect) = 0;
    virtual void drawFocusRing(const Vector<IntRect>& rects, int /* width */,
                       int /* offset */, const Color& color) = 0;
    virtual void drawHighlightForText(const Font& font, const TextRun& run,
                              const FloatPoint& point, int h,
                              const Color& backgroundColor, ColorSpace colorSpace,
                              int from, int to, bool isActive) = 0;
    virtual void drawLine(const IntPoint& point1, const IntPoint& point2) = 0;
    virtual void drawLineForText(const FloatPoint& pt, float width) = 0;
    virtual void drawLineForTextChecking(const FloatPoint& pt, float width,
                                         GraphicsContext::TextCheckingLineStyle) = 0;
    virtual void drawRect(const IntRect& rect) = 0;
    virtual void fillPath(const Path& pathToFill, WindRule fillRule) = 0;
    virtual void fillRect(const FloatRect& rect) = 0;
    void fillRect(const FloatRect& rect, const Color& color, ColorSpace) {
        fillRect(rect, color);
    }
    virtual void fillRect(const FloatRect& rect, const Color& color) = 0;
    void fillRoundedRect(const IntRect& rect, const IntSize& topLeft,
                         const IntSize& topRight, const IntSize& bottomLeft,
                         const IntSize& bottomRight, const Color& color,
                         ColorSpace) {
        fillRoundedRect(rect, topLeft, topRight, bottomLeft, bottomRight, color);
    }
    virtual void fillRoundedRect(const IntRect& rect, const IntSize& topLeft,
                         const IntSize& topRight, const IntSize& bottomLeft,
                         const IntSize& bottomRight, const Color& color) = 0;
    virtual void strokeArc(const IntRect& r, int startAngle, int angleSpan) = 0;
    virtual void strokePath(const Path& pathToStroke) = 0;
    virtual void strokeRect(const FloatRect& rect, float lineWidth) = 0;

    virtual void drawPosText(const void* text, size_t byteLength,
                             const SkPoint pos[], const SkPaint& paint) = 0;
    virtual void drawMediaButton(const IntRect& rect, RenderSkinMediaButton::MediaButton buttonType,
                                 bool translucent = false, bool drawBackground = true,
                                 const IntRect& thumb = IntRect()) = 0;

    virtual SkCanvas* recordingCanvas() = 0;
    virtual void setTextOffset(FloatSize offset) = 0;

    void setRawState(State* state) { m_state = state; }

    virtual void convertToNonRecording() {}
    virtual void clearRecording() {}
    virtual SkPicture* getRecordingPicture() const { return NULL; }

    virtual bool isDefault() const { return true; }
    virtual bool isAnimating() const { return false; }
    virtual bool isRecording() const { return false; }
    virtual bool isDirty() const {return false; }

    virtual void setIsAnimating() {}

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

    class State {
    public:
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

        State();
        State(const State& other);
        ~State();

        void setShadow(int radius, int dx, int dy, SkColor c);
        bool setupShadowPaint(SkPaint* paint, SkPoint* offset,
                              bool shadowsIgnoreTransforms);
        SkColor applyAlpha(SkColor c) const;

        State cloneInheritedProperties();
    private:
        // Not supported.
        void operator=(const State&);

        friend class PlatformGraphicsContextRecording;
        friend class PlatformGraphicsContextSkia;
    };

protected:
    virtual bool shadowsIgnoreTransforms() const = 0;
    void setupPaintCommon(SkPaint* paint) const;
    GraphicsContext* m_gc; // Back-ptr to our parent

    WTF::Vector<State> m_stateStack;
    State* m_state;
};

}
#endif
