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

namespace WebCore {

LayerGroup::LayerGroup()
    : m_dualTiledTexture(0)
    , m_needsTexture(false)
    , m_hasText(false)
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

bool LayerGroup::tryUpdateLayerGroup(LayerGroup* oldLayerGroup)
{
    if (!needsTexture() || !oldLayerGroup->needsTexture())
        return false;

    // merge layer group based on first layer ID
    if (getFirstLayer()->uniqueId() != oldLayerGroup->getFirstLayer()->uniqueId())
        return false;

    m_dualTiledTexture = oldLayerGroup->m_dualTiledTexture;
    SkSafeRef(m_dualTiledTexture);

    XLOG("%p taking old DTT %p from group %p, nt %d",
         this, m_dualTiledTexture, oldLayerGroup, oldLayerGroup->needsTexture());

    if (!m_dualTiledTexture) {
        // no DTT to inval, so don't worry about it.
        return true;
    }

    if (singleLayer() && oldLayerGroup->singleLayer()) {
        // both are single matching layers, simply apply inval
        SkRegion* layerInval = getFirstLayer()->getInvalRegion();
        m_dualTiledTexture->markAsDirty(*layerInval);
    } else {
        SkRegion invalRegion;
        bool fullInval = m_layers.size() != oldLayerGroup->m_layers.size();
        if (!fullInval) {
            for (unsigned int i = 0; i < m_layers.size(); i++) {
                if (m_layers[i]->uniqueId() != oldLayerGroup->m_layers[i]->uniqueId()) {
                    // layer list has changed, fully invalidate
                    // TODO: partially invalidate based on layer size/position
                    fullInval = true;
                    break;
                } else if (!m_layers[i]->getInvalRegion()->isEmpty()) {
                    // merge layer inval - translate the layer's inval region into group coordinates
                    SkPoint pos = m_layers[i]->getPosition();
                    m_layers[i]->getInvalRegion()->translate(pos.fX, pos.fY);
                    invalRegion.op(*(m_layers[i]->getInvalRegion()), SkRegion::kUnion_Op);
                    break;
                }
            }
        }

        if (fullInval)
            invalRegion.setRect(-1e8, -1e8, 2e8, 2e8);

        m_dualTiledTexture->markAsDirty(invalRegion);
    }
    return true;
}

void LayerGroup::addLayer(LayerAndroid* layer, const TransformationMatrix& transform)
{
    m_layers.append(layer);
    SkSafeRef(layer);

    m_needsTexture |= layer->needsTexture();
    m_hasText |= layer->hasText();

    // calculate area size for comparison later
    IntRect rect = layer->unclippedArea();
    SkPoint pos = layer->getPosition();
    rect.setLocation(IntPoint(pos.fX, pos.fY));

    if (layer->needsTexture()) {
        if (m_unclippedArea.isEmpty()) {
            m_drawTransform = transform;
            m_drawTransform.translate3d(-pos.fX, -pos.fY, 0);
            m_unclippedArea = rect;
        } else
            m_unclippedArea.unite(rect);
        XLOG("LG %p adding LA %p, size  %d, %d  %dx%d, now LG size %d,%d  %dx%d",
             this, layer, rect.x(), rect.y(), rect.width(), rect.height(),
             m_unclippedArea.x(), m_unclippedArea.y(),
             m_unclippedArea.width(), m_unclippedArea.height());
    }
}

IntRect LayerGroup::visibleArea()
{
    if (singleLayer())
        return getFirstLayer()->visibleArea();

    IntRect rect = m_unclippedArea;

    // clip with the viewport in documents coordinate
    IntRect documentViewport(TilesManager::instance()->shader()->documentViewport());
    rect.intersect(documentViewport);

    // TODO: handle recursive layer clip

    return rect;
}

void LayerGroup::prepareGL(bool layerTilesDisabled)
{
    if (!m_dualTiledTexture) {
        XLOG("prepareGL on LG %p, no DTT, needsTexture? %d",
             this, m_dualTiledTexture, needsTexture());

        if (needsTexture())
            m_dualTiledTexture = new DualTiledTexture();
        else
            return;
    }

    if (layerTilesDisabled) {
        m_dualTiledTexture->discardTextures();
    } else {
        bool allowZoom = hasText(); // only allow for scale > 1 if painting vectors
        IntRect prepareArea = computePrepareArea();

        XLOG("prepareGL on LG %p with DTT %p, %d layers",
             this, m_dualTiledTexture, m_layers.size());
        m_dualTiledTexture->prepareGL(getFirstLayer()->state(), allowZoom,
                                      prepareArea, this);
    }
}

bool LayerGroup::drawGL(bool layerTilesDisabled)
{
    if (!getFirstLayer()->visible())
        return false;

    FloatRect drawClip = getFirstLayer()->drawClip();
    FloatRect clippingRect = TilesManager::instance()->shader()->rectInScreenCoord(drawClip);
    TilesManager::instance()->shader()->clip(clippingRect);

    bool askRedraw = false;
    if (m_dualTiledTexture && !layerTilesDisabled) {
        XLOG("drawGL on LG %p with DTT %p", this, m_dualTiledTexture);

        IntRect drawArea = visibleArea();
        askRedraw |= m_dualTiledTexture->drawGL(drawArea, opacity(), drawTransform());
    }

    // draw member layers (draws image textures, glextras)
    for (unsigned int i = 0; i < m_layers.size(); i++)
        askRedraw |= m_layers[i]->drawGL(layerTilesDisabled);

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

    if (!getFirstLayer()->contentIsScrollable()
        && getFirstLayer()->state()->layersRenderingMode() == GLWebViewState::kAllTextures) {

        area = singleLayer() ? getFirstLayer()->unclippedArea() : m_unclippedArea;

        double total = ((double) area.width()) * ((double) area.height());
        if (total > MAX_UNCLIPPED_AREA)
            area = visibleArea();
    } else {
        area = visibleArea();
    }

    return area;
}

void LayerGroup::computeTexturesAmount(TexturesResult* result)
{
    if (!m_dualTiledTexture)
        return;

    m_dualTiledTexture->computeTexturesAmount(result, getFirstLayer());
}

bool LayerGroup::paint(BaseTile* tile, SkCanvas* canvas)
{
    if (singleLayer()) {
        getFirstLayer()->contentDraw(canvas, Layer::UnmergedLayers);
    } else {
        SkAutoCanvasRestore acr(canvas, true);
        SkMatrix matrix;
        GLUtils::toSkMatrix(matrix, m_drawTransform);

        SkMatrix inverse;
        inverse.reset();
        matrix.invert(&inverse);

        SkMatrix canvasMatrix = canvas->getTotalMatrix();
        inverse.postConcat(canvasMatrix);
        canvas->setMatrix(inverse);

        for (unsigned int i=0; i<m_layers.size(); i++)
            m_layers[i]->drawCanvas(canvas, false, Layer::MergedLayers);
    }
    return true;
}

float LayerGroup::opacity()
{
    if (singleLayer())
        return getFirstLayer()->getOpacity();
    return 1.0;
}

const TransformationMatrix* LayerGroup::drawTransform()
{
    // single layer groups query the layer's draw transform, while multi-layer
    // groups copy the draw transform once, during initialization
    // TODO: support fixed multi-layer groups by querying the changing drawTransform
    if (singleLayer())
        return getFirstLayer()->drawTransform();

    return &m_drawTransform;
}

} // namespace WebCore
