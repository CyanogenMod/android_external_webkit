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

#define LOG_TAG "PaintTileOperation"
#define LOG_NDEBUG 1

#include "config.h"
#include "PaintTileOperation.h"

#include "AndroidLog.h"
#include "ClassTracker.h"
#include "GLWebViewState.h"
#include "ImageTexture.h"
#include "ImagesManager.h"
#include "LayerAndroid.h"
#include "TexturesGenerator.h"
#include "TilesManager.h"

namespace WebCore {

PaintTileOperation::PaintTileOperation(Tile* tile, TilePainter* painter,
                                       GLWebViewState* state, bool isLowResPrefetch)
    : m_tile(tile)
    , m_painter(painter)
    , m_state(state)
    , m_isLowResPrefetch(isLowResPrefetch)
{
    if (m_tile)
        m_tile->setRepaintPending(true);
    SkSafeRef(m_painter);
#ifdef DEBUG_COUNT
    ClassTracker::instance()->increment("PaintTileOperation");
#endif
}

PaintTileOperation::~PaintTileOperation()
{
    if (m_tile) {
        m_tile->setRepaintPending(false);
        m_tile = 0;
    }

    if (m_painter && m_painter->type() == TilePainter::Image) {
        ImageTexture* image = static_cast<ImageTexture*>(m_painter);
        ImagesManager::instance()->releaseImage(image->imageCRC());
    } else {
        SkSafeUnref(m_painter);
    }
#ifdef DEBUG_COUNT
    ClassTracker::instance()->decrement("PaintTileOperation");
#endif
}

bool PaintTileOperation::operator==(const QueuedOperation* operation)
{
    const PaintTileOperation* op = static_cast<const PaintTileOperation*>(operation);
    return op->m_tile == m_tile;
}

void PaintTileOperation::run()
{
    TRACE_METHOD();

    if (m_tile) {
        m_tile->paintBitmap(m_painter);
        m_tile->setRepaintPending(false);
        m_tile = 0;
    }
}

int PaintTileOperation::priority()
{
    if (!m_tile)
        return -1;

    int priority = 200000;

    // prioritize low res while scrolling, otherwise set priority above gDeferPriorityCutoff
    if (m_isLowResPrefetch)
        priority = m_state->isScrolling() ? 0 : TexturesGenerator::gDeferPriorityCutoff;

    // prioritize higher draw count
    unsigned long long currentDraw = TilesManager::instance()->getDrawGLCount();
    unsigned long long drawDelta = currentDraw - m_tile->drawCount();
    priority += 100000 * (int)std::min(drawDelta, (unsigned long long)1000);

    // prioritize unpainted tiles, within the same drawCount
    if (m_tile->frontTexture())
        priority += 50000;

    // for base tiles, prioritize based on position
    if (!m_tile->isLayerTile()) {
        bool goingDown = m_state->goingDown();
        priority += m_tile->x();

        if (goingDown)
            priority += 100000 - (1 + m_tile->y()) * 1000;
        else
            priority += m_tile->y() * 1000;
    }

    return priority;
}

void PaintTileOperation::updatePainter(TilePainter* painter)
{
    if (m_painter == painter)
        return;

    SkSafeRef(painter);
    SkSafeUnref(m_painter);
    m_painter = painter;
}

}
