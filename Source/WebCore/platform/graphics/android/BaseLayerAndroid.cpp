/*
 * Copyright 2010, The Android Open Source Project
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

#define LOG_TAG "BaseLayerAndroid"
#define LOG_NDEBUG 1

#include "config.h"
#include "BaseLayerAndroid.h"

#include "AndroidLog.h"
#include "ClassTracker.h"
#include "GLUtils.h"
#include "LayerGroup.h"
#include "ShaderProgram.h"
#include "SkCanvas.h"
#include "TilesManager.h"
#include <GLES2/gl2.h>

// TODO: dynamically determine based on DPI
#define PREFETCH_SCALE_MODIFIER 0.3
#define PREFETCH_OPACITY 1
#define PREFETCH_X_DIST 0
#define PREFETCH_Y_DIST 1

namespace WebCore {

using namespace android;

BaseLayerAndroid::BaseLayerAndroid()
#if USE(ACCELERATED_COMPOSITING)
    : m_color(Color::white)
    , m_content(0)
    , m_scrollState(NotScrolling)
#endif
{
#ifdef DEBUG_COUNT
    ClassTracker::instance()->increment("BaseLayerAndroid");
#endif
}

BaseLayerAndroid::~BaseLayerAndroid()
{
    SkSafeUnref(m_content);
#ifdef DEBUG_COUNT
    ClassTracker::instance()->decrement("BaseLayerAndroid");
#endif
}

void BaseLayerAndroid::setContent(LayerContent* content)
{
    SkSafeRef(content);
    SkSafeUnref(m_content);
    m_content = content;
    // FIXME: We cannot set the size of the base layer because it will screw up
    // the matrix used.  We need to fix matrix computation for the base layer
    // and then we can set the size.
    // setSize(src.width(), src.height());
}

bool BaseLayerAndroid::drawCanvas(SkCanvas* canvas)
{
    android::Mutex::Autolock lock(m_drawLock);
    if (m_content && !m_content->isEmpty())
        m_content->draw(canvas);
    return true;
}

#if USE(ACCELERATED_COMPOSITING)

void BaseLayerAndroid::prefetchBasePicture(const SkRect& viewport, float currentScale,
                                           TiledPage* prefetchTiledPage, bool draw)
{
    SkIRect bounds;
    float prefetchScale = currentScale * PREFETCH_SCALE_MODIFIER;

    float invTileWidth = (prefetchScale)
        / TilesManager::instance()->tileWidth();
    float invTileHeight = (prefetchScale)
        / TilesManager::instance()->tileHeight();
    bool goingDown = m_state->goingDown();
    bool goingLeft = m_state->goingLeft();


    ALOGV("fetch rect %f %f %f %f, scale %f",
          viewport.fLeft,
          viewport.fTop,
          viewport.fRight,
          viewport.fBottom,
          currentScale);

    bounds.fLeft = static_cast<int>(floorf(viewport.fLeft * invTileWidth)) - PREFETCH_X_DIST;
    bounds.fTop = static_cast<int>(floorf(viewport.fTop * invTileHeight)) - PREFETCH_Y_DIST;
    bounds.fRight = static_cast<int>(ceilf(viewport.fRight * invTileWidth)) + PREFETCH_X_DIST;
    bounds.fBottom = static_cast<int>(ceilf(viewport.fBottom * invTileHeight)) + PREFETCH_Y_DIST;

    ALOGV("prefetch rect %d %d %d %d, scale %f, preparing page %p",
          bounds.fLeft, bounds.fTop,
          bounds.fRight, bounds.fBottom,
          prefetchScale,
          prefetchTiledPage);

    prefetchTiledPage->setScale(prefetchScale);
    prefetchTiledPage->updateTileDirtiness();
    prefetchTiledPage->prepare(goingDown, goingLeft, bounds,
                               TiledPage::ExpandedBounds);
    prefetchTiledPage->swapBuffersIfReady(bounds,
                                          prefetchScale);
    if (draw)
        prefetchTiledPage->prepareForDrawGL(PREFETCH_OPACITY, bounds);
}

bool BaseLayerAndroid::isReady()
{
    ZoomManager* zoomManager = m_state->zoomManager();
    if (ZoomManager::kNoScaleRequest != zoomManager->scaleRequestState()) {
        ALOGV("base layer not ready, still zooming");
        return false; // still zooming
    }

    if (!m_state->frontPage()->isReady(m_state->preZoomBounds())) {
        ALOGV("base layer not ready, front page not done painting");
        return false;
    }

    return true;
}

void BaseLayerAndroid::swapTiles()
{
    m_state->frontPage()->swapBuffersIfReady(m_state->preZoomBounds(),
                                             m_state->zoomManager()->currentScale());

    m_state->backPage()->swapBuffersIfReady(m_state->preZoomBounds(),
                                            m_state->zoomManager()->currentScale());
}

void BaseLayerAndroid::setIsPainting()
{
    ALOGV("BLA %p setIsPainting, dirty %d", this, isDirty());
    m_state->invalRegion(m_dirtyRegion);
    m_dirtyRegion.setEmpty();
}

void BaseLayerAndroid::mergeInvalsInto(BaseLayerAndroid* replacementLayer)
{
    replacementLayer->markAsDirty(m_dirtyRegion);
}

void BaseLayerAndroid::prepareGL(const SkRect& viewport, float scale, double currentTime)
{
    ALOGV("prepareGL BLA %p, m_state %p", this, m_state);

    ZoomManager* zoomManager = m_state->zoomManager();

    bool goingDown = m_state->goingDown();
    bool goingLeft = m_state->goingLeft();

    const SkIRect& viewportTileBounds = m_state->viewportTileBounds();
    ALOGV("drawBasePicture, TX: %d, TY: %d scale %.2f", viewportTileBounds.fLeft,
          viewportTileBounds.fTop, scale);

    // Query the resulting state from the zoom manager
    bool prepareNextTiledPage = zoomManager->needPrepareNextTiledPage();

    // Display the current page
    TiledPage* tiledPage = m_state->frontPage();
    TiledPage* nextTiledPage = m_state->backPage();
    tiledPage->setScale(zoomManager->currentScale());

    // Let's prepare the page if needed so that it will start painting
    if (prepareNextTiledPage) {
        nextTiledPage->setScale(scale);
        m_state->setFutureViewport(viewportTileBounds);

        nextTiledPage->updateTileDirtiness();

        nextTiledPage->prepare(goingDown, goingLeft, viewportTileBounds,
                               TiledPage::VisibleBounds);
        // Cancel pending paints for the foreground page
        TilesManager::instance()->removePaintOperationsForPage(tiledPage, false);
    }

    // If we fired a request, let's check if it's ready to use
    if (zoomManager->didFireRequest()) {
        if (nextTiledPage->swapBuffersIfReady(viewportTileBounds,
                                              zoomManager->futureScale()))
            zoomManager->setReceivedRequest(); // transition to received request state
    }

    float transparency = 1;
    bool doZoomPageSwap = false;

    // If the page is ready, display it. We do a short transition between
    // the two pages (current one and future one with the new scale factor)
    if (zoomManager->didReceivedRequest()) {
        float nextTiledPageTransparency = 1;
        m_state->resetFrameworkInval();
        zoomManager->processTransition(currentTime, scale, &doZoomPageSwap,
                                       &nextTiledPageTransparency, &transparency);
        nextTiledPage->prepareForDrawGL(nextTiledPageTransparency, viewportTileBounds);
    }

    const SkIRect& preZoomBounds = m_state->preZoomBounds();

    bool zooming = ZoomManager::kNoScaleRequest != zoomManager->scaleRequestState();

    if (doZoomPageSwap) {
        zoomManager->setCurrentScale(scale);
        m_state->swapPages();
    }

    tiledPage->updateTileDirtiness();

    // paint what's needed unless we're zooming, since the new tiles won't
    // be relevant soon anyway
    if (!zooming)
        tiledPage->prepare(goingDown, goingLeft, preZoomBounds,
                           TiledPage::ExpandedBounds);

    ALOGV("scrollState %d, zooming %d", m_scrollState, zooming);

    // prefetch in the nextTiledPage if unused by zooming (even if not scrolling
    // since we want the tiles to be ready before they're needed)
    bool usePrefetchPage = !zooming;
    nextTiledPage->setIsPrefetchPage(usePrefetchPage);
    if (usePrefetchPage) {
        // if the non-prefetch page isn't missing tiles, don't bother drawing
        // prefetch page
        bool drawPrefetchPage = tiledPage->hasMissingContent(preZoomBounds);
        prefetchBasePicture(viewport, scale, nextTiledPage, drawPrefetchPage);
    }

    tiledPage->prepareForDrawGL(transparency, preZoomBounds);
}

void BaseLayerAndroid::drawBasePictureInGL()
{
    m_state->backPage()->drawGL();
    m_state->frontPage()->drawGL();
}

void BaseLayerAndroid::updateLayerPositions(const SkRect& visibleRect)
{
    LayerAndroid* compositedRoot = static_cast<LayerAndroid*>(getChild(0));
    if (!compositedRoot)
        return;
    TransformationMatrix ident;
    compositedRoot->updateLayerPositions(visibleRect);
    FloatRect clip(0, 0, content()->width(), content()->height());

    // Note that this function may be called (and should still work) with no m_state in SW mode
    // TODO: is this the best thing to do in software rendering
    float scale = m_state ? m_state->scale() : 1.0f;
    compositedRoot->updateGLPositionsAndScale(ident, clip, 1, scale);

#ifdef DEBUG
    compositedRoot->showLayer(0);
    ALOGV("We have %d layers, %d textured",
          compositedRoot->nbLayers(),
          compositedRoot->nbTexturedLayers());
#endif
}

#endif // USE(ACCELERATED_COMPOSITING)

void BaseLayerAndroid::drawGL(float scale)
{
    ALOGV("drawGL BLA %p", this);

    // TODO: consider moving drawBackground outside of prepare (into tree manager)
    m_state->drawBackground(m_color);
    drawBasePictureInGL();
    m_state->glExtras()->drawGL(0);
}

} // namespace WebCore
