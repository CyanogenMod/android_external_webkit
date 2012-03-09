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

#define LOG_TAG "InspectorCanvas"
#define LOG_NDEBUG 1

#include "config.h"
#include "InspectorCanvas.h"

#include "AndroidLog.h"
#include "SkPicture.h"

namespace WebCore {


void InspectorCanvas::setHasText()
{
    m_hasText = true;
    setHasContent();
}

void InspectorCanvas::setHasContent()
{
    m_hasContent = true;
    if (m_hasText) {
        // has text. Have to paint properly, so no further
        // information is useful
        m_picture->abortPlayback();
    }
}

void InspectorCanvas::setIsBackground(const SkPaint& paint)
{
    // TODO: if the paint is a solid color, opaque, and the last instruction in
    // the picture, replace the picture with simple draw rect info
    setHasContent();
}

void InspectorCanvas::commonDrawBitmap(const SkBitmap& bitmap,
                                       const SkIRect* rect,
                                       const SkMatrix&,
                                       const SkPaint&)
{
    setHasContent();
}

void InspectorCanvas::drawPaint(const SkPaint& paint)
{
    setHasContent();
}

void InspectorCanvas::drawPath(const SkPath&, const SkPaint& paint)
{
    setHasContent();
}
void InspectorCanvas::drawPoints(PointMode, size_t,
                                 const SkPoint [], const SkPaint& paint)
{
    setHasContent();
}

void InspectorCanvas::drawRect(const SkRect& rect, const SkPaint& paint)
{
    if (rect.fLeft == 0
            && rect.fTop == 0
            && rect.width() >= m_picture->width()
            && rect.height() >= m_picture->height()) {
        // rect same size as canvas, treat layer as a single color rect until
        // more content is drawn
        setIsBackground(paint);
    } else {
        // regular rect drawing path
        setHasContent();
    }
    ALOGV("draw rect at %f %f, size %f %f, picture size %d %d",
          rect.fLeft, rect.fTop, rect.width(), rect.height(),
          m_picture->width(), m_picture->height());
}
void InspectorCanvas::drawSprite(const SkBitmap& , int , int ,
                                 const SkPaint* paint)
{
    setHasContent();
}

void InspectorCanvas::drawText(const void*, size_t byteLength, SkScalar,
                               SkScalar, const SkPaint& paint)
{
    setHasText();
}

void InspectorCanvas::drawPosText(const void* , size_t byteLength,
                                  const SkPoint [], const SkPaint& paint)
{
    setHasText();
}

void InspectorCanvas::drawPosTextH(const void*, size_t byteLength,
                                   const SkScalar [], SkScalar,
                                   const SkPaint& paint)
{
    setHasText();
}

void InspectorCanvas::drawTextOnPath(const void*, size_t byteLength,
                                     const SkPath&, const SkMatrix*,
                                     const SkPaint& paint)
{
    setHasText();
}

} // namespace WebCore
