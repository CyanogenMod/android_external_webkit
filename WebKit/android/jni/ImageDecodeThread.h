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

#ifdef CACHED_IMAGE_DECODE

#ifndef IMAGEDECODETHREAD_H
#define IMAGEDECODETHREAD_H

#include <wtf/HashSet.h>
#include <wtf/MessageQueue.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/Threading.h>
#include <wtf/Vector.h>
#include "ImageDecodeTask.h"
#include "SkBitmap.h"
#include "SkRect.h"

namespace android {

class WebViewCore;

class ImageDecodeThread : public Noncopyable {
public:
    // WebViewCore lifetime is guaranteed, so can use raw pointer
    static PassOwnPtr<ImageDecodeThread> create(WebViewCore* view) {
        return new ImageDecodeThread(view);
    }

    bool start();
    void scheduleDecodeBitmaps(const WTF::Vector<const SkBitmap*>& bitmaps,
                               const WTF::Vector<SkRect>& rects);

    // Called from the main thread to synchronously shut down this thread
    void terminate();
    // Background thread part of the terminate procedure
    void performTerminate();

private:
    ImageDecodeThread(WebViewCore*);

    static void* imageDecodeThreadStart(void*);
    void* imageDecodeThread();

    ThreadIdentifier m_threadID;
    MessageQueue<ImageDecodeTask> m_queue;
    WebViewCore* m_view;
};

} // namespace android

#endif // IMAGEDECODETHREAD_H
#endif // CACHED_IMAGE_DECODE
