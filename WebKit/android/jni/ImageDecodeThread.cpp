/*
 * Copyright (c) 2010, Code Aurora Forum. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of Code Aurora Forum, Inc. nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ImageDecodeThread.h"

#ifdef CACHED_IMAGE_DECODE
#include "ImageDecodeTask.h"

namespace android {

ImageDecodeThread::ImageDecodeThread(WebViewCore* view)
    : m_threadID(0)
    , m_view(view)
{
    ASSERT(m_view);
}

bool ImageDecodeThread::start()
{
    if (!m_threadID)
        m_threadID = createThread(ImageDecodeThread::imageDecodeThreadStart, this, "android: ImageDecodeThread");
    return m_threadID;
}

void* ImageDecodeThread::imageDecodeThreadStart(void* thread)
{
    return static_cast<ImageDecodeThread*>(thread)->imageDecodeThread();
}

void* ImageDecodeThread::imageDecodeThread()
{
    while (OwnPtr<ImageDecodeTask> task = m_queue.waitForMessage()) {
        // Deplete the message queue. Throw away old DecodeBitmap messages.
        // nextTask has to be declared here because OwnPtr type can't be declared within a condition
        OwnPtr<ImageDecodeTask> nextTask;
        while ((task->getType() == ImageDecodeTask::DecodeBitmaps)
           && (nextTask = m_queue.tryGetMessage())) {
            task.set(nextTask.release());
        }
        task->performTask();
    }
    // Clear WebViewCore reference
    m_view = 0;
    m_threadID = 0;
    return 0;
}

void ImageDecodeThread::scheduleDecodeBitmaps(const WTF::Vector<const SkBitmap*>& bitmaps,
                                              const WTF::Vector<SkRect>& rects)
{
    ASSERT(!m_queue.killed() && m_threadID);
    m_queue.append(ImageDecodeTask::createDecodeBitmaps(m_view, bitmaps, rects));
}

void ImageDecodeThread::terminate()
{
    ASSERT(!m_queue.killed() && m_threadID);
    if (!m_threadID)
        return;

    void* returnValue;
    m_queue.append(ImageDecodeTask::createTerminate(this));
    waitForThreadCompletion(m_threadID, &returnValue);
    ASSERT(m_queue.killed());
    m_threadID = 0;
}

void ImageDecodeThread::performTerminate()
{
    m_queue.kill();
}

}

#endif //CACHED_IMAGE_DECODE
