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

#define LOG_TAG "TileGrid"
#define LOG_NDEBUG 1

#include "config.h"
#include "TileGrid.h"

#include "AndroidLog.h"
#include "DrawQuadData.h"
#include "GLWebViewState.h"
#include "PaintTileOperation.h"
#include "Tile.h"
#include "TileTexture.h"
#include "TilesManager.h"

#include <wtf/CurrentTime.h>

#define EXPANDED_BOUNDS_INFLATE 1
#define EXPANDED_PREFETCH_BOUNDS_Y_INFLATE 1

namespace WebCore {

TileGrid::TileGrid(bool isBaseSurface)
    : m_prevTileY(0)
    , m_scale(1)
    , m_isBaseSurface(isBaseSurface)
{
    m_dirtyRegion.setEmpty();
#ifdef DEBUG_COUNT
    ClassTracker::instance()->increment("TileGrid");
#endif
}

TileGrid::~TileGrid()
{
#ifdef DEBUG_COUNT
    ClassTracker::instance()->decrement("TileGrid");
#endif
    removeTiles();
}

bool TileGrid::isReady()
{
    bool tilesAllReady = true;
    bool tilesVisible = false;
    for (unsigned int i = 0; i < m_tiles.size(); i++) {
        Tile* tile = m_tiles[i];
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
    ALOGV("TG %p, ready %d, visible %d, texturesRemain %d",
          this, tilesAllReady, tilesVisible,
          TilesManager::instance()->layerTexturesRemain());

    return !TilesManager::instance()->layerTexturesRemain()
            || !tilesVisible || tilesAllReady;
}

bool TileGrid::isMissingContent()
{
    for (unsigned int i = 0; i < m_tiles.size(); i++)
        if (m_tiles[i]->isTileVisible(m_area) && !m_tiles[i]->frontTexture())
            return true;
    return false;
}

bool TileGrid::swapTiles()
{
    int swaps = 0;
    for (unsigned int i = 0; i < m_tiles.size(); i++)
        if (m_tiles[i]->swapTexturesIfNeeded())
            swaps++;
    ALOGV("TG %p swapping, swaps = %d", this, swaps);
    return swaps != 0;
}

IntRect TileGrid::computeTilesArea(const IntRect& contentArea, float scale)
{
    IntRect computedArea;
    IntRect area(contentArea.x() * scale,
                 contentArea.y() * scale,
                 ceilf(contentArea.width() * scale),
                 ceilf(contentArea.height() * scale));

    ALOGV("TG prepare, scale %f, area %d x %d", scale, area.width(), area.height());

    if (area.width() == 0 && area.height() == 0) {
        computedArea.setWidth(0);
        computedArea.setHeight(0);
        return computedArea;
    }

    int tileWidth = TilesManager::tileWidth();
    int tileHeight = TilesManager::tileHeight();

    computedArea.setX(area.x() / tileWidth);
    computedArea.setY(area.y() / tileHeight);
    float right = (area.x() + area.width()) / (float) tileWidth;
    float bottom = (area.y() + area.height()) / (float) tileHeight;
    computedArea.setWidth(ceilf(right) - computedArea.x());
    computedArea.setHeight(ceilf(bottom) - computedArea.y());
    return computedArea;
}

void TileGrid::prepareGL(GLWebViewState* state, float scale,
                         const IntRect& prepareArea, const IntRect& fullContentArea,
                         TilePainter* painter, int regionFlags, bool isLowResPrefetch,
                         bool updateWithBlit)
{
    // first, how many tiles do we need
    m_area = computeTilesArea(prepareArea, scale);
    if (m_area.isEmpty())
        return;

    ALOGV("prepare TileGrid %p with scale %.2f, prepareArea "
          " %d, %d - %d x %d, corresponding to %d, %d x - %d x %d tiles",
          this, scale,
          prepareArea.x(), prepareArea.y(),
          prepareArea.width(), prepareArea.height(),
          m_area.x(), m_area.y(),
          m_area.width(), m_area.height());

    bool goingDown = m_prevTileY < m_area.y();
    m_prevTileY = m_area.y();

    TilesManager* tilesManager = TilesManager::instance();
    if (scale != m_scale)
        tilesManager->removeOperationsForFilter(new ScaleFilter(painter, m_scale));

    m_scale = scale;

    // apply dirty region to affected tiles
    if (!m_dirtyRegion.isEmpty()) {
        for (unsigned int i = 0; i < m_tiles.size(); i++)
            m_tiles[i]->markAsDirty(m_dirtyRegion);

        // log inval region for the base surface
        if (m_isBaseSurface && tilesManager->getProfiler()->enabled()) {
            SkRegion::Iterator iterator(m_dirtyRegion);
            while (!iterator.done()) {
                SkIRect r = iterator.rect();
                tilesManager->getProfiler()->nextInval(r, scale);
                iterator.next();
            }
        }
        m_dirtyRegion.setEmpty();
    }

    if (regionFlags & StandardRegion) {
        for (int i = 0; i < m_area.width(); i++) {
            if (goingDown) {
                for (int j = 0; j < m_area.height(); j++)
                    prepareTile(m_area.x() + i, m_area.y() + j,
                                painter, state, isLowResPrefetch, false, updateWithBlit);
            } else {
                for (int j = m_area.height() - 1; j >= 0; j--)
                    prepareTile(m_area.x() + i, m_area.y() + j,
                                painter, state, isLowResPrefetch, false, updateWithBlit);
            }
        }
    }

    if (regionFlags & ExpandedRegion) {
        IntRect fullArea = computeTilesArea(fullContentArea, scale);
        IntRect expandedArea = m_area;

        // on systems reporting highEndGfx=true and useMinimalMemory not set, use expanded bounds
        if (tilesManager->highEndGfx() && !tilesManager->useMinimalMemory())
            expandedArea.inflate(EXPANDED_BOUNDS_INFLATE);

        if (isLowResPrefetch)
            expandedArea.inflateY(EXPANDED_PREFETCH_BOUNDS_Y_INFLATE);

        // clip painting area to content
        expandedArea.intersect(fullArea);

        for (int i = expandedArea.x(); i < expandedArea.maxX(); i++)
            for (int j = expandedArea.y(); j < expandedArea.maxY(); j++)
                if (!m_area.contains(i, j))
                    prepareTile(i, j, painter, state, isLowResPrefetch, true, updateWithBlit);
    }
}

void TileGrid::markAsDirty(const SkRegion& invalRegion)
{
    ALOGV("TG %p markAsDirty, current region empty %d, new empty %d",
          this, m_dirtyRegion.isEmpty(), invalRegion.isEmpty());
    m_dirtyRegion.op(invalRegion, SkRegion::kUnion_Op);
}

void TileGrid::prepareTile(int x, int y, TilePainter* painter,
                           GLWebViewState* state, bool isLowResPrefetch,
                           bool isExpandPrefetch, bool shouldTryUpdateWithBlit)
{
    Tile* tile = getTile(x, y);
    if (!tile) {
        bool isLayerTile = !m_isBaseSurface;
        tile = new Tile(isLayerTile);
        m_tiles.append(tile);
    }

    ALOGV("preparing tile %p at %d, %d, painter is %p", tile, x, y, painter);

    tile->setContents(x, y, m_scale, isExpandPrefetch);

    if (shouldTryUpdateWithBlit && tryBlitFromContents(tile, painter))
        return;

    if (tile->isDirty() || !tile->frontTexture())
        tile->reserveTexture();

    if (tile->backTexture() && tile->isDirty()) {
        TilesManager* tilesManager = TilesManager::instance();

        // if a scheduled repaint is still outstanding, update it with the new painter
        if (tile->isRepaintPending() && tilesManager->tryUpdateOperationWithPainter(tile, painter))
            return;

        ALOGV("painting TG %p's tile %d %d for LG %p, scale %f", this, x, y, painter, m_scale);
        PaintTileOperation *operation = new PaintTileOperation(tile, painter,
                                                               state, isLowResPrefetch);
        tilesManager->scheduleOperation(operation);
    }
}

bool TileGrid::tryBlitFromContents(Tile* tile, TilePainter* painter)
{
    return tile->frontTexture()
           && !tile->frontTexture()->isPureColor()
           && tile->frontTexture()->m_ownTextureId
           && !tile->isRepaintPending()
           && painter->blitFromContents(tile);
}

Tile* TileGrid::getTile(int x, int y)
{
    for (unsigned int i = 0; i <m_tiles.size(); i++) {
        Tile* tile = m_tiles[i];
        if (tile->x() == x && tile->y() == y)
            return tile;
    }
    return 0;
}

unsigned int TileGrid::getImageTextureId()
{
    if (m_tiles.size() == 1) {
        if (m_tiles[0]->frontTexture())
            return m_tiles[0]->frontTexture()->m_ownTextureId;
    }
    return 0;
}

int TileGrid::nbTextures(const IntRect& area, float scale)
{
    IntRect tileBounds = computeTilesArea(area, scale);
    int numberTextures = tileBounds.width() * tileBounds.height();

    // add the number of dirty tiles in the bounds, as they take up double
    // textures for double buffering
    for (unsigned int i = 0; i <m_tiles.size(); i++) {
        Tile* tile = m_tiles[i];
        if (tile->isDirty()
                && tile->x() >= tileBounds.x() && tile->x() <= tileBounds.maxX()
                && tile->y() >= tileBounds.y() && tile->y() <= tileBounds.maxY())
            numberTextures++;
    }
    return numberTextures;
}

void TileGrid::drawGL(const IntRect& visibleContentArea, float opacity,
                      const TransformationMatrix* transform,
                      const Color* background)
{
    m_area = computeTilesArea(visibleContentArea, m_scale);
    if (m_area.width() == 0 || m_area.height() == 0)
        return;

    float invScale = 1.0 / m_scale;
    const float tileWidth = TilesManager::tileWidth() * invScale;
    const float tileHeight = TilesManager::tileHeight() * invScale;

    int drawn = 0;

    SkRegion missingRegion;
    bool semiOpaqueBaseSurface =
        background ? (background->hasAlpha() && background->alpha() > 0) : false;
    if (semiOpaqueBaseSurface) {
        SkIRect totalArea = SkIRect::MakeXYWH(m_area.x(), m_area.y(),
                                              m_area.width(), m_area.height());
        missingRegion = SkRegion(totalArea);
    }

    bool usePointSampling =
        TilesManager::instance()->shader()->usePointSampling(m_scale, transform);

    float minTileX =  visibleContentArea.x() / tileWidth;
    float minTileY =  visibleContentArea.y() / tileWidth;
    float maxTileWidth = visibleContentArea.maxX() / tileWidth;
    float maxTileHeight = visibleContentArea.maxY() / tileWidth;
    ALOGV("minTileX, minTileY, maxTileWidth, maxTileHeight %f, %f, %f %f",
          minTileX, minTileY, maxTileWidth, maxTileHeight);
    for (unsigned int i = 0; i < m_tiles.size(); i++) {
        Tile* tile = m_tiles[i];

        bool tileInView = tile->isTileVisible(m_area);
        if (tileInView) {
            SkRect rect;
            rect.fLeft = tile->x() * tileWidth;
            rect.fTop = tile->y() * tileHeight;
            rect.fRight = rect.fLeft + tileWidth;
            rect.fBottom = rect.fTop + tileHeight;
            ALOGV("tile %p (layer tile: %d) %d,%d at scale %.2f vs %.2f [ready: %d] dirty: %d",
                  tile, tile->isLayerTile(), tile->x(), tile->y(),
                  tile->scale(), m_scale, tile->isTileReady(), tile->isDirty());

            bool forceBaseBlending = background ? background->hasAlpha() : false;

            float left = std::max(minTileX - tile->x(), 0.0f);
            float top = std::max(minTileY - tile->y(), 0.0f);
            float right = std::min(maxTileWidth - tile->x(), 1.0f);
            float bottom = std::min(maxTileHeight - tile->y(), 1.0f);
            if (left > 1.0f || top > 1.0f || right < 0.0f || bottom < 0.0f) {
                ALOGE("Unexpected portion:left, top, right, bottom %f %f %f %f",
                      left, top, right, bottom);
                left = 0.0f;
                top = 0.0f;
                right = 1.0f;
                bottom = 1.0f;
            }
            FloatRect fillPortion(left, top, right - left, bottom - top);

            bool success = tile->drawGL(opacity, rect, m_scale, transform,
                                        forceBaseBlending, usePointSampling, fillPortion);
            if (semiOpaqueBaseSurface && success) {
                // Cut the successful drawn tile area from the missing region.
                missingRegion.op(SkIRect::MakeXYWH(tile->x(), tile->y(), 1, 1),
                                 SkRegion::kDifference_Op);
            }
            if (tile->frontTexture())
                drawn++;
        }

        // log tile information for base, high res tiles
        if (m_isBaseSurface && background)
            TilesManager::instance()->getProfiler()->nextTile(tile, invScale, tileInView);
    }

    // Draw missing Regions with blend turned on
    if (semiOpaqueBaseSurface)
        drawMissingRegion(missingRegion, opacity, background);

    ALOGV("TG %p drew %d tiles, scale %f",
          this, drawn, m_scale);
}

void TileGrid::drawMissingRegion(const SkRegion& region, float opacity,
                                     const Color* background)
{
    SkRegion::Iterator iterator(region);
    const float tileWidth = TilesManager::tileWidth() / m_scale;
    const float tileHeight = TilesManager::tileHeight() / m_scale;
    while (!iterator.done()) {
        SkIRect r = iterator.rect();
        SkRect rect;
        rect.fLeft = r.x() * tileWidth;
        rect.fTop =  r.y() * tileHeight;
        rect.fRight = rect.fLeft + tileWidth * r.width();
        rect.fBottom = rect.fTop + tileHeight * r.height();
        ALOGV("draw tile x y, %d %d (%d %d) opacity %f", r.x(), r.y(),
              r.width(), r.height(), opacity);
        // Skia is using pre-multiplied color.
        Color postAlpha = Color(background->red() * background->alpha() / 255,
                                background->green() * background->alpha() / 255,
                                background->blue() * background->alpha() / 255,
                                background->alpha() );

        PureColorQuadData backGroundData(postAlpha, BaseQuad, 0, &rect, opacity);
        TilesManager::instance()->shader()->drawQuad(&backGroundData);
        iterator.next();
    }
}

void TileGrid::removeTiles()
{
    for (unsigned int i = 0; i < m_tiles.size(); i++) {
        delete m_tiles[i];
    }
    m_tiles.clear();
}

void TileGrid::discardTextures()
{
    ALOGV("TG %p discarding textures", this);
    for (unsigned int i = 0; i < m_tiles.size(); i++)
        m_tiles[i]->discardTextures();
}

} // namespace WebCore
