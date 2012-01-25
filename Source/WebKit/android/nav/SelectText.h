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

#ifndef SelectText_h
#define SelectText_h

#include "DrawExtra.h"
#include "IntPoint.h"
#include "IntRect.h"
#include "PlatformString.h"
#include "SkPath.h"
#include "SkPicture.h"
#include "SkRect.h"
#include "SkRegion.h"
#include "wtf/Vector.h"

namespace android {

class SelectText : public DrawExtra {
public:
    enum HandleId {
        StartHandle = 0,
        EndHandle = 1,
        BaseHandle = 2,
        ExtentHandle = 3,
    };

    SelectText() {}
    virtual ~SelectText();

    SkRegion* getHightlightRegionsForLayer(int layerId) {
        return m_highlightRegions.get(layerId);
    }

    void setHighlightRegionsForLayer(int layerId, SkRegion* region) {
        m_highlightRegions.set(layerId, region);
    }

    virtual void draw(SkCanvas*, LayerAndroid*);
    virtual void drawGL(GLExtras*, const LayerAndroid*);

    IntRect& caretRect(HandleId id) { return m_caretRects[mapId(id)]; }
    void setCaretRect(HandleId id, const IntRect& rect) { m_caretRects[mapId(id)] = rect; }
    int caretLayerId(HandleId id) { return m_caretLayerId[mapId(id)]; }
    void setCaretLayerId(HandleId id, int layerId) { m_caretLayerId[mapId(id)] = layerId; }

    bool isBaseFirst() const { return m_baseIsFirst; }
    void setBaseFirst(bool isFirst) { m_baseIsFirst = isFirst; }

    void setText(const String& text) { m_text = text.threadsafeCopy(); }
    String& getText() { return m_text; }

private:
    HandleId mapId(HandleId id);

    typedef HashMap<int, SkRegion* > HighlightRegionMap;
    HighlightRegionMap m_highlightRegions;
    IntRect m_caretRects[2];
    int m_caretLayerId[2];
    bool m_baseIsFirst;
    String m_text;
};

}

namespace WebCore {

void ReverseBidi(UChar* chars, int len);

}

#endif
