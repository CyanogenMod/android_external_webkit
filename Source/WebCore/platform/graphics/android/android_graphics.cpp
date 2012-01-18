/*
 * Copyright 2007, The Android Open Source Project
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

#include "CachedPrefix.h"
#include "android_graphics.h"
#include "CachedRoot.h"
#include "IntRect.h"
#include "LayerAndroid.h"
#include "SkCanvas.h"
#include "SkCornerPathEffect.h"
#include "SkPath.h"
#include "SkRegion.h"
#include "WebViewCore.h"

namespace android {

#define RING_OUTSET 3
#define RING_RADIUS 1
#define RING_INNER_WIDTH 16
#define RING_OUTER_WIDTH 16

static const RGBA32 ringFill = 0x666699FF;
static const RGBA32 ringPressedInner = 0x006699FF;
static const RGBA32 ringPressedOuter = 0x336699FF;
static const RGBA32 ringSelectedInner = 0xAA6699FF;
static const RGBA32 ringSelectedOuter = 0x336699FF;


CursorRing::CursorRing(WebViewCore* core)
    : m_viewImpl(core)
    , m_layerId(-1)
{
}

// The CSS values for the inner and outer widths may be specified as fractions
#define WIDTH_SCALE 0.0625f // 1/16, to offset the scale in CSSStyleSelector

void CursorRing::draw(SkCanvas* canvas, LayerAndroid* layer, IntRect* inval)
{
    if (!m_lastBounds.isEmpty()) {
        *inval = m_lastBounds;
        m_lastBounds = IntRect(0, 0, 0, 0);
    }
#if USE(ACCELERATED_COMPOSITING)
    int layerId = m_node->isInLayer() ? m_frame->layer(m_node)->uniqueId() : -1;
    if (layer->uniqueId() != layerId)
        return;
#endif
    if (canvas->quickReject(m_bounds, SkCanvas::kAA_EdgeType)) {
        DBG_NAV_LOGD("canvas->quickReject cursorNode=%d (nodePointer=%p)"
            " bounds=(%d,%d,w=%d,h=%d)", m_node->index(), m_node->nodePointer(),
            m_bounds.x(), m_bounds.y(), m_bounds.width(), m_bounds.height());
        return;
    }
    unsigned rectCount = m_rings.size();
    SkRegion rgn;
    SkPath path;
    for (unsigned i = 0; i < rectCount; i++)
    {
        SkRect  r(m_rings[i]);
        SkIRect ir;

        r.round(&ir);
        ir.inset(-RING_OUTSET, -RING_OUTSET);
        rgn.op(ir, SkRegion::kUnion_Op);
    }
    rgn.getBoundaryPath(&path);

    SkPaint paint;
    paint.setAntiAlias(true);
    paint.setPathEffect(new SkCornerPathEffect(
        SkIntToScalar(RING_RADIUS)))->unref();
    SkColor outer;
    SkColor inner;
    if (m_isPressed) {
        SkColor pressed;
        pressed = ringFill;
        paint.setColor(pressed);
        canvas->drawPath(path, paint);
        outer = ringPressedInner;
        inner = ringPressedOuter;
    } else {
        outer = ringSelectedOuter;
        inner = ringSelectedInner;
    }
    paint.setStyle(SkPaint::kStroke_Style);
    paint.setStrokeWidth(RING_OUTER_WIDTH * WIDTH_SCALE);
    paint.setColor(outer);
    canvas->drawPath(path, paint);
    paint.setStrokeWidth(RING_INNER_WIDTH * WIDTH_SCALE);
    paint.setColor(inner);
    canvas->drawPath(path, paint);
    SkRect localBounds, globalBounds;
    localBounds = path.getBounds();
    float width = std::max(RING_INNER_WIDTH, RING_OUTER_WIDTH);
    width *= WIDTH_SCALE;
    localBounds.inset(-width, -width);
    const SkMatrix& matrix = canvas->getTotalMatrix();
    matrix.mapRect(&globalBounds, localBounds);
    SkIRect globalIBounds;
    globalBounds.round(&globalIBounds);
    m_lastBounds = globalIBounds;
    inval->unite(m_lastBounds);
}

void CursorRing::setIsButton(const CachedNode* node)
{
    m_isButton = false;
}

bool CursorRing::setup()
{
    m_layerId = -1;
    if (m_frame && m_root) {
        const CachedLayer* cachedLayer = m_frame->layer(m_node);
        if (cachedLayer) {
            const WebCore::LayerAndroid* rootLayer = m_root->rootLayer();
            const LayerAndroid* aLayer = cachedLayer->layer(rootLayer);
            if (aLayer)
                m_layerId = aLayer->uniqueId();
        }
    }
    if (m_layerId == -1)
        m_node->cursorRings(m_frame, &m_rings);
    else
        m_node->localCursorRings(m_frame, &m_rings);

    if (!m_rings.size()) {
        DBG_NAV_LOG("!rings.size()");
        m_viewImpl->m_hasCursorBounds = false;
        return false;
    }

    setIsButton(m_node);
    m_bounds = m_node->bounds(m_frame);
    m_viewImpl->updateCursorBounds(m_root, m_frame, m_node);

    bool useHitBounds = m_node->useHitBounds();
    if (useHitBounds)
        m_bounds = m_node->hitBounds(m_frame);
    if (useHitBounds || m_node->useBounds()) {
        m_rings.clear();
        m_rings.append(m_bounds);
    }
    m_absBounds = m_node->bounds(m_frame);
    m_bounds.inflate(SkScalarCeil(RING_OUTER_WIDTH));
    m_absBounds.inflate(SkScalarCeil(RING_OUTER_WIDTH));
    if (!m_node->hasCursorRing() || (m_node->isPlugin() && m_node->isFocus()))
        return false;
#if DEBUG_NAV_UI
    const WebCore::IntRect& ring = m_rings[0];
    DBG_NAV_LOGD("cursorNode=%d (nodePointer=%p) pressed=%s rings=%d"
        " (%d, %d, %d, %d) isPlugin=%s",
        m_node->index(), m_node->nodePointer(),
        m_isPressed ? "true" : "false",
        m_rings.size(), ring.x(), ring.y(), ring.width(), ring.height(),
        m_node->isPlugin() ? "true" : "false");
#endif
    return true;
}

}
