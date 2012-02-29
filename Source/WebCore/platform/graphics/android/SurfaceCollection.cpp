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
#include "SurfaceCollection.h"

#include "BaseLayerAndroid.h"
#include "ClassTracker.h"
#include "LayerAndroid.h"
#include "LayerGroup.h"
#include "GLWebViewState.h"
#include "ScrollableLayerAndroid.h"

#include <cutils/log.h>
#include <wtf/CurrentTime.h>
#include <wtf/text/CString.h>

#undef XLOGC
#define XLOGC(...) android_printLog(ANDROID_LOG_DEBUG, "SurfaceCollection", __VA_ARGS__)

#ifdef DEBUG

#undef XLOG
#define XLOG(...) android_printLog(ANDROID_LOG_DEBUG, "SurfaceCollection", __VA_ARGS__)

#else

#undef XLOG
#define XLOG(...)

#endif // DEBUG

namespace WebCore {

////////////////////////////////////////////////////////////////////////////////
//                         TILED PAINTING / GROUPS                            //
////////////////////////////////////////////////////////////////////////////////

SurfaceCollection::SurfaceCollection(BaseLayerAndroid* baseLayer)
        : m_baseLayer(baseLayer)
        , m_compositedRoot(0)
{
    SkSafeRef(m_baseLayer);
    if (m_baseLayer && m_baseLayer->countChildren()) {
        m_compositedRoot = static_cast<LayerAndroid*>(m_baseLayer->getChild(0));

        // calculate draw transforms and z values
        SkRect visibleRect = SkRect::MakeLTRB(0, 0, 1, 1);
        m_baseLayer->updateLayerPositions(visibleRect);

        // allocate groups for layers, merging where possible
        XLOG("new tree, allocating groups for tree %p", m_baseLayer);
        m_compositedRoot->assignGroups(&m_layerGroups);
    }
#ifdef DEBUG_COUNT
    ClassTracker::instance()->increment("SurfaceCollection");
#endif
}

SurfaceCollection::~SurfaceCollection()
{
    SkSafeUnref(m_baseLayer);
    for (unsigned int i = 0; i < m_layerGroups.size(); i++)
        SkSafeUnref(m_layerGroups[i]);
    m_layerGroups.clear();

#ifdef DEBUG_COUNT
    ClassTracker::instance()->decrement("SurfaceCollection");
#endif
}

void SurfaceCollection::prepareGL(const SkRect& visibleRect, float scale, double currentTime)
{
    if (!m_baseLayer)
        return;

    m_baseLayer->prepareGL(visibleRect, scale, currentTime);

    if (m_compositedRoot) {
        m_baseLayer->updateLayerPositions(visibleRect);
        bool layerTilesDisabled = m_compositedRoot->state()->layersRenderingMode()
            > GLWebViewState::kClippedTextures;
        for (unsigned int i = 0; i < m_layerGroups.size(); i++)
            m_layerGroups[i]->prepareGL(layerTilesDisabled);
    }
}

bool SurfaceCollection::drawGL(const SkRect& visibleRect, float scale)
{
#ifdef DEBUG_COUNT
    ClassTracker::instance()->show();
#endif

    if (!m_baseLayer)
        return false;

    m_baseLayer->drawGL(scale);

    bool needsRedraw = false;
    if (m_compositedRoot) {
        m_baseLayer->updateLayerPositions(visibleRect);
        bool layerTilesDisabled = m_compositedRoot->state()->layersRenderingMode()
            > GLWebViewState::kClippedTextures;
        for (unsigned int i = 0; i < m_layerGroups.size(); i++)
            needsRedraw |= m_layerGroups[i]->drawGL(layerTilesDisabled);
    }

    return needsRedraw;
}

void SurfaceCollection::swapTiles()
{
    if (!m_baseLayer)
        return;

    m_baseLayer->swapTiles();

    for (unsigned int i = 0; i < m_layerGroups.size(); i++)
         m_layerGroups[i]->swapTiles();
}

bool SurfaceCollection::isReady()
{
    if (!m_baseLayer)
        return true;

    if (!m_baseLayer->isReady())
        return false;

    if (!m_compositedRoot)
        return true;

    // Override layer readiness check for single surface mode
    if (m_compositedRoot->state()->layersRenderingMode() > GLWebViewState::kClippedTextures) {
        // TODO: single surface mode should be properly double buffered
        return true;
    }

    for (unsigned int i = 0; i < m_layerGroups.size(); i++) {
        if (!m_layerGroups[i]->isReady()) {
            XLOG("layer group %p isn't ready", m_layerGroups[i]);
            return false;
        }
    }
    return true;
}

void SurfaceCollection::computeTexturesAmount(TexturesResult* result)
{
    for (unsigned int i = 0; i < m_layerGroups.size(); i++)
        m_layerGroups[i]->computeTexturesAmount(result);
}

void SurfaceCollection::drawCanvas(SkCanvas* canvas, bool drawLayers)
{
    // TODO: move this functionality out!
    if (!m_baseLayer)
        return;

    m_baseLayer->drawCanvas(canvas);

    // draw the layers onto the same canvas (for single surface mode)
    if (drawLayers && m_compositedRoot)
        m_compositedRoot->drawCanvas(canvas);
}


////////////////////////////////////////////////////////////////////////////////
//                  RECURSIVE ANIMATION / INVALS / LAYERS                     //
////////////////////////////////////////////////////////////////////////////////

void SurfaceCollection::setIsPainting(SurfaceCollection* drawingSurface)
{
    if (!m_baseLayer)
        return;

    m_baseLayer->setIsPainting();

    LayerAndroid* oldCompositedSurface = 0;
    if (drawingSurface)
        oldCompositedSurface = drawingSurface->m_compositedRoot;

    if (m_compositedRoot)
        m_compositedRoot->setIsPainting(oldCompositedSurface);
}

void SurfaceCollection::setIsDrawing()
{
    if (m_compositedRoot)
        m_compositedRoot->initAnimations();
}

void SurfaceCollection::mergeInvalsInto(SurfaceCollection* replacementSurface)
{
    if (!m_baseLayer)
        return;

    m_baseLayer->mergeInvalsInto(replacementSurface->m_baseLayer);

    if (m_compositedRoot && replacementSurface->m_compositedRoot)
        m_compositedRoot->mergeInvalsInto(replacementSurface->m_compositedRoot);
}

void SurfaceCollection::evaluateAnimations(double currentTime)
{
    if (m_compositedRoot)
        m_compositedRoot->evaluateAnimations(currentTime);
}

bool SurfaceCollection::hasCompositedLayers()
{
    return m_compositedRoot != 0;
}

bool SurfaceCollection::hasCompositedAnimations()
{
    return m_compositedRoot != 0 && m_compositedRoot->hasAnimations();
}

int SurfaceCollection::baseContentWidth()
{
    // TODO: move this functionality out!
    return m_baseLayer ? m_baseLayer->content()->width() : 0;
}

int SurfaceCollection::baseContentHeight()
{
    // TODO: move this functionality out!
    return m_baseLayer ? m_baseLayer->content()->height() : 0;
}

void SurfaceCollection::updateScrollableLayer(int layerId, int x, int y)
{
    if (m_compositedRoot) {
        LayerAndroid* layer = m_compositedRoot->findById(layerId);
        if (layer && layer->contentIsScrollable())
            static_cast<ScrollableLayerAndroid*>(layer)->scrollTo(x, y);
    }
}

} // namespace WebCore
