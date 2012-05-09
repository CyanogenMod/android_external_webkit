/*
 * Copyright 2008, The Android Open Source Project
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

#define LOG_TAG "webviewglue"

#include "config.h"

#include "BidiResolver.h"
#include "BidiRunList.h"
#include "GLExtras.h"
#include "LayerAndroid.h"
#include "SelectText.h"
#include "SkBitmap.h"
#include "SkBounder.h"
#include "SkCanvas.h"
#include "SkPicture.h"
#include "SkPoint.h"
#include "SkRect.h"
#include "SkRegion.h"
#include "TextRun.h"

#ifdef DEBUG_NAV_UI
#include <wtf/text/CString.h>
#endif

#define VERBOSE_LOGGING 0
// #define EXTRA_NOISY_LOGGING 1
#define DEBUG_TOUCH_HANDLES 0
#if DEBUG_TOUCH_HANDLES
#define DBG_HANDLE_LOG(format, ...) ALOGD("%s " format, __FUNCTION__, __VA_ARGS__)
#else
#define DBG_HANDLE_LOG(...)
#endif

// TextRunIterator has been copied verbatim from GraphicsContext.cpp
namespace WebCore {

class TextRunIterator {
public:
    TextRunIterator()
        : m_textRun(0)
        , m_offset(0)
    {
    }

    TextRunIterator(const TextRun* textRun, unsigned offset)
        : m_textRun(textRun)
        , m_offset(offset)
    {
    }

    TextRunIterator(const TextRunIterator& other)
        : m_textRun(other.m_textRun)
        , m_offset(other.m_offset)
    {
    }

    unsigned offset() const { return m_offset; }
    void increment() { m_offset++; }
    bool atEnd() const { return !m_textRun || m_offset >= m_textRun->length(); }
    UChar current() const { return (*m_textRun)[m_offset]; }
    WTF::Unicode::Direction direction() const { return atEnd() ? WTF::Unicode::OtherNeutral : WTF::Unicode::direction(current()); }

    bool operator==(const TextRunIterator& other)
    {
        return m_offset == other.m_offset && m_textRun == other.m_textRun;
    }

    bool operator!=(const TextRunIterator& other) { return !operator==(other); }

private:
    const TextRun* m_textRun;
    int m_offset;
};

// ReverseBidi is a trimmed-down version of GraphicsContext::drawBidiText()
void ReverseBidi(UChar* chars, int len) {
    using namespace WTF::Unicode;
    WTF::Vector<UChar> result;
    result.reserveCapacity(len);
    TextRun run(chars, len);
    BidiResolver<TextRunIterator, BidiCharacterRun> bidiResolver;
    BidiRunList<BidiCharacterRun>& bidiRuns = bidiResolver.runs();
    bidiResolver.setStatus(BidiStatus(LeftToRight, LeftToRight, LeftToRight,
        BidiContext::create(0, LeftToRight, false)));
    bidiResolver.setPosition(TextRunIterator(&run, 0));
    bidiResolver.createBidiRunsForLine(TextRunIterator(&run, len));
    if (!bidiRuns.runCount())
        return;
    BidiCharacterRun* bidiRun = bidiRuns.firstRun();
    while (bidiRun) {
        int bidiStart = bidiRun->start();
        int bidiStop = bidiRun->stop();
        int size = result.size();
        int bidiCount = bidiStop - bidiStart;
        result.append(chars + bidiStart, bidiCount);
        if (bidiRun->level() % 2) {
            UChar* start = &result[size];
            UChar* end = start + bidiCount;
            // reverse the order of any RTL substrings
            while (start < end) {
                UChar temp = *start;
                *start++ = *--end;
                *end = temp;
            }
            start = &result[size];
            end = start + bidiCount - 1;
            // if the RTL substring had a surrogate pair, restore its order
            while (start < end) {
                UChar trail = *start++;
                if (!U16_IS_SURROGATE(trail))
                    continue;
                start[-1] = *start; // lead
                *start++ = trail;
            }
        }
        bidiRun = bidiRun->next();
    }
    bidiRuns.deleteRuns();
    memcpy(chars, &result[0], len * sizeof(UChar));
}

}

