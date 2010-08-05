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
#include "ImageDecodeTask.h"

#ifdef CACHED_IMAGE_DECODE
#include "ImageDecodeThread.h"
#include "SkPixelRef.h"
#include "WebViewCore.h"

namespace android {

/* Constructors are called on the caller's thread (can be UI thread or WebCoreView thread) */
ImageDecodeTask::ImageDecodeTask(Type type, WebViewCore* view, const WTF::Vector<const SkBitmap*>& bitmaps, const WTF::Vector<SkRect>& rects)
    : m_type(type)
    , m_view(view)
    , m_rects(rects)
{
    ASSERT(bitmaps);
    ASSERT(m_type == DecodeBitmaps);
    ASSERT(rects.size() == bitmaps.size());

    m_bitmaps.resize(bitmaps.size());

    for (size_t i = 0; i < bitmaps.size(); ++i) {
        m_bitmaps[i] = SkBitmap(*bitmaps[i]);
    }
}

ImageDecodeTask::ImageDecodeTask(Type type, ImageDecodeThread* thread)
    : m_type(type)
    , m_thread(thread)
{
    ASSERT(m_thread);
    ASSERT(m_type == TerminateThread);
}

ImageDecodeTask::~ImageDecodeTask()
{
    m_bitmaps.clear();
    m_rects.clear();
}

/* This function runs on the ImageDecodeThread */
void ImageDecodeTask::performTask()
{
    switch (m_type) {
    case DecodeBitmaps:
        for (size_t i = 0; i < m_bitmaps.size(); ++i) {
            if (!m_bitmaps[i].pixelRef()->pixelsAvailable()) {
                SkAutoLockPixels alp(m_bitmaps[i]);
                // Invalidate view after every image locked.
                // Assumes that invalidate calls are accumulated.
                if (m_view) {
                    // Convert SkRect to IntRect
                    SkRect  bitmapRect(m_rects[i]);
                    SkIRect irect;
                    bitmapRect.roundOut(&irect);
                    WebCore::IntRect invalRect(irect);
                    m_view->viewInvalidate(invalRect);
                }
            }
        }
        break;
    case TerminateThread:
        m_thread->performTerminate();
        break;
    default:
        ASSERT_NOT_REACHED();
        break;
    }
}

} // namespace android

#endif // CACHED_IMAGE_DECODE
