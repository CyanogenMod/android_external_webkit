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

#include "config.h"
#include "RecordingContextCanvasProxy.h"

#include "PlatformGraphicsContextRecording.h"

namespace WebCore {

// Return value is unused by FontAndroid
int RecordingContextCanvasProxy::save(SaveFlags)
{
    m_pgc->save();
    return -1;
}

void RecordingContextCanvasProxy::restore()
{
    m_pgc->restore();
}

void RecordingContextCanvasProxy::drawPosText(const void* text,
                                              size_t byteLength,
                                              const SkPoint pos[],
                                              const SkPaint& paint)
{
    m_pgc->drawPosText(text, byteLength, pos, paint);
}

void RecordingContextCanvasProxy::drawBitmapRect(const SkBitmap& bitmap,
                                                 const SkIRect* src,
                                                 const SkRect& dst,
                                                 const SkPaint*)
{
    m_pgc->drawBitmapRect(bitmap, src, dst);
}

// Return value is unused by FontAndroid
bool RecordingContextCanvasProxy::rotate(SkScalar degrees)
{
    m_pgc->rotate(degrees / (180.0f / 3.14159265f));
    return true;
}

} // WebCore
