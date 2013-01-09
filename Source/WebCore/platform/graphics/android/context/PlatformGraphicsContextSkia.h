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
    // Create a recording canvas
    PlatformGraphicsContextSkia(int width, int height, PlatformGraphicsContext* existing);
    virtual ~PlatformGraphicsContextSkia();
    virtual bool isPaintingDisabled();
    SkCanvas* canvas() { return mCanvas; }

    virtual ContextType type() { return PaintingContext; }
    virtual SkCanvas* recordingCanvas() { return mCanvas; }
    virtual void setTextOffset(FloatSize offset) {}

    // FIXME: This is used by ImageBufferAndroid, which should really be
    //        managing the canvas lifecycle itself

    virtual bool deleteUs() const { return (m_deleteCanvas || (m_picture != NULL)); }

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
    virtual bool clip(const FloatRect& rect);
    virtual bool clip(const Path& path);
    virtual bool clipConvexPolygon(size_t numPoints, const FloatPoint*, bool antialias);
    virtual bool clipOut(const IntRect& r);
    virtual bool clipOut(const Path& p);
    virtual bool clipPath(const Path& pathToClip, WindRule clipRule);
    virtual SkIRect getTotalClipBounds() { return mCanvas->getTotalClip().getBounds(); }

    // Drawing
    virtual void clearRect(const FloatRect& rect);
    virtual void drawBitmapPattern(const SkBitmap& bitmap, const SkMatrix& matrix,
                           CompositeOperator compositeOp, const FloatRect& destRect);
    virtual void drawBitmapRect(const SkBitmap& bitmap, const SkIRect* src,
                        const SkRect& dst, CompositeOperator op = CompositeSourceOver);
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
    virtual void drawPosText(const void* text, size_t byteLength,
                             const SkPoint pos[], const SkPaint& paint);
    virtual void drawMediaButton(const IntRect& rect, RenderSkinMediaButton::MediaButton buttonType,
                                 bool translucent = false, bool drawBackground = true,
                                 const IntRect& thumb = IntRect());

    virtual void convertToNonRecording();
    virtual void clearRecording();
    virtual SkPicture* getRecordingPicture() const { return m_picture; }

    // When we detect animation we switch to the recording canvas for better
    // performance. If JS tries to access the pixels of the canvas, the
    // recording canvas becomes tainted and must be converted back to a bitmap
    // backed canvas.
    // CanvasState represents a directed acyclic graph:
    // DEFAULT ----> ANIMATION_DETECTED ----> RECORDING ----> DIRTY
    enum CanvasState {
        DEFAULT, // SkBitmap backed
        ANIMATION_DETECTED, // JavaScript clearRect of canvas is occuring at a high enough rate SkBitmap backed
        RECORDING, // SkPicture backed
        DIRTY // A pixel readback occured; convert to SkBitmap backed.
    };

    virtual bool isDefault() const { return m_canvasState == DEFAULT; }
    virtual bool isAnimating() const { return m_canvasState == ANIMATION_DETECTED; }
    virtual bool isRecording() const { return m_canvasState == RECORDING; }
    virtual bool isDirty() const { return m_canvasState == DIRTY; }

    virtual void setIsAnimating();
    virtual State* getState()     {   return m_state; }
    virtual WTF::Vector<State>& getStateStack() {   return m_stateStack;    }

private:

    // shadowsIgnoreTransforms is only true for canvas's ImageBuffer, which will
    // have a GraphicsContext
    virtual bool shadowsIgnoreTransforms() const {
        return m_gc && m_gc->shadowsIgnoreTransforms();
    }

    SkCanvas* mCanvas;
    bool m_deleteCanvas;

    enum CanvasState m_canvasState;
    SkPicture* m_picture;
};

}
#endif
