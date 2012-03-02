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
#include "LayerGroup.h"

#include "ClassTracker.h"
#include "LayerAndroid.h"
#include "TiledTexture.h"
#include "TilesManager.h"

#include <cutils/log.h>
#include <wtf/CurrentTime.h>
#include <wtf/text/CString.h>

#undef XLOGC
#define XLOGC(...) android_printLog(ANDROID_LOG_DEBUG, "LayerGroup", __VA_ARGS__)

#ifdef DEBUG

#undef XLOG
#define XLOG(...) android_printLog(ANDROID_LOG_DEBUG, "LayerGroup", __VA_ARGS__)

#else

#undef XLOG
#define XLOG(...)

#endif // DEBUG

// LayerGroups with an area larger than 2048*2048 should never be unclipped
#define MAX_UNCLIPPED_AREA 4194304

#define TEMP_LAYER m_layers[0]

namespace WebCore {

LayerGroup::LayerGroup()
    : m_hasText(false)
    , m_dualTiledTexture(0)
{
#ifdef DEBUG_COUNT
    ClassTracker::instance()->increment("LayerGroup");
#endif
}

LayerGroup::~LayerGroup()
{
    for (unsigned int i = 0; i < m_layers.size(); i++)
        SkSafeUnref(m_layers[i]);
    if (m_dualTiledTexture)
        SkSafeUnref(m_dualTiledTexture);
#ifdef DEBUG_COUNT
    ClassTracker::instance()->decrement("LayerGroup");
#endif
}

void LayerGroup::initializeGroup(LayerAndroid* newLayer, const SkRegion& newLayerInval,
                                 LayerAndroid* oldLayer)
{
    if (!newLayer->needsTexture())
        return;

    XLOG("init on LG %p, layer %p, oldlayer %p", this, newLayer, oldLayer);
    if (oldLayer && oldLayer->group() && oldLayer->group()->m_dualTiledTexture) {
        // steal DTT from old group, and apply new inval
        m_dualTiledTexture = oldLayer->group()->m_dualTiledTexture;
        SkSafeRef(m_dualTiledTexture);
        m_dualTiledTexture->markAsDirty(newLayerInval);
    } else
        m_dualTiledTexture = new DualTiledTexture();
 }

void LayerGroup::addLayer(LayerAndroid* layer)
{
    m_layers.append(layer);
    SkSafeRef(layer);
}

void LayerGroup::prepareGL(bool layerTilesDisabled)
{
    if (!m_dualTiledTexture)
        return;

    if (layerTilesDisabled) {
        m_dualTiledTexture->discardTextures();
    } else {
        XLOG("prepareGL on LG %p with DTT %p", this, m_dualTiledTexture);
        bool allowZoom = m_hasText; // only allow for scale > 1 if painting vectors
        IntRect prepareArea = computePrepareArea();
        m_dualTiledTexture->prepareGL(TEMP_LAYER->state(), TEMP_LAYER->hasText(),
                                      prepareArea, this);
    }
}

bool LayerGroup::drawGL(bool layerTilesDisabled)
{
    if (!TEMP_LAYER->visible())
        return false;

    FloatRect drawClip = TEMP_LAYER->drawClip();
    FloatRect clippingRect = TilesManager::instance()->shader()->rectInScreenCoord(drawClip);
    TilesManager::instance()->shader()->clip(clippingRect);

    bool askRedraw = false;
    if (m_dualTiledTexture && !layerTilesDisabled) {
        XLOG("drawGL on LG %p with DTT %p", this, m_dualTiledTexture);
        IntRect visibleArea = TEMP_LAYER->visibleArea();
        const TransformationMatrix* transform = TEMP_LAYER->drawTransform();
        askRedraw |= m_dualTiledTexture->drawGL(visibleArea, opacity(), transform);
    }
    askRedraw |= TEMP_LAYER->drawGL(layerTilesDisabled);

    return askRedraw;
}

void LayerGroup::swapTiles()
{
    if (!m_dualTiledTexture)
        return;

    m_dualTiledTexture->swapTiles();
}

bool LayerGroup::isReady()
{
    if (!m_dualTiledTexture)
        return true;

    return m_dualTiledTexture->isReady();
}

IntRect LayerGroup::computePrepareArea() {
    IntRect area;

    if (!TEMP_LAYER->contentIsScrollable()
        && TEMP_LAYER->state()->layersRenderingMode() == GLWebViewState::kAllTextures) {
        area = TEMP_LAYER->unclippedArea();

        double total = ((double) area.width()) * ((double) area.height());
        if (total > MAX_UNCLIPPED_AREA)
            area = TEMP_LAYER->visibleArea();
    } else {
        area = TEMP_LAYER->visibleArea();
    }

    return area;
}

void LayerGroup::computeTexturesAmount(TexturesResult* result)
{
    if (!m_dualTiledTexture)
        return;

    // TODO: don't calculate through layer recursion, use the group list
    m_dualTiledTexture->computeTexturesAmount(result, TEMP_LAYER);
}

bool LayerGroup::paint(BaseTile* tile, SkCanvas* canvas, unsigned int* pictureUsed)
{
    SkPicture *picture = TEMP_LAYER->picture();
    if (!picture) {
        XLOGC("LG %p couldn't paint, no picture in layer %p", this, TEMP_LAYER);
        return false;
    }

    canvas->drawPicture(*picture);

    return true;
}

float LayerGroup::opacity()
{
    return TEMP_LAYER->getOpacity();
}

} // namespace WebCore
