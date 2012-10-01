/*
 * Copyright 2011, The Android Open Source Project
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

#define LOG_TAG "SurfaceBacking"
#define LOG_NDEBUG 1

#include "config.h"
#include "SurfaceBacking.h"

#include "AndroidLog.h"
#include "Color.h"
#include "ClassTracker.h"
#include "GLWebViewState.h"
#include "LayerAndroid.h"

#define LOW_RES_PREFETCH_SCALE_MODIFIER 0.3f

namespace WebCore {

SurfaceBacking::SurfaceBacking(bool isBaseSurface)
    : m_frontTileGrid(new TileGrid(isBaseSurface))
    , m_backTileGrid(new TileGrid(isBaseSurface))
    , m_lowResTileGrid(new TileGrid(isBaseSurface))
    , m_scale(-1)
    , m_futureScale(-1)
    , m_zooming(false)
    , m_maxZoomScale(1)

{
#ifdef DEBUG_COUNT
    ClassTracker::instance()->increment("SurfaceBacking");
#endif
}

SurfaceBacking::~SurfaceBacking()
{
    delete m_frontTileGrid;
    delete m_backTileGrid;
    delete m_lowResTileGrid;
#ifdef DEBUG_COUNT
    ClassTracker::instance()->decrement("SurfaceBacking");
#endif
}

void SurfaceBacking::prepareGL(GLWebViewState* state, float maxZoomScale,
                               const IntRect& prepareArea, const IntRect& fullContentArea,
                               TilePainter* painter, bool aggressiveRendering,
                               bool updateWithBlit)
{
    // If the surface backing has ever zoomed beyond 1.0 scale, it's always
    // allowed to (so repaints aren't necessary when allowZoom toggles). If not,
    // and allowZoom is false, don't allow scale greater than 1.0
    m_maxZoomScale = std::max(m_maxZoomScale, maxZoomScale);
    float scale = state->scale();
    bool scaleOverridden = false;
    if (scale > m_maxZoomScale) {
        scale = m_maxZoomScale;
        scaleOverridden = true;
    }

    if (m_scale == -1) {
        m_scale = scale;
        m_futureScale = scale;
    }

    if (m_futureScale != scale) {
        m_futureScale = scale;
        if (scaleOverridden)
            m_zoomUpdateTime = 0; // start rendering immediately
        else
            m_zoomUpdateTime = WTF::currentTime() + SurfaceBacking::s_zoomUpdateDelay;
        m_zooming = true;

        // release back TileGrid's TileTextures, so they can be reused immediately
        m_backTileGrid->discardTextures();
    }

    int prepareRegionFlags = TileGrid::StandardRegion;
    if (aggressiveRendering)
        prepareRegionFlags |= TileGrid::ExpandedRegion;

    ALOGV("Prepare SurfBack %p, scale %.2f, m_scale %.2f, futScale: %.2f, zooming: %d, f %p, b %p",
          this, scale, m_scale, m_futureScale, m_zooming,
          m_frontTileGrid, m_backTileGrid);

    if (m_zooming && (m_zoomUpdateTime < WTF::currentTime())) {
        // prepare the visible portions of the back tile grid at the futureScale
        m_backTileGrid->prepareGL(state, m_futureScale,
                                  prepareArea, fullContentArea, painter,
                                  TileGrid::StandardRegion, false);

        if (m_backTileGrid->isReady()) {
            // zooming completed, swap the TileGrids and new front tiles
            swapTileGrids();

            m_frontTileGrid->swapTiles();
            m_backTileGrid->discardTextures();
            m_lowResTileGrid->discardTextures();

            m_scale = m_futureScale;
            m_zooming = false;

            // clear the StandardRegion flag, to prevent preparing it twice -
            // the new frontTileGrid has already had its StandardRegion prepared
            prepareRegionFlags &= ~TileGrid::StandardRegion;
        }
    }

    if (!m_zooming) {
        if (prepareRegionFlags) {
            // if the front grid hasn't already prepared, or needs to prepare
            // expanded bounds do so now
            m_frontTileGrid->prepareGL(state, m_scale,
                                       prepareArea, fullContentArea, painter,
                                       prepareRegionFlags, false, updateWithBlit);
        }
        if (aggressiveRendering) {
            // prepare low res content
            float lowResPrefetchScale = m_scale * LOW_RES_PREFETCH_SCALE_MODIFIER;
            m_lowResTileGrid->prepareGL(state, lowResPrefetchScale,
                                       prepareArea, fullContentArea, painter,
                                       TileGrid::StandardRegion | TileGrid::ExpandedRegion, true);
            m_lowResTileGrid->swapTiles();
        }
    }
}

void SurfaceBacking::drawGL(const IntRect& visibleContentArea, float opacity,
                            const TransformationMatrix* transform,
                            bool aggressiveRendering, const Color* background)
{
    // draw low res prefetch page if zooming or front texture missing content
    if (aggressiveRendering && isMissingContent())
        m_lowResTileGrid->drawGL(visibleContentArea, opacity, transform);

    m_frontTileGrid->drawGL(visibleContentArea, opacity, transform, background);
}

void SurfaceBacking::markAsDirty(const SkRegion& dirtyArea)
{
    m_backTileGrid->markAsDirty(dirtyArea);
    m_frontTileGrid->markAsDirty(dirtyArea);
    m_lowResTileGrid->markAsDirty(dirtyArea);
}

bool SurfaceBacking::swapTiles()
{
    bool swap = m_backTileGrid->swapTiles();
    swap |= m_frontTileGrid->swapTiles();
    swap |= m_lowResTileGrid->swapTiles();
    return swap;
}

void SurfaceBacking::computeTexturesAmount(TexturesResult* result,
                                           const IntRect& visibleContentArea,
                                           const IntRect& fullContentArea,
                                           LayerAndroid* layer)
{

    // get two numbers here:
    // - textures needed for a clipped area
    // - textures needed for an un-clipped area
    TileGrid* tileGrid = m_zooming ? m_backTileGrid : m_frontTileGrid;
    int nbTexturesUnclipped = tileGrid->nbTextures(fullContentArea, m_futureScale);
    int nbTexturesClipped = tileGrid->nbTextures(visibleContentArea, m_futureScale);

    if (layer) {
        // TODO: should handle multi-layer case better

        // Set kFixedLayers level
        if (layer->isPositionFixed())
            result->fixed += nbTexturesClipped;

        // Set kScrollableAndFixedLayers level
        if (layer->contentIsScrollable()
            || layer->isPositionFixed())
            result->scrollable += nbTexturesClipped;
    }

    // Set kClippedTextures level
    result->clipped += nbTexturesClipped;

    // Set kAllTextures level
    if (layer && layer->contentIsScrollable())
        result->full += nbTexturesClipped;
    else
        result->full += nbTexturesUnclipped;
}

void SurfaceBacking::swapTileGrids()
{
    TileGrid* temp = m_frontTileGrid;
    m_frontTileGrid = m_backTileGrid;
    m_backTileGrid = temp;
}

} // namespace WebCore
