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
#include "GLWebViewState.h"
#include "LayerAndroid.h"

#define LOW_RES_PREFETCH_SCALE_MODIFIER 0.3f

namespace WebCore {

SurfaceBacking::SurfaceBacking(bool isBaseSurface)
{
    m_frontTexture = new TileGrid(isBaseSurface);
    m_backTexture = new TileGrid(isBaseSurface);
    m_lowResTexture = new TileGrid(isBaseSurface);
    m_scale = -1;
    m_futureScale = -1;
    m_zooming = false;
}

SurfaceBacking::~SurfaceBacking()
{
    delete m_frontTexture;
    delete m_backTexture;
    delete m_lowResTexture;
}

void SurfaceBacking::prepareGL(GLWebViewState* state, bool allowZoom,
                               const IntRect& prepareArea, const IntRect& unclippedArea,
                               TilePainter* painter, bool aggressiveRendering)
{
    float scale = state->scale();
    if (scale > 1 && !allowZoom)
        scale = 1;

    if (m_scale == -1) {
        m_scale = scale;
        m_futureScale = scale;
    }

    if (m_futureScale != scale) {
        m_futureScale = scale;
        m_zoomUpdateTime = WTF::currentTime() + SurfaceBacking::s_zoomUpdateDelay;
        m_zooming = true;

        // release back texture's TileTextures, so they can be reused immediately
        m_backTexture->discardTextures();
    }

    bool useExpandPrefetch = aggressiveRendering;
    ALOGV("Prepare SurfBack %p, scale %.2f, m_scale %.2f, futScale: %.2f, zooming: %d, f %p, b %p",
          this, scale, m_scale, m_futureScale, m_zooming,
          m_frontTexture, m_backTexture);

    if (m_zooming && (m_zoomUpdateTime < WTF::currentTime())) {
        m_backTexture->prepareGL(state, m_futureScale,
                                 prepareArea, unclippedArea, painter, false, false);
        if (m_backTexture->isReady()) {
            // zooming completed, swap the textures and new front tiles
            swapTileGrids();

            m_frontTexture->swapTiles();
            m_backTexture->discardTextures();
            m_lowResTexture->discardTextures();

            m_scale = m_futureScale;
            m_zooming = false;
        }
    }

    if (!m_zooming) {
        m_frontTexture->prepareGL(state, m_scale,
                                  prepareArea, unclippedArea, painter, false, useExpandPrefetch);
        if (aggressiveRendering) {
            // prepare low res content
            float lowResPrefetchScale = m_scale * LOW_RES_PREFETCH_SCALE_MODIFIER;
            m_lowResTexture->prepareGL(state, lowResPrefetchScale,
                                       prepareArea, unclippedArea, painter,
                                       true, useExpandPrefetch);
            m_lowResTexture->swapTiles();
        }
    }
}

void SurfaceBacking::drawGL(const IntRect& visibleArea, float opacity,
                            const TransformationMatrix* transform,
                            bool aggressiveRendering, const Color* background)
{
    // draw low res prefetch page if zooming or front texture missing content
    if (aggressiveRendering && isMissingContent())
        m_lowResTexture->drawGL(visibleArea, opacity, transform);

    m_frontTexture->drawGL(visibleArea, opacity, transform, background);
}

void SurfaceBacking::markAsDirty(const SkRegion& dirtyArea)
{
    m_backTexture->markAsDirty(dirtyArea);
    m_frontTexture->markAsDirty(dirtyArea);
    m_lowResTexture->markAsDirty(dirtyArea);
}

void SurfaceBacking::swapTiles()
{
    m_backTexture->swapTiles();
    m_frontTexture->swapTiles();
    m_lowResTexture->swapTiles();
}

void SurfaceBacking::computeTexturesAmount(TexturesResult* result, LayerAndroid* layer)
{
    // TODO: shouldn't use layer, as this SB may paint multiple layers
    if (!layer)
        return;

    IntRect unclippedArea = layer->unclippedArea();
    IntRect clippedVisibleArea = layer->visibleArea();

    // get two numbers here:
    // - textures needed for a clipped area
    // - textures needed for an un-clipped area
    TileGrid* tiledTexture = m_zooming ? m_backTexture : m_frontTexture;
    int nbTexturesUnclipped = tiledTexture->nbTextures(unclippedArea, m_scale);
    int nbTexturesClipped = tiledTexture->nbTextures(clippedVisibleArea, m_scale);

    // Set kFixedLayers level
    if (layer->isPositionFixed())
        result->fixed += nbTexturesClipped;

    // Set kScrollableAndFixedLayers level
    if (layer->contentIsScrollable()
        || layer->isPositionFixed())
        result->scrollable += nbTexturesClipped;

    // Set kClippedTextures level
    result->clipped += nbTexturesClipped;

    // Set kAllTextures level
    if (layer->contentIsScrollable())
        result->full += nbTexturesClipped;
    else
        result->full += nbTexturesUnclipped;
}

void SurfaceBacking::swapTileGrids()
{
    TileGrid* temp = m_frontTexture;
    m_frontTexture = m_backTexture;
    m_backTexture = temp;
}

} // namespace WebCore
