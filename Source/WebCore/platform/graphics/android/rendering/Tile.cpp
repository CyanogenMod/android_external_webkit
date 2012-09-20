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

#define LOG_TAG "Tile"
#define LOG_NDEBUG 1

#include "config.h"
#include "Tile.h"

#if USE(ACCELERATED_COMPOSITING)

#include "AndroidLog.h"
#include "GLUtils.h"
#include "BaseRenderer.h"
#include "TextureInfo.h"
#include "TileTexture.h"
#include "TilesManager.h"

// If the dirty portion of a tile exceeds this ratio, fully repaint.
// Lower values give fewer partial repaints, thus fewer front-to-back
// texture copies (cost will vary by device). It's a tradeoff between
// the rasterization cost and the FBO texture recopy cost when using
// GPU for the transfer queue.
#define MAX_INVAL_AREA 0.6

namespace WebCore {

Tile::Tile(bool isLayerTile)
    : m_x(-1)
    , m_y(-1)
    , m_frontTexture(0)
    , m_backTexture(0)
    , m_scale(1)
    , m_dirty(true)
    , m_repaintsPending(0)
    , m_fullRepaint(true)
    , m_isLayerTile(isLayerTile)
    , m_drawCount(0)
    , m_state(Unpainted)
{
#ifdef DEBUG_COUNT
    ClassTracker::instance()->increment("Tile");
#endif
}

Tile::~Tile()
{
    if (m_backTexture)
        m_backTexture->release(this);
    if (m_frontTexture)
        m_frontTexture->release(this);

#ifdef DEBUG_COUNT
    ClassTracker::instance()->decrement("Tile");
#endif
}

// All the following functions must be called from the main GL thread.

void Tile::setContents(int x, int y, float scale, bool isExpandedPrefetchTile)
{
    // TODO: investigate whether below check/discard is necessary
    if ((m_x != x)
        || (m_y != y)
        || (m_scale != scale)) {
        // neither texture is relevant
        discardTextures();
    }

    android::AutoMutex lock(m_atomicSync);
    m_x = x;
    m_y = y;
    m_scale = scale;
    m_drawCount = TilesManager::instance()->getDrawGLCount();
    if (isExpandedPrefetchTile)
        m_drawCount--; // deprioritize expanded painting region
}

void Tile::reserveTexture()
{
    TileTexture* texture = TilesManager::instance()->getAvailableTexture(this);

    android::AutoMutex lock(m_atomicSync);
    if (texture && m_backTexture != texture) {
        ALOGV("tile %p reserving texture %p, back was %p (front %p)",
              this, texture, m_backTexture, m_frontTexture);
        m_state = Unpainted;
        m_backTexture = texture;
    }

    if (m_state == UpToDate) {
        ALOGV("moving tile %p to unpainted, since it reserved while up to date", this);
        m_dirty = true;
        m_state = Unpainted;
    }
}

bool Tile::removeTexture(TileTexture* texture)
{
    ALOGV("%p removeTexture %p, back %p front %p",
          this, texture, m_backTexture, m_frontTexture);
    // We update atomically, so paintBitmap() can see the correct value
    android::AutoMutex lock(m_atomicSync);
    if (m_frontTexture == texture) {
        if (m_state == UpToDate) {
            ALOGV("front texture removed, state was UpToDate, now becoming unpainted, bt is %p", m_backTexture);
            m_state = Unpainted;
        }

        m_frontTexture = 0;
    }
    if (m_backTexture == texture) {
        m_state = Unpainted;
        m_backTexture = 0;
    }

    // mark dirty regardless of which texture was taken - the back texture may
    // have been ready to swap
    m_dirty = true;

    return true;
}

void Tile::markAsDirty()
{
    android::AutoMutex lock(m_atomicSync);
    m_dirtyArea.setEmpty(); // empty dirty rect prevents fast blit path
    markAsDirtyInternal();
}

void Tile::markAsDirty(const SkRegion& dirtyArea)
{
    if (dirtyArea.isEmpty())
        return;
    android::AutoMutex lock(m_atomicSync);
    m_dirtyArea.op(dirtyArea, SkRegion::kUnion_Op);

    // Check if we actually intersect with the area
    bool intersect = false;
    SkRegion::Iterator cliperator(dirtyArea);
    SkRect realTileRect;
    SkRect dirtyRect;
    while (!cliperator.done()) {
        dirtyRect.set(cliperator.rect());
        if (intersectWithRect(m_x, m_y, TilesManager::tileWidth(), TilesManager::tileHeight(),
                              m_scale, dirtyRect, realTileRect)) {
            intersect = true;
            break;
        }
        cliperator.next();
    }

    if (!intersect)
        return;

    markAsDirtyInternal();
}

void Tile::markAsDirtyInternal()
{
    // NOTE: callers must hold lock on m_atomicSync

    m_dirty = true;
    if (m_state == UpToDate) {
        // We only mark a tile as unpainted in 'markAsDirty' if its status is
        // UpToDate: marking dirty means we need to repaint, but don't stop the
        // current paint
        m_state = Unpainted;
    } else if (m_state != Unpainted) {
        // TODO: fix it so that they can paint while deferring the markAsDirty
        // call (or block updates)
        ALOGV("Warning: tried to mark tile %p at %d, %d islayertile %d as dirty, state %d",
              this, m_x, m_y, isLayerTile(), m_state);

        // prefetch tiles can be marked dirty while in the process of painting,
        // due to not using an update lock. force them to fail validate step.
        m_state = Unpainted;
    }
}

bool Tile::isDirty()
{
    android::AutoMutex lock(m_atomicSync);
    return m_dirty;
}

bool Tile::isRepaintPending()
{
    android::AutoMutex lock(m_atomicSync);
    return m_repaintsPending != 0;
}

void Tile::setRepaintPending(bool pending)
{
    android::AutoMutex lock(m_atomicSync);
    m_repaintsPending += pending ? 1 : -1;
}

bool Tile::drawGL(float opacity, const SkRect& rect, float scale,
                  const TransformationMatrix* transform,
                  bool forceBlending, bool usePointSampling,
                  const FloatRect& fillPortion)
{
    if (m_x < 0 || m_y < 0 || m_scale != scale)
        return false;

    // No need to mutex protect reads of m_backTexture as it is only written to by
    // the consumer thread.
    if (!m_frontTexture)
        return false;

    if (fillPortion.maxX() < 1.0f || fillPortion.maxY() < 1.0f
        || fillPortion.x() > 0.0f || fillPortion.y() > 0.0f)
        ALOGV("drawing tile %p (%d, %d with fill portions %f %f->%f, %f",
              this, m_x, m_y, fillPortion.x(), fillPortion.y(),
              fillPortion.maxX(), fillPortion.maxY());

    m_frontTexture->drawGL(isLayerTile(), rect, opacity, transform,
                           forceBlending, usePointSampling, fillPortion);
    return true;
}

bool Tile::isTileReady()
{
    // Return true if the tile's most recently drawn texture is up to date
    android::AutoMutex lock(m_atomicSync);
    TileTexture * texture = (m_state == ReadyToSwap) ? m_backTexture : m_frontTexture;

    if (!texture)
        return false;

    if (texture->owner() != this)
        return false;

    if (m_dirty)
        return false;

    if (m_state != ReadyToSwap && m_state != UpToDate)
        return false;

    return true;
}

bool Tile::intersectWithRect(int x, int y, int tileWidth, int tileHeight,
                             float scale, const SkRect& dirtyRect,
                             SkRect& realTileRect)
{
    // compute the rect to corresponds to pixels
    realTileRect.fLeft = x * tileWidth;
    realTileRect.fTop = y * tileHeight;
    realTileRect.fRight = realTileRect.fLeft + tileWidth;
    realTileRect.fBottom = realTileRect.fTop + tileHeight;

    // scale the dirtyRect for intersect computation.
    SkRect realDirtyRect = SkRect::MakeWH(dirtyRect.width() * scale,
                                          dirtyRect.height() * scale);
    realDirtyRect.offset(dirtyRect.fLeft * scale, dirtyRect.fTop * scale);

    if (!realTileRect.intersect(realDirtyRect))
        return false;
    return true;
}

bool Tile::isTileVisible(const IntRect& viewTileBounds)
{
    return (m_x >= viewTileBounds.x()
            && m_x < viewTileBounds.x() + viewTileBounds.width()
            && m_y >= viewTileBounds.y()
            && m_y < viewTileBounds.y() + viewTileBounds.height());
}

// This is called from the texture generation thread
void Tile::paintBitmap(TilePainter* painter, BaseRenderer* renderer)
{
    // We acquire the values below atomically. This ensures that we are reading
    // values correctly across cores. Further, once we have these values they
    // can be updated by other threads without consequence.
    m_atomicSync.lock();
    bool dirty = m_dirty;
    TileTexture* texture = m_backTexture;
    SkRegion dirtyArea = m_dirtyArea;
    float scale = m_scale;
    const int x = m_x;
    const int y = m_y;

    if (!dirty || !texture) {
        m_atomicSync.unlock();
        return;
    }
    if (m_state != Unpainted) {
        ALOGV("Warning: started painting tile %p, but was at state %d, ft %p bt %p",
              this, m_state, m_frontTexture, m_backTexture);
    }
    m_state = PaintingStarted;
    TextureInfo* textureInfo = texture->getTextureInfo();
    m_atomicSync.unlock();

    // at this point we can safely check the ownership (if the texture got
    // transferred to another Tile under us)
    if (texture->owner() != this) {
        return;
    }

    // setup the common renderInfo fields;
    TileRenderInfo renderInfo;
    renderInfo.x = x;
    renderInfo.y = y;
    renderInfo.scale = scale;
    renderInfo.tileSize = texture->getSize();
    renderInfo.tilePainter = painter;
    renderInfo.baseTile = this;
    renderInfo.textureInfo = textureInfo;

    const float tileWidth = renderInfo.tileSize.width();
    const float tileHeight = renderInfo.tileSize.height();

    renderer->renderTiledContent(renderInfo);

    m_atomicSync.lock();

    if (texture == m_backTexture) {
        // set the fullrepaint flags
        m_fullRepaint = false;

        // The various checks to see if we are still dirty...

        m_dirty = false;

        if (m_scale != scale)
            m_dirty = true;

        m_dirtyArea.setEmpty();

        ALOGV("painted tile %p (%d, %d), texture %p, dirty=%d", this, x, y, texture, m_dirty);

        validatePaint();
    } else {
        ALOGV("tile %p no longer owns texture %p, m_state %d. ft %p bt %p",
              this, texture, m_state, m_frontTexture, m_backTexture);
    }

    m_atomicSync.unlock();
}

void Tile::discardTextures() {
    android::AutoMutex lock(m_atomicSync);
    ALOGV("%p discarding bt %p, ft %p",
          this, m_backTexture, m_frontTexture);
    if (m_frontTexture) {
        m_frontTexture->release(this);
        m_frontTexture = 0;
    }
    if (m_backTexture) {
        m_backTexture->release(this);
        m_backTexture = 0;
    }
    m_dirtyArea.setEmpty();
    m_fullRepaint = true;

    m_dirty = true;
    m_state = Unpainted;
}

void Tile::discardBackTexture() {
    android::AutoMutex lock(m_atomicSync);
    if (m_backTexture) {
        m_backTexture->release(this);
        m_backTexture = 0;
    }
    m_state = Unpainted;
    m_dirty = true;
}

bool Tile::swapTexturesIfNeeded() {
    android::AutoMutex lock(m_atomicSync);
    if (m_state == ReadyToSwap) {
        // discard old texture and swap the new one in its place
        if (m_frontTexture)
            m_frontTexture->release(this);

        m_frontTexture = m_backTexture;
        m_backTexture = 0;
        m_state = UpToDate;
        ALOGV("display texture for %p at %d, %d front is now %p, back is %p",
              this, m_x, m_y, m_frontTexture, m_backTexture);

        return true;
    }
    return false;
}

void Tile::backTextureTransfer() {
    android::AutoMutex lock(m_atomicSync);
    if (m_state == PaintingStarted)
        m_state = TransferredUnvalidated;
    else if (m_state == ValidatedUntransferred)
        m_state = ReadyToSwap;
    else {
        // shouldn't have transferred a tile in any other state, log
        ALOGV("Note: transferred tile %p at %d %d, state wasn't paintingstarted or validated: %d",
              this, m_x, m_y, m_state);
    }
}

void Tile::backTextureTransferFail() {
    // transfer failed for some reason, mark dirty so it will (repaint and) be
    // retransferred.
    android::AutoMutex lock(m_atomicSync);
    m_state = Unpainted;
    m_dirty = true;
    // whether validatePaint is called before or after, it won't do anything
}

void Tile::onBlitUpdate()
{
    // The front texture was directly updated with a blit, so mark this as clean
    android::AutoMutex lock(m_atomicSync);
    m_dirty = false;
    m_dirtyArea.setEmpty();
    m_state = Tile::UpToDate;
}

void Tile::validatePaint() {
    // ONLY CALL while m_atomicSync is locked (at the end of paintBitmap())

    if (!m_dirty) {
        // since after the paint, the tile isn't dirty, 'validate' it - this
        // may happed before or after the transfer queue operation. Only
        // when both have happened, mark as 'ReadyToSwap'
        if (m_state == PaintingStarted)
            m_state = ValidatedUntransferred;
        else if (m_state == TransferredUnvalidated) {
            // When the backTexture has been marked pureColor, we will skip the
            // transfer and marked as ReadyToSwap, in this case, we don't want
            // to reset m_dirty bit to true.
            m_state = ReadyToSwap;
        } else {
            ALOGV("Note: validated tile %p at %d %d, state wasn't paintingstarted or transferred %d",
                  this, m_x, m_y, m_state);
            // failed transferring, in which case mark dirty (since
            // paintBitmap() may have cleared m_dirty)
            m_dirty = true;
        }
    } else {
        ALOGV("Note: paint was unsuccessful.");
        m_state = Unpainted;
    }

}

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
