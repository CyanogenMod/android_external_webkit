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

#define LOG_TAG "SurfaceCollectionManager"
#define LOG_NDEBUG 1

#include "config.h"
#include "SurfaceCollectionManager.h"

#include "AndroidLog.h"
#include "private/hwui/DrawGlInfo.h"
#include "TilesManager.h"
#include "SurfaceCollection.h"

namespace WebCore {

using namespace android::uirenderer;

// Tag used to display current number of SurfaceCollections.
// Note: this will only work if one webview is actively drawing at a time.
static const char* COLLECTION_COUNT_TAG = "CollectionCount";

SurfaceCollectionManager::SurfaceCollectionManager()
    : m_drawingCollection(0)
    , m_paintingCollection(0)
    , m_queuedCollection(0)
    , m_fastSwapMode(false)
    , m_previouslyScrolling(false)
    , m_newPaintingCollection(false)
{
}

SurfaceCollectionManager::~SurfaceCollectionManager()
{
    clearCollections();
}

// the painting collection has finished painting:
//   discard the drawing collection
//   swap the painting collection in place of the drawing collection
//   and start painting the queued collection
void SurfaceCollectionManager::swap()
{
    // swap can't be called unless painting just finished
    ASSERT(m_paintingCollection);

    ALOGV("SWAPPING, D %p, P %p, Q %p",
          m_drawingCollection, m_paintingCollection, m_queuedCollection);

    // if we have a drawing collection, discard it since the painting collection is done
    if (m_drawingCollection) {
        ALOGV("destroying drawing collection %p", m_drawingCollection);
        m_drawingCollection->addFrameworkInvals();
        m_drawingCollection->removePainterOperations();
        SkSafeUnref(m_drawingCollection);
    }

    // painting collection becomes the drawing collection
    ALOGV("drawing collection %p", m_paintingCollection);
    m_paintingCollection->setIsDrawing(); // initialize animations
    m_paintingCollection->addFrameworkInvals();

    if (m_queuedCollection) {
        // start painting with the queued collection
        ALOGV("now painting collection %p", m_queuedCollection);
        m_queuedCollection->setIsPainting(m_paintingCollection);
    }
    m_drawingCollection = m_paintingCollection;
    m_paintingCollection = m_queuedCollection;
    m_queuedCollection = 0;

    if (ATRACE_ENABLED()) {
        ATRACE_INT(COLLECTION_COUNT_TAG,
                   (m_drawingCollection ? 1 : 0)
                   + (m_paintingCollection ? 1 : 0));
    }

    ALOGV("SWAPPING COMPLETE, D %p, P %p, Q %p",
         m_drawingCollection, m_paintingCollection, m_queuedCollection);
}

// clear all of the content in the three collections held by the collection manager
void SurfaceCollectionManager::clearCollections()
{
    // remove all painting operations, since they're no longer relevant
    if (m_drawingCollection)
        m_drawingCollection->removePainterOperations();
    if (m_paintingCollection)
        m_paintingCollection->removePainterOperations();

    SkSafeUnref(m_drawingCollection);
    m_drawingCollection = 0;
    SkSafeUnref(m_paintingCollection);
    m_paintingCollection = 0;
    SkSafeUnref(m_queuedCollection);
    m_queuedCollection = 0;

    ATRACE_INT(COLLECTION_COUNT_TAG, 0);
}

void SurfaceCollectionManager::updatePaintingCollection(SurfaceCollection* newCollection)
{
    m_paintingCollection = newCollection;
    m_paintingCollection->setIsPainting(m_drawingCollection);
    m_newPaintingCollection = true;
}

// a new layer collection has arrived, queue it if we're painting something already,
// or start painting it if we aren't. Returns true if the manager has two collections
// already queued.
bool SurfaceCollectionManager::updateWithSurfaceCollection(SurfaceCollection* newCollection,
                                                           bool brandNew)
{
    // can't have a queued collection unless have a painting collection too
    ASSERT(m_paintingCollection || !m_queuedCollection);

    if (!newCollection || brandNew) {
        clearCollections();
        if (brandNew) {
            updatePaintingCollection(newCollection);
            ATRACE_INT(COLLECTION_COUNT_TAG, 1);
        }
        return false;
    }

    ALOGV("updateWithSurfaceCollection - %p, has children %d, has animations %d",
          newCollection, newCollection->hasCompositedLayers(),
          newCollection->hasCompositedAnimations());

    if (m_queuedCollection || m_paintingCollection) {
        // currently painting, so defer this new collection
        if (m_queuedCollection) {
            // already have a queued collection, copy over invals so the regions are
            // eventually repainted and let the old queued collection be discarded
            m_queuedCollection->mergeInvalsInto(newCollection);

            if (!TilesManager::instance()->useDoubleBuffering()) {
                // not double buffering, count discarded collection/webkit paint as an update
                TilesManager::instance()->incContentUpdates();
            }

            ALOGV("DISCARDING collection - %p, has children %d, has animations %d",
                  newCollection, newCollection->hasCompositedLayers(),
                  newCollection->hasCompositedAnimations());
        }
        SkSafeUnref(m_queuedCollection);
        m_queuedCollection = newCollection;
    } else {
        // don't have painting collection, paint this one!
        updatePaintingCollection(newCollection);
    }

    if (ATRACE_ENABLED()) {
        ATRACE_INT(COLLECTION_COUNT_TAG,
                   (m_drawingCollection ? 1 : 0)
                   + (m_paintingCollection ? 1 : 0)
                   + (m_queuedCollection ? 1 : 0));
    }
    return m_drawingCollection && TilesManager::instance()->useDoubleBuffering();
}

void SurfaceCollectionManager::updateScrollableLayer(int layerId, int x, int y)
{
    if (m_queuedCollection)
        m_queuedCollection->updateScrollableLayer(layerId, x, y);
    if (m_paintingCollection)
        m_paintingCollection->updateScrollableLayer(layerId, x, y);
    if (m_drawingCollection)
        m_drawingCollection->updateScrollableLayer(layerId, x, y);
}


int SurfaceCollectionManager::singleSurfaceModeInvalidation(bool hasRunningAnimation,
                                                            bool scrolling,
                                                            bool shouldDraw)
{
    int returnFlags = 0;
    // In single surface mode, we need to dirty all the tiles when we are finishing
    // scrolling or have an incoming painting tree.
    bool requireDirtyAll = (m_previouslyScrolling && !scrolling)
                           || m_newPaintingCollection;

    // We also need to tell the framework to continue to invoke until
    // the base layer is ready.
    bool drawingBaseSurfaceReady = m_drawingCollection
                                   && m_drawingCollection->isReady();

    // When the base layer is ready, we can ask the framework to draw. And if
    // animation is running, dirty all the tiles, otherwise the animation will
    // be paused.
    if (drawingBaseSurfaceReady) {
        if (!shouldDraw)
            returnFlags |= DrawGlInfo::kStatusDraw;
        else
            requireDirtyAll |= hasRunningAnimation;
    }
    if (requireDirtyAll)
        TilesManager::instance()->dirtyAllTiles();

    bool requireInvoke = requireDirtyAll || !drawingBaseSurfaceReady;
    if (requireInvoke)
        returnFlags |= DrawGlInfo::kStatusInvoke;

    m_newPaintingCollection = false;
    m_previouslyScrolling = scrolling;

    return returnFlags;
}

int SurfaceCollectionManager::drawGL(double currentTime, IntRect& viewRect,
                            SkRect& visibleContentRect, float scale,
                            bool scrolling, bool singleSurfaceMode,
                            bool* collectionsSwappedPtr, bool* newCollectionHasAnimPtr,
                            TexturesResult* texturesResultPtr, bool shouldDraw)
{
    m_fastSwapMode |= scrolling || singleSurfaceMode;

    ALOGV("drawGL, D %p, P %p, Q %p, fastSwap %d shouldDraw %d",
          m_drawingCollection, m_paintingCollection,
          m_queuedCollection, m_fastSwapMode, shouldDraw);

    // ask for kStatusInvoke while painting, kStatusDraw if we have content to be redrawn next frame
    // returning 0 indicates all painting complete, no framework inval needed.
    int returnFlags = 0;

    bool didCollectionSwap = false;
    bool tryFastBlit = !m_fastSwapMode;
    if (m_paintingCollection) {
        ALOGV("preparing painting collection %p", m_paintingCollection);

        m_paintingCollection->evaluateAnimations(currentTime);

        m_paintingCollection->prepareGL(visibleContentRect, tryFastBlit);
        m_paintingCollection->computeTexturesAmount(texturesResultPtr);

        if (!TilesManager::instance()->useDoubleBuffering() || m_paintingCollection->isReady()) {
            ALOGV("have painting collection %p ready, swapping!", m_paintingCollection);
            didCollectionSwap = true;
            m_fastSwapMode = false;
            TilesManager::instance()->incContentUpdates();
            if (collectionsSwappedPtr)
                *collectionsSwappedPtr = true;
            if (newCollectionHasAnimPtr)
                *newCollectionHasAnimPtr = m_paintingCollection->hasCompositedAnimations();
            swap();
            returnFlags |= uirenderer::DrawGlInfo::kStatusDraw;
        }
    } else if (m_drawingCollection) {
        ALOGV("preparing drawing collection %p", m_drawingCollection);
        m_drawingCollection->prepareGL(visibleContentRect);
        m_drawingCollection->computeTexturesAmount(texturesResultPtr);
    }

    if (m_paintingCollection)
        returnFlags |= DrawGlInfo::kStatusInvoke;

    if (!shouldDraw) {
        if (didCollectionSwap
            || (!m_paintingCollection
                && m_drawingCollection
                && m_drawingCollection->isReady())) {
            // either a swap just occurred, or there is no more work to be done: do a full draw
            m_drawingCollection->swapTiles();

            if (didCollectionSwap && m_paintingCollection)
                m_paintingCollection->prepareGL(visibleContentRect, tryFastBlit);
            returnFlags |= DrawGlInfo::kStatusDraw;
        } else {
            // current collection not ready - invoke functor in process mode
            // until either drawing or painting collection is ready
            returnFlags |= DrawGlInfo::kStatusInvoke;
        }

        return returnFlags;
    }

    // ===========================================================================
    // Don't have a drawing collection, draw white background
    Color background = Color::white;
    bool drawBackground = true;
    bool hasRunningAnimation = false;
    if (m_drawingCollection) {
        bool drawingReady = didCollectionSwap || m_drawingCollection->isReady();

        if (didCollectionSwap || m_fastSwapMode || (drawingReady && !m_paintingCollection))
            m_drawingCollection->swapTiles();

        if (didCollectionSwap && m_paintingCollection)
            m_paintingCollection->prepareGL(visibleContentRect, tryFastBlit);

        if (drawingReady) {
            // exit fast swap mode, as content is up to date
            m_fastSwapMode = false;
        } else {
            // drawing isn't ready, must redraw
            returnFlags |= DrawGlInfo::kStatusInvoke;
        }

        hasRunningAnimation = m_drawingCollection->evaluateAnimations(currentTime);

        ALOGV("drawing collection %p", m_drawingCollection);
        background = m_drawingCollection->getBackgroundColor();
        drawBackground = m_drawingCollection->isMissingBackgroundContent();
    } else if (m_paintingCollection) {
        // Use paintingCollection background color while tiles are not done painting.
        background = m_paintingCollection->getBackgroundColor();
    }

    if (singleSurfaceMode)
        returnFlags |= singleSurfaceModeInvalidation(hasRunningAnimation,
                                                     scrolling, shouldDraw);
    // Start doing the actual GL drawing.
    if (drawBackground) {
        ALOGV("background is %x", background.rgb());
        // If background is opaque, we can safely and efficiently clear it here.
        // Otherwise, we have to calculate all the missing tiles and blend the background.
        GLUtils::clearBackgroundIfOpaque(&background);
    }

#ifdef DEBUG
    ALOGV("Drawing %d / %d surfaces",
        m_drawingCollection ? m_drawingCollection->backedSize() : -1,
        m_drawingCollection ? m_drawingCollection->size() : -1);
#endif

    if (m_drawingCollection && m_drawingCollection->drawGL(visibleContentRect))
        returnFlags |= DrawGlInfo::kStatusDraw;

    ALOGV("returnFlags %d,  m_paintingCollection %d ", returnFlags, m_paintingCollection);
    return returnFlags;
}

} // namespace WebCore
