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

#define LOG_TAG "TiledTexture"
#define LOG_NDEBUG 1

#include "config.h"
#include "TiledTexture.h"

#include "AndroidLog.h"
#include "PaintTileOperation.h"
#include "SkCanvas.h"
#include "SkPicture.h"
#include "TilesManager.h"

#include <wtf/CurrentTime.h>

namespace WebCore {

TiledTexture::~TiledTexture()
{
#ifdef DEBUG_COUNT
    ClassTracker::instance()->decrement("TiledTexture");
#endif
    removeTiles();
}

bool TiledTexture::isReady()
{
    bool tilesAllReady = true;
    bool tilesVisible = false;
    for (unsigned int i = 0; i < m_tiles.size(); i++) {
        BaseTile* tile = m_tiles[i];
        if (tile->isTileVisible(m_area)) {
            tilesVisible = true;
            if (!tile->isTileReady()) {
                tilesAllReady = false;
                break;
            }
        }
    }
    // For now, if no textures are available, consider ourselves as ready
    // in order to unblock the zooming process.
    // FIXME: have a better system -- maybe keeping the last scale factor
    // able to fully render everything
    ALOGV("TT %p, ready %d, visible %d, texturesRemain %d",
          this, tilesAllReady, tilesVisible,
          TilesManager::instance()->layerTexturesRemain());

    return !TilesManager::instance()->layerTexturesRemain()
            || !tilesVisible || tilesAllReady;
}

void TiledTexture::swapTiles()
{
    int swaps = 0;
    for (unsigned int i = 0; i < m_tiles.size(); i++)
        if (m_tiles[i]->swapTexturesIfNeeded())
            swaps++;
    ALOGV("TT %p swapping, swaps = %d", this, swaps);
}

IntRect TiledTexture::computeTilesArea(const IntRect& contentArea, float scale)
{
    IntRect computedArea;
    IntRect area(contentArea.x() * scale,
                 contentArea.y() * scale,
                 ceilf(contentArea.width() * scale),
                 ceilf(contentArea.height() * scale));

    ALOGV("TT %p prepare, scale %f, area %d x %d", this, scale, area.width(), area.height());

    if (area.width() == 0 && area.height() == 0) {
        computedArea.setWidth(0);
        computedArea.setHeight(0);
        return computedArea;
    }

    int tileWidth = TilesManager::instance()->layerTileWidth();
    int tileHeight = TilesManager::instance()->layerTileHeight();

    computedArea.setX(area.x() / tileWidth);
    computedArea.setY(area.y() / tileHeight);
    float right = (area.x() + area.width()) / (float) tileWidth;
    float bottom = (area.y() + area.height()) / (float) tileHeight;
    computedArea.setWidth(ceilf(right) - computedArea.x());
    computedArea.setHeight(ceilf(bottom) - computedArea.y());
    return computedArea;
}

void TiledTexture::prepareGL(GLWebViewState* state, float scale,
                             const IntRect& prepareArea, TilePainter* painter)
{
    // first, how many tiles do we need
    m_area = computeTilesArea(prepareArea, scale);
    if (m_area.isEmpty())
        return;

    ALOGV("prepare TiledTexture %p with scale %.2f, prepareArea "
          " %d, %d - %d x %d, corresponding to %d, %d x - %d x %d tiles",
          this, scale,
          prepareArea.x(), prepareArea.y(),
          prepareArea.width(), prepareArea.height(),
          m_area.x(), m_area.y(),
          m_area.width(), m_area.height());

    bool goingDown = m_prevTileY < m_area.y();
    m_prevTileY = m_area.y();

    if (scale != m_scale)
        TilesManager::instance()->removeOperationsForFilter(new ScaleFilter(painter, scale));

    m_scale = scale;

    // apply dirty region to affected tiles
    if (!m_dirtyRegion.isEmpty()) {
        for (unsigned int i = 0; i < m_tiles.size(); i++)
            m_tiles[i]->markAsDirty(m_dirtyRegion);
        m_dirtyRegion.setEmpty();
    }

    for (int i = 0; i < m_area.width(); i++) {
        if (goingDown) {
            for (int j = 0; j < m_area.height(); j++)
                prepareTile(m_area.x() + i, m_area.y() + j, painter);
        } else {
            for (int j = m_area.height() - 1; j >= 0; j--)
                prepareTile(m_area.x() + i, m_area.y() + j, painter);
        }
    }
}

void TiledTexture::markAsDirty(const SkRegion& invalRegion)
{
    ALOGV("TT %p markAsDirty, current region empty %d, new empty %d",
          this, m_dirtyRegion.isEmpty(), invalRegion.isEmpty());
    m_dirtyRegion.op(invalRegion, SkRegion::kUnion_Op);
}

void TiledTexture::prepareTile(int x, int y, TilePainter* painter)
{
    BaseTile* tile = getTile(x, y);
    if (!tile) {
        tile = new BaseTile(true);
        m_tiles.append(tile);
    }

    ALOGV("preparing tile %p at %d, %d, painter is %p", tile, x, y, painter);
    tile->setContents(painter, x, y, m_scale);

    // TODO: move below (which is largely the same for layers / tiled page) into
    // prepareGL() function

    if (tile->isDirty() || !tile->frontTexture())
        tile->reserveTexture();

    if (tile->backTexture() && tile->isDirty() && !tile->isRepaintPending()) {
        ALOGV("painting TT %p's tile %d %d for LG %p", this, x, y, painter);
        PaintTileOperation *operation = new PaintTileOperation(tile, painter);
        TilesManager::instance()->scheduleOperation(operation);
    }
}

BaseTile* TiledTexture::getTile(int x, int y)
{
    for (unsigned int i = 0; i <m_tiles.size(); i++) {
        BaseTile* tile = m_tiles[i];
        if (tile->x() == x && tile->y() == y)
            return tile;
    }
    return 0;
}

int TiledTexture::nbTextures(IntRect& area, float scale)
{
    IntRect tileBounds = computeTilesArea(area, scale);
    int numberTextures = tileBounds.width() * tileBounds.height();

    // add the number of dirty tiles in the bounds, as they take up double
    // textures for double buffering
    for (unsigned int i = 0; i <m_tiles.size(); i++) {
        BaseTile* tile = m_tiles[i];
        if (tile->isDirty()
                && tile->x() >= tileBounds.x() && tile->x() <= tileBounds.maxX()
                && tile->y() >= tileBounds.y() && tile->y() <= tileBounds.maxY())
            numberTextures++;
    }
    return numberTextures;
}

bool TiledTexture::drawGL(const IntRect& visibleArea, float opacity,
                          const TransformationMatrix* transform)
{
    m_area = computeTilesArea(visibleArea, m_scale);
    if (m_area.width() == 0 || m_area.height() == 0)
        return false;

    float m_invScale = 1 / m_scale;
    const float tileWidth = TilesManager::layerTileWidth() * m_invScale;
    const float tileHeight = TilesManager::layerTileHeight() * m_invScale;

    int drawn = 0;
    bool askRedraw = false;
    for (unsigned int i = 0; i < m_tiles.size(); i++) {
        BaseTile* tile = m_tiles[i];

        if (tile->isTileVisible(m_area)) {
            askRedraw |= !tile->isTileReady();
            SkRect rect;
            rect.fLeft = tile->x() * tileWidth;
            rect.fTop = tile->y() * tileHeight;
            rect.fRight = rect.fLeft + tileWidth;
            rect.fBottom = rect.fTop + tileHeight;
            ALOGV("tile %p (layer tile: %d) %d,%d at scale %.2f vs %.2f [ready: %d] dirty: %d",
                  tile, tile->isLayerTile(), tile->x(), tile->y(),
                  tile->scale(), m_scale, tile->isTileReady(), tile->isDirty());
            tile->drawGL(opacity, rect, m_scale, transform);
            if (tile->frontTexture())
                drawn++;
        }
    }
    ALOGV("TT %p drew %d tiles, redraw due to notready %d, scale %f",
          this, drawn, askRedraw, m_scale);

    // need to redraw if some visible tile wasn't ready
    return askRedraw;
}

void TiledTexture::removeTiles()
{
    for (unsigned int i = 0; i < m_tiles.size(); i++) {
        delete m_tiles[i];
    }
    m_tiles.clear();
}

void TiledTexture::discardTextures()
{
    ALOGV("TT %p discarding textures", this);
    for (unsigned int i = 0; i < m_tiles.size(); i++)
        m_tiles[i]->discardTextures();
}

bool TiledTexture::owns(BaseTileTexture* texture)
{
    for (unsigned int i = 0; i < m_tiles.size(); i++) {
        BaseTile* tile = m_tiles[i];
        if (tile->frontTexture() == texture)
            return true;
        if (tile->backTexture() == texture)
            return true;
    }
    return false;
}

DualTiledTexture::DualTiledTexture()
{
    m_textureA = new TiledTexture();
    m_textureB = new TiledTexture();
    m_frontTexture = m_textureA;
    m_backTexture = m_textureB;
    m_scale = -1;
    m_futureScale = -1;
    m_zooming = false;
}

DualTiledTexture::~DualTiledTexture()
{
    delete m_textureA;
    delete m_textureB;
}

void DualTiledTexture::prepareGL(GLWebViewState* state, bool allowZoom,
                                 const IntRect& prepareArea, TilePainter* painter)
{
    // If we are zooming, we will use the previously used area, to prevent the
    // frontTexture to try to allocate more tiles than what it has already
    if (!m_zooming)
        m_preZoomPrepareArea = prepareArea;

    float scale = state->scale();
    if (scale > 1 && !allowZoom)
        scale = 1;

    if (m_scale == -1) {
        m_scale = scale;
        m_futureScale = scale;
    }

    if (m_futureScale != scale) {
        m_futureScale = scale;
        m_zoomUpdateTime = WTF::currentTime() + DualTiledTexture::s_zoomUpdateDelay;
        m_zooming = true;
    }

    ALOGV("Prepare DTT %p with scale %.2f, m_scale %.2f, futureScale: %.2f, zooming: %d, f %p, b %p",
          this, scale, m_scale, m_futureScale, m_zooming,
          m_frontTexture, m_backTexture);

    if (m_scale > 0)
        m_frontTexture->prepareGL(state, m_scale, m_preZoomPrepareArea, painter);

    // If we had a scheduled update
    if (m_zooming && m_zoomUpdateTime < WTF::currentTime()) {
        m_backTexture->prepareGL(state, m_futureScale, prepareArea, painter);
        if (m_backTexture->isReady()) {
            m_backTexture->swapTiles();
            swap();
            m_zooming = false;
        }
    }
}

void DualTiledTexture::swap()
{
    m_frontTexture = m_frontTexture == m_textureA ? m_textureB : m_textureA;
    m_backTexture = m_backTexture == m_textureA ? m_textureB : m_textureA;
    m_scale = m_futureScale;
    m_backTexture->discardTextures();
}

bool DualTiledTexture::drawGL(const IntRect& visibleArea, float opacity,
                              const TransformationMatrix* transform)
{
    bool needsRepaint = m_frontTexture->drawGL(visibleArea, opacity, transform);
    needsRepaint |= m_zooming;
    needsRepaint |= (m_scale <= 0);
    return needsRepaint;
}

void DualTiledTexture::markAsDirty(const SkRegion& dirtyArea)
{
    m_backTexture->markAsDirty(dirtyArea);
    m_frontTexture->markAsDirty(dirtyArea);
}

void DualTiledTexture::swapTiles()
{
    m_backTexture->swapTiles();
    m_frontTexture->swapTiles();
}

bool DualTiledTexture::owns(BaseTileTexture* texture)
{
    bool owns = m_textureA->owns(texture);
    owns |= m_textureB->owns(texture);
    return owns;
}

void DualTiledTexture::computeTexturesAmount(TexturesResult* result, LayerAndroid* layer)
{
    // TODO: shouldn't use layer, as this DTT may paint multiple layers
    if (!layer)
        return;

    IntRect unclippedArea = layer->unclippedArea();
    IntRect clippedVisibleArea = layer->visibleArea();

    // get two numbers here:
    // - textures needed for a clipped area
    // - textures needed for an un-clipped area
    TiledTexture* tiledTexture = m_zooming ? m_backTexture : m_frontTexture;
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

} // namespace WebCore
