/*
* Copyright (c) 2011, Code Aurora Forum. All rights reserved.
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
#if ENABLE(ACCELERATED_SCROLLING)
#include "config.h"

#define LOG_TAG "renderer"
//#define LOG_NDEBUG 0

#include "Renderer.h"

#include "BackingStore.h"
#include "CurrentTime.h"
#include "SkBitmap.h"
#include "SkCanvas.h"
#include "SkDevice.h"
#include "SkDrawFilter.h"
#include "SkPaint.h"
#include "SkPaintFlagsDrawFilter.h"
#include "SkPoint.h"
#include "SkProxyCanvas.h"
#include "SkShader.h"
#include "SkTypeface.h"
#include "SkXfermode.h"
#include "WebViewCore.h"
#include <cutils/log.h>
#include <dlfcn.h>
#include <stdio.h>

//#define DO_LOG_PERF
//#define DO_LOG_RENDER

#undef LOG_PERF
#ifdef DO_LOG_PERF
#define LOG_PERF(...) SLOGV(__VA_ARGS__)
#else
#define LOG_PERF(...) ((void)0)
#endif

#undef LOG_RENDER
#ifdef DO_LOG_RENDER
#define LOG_RENDER(...) SLOGV(__VA_ARGS__)
#else
#define LOG_RENDER(...) ((void)0)
#endif

using namespace android;

namespace RendererImplNamespace {

struct RendererConfig {
    RendererConfig()
        : allowSplit(true)
        , splitSize(50)
        , enablePartialUpdate(true)
        , enablePartialRender(false)
        , enableDraw(true)
    {
        if (enablePartialRender)
            enablePartialUpdate = false;
    }

    bool allowSplit; //allow the PictureSet to be split into smaller pieces for better performance
    int splitSize; // each block is 50 pages
    bool enablePartialUpdate; // allow backing store to perform partial update on new PictureSet instead of redrawing everything
    bool enablePartialRender;
    bool enableDraw; // enable the backing store
};

static RendererConfig s_config;

// RendererImpl class
// This is the implementation of the Renderer class.  Users of this class will only see it
// through the Renderer interface.  All implementation details are hidden in this RendererImpl
// class.
class RendererImpl : public Renderer, public WebTech::IBackingStore::IUpdater {
    struct RenderTask {
        enum RenderQuality {
            LOW,
            HIGH
        };

        RenderTask()
            : valid(false)
        {
        }
        SkColor color;
        bool invertColor;
        WebTech::IBackingStore::UpdateRegion requestArea; // requested valid region in content space.
        float contentScale;
        SkIPoint contentOrigin; // (x,y) in content space, for the point at (0,0) in the viewport
        int viewportWidth;
        int viewportHeight;
        RenderQuality quality;
        SkBitmap::Config config;
        bool newContent;
        bool valid;
    };

    // implementation of WebTech::IBackingStore::IBuffer interface.
    class BackingStoreBuffer : public WebTech::IBackingStore::IBuffer {
    public:
        BackingStoreBuffer(int w, int h, int bpp)
        {
            SkBitmap::Config config = bppToConfig(bpp);
            m_bitmap.setConfig(config, w, h);
            m_bitmap.allocPixels();
        }

        virtual ~BackingStoreBuffer()
        {
        }

        virtual void release()
        {
            delete this;
        }

        const SkBitmap& getBitmap()
        {
            return m_bitmap;
        }

        bool failed()
        {
            return !m_bitmap.getPixels();
        }

    private:
        static SkBitmap::Config bppToConfig(int bpp)
        {
            if (bpp == 16)
                return SkBitmap::kRGB_565_Config;
            return SkBitmap::kARGB_8888_Config;
        }

        SkBitmap m_bitmap;
    };

    // ContentData - data that must be guarded in mutex.  New content can be set in a webcore thread (webkit main thread),
    // but used in the UI thread.  Any data that can be changed in the webcore thread is encapsulated in
    // this class.
    class ContentData {
    public:
        ContentData()
            : m_contentWidth(0)
            , m_contentHeight(0)
            , m_numIncomingContent(0)
            , m_incomingLoading(false)
            , m_loading(false)
            , m_incomingInvalidateAll(false)
            , m_invalidateAll(false)
        {
        }

        ~ContentData()
        {
        }

        // returns number of times content has changed
        int numContentChanged()
        {
            MutexLocker locker(m_mutex);
            return m_numIncomingContent;
        }

        // if numContentChanged() above returns non zero, changeToNewContent() can be called to
        // switch the currently active content (which may be used by IBackingStore) to the new content.
        // A copy of the reference of the new data is copied for access by the UI thread.  The content
        // previously used by the UI thread is released.
        void changeToNewContent()
        {
            MutexLocker locker(m_mutex);
            m_content.set(m_incomingContent.release());
            if (m_content) {
                m_contentWidth = m_content->width();
                m_contentHeight = m_content->height();
            } else {
                m_contentWidth = 0;
                m_contentHeight = 0;
            }

            if (m_incomingContentInvalidRegion && !m_contentInvalidRegion) {
                m_contentInvalidRegion.set(m_incomingContentInvalidRegion.release());
            }
            else if (m_incomingContentInvalidRegion && m_contentInvalidRegion) {
                m_contentInvalidRegion->op(*m_incomingContentInvalidRegion, SkRegion::kUnion_Op);
                m_incomingContentInvalidRegion.clear();
            }

            m_numIncomingContent = 0;

            m_loading = m_incomingLoading;
            m_invalidateAll |= m_incomingInvalidateAll;
            m_incomingInvalidateAll = false;
        }

        // can be called from webcore thread (webkit main thread) when setting to null content.
        void onClearContent()
        {
            MutexLocker locker(m_mutex);
            /*TESTTSETdelete m_incomingContent;
            m_incomingContent = 0;
            delete m_incomingContentInvalidRegion;*/
            m_incomingContent.clear();
            m_incomingContentInvalidRegion.clear();

            ++m_numIncomingContent;

            m_loading = m_incomingLoading = false;
            m_incomingInvalidateAll = true;
        }

        // can be called from webcore thread (webkit main thread) when new content is available.
        bool onNewContent(const PictureSet& content, SkRegion* region, bool loading)
        {
            MutexLocker locker(m_mutex);
            int num = content.size();
            LOG_RENDER("new content size = %d x %d.  %d pictures", content.width(), content.height(), num);
            if (region) {
                LOG_RENDER("        - region={%d,%d,r=%d,b=%d}.", region->getBounds().fLeft,
                    region->getBounds().fTop, region->getBounds().fRight,
                    region->getBounds().fBottom);
            }
            m_incomingContent.clear();
            m_incomingContent = new PictureSet(content);

            bool invalidateAll = !region || !s_config.enablePartialUpdate;
            invalidateAll |= ((m_contentWidth != content.width()) || (m_contentHeight != content.height()));

            if (region) {
                SkIRect rect;
                rect.set(0, 0, content.width(), content.height());
                if (region->contains(rect))
                    invalidateAll = true;
            }
            if (!invalidateAll) {
                LOG_RENDER("setContent. invalidate region");
                if (!m_incomingContentInvalidRegion)
                    m_incomingContentInvalidRegion = new SkRegion(*region);
                else
                    m_incomingContentInvalidRegion->op(*region, SkRegion::kUnion_Op);
            } else {
                LOG_RENDER("setContent. invalidate All");
                m_incomingContentInvalidRegion.clear();
                m_incomingInvalidateAll = true;
            }
            ++m_numIncomingContent;
            m_incomingLoading = loading;
            return invalidateAll;
        }

        WTF::Mutex m_mutex; // for guarding access from UI thread and webcore thread (webkit main thread).
        OwnPtr<PictureSet> m_incomingContent;
        OwnPtr<PictureSet> m_content;
        OwnPtr<SkRegion> m_incomingContentInvalidRegion;
        OwnPtr<SkRegion> m_contentInvalidRegion;
        int m_contentWidth;
        int m_contentHeight;
        int m_numIncomingContent;
        bool m_incomingLoading;
        bool m_loading;
        bool m_incomingInvalidateAll;
        bool m_invalidateAll;
    };

public:
    RendererImpl()
        : m_backingStore(0)
        , m_loading(false)
        , m_doPartialRender(false)
    {
        m_doPartialRender = s_config.enablePartialRender;
    }

    ~RendererImpl()
    {
        if (m_backingStore)
            m_backingStore->release();
    }

    virtual void release()
    {
        delete this;
    }

    // can be called from webcore thread (webkit main thread).
    virtual void setContent(const PictureSet& content, SkRegion* region, bool loading)
    {
        m_contentData.onNewContent(content, region, loading);
        if (!m_doPartialRender && m_backingStore)
            m_backingStore->invalidate();
    }

    // can be called from webcore thread (webkit main thread).
    virtual void clearContent()
    {
        LOG_RENDER("client clearContent");
        m_contentData.onClearContent();
        if (!m_doPartialRender && m_backingStore)
            m_backingStore->invalidate();
    }

    // can be called from webcore thread (webkit main thread).
    virtual void pause()
    {
        LOG_RENDER("client pause");
        if (m_backingStore)
            m_backingStore->cleanup();
    }

    virtual void finish()
    {
        LOG_RENDER("client finish");
        if (m_backingStore)
            m_backingStore->finish();
    }

    // called in the UI thread.
    virtual bool drawContent(SkCanvas* canvas, SkColor color, bool invertColor, PictureSet& content, bool& splitContent)
    {
        if (!s_config.enableDraw)
            return false;
        LOG_RENDER("drawContent");

#ifdef DO_LOG_RENDER
        const SkMatrix& matrix = canvas->getTotalMatrix();
        SkScalar tx = matrix.getTranslateX();
        SkScalar ty = matrix.getTranslateY();
        SkScalar sx = matrix.getScaleX();
        const SkRegion& clip = canvas->getTotalClip();
        SkIRect clipBound = clip.getBounds();
        int isRect = (clip.isRect())?1:0;
        SLOGV("drawContent tx=%f, ty=%f, scale=%f", tx, ty, sx);
        SLOGV("  clip %d, %d to %d, %d.  isRect=%d", clipBound.fLeft, clipBound.fTop, clipBound.fRight, clipBound.fBottom, isRect);
#endif

        double startTime =  WTF::currentTimeMS();
        bool drawn = false;
        bool shouldUpdate = true;
        bool abort = false;
        splitContent = false;

        RenderTask request;
        generateRequest(canvas, color, invertColor, request);

        bool interactiveZoom = detectInteractiveZoom(request);
        if (interactiveZoom && !m_request.valid)
            shouldUpdate = false;

        bool result = false;
        if (!m_doPartialRender || request.quality == RenderTask::HIGH)
            handleNewContent(request);

        if (shouldUpdate)
            result = RenderRequest((interactiveZoom)? m_request : request);

        if (result && !drawn)
            drawn = drawResult(canvas, request);
        
        if (s_config.allowSplit) {
            int split = suggestContentSplitting(content, request);
            if (split) {
                LOG_RENDER("renderer client triggers content splitting %d", split);
                content.setDrawTimes((uint32_t)(100 << (split - 1)));
                splitContent = true;
            }
        }

        if (!drawn)
            finish();

        double endTime =  WTF::currentTimeMS();
        double elapsedTime = endTime-startTime;
        LOG_PERF("drawContent %s %s took %f msec.",
            (request.newContent)? "(with new content)" : "",
            (drawn)? "" : "aborted and",
            elapsedTime);

        return drawn;
    }

    ////////////////////////////// IBackingStore::IUpdater methods ///////////////////////
    virtual void inPlaceScroll(WebTech::IBackingStore::IBuffer* buffer, int x, int y, int w, int h, int dx, int dy)
    {
        BackingStoreBuffer* bitmap = static_cast<BackingStoreBuffer*>(buffer);
        if (!bitmap)
            return;

        int pitch = bitmap->getBitmap().rowBytes();
        int bpp = bitmap->getBitmap().bytesPerPixel();
        int ny = h;
        int nx = w * bpp;
        char* src = static_cast<char*>(bitmap->getBitmap().getPixels());
        src = src + x * bpp + y * pitch;
        int dptr = pitch;

        if (dy>0) {
            src = src + (h-1) * pitch;
            dptr = -dptr;
        }

        char* dst = src + dx*bpp + dy*pitch;

        if (!dy) {
            for (int i = 0; i < ny; ++i) {
                memmove(dst, src, nx);
                dst += dptr;
                src += dptr;
            }
        } else {
            for (int i = 0; i < ny; ++i) {
                memcpy(dst, src, nx);
                dst += dptr;
                src += dptr;
            }
        }
    }

    virtual WebTech::IBackingStore::IBuffer* createBuffer(int w, int h)
    {
        LOG_RENDER("RendererImpl::createBuffer");
        BackingStoreBuffer* buffer = new BackingStoreBuffer(w, h, SkBitmap::ComputeBytesPerPixel(m_request.config));
        if (!buffer || buffer->failed()) {
            SLOGV("failed to allocate buffer for backing store");
            return 0;
        }
        return static_cast<WebTech::IBackingStore::IBuffer*>(buffer);
    }

    virtual void renderToBackingStoreRegion(WebTech::IBackingStore::IBuffer* buffer, int bufferX, int bufferY, WebTech::IBackingStore::UpdateRegion& region, WebTech::IBackingStore::UpdateQuality quality, bool existingRegion)
    {
        if (!m_contentData.m_content)
            return;

        BackingStoreBuffer* bitmap = static_cast<BackingStoreBuffer*>(buffer);
        if (!bitmap)
            return;

        LOG_RENDER("renderToBackingStoreRegion. out(%d, %d), area=(%d, %d) to (%d, %d) size=(%d, %d)",
            bufferX, bufferY, region.x1, region.y1, region.x2, region.y2, region.x2 - region.x1, region.y2 - region.y1);
        SkCanvas srcCanvas(bitmap->getBitmap());
        SkCanvas* canvas = static_cast<SkCanvas*>(&srcCanvas);
        SkRect clipRect;
        clipRect.set(bufferX, bufferY, bufferX + region.x2 - region.x1, bufferY + region.y2 - region.y1);
        canvas->clipRect(clipRect, SkRegion::kReplace_Op);

        SkScalar s = m_request.contentScale;
        SkScalar dx = -static_cast<SkScalar>(region.x1) + bufferX;
        SkScalar dy = -static_cast<SkScalar>(region.y1) + bufferY;
        canvas->translate(dx, dy);
        canvas->scale(s, s);

        uint32_t removeFlags, addFlags;

        removeFlags = SkPaint::kFilterBitmap_Flag | SkPaint::kDither_Flag;
        addFlags = 0;
        SkPaintFlagsDrawFilter filterLo(removeFlags, addFlags);

        int sc = canvas->save(SkCanvas::kClip_SaveFlag);

        clipRect.set(0, 0, m_contentData.m_content->width(), m_contentData.m_content->height());
        canvas->clipRect(clipRect, SkRegion::kDifference_Op);
        canvas->drawColor(m_request.color);
        canvas->restoreToCount(sc);

        if (existingRegion) {
            SkRegion* clip = m_contentData.m_contentInvalidRegion.get();
            SkRegion* transformedClip = transformContentClip(*clip, s, dx, dy);

            if (transformedClip) {
                canvas->clipRegion(*transformedClip);
                delete transformedClip;
                const SkRegion& totalClip = canvas->getTotalClip();
                if (totalClip.isEmpty()) {
                    LOG_RENDER("renderToBackingStoreRegion exiting because outside clip region");
                    return;
                }
            }
        }

        m_contentData.m_content->draw(canvas
#if ENABLE(COLOR_INVERSION)
            , m_request.invertColor);
#else
            );
#endif

    }

private:
    SkRegion* transformContentClip(SkRegion& rgn, float scale, float dx, float dy)
    {
        SkRegion* clip = new SkRegion;
        SkRegion::Iterator iter(rgn);
        int num = 0;
        while (!iter.done()) {
            SkIRect rect = iter.rect();
            rect.fLeft = static_cast<int32_t>(floor(rect.fLeft*scale + dx));
            rect.fTop = static_cast<int32_t>(floor(rect.fTop*scale + dy));
            rect.fRight = static_cast<int32_t>(ceil(rect.fRight*scale + dx));
            rect.fBottom = static_cast<int32_t>(ceil(rect.fBottom*scale + dy));
            clip->op(rect, SkRegion::kUnion_Op);
            iter.next();
            ++num;
        }
        LOG_RENDER("scaleContentClip - created clip region of %d rectangles", num);
        return clip;
    }

    bool detectInteractiveZoom(RenderTask& request)
    {
        if (m_request.valid && (request.contentScale != m_request.contentScale) && request.quality == RenderTask::LOW) {
                LOG_RENDER("Renderer client detected interactive zoom");
                return true;
        }
        return false;
    }

    // return true if there's new content
    void generateRequest(SkCanvas* canvas, SkColor color, bool invertColor, RenderTask& task)
    {
        const SkRegion& clip = canvas->getTotalClip();
        const SkMatrix & matrix = canvas->getTotalMatrix();
        SkRegion* region = new SkRegion(clip);

        SkIRect clipBound = clip.getBounds();

        SkDevice* device = canvas->getDevice();
        const SkBitmap& bitmap = device->accessBitmap(true);

        SkDrawFilter* f = canvas->getDrawFilter();
        bool filterBitmap = true;
        if (f) {
            SkPaint tmpPaint;
            tmpPaint.setFilterBitmap(true);
            f->filter(canvas, &tmpPaint, SkDrawFilter::kBitmap_Type);
            filterBitmap = tmpPaint.isFilterBitmap();
        }

        task.requestArea.x1 = -matrix.getTranslateX() + clipBound.fLeft;
        task.requestArea.y1 = -matrix.getTranslateY() + clipBound.fTop;
        task.requestArea.x2 = -matrix.getTranslateX()+ clipBound.fRight;
        task.requestArea.y2 = -matrix.getTranslateY() + clipBound.fBottom;
        task.contentScale = matrix.getScaleX();
        task.contentOrigin.fX = -matrix.getTranslateX();
        task.contentOrigin.fY = -matrix.getTranslateY();
        task.viewportWidth = bitmap.width();
        task.viewportHeight = bitmap.height();
        task.config = bitmap.getConfig();
        task.color = color;
        task.invertColor = invertColor;
        task.quality = (filterBitmap)? RenderTask::HIGH : RenderTask::LOW;
        task.valid = true;
        task.newContent = false;
    };

    void handleNewContent(RenderTask& task)
    {
        task.newContent = false;
        if (m_contentData.numContentChanged() > 0) {
            task.newContent = true;
            if (m_backingStore)
                m_backingStore->finish();
            m_contentData.changeToNewContent();
            if (m_contentData.m_invalidateAll && m_doPartialRender) {
                if (m_backingStore)
                    m_backingStore->invalidate();
                m_contentData.m_invalidateAll = false;
            }
        }
    }

    bool RenderRequest(RenderTask& request)
    {
        bool result = false;
        if (!m_backingStore)
            m_backingStore = WebTech::createBackingStore(static_cast<WebTech::IBackingStore::IUpdater*>(this));

        if (!m_backingStore)
            return false;

        if (m_backingStore->checkError()) {
            m_backingStore->release();
            m_backingStore = 0;
            return false;
        }

        m_backingStore->setParam(WebTech::IBackingStore::ALLOW_PARTIAL_RENDER, m_doPartialRender? 1 : 0);
        m_backingStore->setParam(WebTech::IBackingStore::QUALITY, request.quality == RenderTask::HIGH? 1 : 0);

        // see if we need to invalidate
        if (request.contentScale != m_request.contentScale
            || request.config != m_request.config
            || request.viewportWidth != m_request.viewportWidth
            || request.viewportHeight != m_request.viewportHeight) {
            m_backingStore->invalidate();
            m_contentData.m_contentInvalidRegion.clear();
        }

        if (m_loading != m_contentData.m_loading) {
            m_loading = m_contentData.m_loading;
            m_backingStore->setParam(WebTech::IBackingStore::PRIORITY, (m_loading)? -1 : 0);
        }

        m_request = request;
        result = m_backingStore->update(&(request.requestArea),
                                (m_contentData.m_contentInvalidRegion)? WebTech::IBackingStore::UPDATE_ALL : WebTech::IBackingStore::UPDATE_EXPOSED_ONLY,
                                request.contentOrigin.fX, request.contentOrigin.fY,
                                request.viewportWidth, request.viewportHeight,
                                static_cast<int>(ceil(m_contentData.m_contentWidth * request.contentScale)),
                                static_cast<int>(ceil(m_contentData.m_contentHeight * request.contentScale)),
                                request.newContent
                                );

        if (result) {
            m_contentData.m_contentInvalidRegion.clear();
        }

        return result;
    };

    // draw sub-region in backing store onto output
    void drawAreaToOutput(SkCanvas* srcCanvas,
        int outWidth, int outHeight, int outPitch, void* outPixels, SkBitmap::Config outConfig,
        float scale,
        SkPaint& paint,
        WebTech::IBackingStore::IDrawRegionIterator* iter,
        bool noClipping)
    {
        SkIPoint o;
        o.fX = iter->outX();
        o.fY = iter->outY();
        SkIPoint i;
        i.fX = iter->inX();
        i.fY = iter->inY();
        int width = iter->width();
        int height = iter->height();

        BackingStoreBuffer* bufferImpl = static_cast<BackingStoreBuffer*>(iter->buffer());
        const SkBitmap& backingStoreBitmap = bufferImpl->getBitmap();

        SkBitmap bitmap;
        int inPitch = backingStoreBitmap.rowBytes();
        SkBitmap::Config inConfig = backingStoreBitmap.getConfig();
        bitmap.setConfig(inConfig, width, height, inPitch);
        char* pixels = static_cast<char*>(backingStoreBitmap.getPixels());
        int bpp = backingStoreBitmap.bytesPerPixel();
        pixels = pixels + i.fY * inPitch + i.fX * bpp;
        bitmap.setPixels(static_cast<void*>(pixels));

        // do memcpy instead of using SkCanvas to draw if possible
        if (noClipping && scale == 1.0f && outConfig == inConfig) {
            if (!i.fX && !o.fX && width == outWidth && height == outHeight && outPitch == inPitch) {
                    char* optr = static_cast<char*>(outPixels);
                    optr = optr + o.fY*outPitch;
                    char* iptr = static_cast<char*>(pixels);
                    memcpy(static_cast<void*>(optr), static_cast<void*>(iptr), height*outPitch);
            } else {
                int w = width * bpp;
                int h = height;
                char* optr = static_cast<char*>(outPixels);
                optr = optr + o.fY*outPitch + o.fX*bpp;
                char* iptr = static_cast<char*>(pixels);
                for (int y = 0; y < h; ++y) {
                    memcpy(static_cast<void*>(optr), static_cast<void*>(iptr), w);
                    optr += outPitch;
                    iptr += inPitch;
                }
            }
        } else {
            SkRect rect;
            rect.set(o.fX, o.fY, o.fX + bitmap.width(), o.fY + bitmap.height());
            srcCanvas->drawBitmapRect(bitmap, 0, rect, &paint);
        }
    }

    // draw valid region received from render thread to output.  The regions can be broken down
    // into sub-regions
    bool drawResult(SkCanvas* srcCanvas, RenderTask& request)
    {
        bool ret = m_doPartialRender;
        if (!m_backingStore)
            return ret;

        bool simpleClip = false;
        const SkRegion& clip = srcCanvas->getTotalClip();
        SkIRect clipBound = clip.getBounds();
        if (clip.isRect())
            simpleClip = true;

        WebTech::IBackingStore::UpdateRegion areaToDraw = request.requestArea;
        SkIPoint contentOrigin = request.contentOrigin;
        float deltaScale = 1.0f;
        if (m_request.contentScale != request.contentScale) {
            if (request.quality >= 1) {
                LOG_RENDER("Renderer client can't zoom result in high quality.  should wait.");
                return ret;
            }
            deltaScale = m_request.contentScale / request.contentScale;
            areaToDraw.x1 *= deltaScale;
            areaToDraw.y1 *= deltaScale;
            areaToDraw.x2 *= deltaScale;
            areaToDraw.y2 *= deltaScale;
            contentOrigin.fX *= deltaScale;
            contentOrigin.fY *= deltaScale;
        }

        LOG_RENDER("drawResult.  scale = %f", deltaScale);

        WebTech::IBackingStore::UpdateRegion areaAvailable;
        WebTech::IBackingStore::RegionAvailability availability = m_backingStore->canDrawRegion(areaToDraw, areaAvailable);
        bool allDrawn;
        if (m_doPartialRender)
            allDrawn = (availability >= WebTech::IBackingStore::FULLY_AVAILABLE);
        else
            allDrawn = (availability == WebTech::IBackingStore::FULLY_AVAILABLE);

        LOG_RENDER("drawing viewport area (%d, %d) to (%d, %d).  All valid in backing store: %d",
            areaToDraw.x1, areaToDraw.y1, areaToDraw.x2, areaToDraw.y2,
            (allDrawn)?1:0);
        if (!allDrawn)
            return ret;

        SkPaint paint;
        paint.setFilterBitmap(false);
        paint.setDither(false);
        paint.setAntiAlias(false);
        paint.setColor(0xffffff);
        paint.setAlpha(255);
        paint.setXfermodeMode(SkXfermode::kSrcOver_Mode);

        srcCanvas->save();
        srcCanvas->setDrawFilter(0);

        srcCanvas->resetMatrix();

        srcCanvas->scale(1.0f / deltaScale, 1.0f / deltaScale);

        SkDevice* device = srcCanvas->getDevice();
        const SkBitmap& bitmap = device->accessBitmap(true);

        int outWidth = bitmap.width();
        int outHeight = bitmap.height();
        void* outPixels = bitmap.getPixels();
        int outPitch = bitmap.rowBytes();
        SkBitmap::Config outConfig = bitmap.getConfig();

        WebTech::IBackingStore::IDrawRegionIterator* iter = m_backingStore->beginDrawRegion(areaAvailable, contentOrigin.fX, contentOrigin.fY);
        if (iter) {
            do {
                drawAreaToOutput(srcCanvas, outWidth, outHeight, outPitch, outPixels, outConfig,
                            1.0f / deltaScale, paint, iter, simpleClip);
            } while (iter->next());
            iter->release();
            ret = true;
        } else
            ret = m_doPartialRender;

        srcCanvas->restore();

        return ret;
    }

    int suggestContentSplitting(PictureSet& content, RenderTask& request)
    {
        unsigned int numBlocks = (content.height() / (request.viewportHeight * s_config.splitSize));
        if (numBlocks>content.size()) {
            numBlocks /= content.size();
            int numSplit = 0;
            while (numBlocks>1) {
                ++numSplit;
                numBlocks = numBlocks >> 1;
            }
            LOG_RENDER("suggestContentSplitting: content length=%d.  num pictures=%d.  num split=%d",
                content.height(), content.size(), numSplit);
            return numSplit;
        }
        return 0;
    }

private:
    WebTech::IBackingStore* m_backingStore;
    ContentData m_contentData;
    RenderTask m_request;
    bool m_loading;
    bool m_doPartialRender;
};

} // namespace RendererImplNamespace

using namespace RendererImplNamespace;
namespace android {
Renderer* android::Renderer::createRenderer()
{
    return static_cast<Renderer*>(new RendererImpl());
}

}

#endif // ACCELERATED_SCROLLING
