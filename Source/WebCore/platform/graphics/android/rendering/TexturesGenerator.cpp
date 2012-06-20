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

#define LOG_TAG "TexturesGenerator"
#define LOG_NDEBUG 1

#include "config.h"
#include "TexturesGenerator.h"

#if USE(ACCELERATED_COMPOSITING)

#include "AndroidLog.h"
#include "BaseRenderer.h"
#include "GLUtils.h"
#include "PaintTileOperation.h"
#include "TilesManager.h"
#include "TransferQueue.h"

namespace WebCore {

TexturesGenerator::TexturesGenerator(TilesManager* instance)
  : Thread(false)
  , m_tilesManager(instance)
  , m_deferredMode(false)
  , m_renderer(0)
{
}

TexturesGenerator::~TexturesGenerator()
{
    delete m_renderer;
}

bool TexturesGenerator::tryUpdateOperationWithPainter(Tile* tile, TilePainter* painter)
{
    android::Mutex::Autolock lock(mRequestedOperationsLock);
    if (!mRequestedOperationsHash.contains(tile))
        return false;

    static_cast<PaintTileOperation*>(mRequestedOperationsHash.get(tile))->updatePainter(painter);
    return true;
}

void TexturesGenerator::scheduleOperation(QueuedOperation* operation)
{
    bool signal = false;
    {
        android::Mutex::Autolock lock(mRequestedOperationsLock);
        mRequestedOperations.append(operation);
        mRequestedOperationsHash.set(operation->uniquePtr(), operation);

        bool deferrable = operation->priority() >= gDeferPriorityCutoff;
        m_deferredMode &= deferrable;

        // signal if we weren't in deferred mode, or if we can no longer defer
        signal = !m_deferredMode || !deferrable;
    }
    if (signal)
        mRequestedOperationsCond.signal();
}

void TexturesGenerator::removeOperationsForFilter(OperationFilter* filter)
{
    if (!filter)
        return;

    android::Mutex::Autolock lock(mRequestedOperationsLock);
    for (unsigned int i = 0; i < mRequestedOperations.size();) {
        QueuedOperation* operation = mRequestedOperations[i];
        if (filter->check(operation)) {
            mRequestedOperations.remove(i);
            mRequestedOperationsHash.remove(operation->uniquePtr());
            delete operation;
        } else {
            i++;
        }
    }
}

status_t TexturesGenerator::readyToRun()
{
    m_renderer = BaseRenderer::createRenderer();
    return NO_ERROR;
}

// Must be called from within a lock!
QueuedOperation* TexturesGenerator::popNext()
{
    // Priority can change between when it was added and now
    // Hence why the entire queue is rescanned
    QueuedOperation* current = mRequestedOperations.last();
    int currentPriority = current->priority();
    if (currentPriority < 0) {
        mRequestedOperations.removeLast();
        mRequestedOperationsHash.remove(current->uniquePtr());
        return current;
    }
    int currentIndex = mRequestedOperations.size() - 1;
    // Scan from the back to make removing faster (less items to copy)
    for (int i = mRequestedOperations.size() - 2; i >= 0; i--) {
        QueuedOperation *next = mRequestedOperations[i];
        int nextPriority = next->priority();
        if (nextPriority < 0) {
            // Found a very high priority item, go ahead and just handle it now
            mRequestedOperations.remove(i);
            mRequestedOperationsHash.remove(next->uniquePtr());
            return next;
        }
        // pick items preferrably by priority, or if equal, by order of
        // insertion (as we add items at the back of the queue)
        if (nextPriority <= currentPriority) {
            current = next;
            currentPriority = nextPriority;
            currentIndex = i;
        }
    }

    if (!m_deferredMode && currentPriority >= gDeferPriorityCutoff) {
        // finished with non-deferred rendering, enter deferred mode to wait
        m_deferredMode = true;
        return 0;
    }

    mRequestedOperations.remove(currentIndex);
    mRequestedOperationsHash.remove(current->uniquePtr());
    return current;
}

bool TexturesGenerator::threadLoop()
{
    // Check if we have any pending operations.
    mRequestedOperationsLock.lock();

    if (!m_deferredMode) {
        // if we aren't currently deferring work, wait for new work to arrive
        while (!mRequestedOperations.size())
            mRequestedOperationsCond.wait(mRequestedOperationsLock);
    } else {
        // if we only have deferred work, wait for better work, or a timeout
        mRequestedOperationsCond.waitRelative(mRequestedOperationsLock, gDeferNsecs);
    }

    mRequestedOperationsLock.unlock();

    bool stop = false;
    while (!stop) {
        QueuedOperation* currentOperation = 0;

        mRequestedOperationsLock.lock();
        ALOGV("threadLoop, %d operations in the queue", mRequestedOperations.size());

        if (mRequestedOperations.size())
            currentOperation = popNext();
        mRequestedOperationsLock.unlock();

        if (currentOperation) {
            ALOGV("threadLoop, painting the request with priority %d",
                  currentOperation->priority());
            // swap out the renderer if necessary
            BaseRenderer::swapRendererIfNeeded(m_renderer);
            currentOperation->run(m_renderer);
        }

        mRequestedOperationsLock.lock();
        if (m_deferredMode && !currentOperation)
            stop = true;
        if (!mRequestedOperations.size()) {
            m_deferredMode = false;
            stop = true;
        }
        mRequestedOperationsLock.unlock();

        if (currentOperation)
            delete currentOperation; // delete outside lock
    }
    ALOGV("threadLoop empty");

    return true;
}

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
