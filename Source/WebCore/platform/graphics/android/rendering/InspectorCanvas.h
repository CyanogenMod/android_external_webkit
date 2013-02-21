/*
 * Copyright 2011, The Android Open Source Project
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

#ifndef InspectorCanvas_h
#define InspectorCanvas_h

#include "SkBounder.h"
#include "SkCanvas.h"

namespace WebCore {

class InspectorBounder : public SkBounder {
    virtual bool onIRect(const SkIRect& rect)
    {
        return false;
    }
};

class InspectorCanvas : public SkCanvas {
public:
    InspectorCanvas(SkBounder* bounder, SkPicture* picture, SkBitmap& bitmap)
        : SkCanvas(bitmap)
        , m_picture(picture)
        , m_hasText(false)
        , m_hasContent(false)
    {
        setBounder(bounder);
    }

    bool hasText() {return m_hasText;}
    bool hasContent() {return m_hasContent;}

    virtual bool clipPath(const SkPath&, SkRegion::Op) {
        return true;
    }

    virtual void commonDrawBitmap(const SkBitmap& bitmap,
                                  const SkIRect* rect,
                                  const SkMatrix&,
                                  const SkPaint&);
    virtual void drawBitmapRectToRect(const SkBitmap& bitmap,
                                      const SkRect* src,
                                      const SkRect& dst,
                                      const SkPaint* paint);

    virtual void drawPaint(const SkPaint& paint);
    virtual void drawPath(const SkPath&, const SkPaint& paint);
    virtual void drawPoints(PointMode, size_t,
                            const SkPoint [], const SkPaint& paint);

    virtual void drawRect(const SkRect& , const SkPaint& paint);
    virtual void drawSprite(const SkBitmap& , int , int ,
                            const SkPaint* paint = NULL);

    virtual void drawText(const void*, size_t byteLength, SkScalar,
                          SkScalar, const SkPaint& paint);
    virtual void drawPosText(const void* , size_t byteLength,
                             const SkPoint [], const SkPaint& paint);
    virtual void drawPosTextH(const void*, size_t byteLength,
                              const SkScalar [], SkScalar,
                              const SkPaint& paint);
    virtual void drawTextOnPath(const void*, size_t byteLength,
                                const SkPath&, const SkMatrix*,
                                const SkPaint& paint);

private:

    // vector instructions exist, must repaint at any scale
    void setHasText();

    // painting is required
    void setHasContent();

    // rect covering entire content, don't need to use a texture if nothing else
    // is painted
    void setIsBackground(const SkPaint& paint);

    SkPicture* m_picture;
    bool m_hasText;
    bool m_hasContent;
};

} // namespace WebCore

#endif // InspectorCanvas_h
