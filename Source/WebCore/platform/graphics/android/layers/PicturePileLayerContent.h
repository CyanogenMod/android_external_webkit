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

#ifndef PicturePileLayerContent_h
#define PicturePileLayerContent_h

#include "LayerContent.h"
#include "PicturePile.h"

namespace WebCore {

class PicturePileLayerContent : public LayerContent {
public:
    PicturePileLayerContent(const PicturePile& picturePile);

    // return 0 when no content, so don't have to paint
    virtual int width() { return m_hasContent ? m_picturePile.size().width() : 0; }
    virtual int height() { return m_hasContent ? m_picturePile.size().height() : 0; }

    virtual void setCheckForOptimisations(bool check) {}
    virtual void checkForOptimisations() {} // already performed, stored in m_hasText/m_hasContent
    virtual float maxZoomScale() { return m_maxZoomScale; }
    virtual void draw(SkCanvas* canvas);
    virtual void serialize(SkWStream* stream);
    virtual PrerenderedInval* prerenderForRect(const IntRect& dirty);
    virtual void clearPrerenders();
    PicturePile* picturePile() { return &m_picturePile; }

private:
    PicturePile m_picturePile;
    float m_maxZoomScale;
    bool m_hasContent;
};

} // WebCore

#endif // PicturePileLayerContent_h
