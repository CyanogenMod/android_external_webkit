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

#define LOG_TAG "PicturePile"
#define LOG_NDEBUG 1

#include "config.h"
#include "PicturePile.h"

#include "AndroidLog.h"
#include "FloatRect.h"
#include "GraphicsContext.h"
#include "PlatformGraphicsContextSkia.h"
#include "SkCanvas.h"
#include "SkNWayCanvas.h"
#include "SkPixelRef.h"
#include "SkRect.h"
#include "SkRegion.h"

#if USE_RECORDING_CONTEXT
#include "PlatformGraphicsContextRecording.h"
#else
#include "SkPicture.h"
#endif

#define ENABLE_PRERENDERED_INVALS true
#define MAX_OVERLAP_COUNT 2
#define MAX_OVERLAP_AREA .7

namespace WebCore {

static SkIRect toSkIRect(const IntRect& rect) {
    return SkIRect::MakeXYWH(rect.x(), rect.y(), rect.width(), rect.height());
}

PictureContainer::PictureContainer(const PictureContainer& other)
    : picture(other.picture)
    , area(other.area)
    , dirty(other.dirty)
    , prerendered(other.prerendered)
{
    SkSafeRef(picture);
}

PictureContainer::~PictureContainer()
{
    SkSafeUnref(picture);
}

PicturePile::PicturePile(const PicturePile& other)
    : m_size(other.m_size)
    , m_pile(other.m_pile)
    , m_webkitInvals(other.m_webkitInvals)
{
}

void PicturePile::draw(SkCanvas* canvas)
{
    /* Loop down recursively, subtracting the previous clip from the SkRegion,
     * stopping when the SkRegion is empty. This will still draw back-to-front,
     * but it will clip out anything obscured. For performance reasons we use
     * the rect bounds of the SkRegion for the clip, so this still can't be
     * used for translucent surfaces
     */
    if (canvas->quickReject(SkRect::MakeWH(m_size.width(), m_size.height()),
            SkCanvas::kBW_EdgeType))
        return;
    drawWithClipRecursive(canvas, m_pile.size() - 1);
}

void PicturePile::clearPrerenders()
{
    for (size_t i = 0; i < m_pile.size(); i++)
        m_pile[i].prerendered.clear();
}

void PicturePile::drawWithClipRecursive(SkCanvas* canvas, int index)
{
    // TODO: Add some debug visualizations of this
    if (index < 0)
        return;
    PictureContainer& pc = m_pile[index];
    if (pc.picture && !canvas->quickReject(pc.area, SkCanvas::kBW_EdgeType)) {
        int saved = canvas->save(SkCanvas::kClip_SaveFlag);
        if (canvas->clipRect(pc.area, SkRegion::kDifference_Op))
            drawWithClipRecursive(canvas, index - 1);
        canvas->restoreToCount(saved);
        saved = canvas->save(SkCanvas::kClip_SaveFlag);
        if (canvas->clipRect(pc.area))
            drawPicture(canvas, pc);
        canvas->restoreToCount(saved);
    } else
        drawWithClipRecursive(canvas, index - 1);
}

// Used by WebViewCore
void PicturePile::invalidate(const IntRect& dirtyRect)
{
    // This will typically happen if the document has been resized but we haven't
    // drawn yet. As the first draw after a size change will do a full inval anyway,
    // don't bother tracking individual rects
    // TODO: Instead of clipping here, we should take the invals as given
    // and when the size changes just inval the deltas. This prevents a full
    // redraw for a page that grows
    IntRect inval = dirtyRect;
    inval.intersect(IntRect(0, 0, m_size.width(), m_size.height()));
    if (inval.isEmpty()) {
        ALOGV("Rejecting inval " INT_RECT_FORMAT, INT_RECT_ARGS(dirtyRect));
        return;
    }
    // TODO: Support multiple non-intersecting webkit invals
    if (m_webkitInvals.size())
        m_webkitInvals[0].unite(inval);
    else
        m_webkitInvals.append(inval);
}

void PicturePile::setSize(const IntSize& size)
{
    if (m_size == size)
        return;
    IntSize oldSize = m_size;
    m_size = size;
    if (size.width() <= oldSize.width() && size.height() <= oldSize.height()) {
        // We are shrinking - huzzah, nothing to do!
        // TODO: Loop through and throw out Pictures that are now clipped out
    } else if (oldSize.width() == size.width()) {
        // Only changing vertically
        IntRect rect(0, std::min(oldSize.height(), size.height()),
                     size.width(), std::abs(oldSize.height() - size.height()));
        invalidate(rect);
    } else if (oldSize.height() == size.height()) {
        // Only changing horizontally
        IntRect rect(std::min(oldSize.width(), size.width()), 0,
                     std::abs(oldSize.width() - size.width()), size.height());
        invalidate(rect);
    } else {
        // Both width & height changed, full inval :(
        m_pile.clear();
        m_webkitInvals.clear();
        if (!size.isEmpty()) {
            IntRect area(0, 0, size.width(), size.height());
            m_webkitInvals.append(area);
            m_pile.append(area);
        }
    }
}

void PicturePile::updatePicturesIfNeeded(PicturePainter* painter)
{
    applyWebkitInvals();
    for (size_t i = 0; i < m_pile.size(); i++) {
        PictureContainer& pc = m_pile[i];
        if (pc.dirty)
            updatePicture(painter, pc);
    }
}

void PicturePile::updatePicture(PicturePainter* painter, PictureContainer& pc)
{
    TRACE_METHOD();
    Picture* picture = recordPicture(painter, pc);
    SkSafeUnref(pc.picture);
    pc.picture = picture;
    pc.dirty = false;
}

void PicturePile::reset()
{
    m_size = IntSize(0,0);
    m_pile.clear();
    m_webkitInvals.clear();
}

void PicturePile::applyWebkitInvals()
{
    m_dirtyRegion.setEmpty();
    if (!m_webkitInvals.size())
        return;
    // Build the invals (TODO: Support multiple inval regions)
    IntRect inval = m_webkitInvals[0];
    m_dirtyRegion.setRect(toSkIRect(inval));
    for (size_t i = 1; i < m_webkitInvals.size(); i++) {
        inval.unite(m_webkitInvals[i]);
        m_dirtyRegion.op(toSkIRect(m_webkitInvals[i]), SkRegion::kUnion_Op);
    }
    m_webkitInvals.clear();
    ALOGV("Webkit inval: " INT_RECT_FORMAT, INT_RECT_ARGS(inval));
    if (inval.isEmpty())
        return;

    // Find the overlaps
    Vector<int> overlaps;
    for (size_t i = 0; i < m_pile.size(); i++) {
        PictureContainer& pc = m_pile[i];
        if (pc.area.contains(inval)) {
            if (pc.dirty) {
                ALOGV("Found already dirty intersection");
                return;
            }
            if (pc.area == inval) {
                appendToPile(inval);
                return;
            }
            // Don't count the base surface as an overlap
            if (pc.area.size() != m_size)
                overlaps.append(i);
        } else if (pc.area.intersects(inval))
            overlaps.append(i);
    }

    if (overlaps.size() >= MAX_OVERLAP_COUNT) {
        ALOGV("Exceeds overlap count");
        IntRect overlap = inval;
        for (int i = (int) overlaps.size() - 1; i >= 0; i--) {
            overlap.unite(m_pile[overlaps[i]].area);
            m_pile.remove(overlaps[i]);
        }
        float overlapArea = overlap.width() * overlap.height();
        float totalArea = m_size.width() * m_size.height();
        if (overlapArea / totalArea > MAX_OVERLAP_AREA)
            overlap = IntRect(0, 0, m_size.width(), m_size.height());
        appendToPile(overlap, inval);
        return;
    }

    // Append!
    appendToPile(inval);
}

void PicturePile::appendToPile(const IntRect& inval, const IntRect& originalInval)
{
    ALOGV("Adding inval " INT_RECT_FORMAT " for original inval " INT_RECT_FORMAT,
            INT_RECT_ARGS(inval), INT_RECT_ARGS(originalInval));
    // Remove any entries this obscures
    for (int i = (int) m_pile.size() - 1; i >= 0; i--) {
        if (inval.contains(m_pile[i].area))
            m_pile.remove(i);
    }
    PictureContainer container(inval);
    if (ENABLE_PRERENDERED_INVALS) {
        container.prerendered = PrerenderedInval::create(originalInval.isEmpty()
                                                         ? inval : originalInval);
    }
    m_pile.append(container);
}

PrerenderedInval* PicturePile::prerenderedInvalForArea(const IntRect& area)
{
    for (int i = (int) m_pile.size() - 1; i >= 0; i--) {
        if (m_pile[i].area.intersects(area)) {
            RefPtr<PrerenderedInval> inval = m_pile[i].prerendered;
            if (inval.get() && inval->area.contains(area))
                return inval.get();
            return 0;
        }
    }
    return 0;
}

float PicturePile::maxZoomScale() const
{
    float maxZoomScale = 1;
    for (size_t i = 0; i < m_pile.size(); i++) {
        maxZoomScale = std::max(maxZoomScale, m_pile[i].maxZoomScale);
    }
    return maxZoomScale;
}

bool PicturePile::isEmpty() const
{
    for (size_t i = 0; i < m_pile.size(); i++) {
        if (m_pile[i].picture)
            return false;
    }
    return true;
}

#if USE_RECORDING_CONTEXT
void PicturePile::drawPicture(SkCanvas* canvas, PictureContainer& pc)
{
    TRACE_METHOD();
    pc.picture->draw(canvas);
}

Picture* PicturePile::recordPicture(PicturePainter* painter, PictureContainer& pc)
{
    pc.prerendered.clear(); // TODO: Support? Not needed?

    Recording* picture = new Recording();
    WebCore::PlatformGraphicsContextRecording pgc(picture);
    WebCore::GraphicsContext gc(&pgc);
    painter->paintContents(&gc, pc.area);
    pc.maxZoomScale = pgc.maxZoomScale();
    if (pgc.isEmpty()) {
        SkSafeUnref(picture);
        picture = 0;
    }

    return picture;
}
#else
void PicturePile::drawPicture(SkCanvas* canvas, PictureContainer& pc)
{
    canvas->translate(pc.area.x(), pc.area.y());
    pc.picture->draw(canvas);
}

Picture* PicturePile::recordPicture(PicturePainter* painter, PictureContainer& pc)
{
    /* The ref counting here is a bit unusual. What happens is begin/end recording
     * will ref/unref the recording canvas. However, 'canvas' might be pointing
     * at an SkNWayCanvas instead of the recording canvas, which needs to be
     * unref'd. Thus what we do is ref the recording canvas so that we can
     * always unref whatever canvas we have at the end.
     */
    SkPicture* picture = new SkPicture();
    SkCanvas* canvas = picture->beginRecording(pc.area.width(), pc.area.height(),
            SkPicture::kUsePathBoundsForClip_RecordingFlag);
    SkSafeRef(canvas);
    canvas->translate(-pc.area.x(), -pc.area.y());
    IntRect drawArea = pc.area;
    if (pc.prerendered.get()) {
        SkCanvas* prerender = painter->createPrerenderCanvas(pc.prerendered.get());
        if (!prerender) {
            ALOGV("Failed to create prerendered for " INT_RECT_FORMAT,
                    INT_RECT_ARGS(pc.prerendered->area));
            pc.prerendered.clear();
        } else {
            drawArea.unite(pc.prerendered->area);
            SkNWayCanvas* nwayCanvas = new SkNWayCanvas(drawArea.width(), drawArea.height());
            nwayCanvas->translate(-drawArea.x(), -drawArea.y());
            nwayCanvas->addCanvas(canvas);
            nwayCanvas->addCanvas(prerender);
            SkSafeUnref(canvas);
            SkSafeUnref(prerender);
            canvas = nwayCanvas;
        }
    }
    WebCore::PlatformGraphicsContextSkia pgc(canvas);
    WebCore::GraphicsContext gc(&pgc);
    ALOGV("painting picture: " INT_RECT_FORMAT, INT_RECT_ARGS(drawArea));
    painter->paintContents(&gc, drawArea);

    // TODO: consider paint-time checking for these with SkPicture painting?
    pc.maxZoomScale = 1e6;

    SkSafeUnref(canvas);
    picture->endRecording();
    return picture;
}
#endif

} // namespace WebCore
