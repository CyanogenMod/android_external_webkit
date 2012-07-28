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

#ifndef RecordingContextCanvasProxy_h
#define RecordingContextCanvasProxy_h

#include "SkCanvas.h"

namespace WebCore {

class PlatformGraphicsContextRecording;

class RecordingContextCanvasProxy : public SkCanvas {
public:
    RecordingContextCanvasProxy(PlatformGraphicsContextRecording* pgc)
        : m_pgc(pgc)
    {}

    // Used by FontAndroid

    // Return value is unused by FontAndroid
    virtual int save(SaveFlags);
    virtual void restore();
    virtual void drawPosText(const void* text, size_t byteLength,
                             const SkPoint pos[], const SkPaint& paint);
    virtual bool rotate(SkScalar degrees);

    // Used by EmojiFont

    virtual void drawBitmapRect(const SkBitmap& bitmap, const SkIRect* src,
                                    const SkRect& dst, const SkPaint*);

    // These aren't used by anyone

    virtual int saveLayer(const SkRect* bounds, const SkPaint*, SaveFlags) { /* NOT IMPLEMENTED*/ CRASH(); return -1; }
    virtual bool translate(SkScalar dx, SkScalar dy) { /* NOT IMPLEMENTED*/ CRASH(); return -1; }
    virtual bool scale(SkScalar sx, SkScalar sy) { /* NOT IMPLEMENTED*/ CRASH(); return -1; }
    virtual bool skew(SkScalar sx, SkScalar sy) { /* NOT IMPLEMENTED*/ CRASH(); return -1; }
    virtual bool concat(const SkMatrix& matrix) { /* NOT IMPLEMENTED*/ CRASH(); return -1; }
    virtual void setMatrix(const SkMatrix& matrix) { /* NOT IMPLEMENTED*/ CRASH(); }
    virtual bool clipRect(const SkRect&, SkRegion::Op, bool) { /* NOT IMPLEMENTED*/ CRASH(); return -1; }
    virtual bool clipPath(const SkPath&, SkRegion::Op, bool) { /* NOT IMPLEMENTED*/ CRASH(); return -1; }
    virtual bool clipRegion(const SkRegion& region, SkRegion::Op op) { /* NOT IMPLEMENTED*/ CRASH(); return -1; }
    virtual void clear(SkColor) { /* NOT IMPLEMENTED*/ CRASH(); }
    virtual void drawPaint(const SkPaint& paint) { /* NOT IMPLEMENTED*/ CRASH(); }
    virtual void drawPoints(PointMode, size_t count, const SkPoint pts[],
                            const SkPaint&) { /* NOT IMPLEMENTED*/ CRASH(); }
    virtual void drawRect(const SkRect& rect, const SkPaint&) { /* NOT IMPLEMENTED*/ CRASH(); }
    virtual void drawPath(const SkPath& path, const SkPaint&) { /* NOT IMPLEMENTED*/ CRASH(); }
    virtual void drawBitmap(const SkBitmap&, SkScalar left, SkScalar top,
                            const SkPaint*) { /* NOT IMPLEMENTED*/ CRASH(); }
    virtual void drawBitmapMatrix(const SkBitmap&, const SkMatrix&,
                                  const SkPaint*) { /* NOT IMPLEMENTED*/ CRASH(); }
    virtual void drawBitmapNine(const SkBitmap& bitmap, const SkIRect& center,
                                const SkRect& dst, const SkPaint*) { /* NOT IMPLEMENTED*/ CRASH(); }
    virtual void drawSprite(const SkBitmap&, int left, int top,
                            const SkPaint*) { /* NOT IMPLEMENTED*/ CRASH(); }
    virtual void drawText(const void* text, size_t byteLength, SkScalar x,
                          SkScalar y, const SkPaint&) { /* NOT IMPLEMENTED*/ CRASH(); }
    virtual void drawPosTextH(const void* text, size_t byteLength,
                      const SkScalar xpos[], SkScalar constY, const SkPaint&) { /* NOT IMPLEMENTED*/ CRASH(); }
    virtual void drawTextOnPath(const void* text, size_t byteLength,
                            const SkPath& path, const SkMatrix* matrix,
                                const SkPaint&) { /* NOT IMPLEMENTED*/ CRASH(); }
    virtual void drawPicture(SkPicture& picture) { /* NOT IMPLEMENTED*/ CRASH(); }
    virtual void drawVertices(VertexMode, int vertexCount,
                          const SkPoint vertices[], const SkPoint texs[],
                          const SkColor colors[], SkXfermode*,
                          const uint16_t indices[], int indexCount,
                              const SkPaint&) { /* NOT IMPLEMENTED*/ CRASH(); }
    virtual void drawData(const void*, size_t) { /* NOT IMPLEMENTED*/ CRASH(); }
    virtual bool isDrawingToLayer() const { /* NOT IMPLEMENTED*/ CRASH(); return -1; }

private:
    PlatformGraphicsContextRecording* m_pgc;
};

} // namespace WebCore

#endif // RecordingContextCanvasProxy_h
