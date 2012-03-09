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
#include "BaseLayerAndroid.h"
#include "LayerGroup.h"
#include "TilesManager.h"
#include "SurfaceCollection.h"

namespace WebCore {

SurfaceCollectionManager::SurfaceCollectionManager(GLWebViewState* state)
    : m_state(state)
    , m_drawingCollection(0)
    , m_paintingCollection(0)
    , m_queuedCollection(0)
    , m_fastSwapMode(false)
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

    android::Mutex::Autolock lock(m_paintSwapLock);

    ALOGV("SWAPPING, D %p, P %p, Q %p",
          m_drawingCollection, m_paintingCollection, m_queuedCollection);

    // if we have a drawing collection, discard it since the painting collection is done
    if (m_drawingCollection) {
        ALOGV("destroying drawing collection %p", m_drawingCollection);
        SkSafeUnref(m_drawingCollection);
    }

    // painting collection becomes the drawing collection
    ALOGV("drawing collection %p", m_paintingCollection);
    m_paintingCollection->setIsDrawing(); // initialize animations

    if (m_queuedCollection) {
        // start painting with the queued collection
        ALOGV("now painting collection %p", m_queuedCollection);
        m_queuedCollection->setIsPainting(m_paintingCollection);
    }
    m_drawingCollection = m_paintingCollection;
    m_paintingCollection = m_queuedCollection;
    m_queuedCollection = 0;

    ALOGV("SWAPPING COMPLETE, D %p, P %p, Q %p",
         m_drawingCollection, m_paintingCollection, m_queuedCollection);
}

// clear all of the content in the three collections held by the collection manager
void SurfaceCollectionManager::clearCollections()
{
    ALOGV("SurfaceCollectionManager %p removing PS from state %p", this, m_state);

    SkSafeUnref(m_drawingCollection);
    m_drawingCollection = 0;
    SkSafeUnref(m_paintingCollection);
    m_paintingCollection = 0;
    SkSafeUnref(m_queuedCollection);
    m_queuedCollection = 0;
}

// a new layer collection has arrived, queue it if we're painting something already,
// or start painting it if we aren't. Returns true if the manager has two collections
// already queued.
bool SurfaceCollectionManager::updateWithSurfaceCollection(SurfaceCollection* newCollection,
                                                           bool brandNew)
{
    ALOGV("updateWithSurfaceCollection - %p, has children %d, has animations %d",
          newCollection, newCollection->hasCompositedLayers(),
          newCollection->hasCompositedAnimations());

    // can't have a queued collection unless have a painting collection too
    ASSERT(m_paintingCollection || !m_queuedCollection);

    android::Mutex::Autolock lock(m_paintSwapLock);

    if (!newCollection || brandNew) {
        clearCollections();
        if (brandNew) {
            m_paintingCollection = newCollection;
            m_paintingCollection->setIsPainting(m_drawingCollection);
        }
        return false;
    }

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
        m_paintingCollection = newCollection;
        m_paintingCollection->setIsPainting(m_drawingCollection);
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

bool SurfaceCollectionManager::drawGL(double currentTime, IntRect& viewRect,
                            SkRect& visibleRect, float scale,
                            bool enterFastSwapMode,
                            bool* collectionsSwappedPtr, bool* newCollectionHasAnimPtr,
                            TexturesResult* texturesResultPtr)
{
    m_fastSwapMode |= enterFastSwapMode;

    ALOGV("drawGL, D %p, P %p, Q %p, fastSwap %d",
          m_drawingCollection, m_paintingCollection, m_queuedCollection, m_fastSwapMode);

    bool ret = false;
    bool didCollectionSwap = false;
    if (m_paintingCollection) {
        ALOGV("preparing painting collection %p", m_paintingCollection);

        m_paintingCollection->evaluateAnimations(currentTime);

        m_paintingCollection->prepareGL(visibleRect, scale, currentTime);
        m_paintingCollection->computeTexturesAmount(texturesResultPtr);

        if (!TilesManager::instance()->useDoubleBuffering() || m_paintingCollection->isReady()) {
            ALOGV("have painting collection %p ready, swapping!", m_paintingCollection);
            didCollectionSwap = true;
            TilesManager::instance()->incContentUpdates();
            if (collectionsSwappedPtr)
                *collectionsSwappedPtr = true;
            if (newCollectionHasAnimPtr)
                *newCollectionHasAnimPtr = m_paintingCollection->hasCompositedAnimations();
            swap();
        }
    } else if (m_drawingCollection) {
        ALOGV("preparing drawing collection %p", m_drawingCollection);
        m_drawingCollection->prepareGL(visibleRect, scale, currentTime);
        m_drawingCollection->computeTexturesAmount(texturesResultPtr);
    }

    if (m_drawingCollection) {
        bool drawingReady = didCollectionSwap || m_drawingCollection->isReady();

        // call the page swap callback if registration happened without more collections enqueued
        if (collectionsSwappedPtr && drawingReady && !m_paintingCollection)
            *collectionsSwappedPtr = true;

        if (didCollectionSwap || m_fastSwapMode || (drawingReady && !m_paintingCollection))
            m_drawingCollection->swapTiles();

        if (drawingReady) {
            // exit fast swap mode, as content is up to date
            m_fastSwapMode = false;
        } else {
            // drawing isn't ready, must redraw
            ret = true;
        }

        m_drawingCollection->evaluateAnimations(currentTime);
        ALOGV("drawing collection %p", m_drawingCollection);
        ret |= m_drawingCollection->drawGL(visibleRect, scale);
    } else {
        // Dont have a drawing collection, draw white background
        Color defaultBackground = Color::white;
        m_state->drawBackground(defaultBackground);
    }

    if (m_paintingCollection) {
        ALOGV("still have painting collection %p", m_paintingCollection);
        return true;
    }

    return ret;
}

// draw for base tile - called on TextureGeneration thread
void SurfaceCollectionManager::drawCanvas(SkCanvas* canvas, bool drawLayers)
{
    SurfaceCollection* paintingCollection = 0;
    m_paintSwapLock.lock();
    paintingCollection = m_paintingCollection ? m_paintingCollection : m_drawingCollection;
    SkSafeRef(paintingCollection);
    m_paintSwapLock.unlock();

    if (!paintingCollection)
        return;

    paintingCollection->drawCanvas(canvas, drawLayers);

    SkSafeUnref(paintingCollection);
}

// TODO: refactor this functionality elsewhere
int SurfaceCollectionManager::baseContentWidth()
{
    if (m_paintingCollection)
        return m_paintingCollection->baseContentWidth();
    else if (m_drawingCollection)
        return m_drawingCollection->baseContentWidth();
    return 0;
}

int SurfaceCollectionManager::baseContentHeight()
{
    if (m_paintingCollection)
        return m_paintingCollection->baseContentHeight();
    else if (m_drawingCollection)
        return m_drawingCollection->baseContentHeight();
    return 0;
}

} // namespace WebCore
