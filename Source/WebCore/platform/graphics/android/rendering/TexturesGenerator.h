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

#ifndef TexturesGenerator_h
#define TexturesGenerator_h

#if USE(ACCELERATED_COMPOSITING)

#include "QueuedOperation.h"
#include "TransferQueue.h"
#include <wtf/HashMap.h>
#include <wtf/Vector.h>

#include <utils/threads.h>

namespace WebCore {

using namespace android;

class TilesManager;

class TexturesGenerator : public Thread {
public:
    TexturesGenerator(TilesManager* instance);
    virtual ~TexturesGenerator();

    virtual status_t readyToRun();

    bool tryUpdateOperationWithPainter(Tile* tile, TilePainter* painter);

    void removeOperationsForFilter(OperationFilter* filter);

    void scheduleOperation(QueuedOperation* operation);

    // low res tiles are put at or above this cutoff when not scrolling,
    // signifying that they should be deferred
    static const int gDeferPriorityCutoff = 500000000;

private:
    QueuedOperation* popNext();
    virtual bool threadLoop();
    WTF::Vector<QueuedOperation*> mRequestedOperations;
    WTF::HashMap<void*, QueuedOperation*> mRequestedOperationsHash;
    android::Mutex mRequestedOperationsLock;
    android::Condition mRequestedOperationsCond;
    TilesManager* m_tilesManager;

    bool m_deferredMode;
    BaseRenderer* m_renderer;

    // defer painting for one second if best in queue has priority
    // QueuedOperation::gDeferPriorityCutoff or higher
    static const nsecs_t gDeferNsecs = 1000000000;
};

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
#endif // TexturesGenerator_h
