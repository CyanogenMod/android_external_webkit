/*
 * Copyright 2009, The Android Open Source Project
 * Copyright (C) 2006 Apple Computer, Inc.
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

//This file is part of the internal font implementation.  It should not be included by anyone other than
// FontMac.cpp, FontWin.cpp and Font.cpp.

#include "config.h"
#include "FontPlatformData.h"

#ifdef SUPPORT_COMPLEX_SCRIPTS
#include "HarfbuzzSkia.h"
#endif
#include "SkAdvancedTypefaceMetrics.h"
#include "SkPaint.h"
#include "SkTypeface.h"

//#define TRACE_FONTPLATFORMDATA_LIFE
//#define COUNT_FONTPLATFORMDATA_LIFE

#ifdef COUNT_FONTPLATFORMDATA_LIFE
static int g_count;
static int g_maxCount;

static void inc_count()
{
    if (++g_count > g_maxCount)
    {
        g_maxCount = g_count;
        SkDebugf("---------- FontPlatformData %d\n", g_maxCount);
    }
}

static void dec_count() { --g_count; }
#else
    #define inc_count()
    #define dec_count()
#endif

#ifdef TRACE_FONTPLATFORMDATA_LIFE
    #define trace(num)  SkDebugf("FontPlatformData%d %p %g %d %d\n", num, m_typeface, m_textSize, m_fakeBold, m_fakeItalic)
#else
    #define trace(num)
#endif

namespace WebCore {

FontPlatformData::RefCountedHarfbuzzFace::~RefCountedHarfbuzzFace()
{
#ifdef SUPPORT_COMPLEX_SCRIPTS
    HB_FreeFace(m_harfbuzzFace);
#endif
}

FontPlatformData::FontPlatformData()
    : m_typeface(NULL), m_textSize(0), m_emSizeInFontUnits(0), m_fakeBold(false), m_fakeItalic(false),
      m_orientation(Horizontal), m_textOrientation(TextOrientationVerticalRight)
{
    inc_count();
    trace(1);
}

FontPlatformData::FontPlatformData(const FontPlatformData& src)
{
    if (hashTableDeletedFontValue() != src.m_typeface) {
        SkSafeRef(src.m_typeface);
    }

    m_typeface = src.m_typeface;
    m_textSize = src.m_textSize;
    m_emSizeInFontUnits = src.m_emSizeInFontUnits;
    m_fakeBold = src.m_fakeBold;
    m_fakeItalic = src.m_fakeItalic;
    m_harfbuzzFace = src.m_harfbuzzFace;
    m_orientation = src.m_orientation;
    m_textOrientation = src.m_textOrientation;
    inc_count();
    trace(2);
}

FontPlatformData::FontPlatformData(SkTypeface* tf, float textSize, bool fakeBold, bool fakeItalic,
    FontOrientation orientation, TextOrientation textOrientation)
    : m_typeface(tf), m_textSize(textSize), m_emSizeInFontUnits(0), m_fakeBold(fakeBold), m_fakeItalic(fakeItalic),
      m_orientation(orientation), m_textOrientation(textOrientation)
{
    if (hashTableDeletedFontValue() != m_typeface) {
        SkSafeRef(m_typeface);
    }

    inc_count();
    trace(3);
}

FontPlatformData::FontPlatformData(const FontPlatformData& src, float textSize)
    : m_typeface(src.m_typeface), m_textSize(textSize), m_emSizeInFontUnits(src.m_emSizeInFontUnits), m_fakeBold(src.m_fakeBold), m_fakeItalic(src.m_fakeItalic),
      m_orientation(src.m_orientation), m_textOrientation(src.m_textOrientation), m_harfbuzzFace(src.m_harfbuzzFace)
{
    if (hashTableDeletedFontValue() != m_typeface) {
        SkSafeRef(m_typeface);
    }

    inc_count();
    trace(4);
}

FontPlatformData::FontPlatformData(float size, bool bold, bool oblique)
    : m_typeface(NULL), m_textSize(size),  m_emSizeInFontUnits(0), m_fakeBold(bold), m_fakeItalic(oblique),
      m_orientation(Horizontal), m_textOrientation(TextOrientationVerticalRight)
{
    inc_count();
    trace(5);
}

FontPlatformData::FontPlatformData(const FontPlatformData& src, SkTypeface* tf)
    : m_typeface(tf), m_textSize(src.m_textSize),  m_emSizeInFontUnits(0), m_fakeBold(src.m_fakeBold),
      m_fakeItalic(src.m_fakeItalic), m_orientation(src.m_orientation),
      m_textOrientation(src.m_textOrientation)
{
    if (hashTableDeletedFontValue() != m_typeface) {
        SkSafeRef(m_typeface);
    }

    inc_count();
    trace(6);
}

FontPlatformData::~FontPlatformData()
{
    dec_count();
#ifdef TRACE_FONTPLATFORMDATA_LIFE
    SkDebugf("----------- ~FontPlatformData\n");
#endif

    if (hashTableDeletedFontValue() != m_typeface) {
        SkSafeUnref(m_typeface);
    }
}

int FontPlatformData::emSizeInFontUnits() const
{
    if (m_emSizeInFontUnits)
        return m_emSizeInFontUnits;

    SkAdvancedTypefaceMetrics* metrics = 0;
    if (m_typeface)
        metrics = m_typeface->getAdvancedTypefaceMetrics(SkAdvancedTypefaceMetrics::kNo_PerGlyphInfo);
    if (metrics) {
        m_emSizeInFontUnits = metrics->fEmSize;
        metrics->unref();
    } else
        m_emSizeInFontUnits = 1000;  // default value copied from Skia.
    return m_emSizeInFontUnits;
}

FontPlatformData& FontPlatformData::operator=(const FontPlatformData& src)
{
    if (hashTableDeletedFontValue() != src.m_typeface) {
        SkSafeRef(src.m_typeface);
    }
    if (hashTableDeletedFontValue() != m_typeface) {
        SkSafeUnref(m_typeface);
    }

    m_typeface = src.m_typeface;
    m_emSizeInFontUnits = src.m_emSizeInFontUnits;
    m_textSize = src.m_textSize;
    m_fakeBold = src.m_fakeBold;
    m_fakeItalic = src.m_fakeItalic;
    m_harfbuzzFace = src.m_harfbuzzFace;
    m_orientation = src.m_orientation;
    m_textOrientation = src.m_textOrientation;

    return *this;
}

SkLanguage FontPlatformData::s_defaultLanguage;
void FontPlatformData::setDefaultLanguage(const char* language) {
    s_defaultLanguage = SkLanguage(language);
}

void FontPlatformData::setupPaint(SkPaint* paint) const
{
    if (hashTableDeletedFontValue() == m_typeface)
        paint->setTypeface(0);
    else
        paint->setTypeface(m_typeface);

    paint->setAntiAlias(true);
    paint->setSubpixelText(true);
    paint->setHinting(SkPaint::kSlight_Hinting);
    paint->setTextSize(SkFloatToScalar(m_textSize));
    paint->setFakeBoldText(m_fakeBold);
    paint->setTextSkewX(m_fakeItalic ? -SK_Scalar1/4 : 0);
    paint->setLanguage(s_defaultLanguage);
#ifndef SUPPORT_COMPLEX_SCRIPTS
    paint->setTextEncoding(SkPaint::kUTF16_TextEncoding);
#endif
}

uint32_t FontPlatformData::uniqueID() const
{
    if (hashTableDeletedFontValue() == m_typeface)
        return SkTypeface::UniqueID(0);
    else
        return SkTypeface::UniqueID(m_typeface);
}

bool FontPlatformData::operator==(const FontPlatformData& a) const
{
    return  m_typeface == a.m_typeface &&
            m_textSize == a.m_textSize &&
            m_fakeBold == a.m_fakeBold &&
            m_fakeItalic == a.m_fakeItalic &&
            m_orientation == a.m_orientation &&
            m_textOrientation == a.m_textOrientation;
}

unsigned FontPlatformData::hash() const
{
    unsigned h;

    if (hashTableDeletedFontValue() == m_typeface) {
        h = reinterpret_cast<unsigned>(m_typeface);
    } else {
        h = SkTypeface::UniqueID(m_typeface);
    }

    uint32_t sizeAsInt = *reinterpret_cast<const uint32_t*>(&m_textSize);

    h ^= 0x01010101 * ((static_cast<int>(m_textOrientation) << 3) | (static_cast<int>(m_orientation) << 2) |
         ((int)m_fakeBold << 1) | (int)m_fakeItalic);
    h ^= sizeAsInt;
    return h;
}

bool FontPlatformData::isFixedPitch() const
{
    if (m_typeface && (m_typeface != hashTableDeletedFontValue()))
        return m_typeface->isFixedWidth();
    else
        return false;
}

HB_FaceRec_* FontPlatformData::harfbuzzFace() const
{
#ifdef SUPPORT_COMPLEX_SCRIPTS
    if (!m_harfbuzzFace)
        m_harfbuzzFace = RefCountedHarfbuzzFace::create(
            HB_NewFace(const_cast<FontPlatformData*>(this), harfbuzzSkiaGetTable));

    return m_harfbuzzFace->face();
#else
    return NULL;
#endif
}
}
