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

#ifndef PicturePile_h
#define PicturePile_h

#include "IntRect.h"
#include "IntSize.h"
#include "PrerenderedInval.h"
#include "SkBitmap.h"
#include "SkRegion.h"
#include "SkRefCnt.h"

#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/Vector.h>

class SkPicture;
class SkCanvas;

namespace WebCore {

class GraphicsContext;

class PicturePainter {
public:
    virtual void paintContents(GraphicsContext* gc, IntRect& dirty) = 0;
    virtual SkCanvas* createPrerenderCanvas(PrerenderedInval* prerendered)
    {
        return 0;
    }
    virtual ~PicturePainter() {}
};

class PictureContainer {
public:
    SkPicture* picture;
    IntRect area;
    bool dirty;
    RefPtr<PrerenderedInval> prerendered;

    PictureContainer(const IntRect& area)
        : picture(0)
        , area(area)
        , dirty(true)
    {}

    PictureContainer(const PictureContainer& other)
        : picture(other.picture)
        , area(other.area)
        , dirty(other.dirty)
        , prerendered(other.prerendered)
    {
        SkSafeRef(picture);
    }

    ~PictureContainer()
    {
        SkSafeUnref(picture);
    }
};

class PicturePile {
public:
    PicturePile() {}
    PicturePile(const PicturePile& other);
    PicturePile(SkPicture* picture);

    const IntSize& size() { return m_size; }

    void clearPrerenders();

    // used by PicturePileLayerContents
    void draw(SkCanvas* canvas);

    // Used by WebViewCore
    void invalidate(const IntRect& dirtyRect);
    void setSize(const IntSize& size);
    void updatePicturesIfNeeded(PicturePainter* painter);
    void reset();
    SkRegion& dirtyRegion() { return m_dirtyRegion; }
    PrerenderedInval* prerenderedInvalForArea(const IntRect& area);

private:
    void applyWebkitInvals();
    void updatePicture(PicturePainter* painter, PictureContainer& container);
    void appendToPile(const IntRect& inval, const IntRect& originalInval = IntRect());
    void drawWithClipRecursive(SkCanvas* canvas, SkRegion& clipRegion, int index);

    IntSize m_size;
    Vector<PictureContainer> m_pile;
    Vector<IntRect> m_webkitInvals;
    SkRegion m_dirtyRegion;
};

} // namespace android

#endif // PicturePile_h
