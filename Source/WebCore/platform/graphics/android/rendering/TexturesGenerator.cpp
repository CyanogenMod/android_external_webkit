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
#include "GLUtils.h"
#include "PaintTileOperation.h"
#include "TilesManager.h"
#include "TransferQueue.h"

namespace WebCore {

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
    {
        android::Mutex::Autolock lock(mRequestedOperationsLock);
        mRequestedOperations.append(operation);
        mRequestedOperationsHash.set(operation->uniquePtr(), operation);
    }
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
    delete filter;
}

status_t TexturesGenerator::readyToRun()
{
    ALOGV("Thread ready to run");
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
    mRequestedOperations.remove(currentIndex);
    mRequestedOperationsHash.remove(current->uniquePtr());
    return current;
}

bool TexturesGenerator::threadLoop()
{
    // Check if we have any pending operations.
    mRequestedOperationsLock.lock();
    while (!mRequestedOperations.size())
        mRequestedOperationsCond.wait(mRequestedOperationsLock);

    ALOGV("threadLoop, got signal");
    mRequestedOperationsLock.unlock();

    m_currentOperation = 0;
    bool stop = false;
    while (!stop) {
        mRequestedOperationsLock.lock();
        ALOGV("threadLoop, %d operations in the queue", mRequestedOperations.size());
        if (mRequestedOperations.size())
            m_currentOperation = popNext();
        mRequestedOperationsLock.unlock();

        if (m_currentOperation) {
            ALOGV("threadLoop, painting the request with priority %d",
                  m_currentOperation->priority());
            m_currentOperation->run();
        }

        QueuedOperation* oldOperation = m_currentOperation;
        mRequestedOperationsLock.lock();
        if (m_currentOperation)
            m_currentOperation = 0;
        if (!mRequestedOperations.size())
            stop = true;
        mRequestedOperationsLock.unlock();
        if (oldOperation)
            delete oldOperation; // delete outside lock
    }
    ALOGV("threadLoop empty");

    return true;
}

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
