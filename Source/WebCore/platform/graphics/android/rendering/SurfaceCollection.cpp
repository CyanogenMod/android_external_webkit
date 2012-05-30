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

#define LOG_TAG "SurfaceCollection"
#define LOG_NDEBUG 1

#include "config.h"
#include "SurfaceCollection.h"

#include "AndroidLog.h"
#include "BaseLayerAndroid.h"
#include "ClassTracker.h"
#include "GLWebViewState.h"
#include "PaintTileOperation.h"
#include "Surface.h"
#include "ScrollableLayerAndroid.h"
#include "TilesManager.h"

namespace WebCore {

////////////////////////////////////////////////////////////////////////////////
//                        TILED PAINTING / SURFACES                           //
////////////////////////////////////////////////////////////////////////////////

SurfaceCollection::SurfaceCollection(BaseLayerAndroid* layer)
        : m_compositedRoot(layer)
{
    // layer must be non-null.
    SkSafeRef(m_compositedRoot);

    // calculate draw transforms and z values
    SkRect visibleRect = SkRect::MakeLTRB(0, 0, 1, 1);
    m_compositedRoot->updatePositionsRecursive(visibleRect);

    // allocate surfaces for layers, merging where possible
    ALOGV("new tree, allocating surfaces for tree %p", m_baseLayer);

    LayerMergeState layerMergeState(&m_surfaces);
    m_compositedRoot->assignSurfaces(&layerMergeState);

    // set the layersurfaces' update count, to be drawn on painted tiles
    unsigned int updateCount = TilesManager::instance()->incWebkitContentUpdates();
    for (unsigned int i = 0; i < m_surfaces.size(); i++)
        m_surfaces[i]->setUpdateCount(updateCount);

#ifdef DEBUG_COUNT
    ClassTracker::instance()->increment("SurfaceCollection");
#endif
}

SurfaceCollection::~SurfaceCollection()
{
    SkSafeUnref(m_compositedRoot);
    for (unsigned int i = 0; i < m_surfaces.size(); i++)
        SkSafeUnref(m_surfaces[i]);
    m_surfaces.clear();

#ifdef DEBUG_COUNT
    ClassTracker::instance()->decrement("SurfaceCollection");
#endif
}

void SurfaceCollection::prepareGL(const SkRect& visibleContentRect, bool tryToFastBlit)
{
    TRACE_METHOD();
    updateLayerPositions(visibleContentRect);
    bool layerTilesDisabled = m_compositedRoot->state()->isSingleSurfaceRenderingMode();
    if (!layerTilesDisabled) {
        for (unsigned int i = 0; tryToFastBlit && i < m_surfaces.size(); i++)
            tryToFastBlit &= m_surfaces[i]->canUpdateWithBlit();
    }
    for (unsigned int i = 0; i < m_surfaces.size(); i++)
        m_surfaces[i]->prepareGL(layerTilesDisabled, tryToFastBlit);
}

static inline bool compareSurfaceZ(const Surface* a, const Surface* b)
{
    const LayerAndroid* la = a->getFirstLayer();
    const LayerAndroid* lb = b->getFirstLayer();

    // swap drawing order if zValue suggests it AND the layers are in the same stacking context
    return (la->zValue() > lb->zValue()) && (la->getParent() == lb->getParent());
}

bool SurfaceCollection::drawGL(const SkRect& visibleContentRect)
{
    TRACE_METHOD();
#ifdef DEBUG_COUNT
    ClassTracker::instance()->show();
#endif

    bool needsRedraw = false;
    updateLayerPositions(visibleContentRect);
    bool layerTilesDisabled = m_compositedRoot->state()->isSingleSurfaceRenderingMode();

    // create a duplicate vector of surfaces, sorted by z value
    Vector <Surface*> surfaces;
    for (unsigned int i = 0; i < m_surfaces.size(); i++)
        surfaces.append(m_surfaces[i]);
    std::stable_sort(surfaces.begin()+1, surfaces.end(), compareSurfaceZ);

    // draw the sorted vector
    for (unsigned int i = 0; i < m_surfaces.size(); i++)
        needsRedraw |= surfaces[i]->drawGL(layerTilesDisabled);

    return needsRedraw;
}

Color SurfaceCollection::getBackgroundColor()
{
    return static_cast<BaseLayerAndroid*>(m_compositedRoot)->getBackgroundColor();
}

void SurfaceCollection::swapTiles()
{
    bool calculateFrameworkInvals = !m_compositedRoot->state()->inUnclippedDraw();

    TRACE_METHOD();
    for (unsigned int i = 0; i < m_surfaces.size(); i++)
         m_surfaces[i]->swapTiles(calculateFrameworkInvals);
}

void SurfaceCollection::addFrameworkInvals()
{
    for (unsigned int i = 0; i < m_surfaces.size(); i++)
         m_surfaces[i]->addFrameworkInvals();
}

bool SurfaceCollection::isReady()
{
    // Override layer readiness check for single surface mode
    if (m_compositedRoot->state()->isSingleSurfaceRenderingMode())
        return m_surfaces[0]->isReady();

    for (unsigned int i = 0; i < m_surfaces.size(); i++) {
        if (!m_surfaces[i]->isReady()) {
            ALOGV("layer surface %p isn't ready", m_surfaces[i]);
            return false;
        }
    }
    return true;
}

bool SurfaceCollection::isMissingBackgroundContent()
{
    // return true when the first surface is missing content (indicating the
    // entire viewport isn't covered)
    return m_surfaces[0]->isMissingContent();
}

void SurfaceCollection::removePainterOperations()
{
    for (unsigned int i = 0; i < m_surfaces.size(); i++)
        TilesManager::instance()->removeOperationsForFilter(new TilePainterFilter(m_surfaces[i]));
}

void SurfaceCollection::computeTexturesAmount(TexturesResult* result)
{
    for (unsigned int i = 0; i < m_surfaces.size(); i++)
        m_surfaces[i]->computeTexturesAmount(result);
}

////////////////////////////////////////////////////////////////////////////////
//                  RECURSIVE ANIMATION / INVALS / LAYERS                     //
////////////////////////////////////////////////////////////////////////////////

void SurfaceCollection::setIsPainting(SurfaceCollection* drawingSurface)
{
    if (!drawingSurface)
        return;

    for (unsigned int i = 0; i < m_surfaces.size(); i++) {
        Surface* newSurface = m_surfaces[i];
        if (!newSurface->needsTexture())
            continue;

        for (unsigned int j = 0; j < drawingSurface->m_surfaces.size(); j++) {
            Surface* oldSurface = drawingSurface->m_surfaces[j];
            if (newSurface->tryUpdateSurface(oldSurface))
                break;
        }
    }
}

void SurfaceCollection::setIsDrawing()
{
    m_compositedRoot->initAnimations();
}

void SurfaceCollection::mergeInvalsInto(SurfaceCollection* replacementSurface)
{
    m_compositedRoot->mergeInvalsInto(replacementSurface->m_compositedRoot);
}

bool SurfaceCollection::evaluateAnimations(double currentTime)
{
    return m_compositedRoot->evaluateAnimations(currentTime);
}

bool SurfaceCollection::hasCompositedLayers()
{
    return m_compositedRoot->countChildren();
}

bool SurfaceCollection::hasCompositedAnimations()
{
    return m_compositedRoot->hasAnimations();
}

void SurfaceCollection::updateScrollableLayer(int layerId, int x, int y)
{
    LayerAndroid* layer = m_compositedRoot->findById(layerId);
    if (layer && layer->contentIsScrollable())
        static_cast<ScrollableLayerAndroid*>(layer)->scrollTo(x, y);
}

void SurfaceCollection::updateLayerPositions(const SkRect& visibleContentRect)
{
    m_compositedRoot->updatePositionsRecursive(visibleContentRect);

#ifdef DEBUG
    m_compositedRoot->showLayer(0);
    ALOGV("We have %d layers, %d textured",
          m_compositedRoot->nbLayers(),
          m_compositedRoot->nbTexturedLayers());
#endif
}

int SurfaceCollection::backedSize()
{
    int count = 0;
    for (unsigned int i = 0; i < m_surfaces.size(); i++) {
        if (m_surfaces[i]->needsTexture())
            count++;
    }
    return count;
}

} // namespace WebCore
