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

#include "DrawExtra.h"
#include "GLExtras.h"
#include "LayerAndroid.h"
#include "SkCanvas.h"
#include "SkRegion.h"
#include "WebViewCore.h"

RegionLayerDrawExtra::RegionLayerDrawExtra()
    : m_highlightColor(COLOR_HOLO_LIGHT)
{}

RegionLayerDrawExtra::~RegionLayerDrawExtra()
{
    HighlightRegionMap::iterator end = m_highlightRegions.end();
    for (HighlightRegionMap::iterator it = m_highlightRegions.begin(); it != end; ++it) {
        delete it->second;
        it->second = 0;
    }
}

SkRegion* RegionLayerDrawExtra::getHighlightRegionsForLayer(const LayerAndroid* layer)
{
    int layerId = layer ? layer->uniqueId() : 0;
    return m_highlightRegions.get(layerId);
}

void RegionLayerDrawExtra::addHighlightRegion(const LayerAndroid* layer, const Vector<IntRect>& rects,
                                              const IntPoint& additionalOffset)
{
    if (rects.isEmpty())
        return;
    int layerId = layer ? layer->uniqueId() : 0;
    SkRegion* region = m_highlightRegions.get(layerId);
    if (!region) {
        region = new SkRegion();
        m_highlightRegions.set(layerId, region);
    }
    IntPoint offset = additionalOffset;
    WebViewCore::layerToAbsoluteOffset(layer, offset);
    for (size_t i = 0; i < rects.size(); i++) {
        IntRect r = rects.at(i);
        r.move(-offset.x(), -offset.y());
        region->op(r.x(), r.y(), r.maxX(), r.maxY(), SkRegion::kUnion_Op);
    }
}

void RegionLayerDrawExtra::draw(SkCanvas* canvas, LayerAndroid* layer)
{
    SkRegion* region = getHighlightRegionsForLayer(layer);
    if (!region || region->isEmpty())
        return;
    SkRegion::Iterator rgnIter(*region);
    SkPaint paint;
    paint.setColor(m_highlightColor.rgb());
    while (!rgnIter.done()) {
        const SkIRect& rect = rgnIter.rect();
        canvas->drawIRect(rect, paint);
        rgnIter.next();
    }
}

void RegionLayerDrawExtra::drawGL(GLExtras* glExtras, const LayerAndroid* layer)
{
    SkRegion* region = getHighlightRegionsForLayer(layer);
    if (!region || region->isEmpty())
        return;
    const TransformationMatrix* transform = layer ? layer->drawTransform() : 0;
    glExtras->drawRegion(*region, true, false, transform, m_highlightColor);
}
