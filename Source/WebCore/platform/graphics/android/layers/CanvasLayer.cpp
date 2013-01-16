/*
 * Copyright 2012, The Android Open Source Project
 * Copyright (c) 2012, The Linux Foundation. All rights reserved.
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

#define LOG_TAG "CanvasLayer"
#define LOG_NDEBUG 1

#include "config.h"
#include "CanvasLayer.h"

#if USE(ACCELERATED_COMPOSITING)

#include "AndroidLog.h"
#include "CanvasTexture.h"
#include "DrawQuadData.h"
#include "Image.h"
#include "ImageBuffer.h"
#include "RenderLayerCompositor.h"
#include "SkBitmap.h"
#include "SkBitmapRef.h"
#include "SkCanvas.h"
#include "TilesManager.h"
#include "PlatformGraphicsContext.h"
#include "PlatformGraphicsContextSkia.h"
#include "CanvasLayerAndroid.h"
#include <cutils/log.h>

namespace WebCore {

std::map<int, SkBitmap*> CanvasLayer::s_recording_bitmap;
std::map<int, SkCanvas*> CanvasLayer::s_recording_canvas;
std::map<int, SkPicture*> CanvasLayer::s_recording_picture;
std::map<int, CanvasLayerAndroid*> CanvasLayer::s_gpu_canvas;
WTF::Mutex CanvasLayer::s_mutex;

CanvasLayer::CanvasLayer(RenderLayer* owner, HTMLCanvasElement* canvas)
    : LayerAndroid(owner)
    , m_canvas(canvas)
    , m_dirtyCanvas()
    , m_bitmap(0)
{
    init();
    m_canvas->addObserver(this);
    m_gpuCanvas = new CanvasLayerAndroid();
    int canvasId = uniqueId();
    m_gpuCanvas->setCanvasID(canvasId);
    m_canvas->setCanvasId(canvasId);
    // Make sure we initialize in case the canvas has already been laid out
    canvasResized(m_canvas);

    MutexLocker locker(s_mutex);
    CanvasLayer::setGpuCanvas(m_gpuCanvas, this);
}

CanvasLayer::CanvasLayer(const CanvasLayer& layer)
    : LayerAndroid(layer)
    , m_canvas(0)
    , m_bitmap(0)
    , m_gpuCanvas(0)
{
    init();
    if (!layer.m_canvas) {
        // The canvas has already been destroyed - this shouldn't happen
        ALOGW("Creating a CanvasLayer for a destroyed canvas!");
        m_visibleContentRect = IntRect();
        m_offsetFromRenderer = IntSize();
        m_texture->setHwAccelerated(false);
        return;
    }
    // We are making a copy for the UI, sync the interesting bits
    m_visibleContentRect = layer.visibleContentRect();
    m_offsetFromRenderer = layer.offsetFromRenderer();
    bool previousState = m_texture->hasValidTexture();

    if(layer.m_canvas->isUsingGpuRendering())
        return;

    ImageBuffer* imageBuffer = layer.m_canvas->buffer();
    
    if (!previousState && layer.m_dirtyCanvas.isEmpty() && imageBuffer && !(imageBuffer->drawsUsingRecording())) {
        // We were previously in software and don't have anything new to draw,
        // so stay in software
        m_bitmap = layer.bitmap();
        SkSafeRef(m_bitmap);
    } else {

        if(imageBuffer && imageBuffer->drawsUsingRecording() && !layer.m_canvas->isUsingGpuRendering())
        {
            bool canUseGpuRendering = imageBuffer->canUseGpuRendering();

            if(canUseGpuRendering && layer.m_canvas->canUseGpuRendering())
            {
                layer.m_canvas->enableGpuRendering();
                CanvasLayer::setGpuCanvasStatus(layer.uniqueId(), true);
            }
        }

        // If recording is being used
        if(imageBuffer && imageBuffer->drawsUsingRecording())
        {
            GraphicsContext* gc = imageBuffer->context();
            //SkPicture* canvasRecording = gc->platformContext()->getRecordingPicture();

            SkPicture* canvasRecording = CanvasLayer::getRecordingPicture(this);
            SkBitmap* bitmap = CanvasLayer::getRecordingBitmap(this);
            SkCanvas* canvas = CanvasLayer::getRecordingCanvas(this);

            if(canvasRecording == NULL)
                return;

            if(bitmap == NULL || bitmap->width() != canvasRecording->width()
                    || bitmap->height() != canvasRecording->height())
            {
                SkBitmap* newBitmap = new SkBitmap();
                newBitmap->setConfig(SkBitmap::kARGB_8888_Config, canvasRecording->width(), canvasRecording->height());
                newBitmap->allocPixels();
                newBitmap->eraseColor(0);
                CanvasLayer::setRecordingBitmap(newBitmap, this);
                bitmap = newBitmap;
                if(canvas != NULL)
                    canvas->setBitmapDevice(*bitmap);
            }

            if(canvas == NULL)
            {
                canvas = new SkCanvas();
                canvas->setBitmapDevice(*bitmap);
                CanvasLayer::setRecordingCanvas(canvas, this);
            }

            canvas->drawARGB(0, 0, 0, 0, SkXfermode::kClear_Mode);
            canvasRecording->draw(canvas);

            if (!m_texture->uploadImageBitmap(bitmap)) {
                //SLOGD("+++++++++++++++++++++ Didn't upload bitmap .. fall back to software");
                // TODO:: Fix this
            }
        }
        else
        {
            if (!m_texture->uploadImageBuffer(layer.m_canvas->buffer())) {
                // Blargh, no surface texture or ImageBuffer - fall back to software
                m_bitmap = layer.bitmap();
                SkSafeRef(m_bitmap);
                // Merge the canvas invals with the layer's invals to repaint the needed
                // tiles.
                SkRegion::Iterator iter(layer.m_dirtyCanvas);
                const IntPoint& offset = m_visibleContentRect.location();
                for (; !iter.done(); iter.next()) {
                    SkIRect diff = iter.rect();
                    diff.fLeft += offset.x();
                    diff.fRight += offset.x();
                    diff.fTop += offset.y();
                    diff.fBottom += offset.y();
                    m_dirtyRegion.op(diff, SkRegion::kUnion_Op);
                }
            }else{
                ImageBuffer* imageBuffer = layer.m_canvas->buffer();
                bool recordingCanvasEnabled = layer.m_canvas->isRecordingCanvasEnabled();

                if(recordingCanvasEnabled && imageBuffer && imageBuffer->isAnimating()){
                    SLOGD("[%s] Animation detected. Converting the HTML5 canvas buffer to a SkPicture.", __FUNCTION__);
                    imageBuffer->convertToRecording();
                }
            }//End of non-recording
        }
        if (previousState != m_texture->hasValidTexture()) {
            // Need to do a full inval of the canvas content as we are mode switching
            m_dirtyRegion.op(m_visibleContentRect.x(), m_visibleContentRect.y(),
                    m_visibleContentRect.maxX(), m_visibleContentRect.maxY(), SkRegion::kUnion_Op);
        }
    }
}

CanvasLayer::~CanvasLayer()
{
    if (m_canvas)
    {
        m_canvas->removeObserver(this);
        MutexLocker lock(s_mutex);
        if(m_gpuCanvas)
        {
            CanvasLayer::eraseGpuCanvas(this);
            int id = m_gpuCanvas->getCanvasID();
            CanvasLayerAndroid::markGLAssetsForRemoval(id);
            delete m_gpuCanvas;
            m_gpuCanvas = NULL;
        }

        int canvas_id = this->uniqueId();
        SkCanvas* canvas = CanvasLayer::getRecordingCanvas(this);
        if(canvas != NULL)
        {
            delete canvas;
            s_recording_canvas.erase(canvas_id);
        }

        SkBitmap* bitmap = CanvasLayer::getRecordingBitmap(this);
        if(bitmap != NULL)
        {
            delete bitmap;
            s_recording_bitmap.erase(canvas_id);
        }

        SkPicture* picture = CanvasLayer::getRecordingPicture(this);
        if(picture != NULL)
        {
            delete picture;
            s_recording_picture.erase(canvas_id);
        }

    }
    SkSafeUnref(m_bitmap);
}

void CanvasLayer::init()
{
    m_texture = CanvasTexture::getCanvasTexture(this);
}

void CanvasLayer::canvasChanged(HTMLCanvasElement*, const FloatRect& changedRect)
{
    if (!m_texture->hasValidTexture()) {
        // We only need to track invals if we aren't using a GLConsumer.
        // If we drop out of hwa, we will do a full inval anyway
        SkIRect irect = SkIRect::MakeXYWH(changedRect.x(), changedRect.y(),
                                          changedRect.width(), changedRect.height());
        m_dirtyCanvas.op(irect, SkRegion::kUnion_Op);
    }
    owningLayer()->compositor()->scheduleLayerFlush();
}

void CanvasLayer::canvasResized(HTMLCanvasElement*)
{
    const IntSize& size = m_canvas->size();
    m_dirtyCanvas.setRect(0, 0, size.width(), size.height());
    // If we are smaller than one tile, don't bother using a surface texture
    if (size.width() <= TilesManager::tileWidth()
            && size.height() <= TilesManager::tileHeight())
        m_texture->setSize(IntSize());
    else
        m_texture->setSize(size);
}

void CanvasLayer::canvasDestroyed(HTMLCanvasElement*)
{
    m_canvas = 0;
}

void CanvasLayer::clearDirtyRegion()
{
    LayerAndroid::clearDirtyRegion();
    m_dirtyCanvas.setEmpty();
    if (m_canvas)
        m_canvas->clearDirtyRect();
}

SkBitmapRef* CanvasLayer::bitmap() const
{
    if (!m_canvas || !m_canvas->buffer())
        return 0;
    return m_canvas->copiedImage()->nativeImageForCurrentFrame();
}

IntRect CanvasLayer::visibleContentRect() const
{
    if (!m_canvas
            || !m_canvas->renderer()
            || !m_canvas->renderer()->style()
            || !m_canvas->inDocument()
            || m_canvas->renderer()->style()->visibility() != VISIBLE)
        return IntRect();
    return m_canvas->renderBox()->contentBoxRect();
}

IntSize CanvasLayer::offsetFromRenderer() const
{
    return m_canvas->renderBox()->layer()->backing()->graphicsLayer()->offsetFromRenderer();
}

bool CanvasLayer::needsTexture()
{
    MutexLocker locker(s_mutex);
    CanvasLayerAndroid* gpuCanvas = CanvasLayer::getGpuCanvas(this);
    if(gpuCanvas && gpuCanvas->isGpuCanvasEnabled())
        return false;
    else
        return (m_bitmap && !masksToBounds()) || LayerAndroid::needsTexture();
}

void CanvasLayer::contentDraw(SkCanvas* canvas, PaintStyle style)
{
    LayerAndroid::contentDraw(canvas, style);
    if (!m_bitmap || masksToBounds())
        return;
    SkBitmap& bitmap = m_bitmap->bitmap();
    SkRect dst = SkRect::MakeXYWH(m_visibleContentRect.x() - m_offsetFromRenderer.width(),
                                  m_visibleContentRect.y() - m_offsetFromRenderer.height(),
                                  m_visibleContentRect.width(), m_visibleContentRect.height());
    canvas->drawBitmapRect(bitmap, 0, dst, 0);
}

bool CanvasLayer::drawGL(bool layerTilesDisabled)
{
    MutexLocker locker(s_mutex);
    CanvasLayerAndroid* gpuCanvas = CanvasLayer::getGpuCanvas(this);
    if(gpuCanvas && gpuCanvas->isGpuCanvasEnabled())
    {
        bool ret = LayerAndroid::drawGL(layerTilesDisabled);
        gpuCanvas->drawGL(layerTilesDisabled, m_drawTransform);
        return ret;
    }
    else
    {
        bool ret = LayerAndroid::drawGL(layerTilesDisabled);
        m_texture->requireTexture();
        if (!m_bitmap && m_texture->updateTexImage()) {
            SkRect rect = SkRect::MakeXYWH(m_visibleContentRect.x() - m_offsetFromRenderer.width(),
                                           m_visibleContentRect.y() - m_offsetFromRenderer.height(),
                                           m_visibleContentRect.width(), m_visibleContentRect.height());
            TextureQuadData data(m_texture->texture(), GL_TEXTURE_EXTERNAL_OES,
                                 GL_LINEAR, LayerQuad, &m_drawTransform, &rect);
            TilesManager::instance()->shader()->drawQuad(&data);
        }
        return ret;
    }
}

/******************************************
 * Recording/GPU Canvas
 * ****************************************/

LayerAndroid::InvalidateFlags CanvasLayer::onSetHwAccelerated(bool hwAccelerated)
{
    if (m_texture->setHwAccelerated(hwAccelerated))
        return LayerAndroid::InvalidateLayers;
    return LayerAndroid::InvalidateNone;
}

SkBitmap* CanvasLayer::getRecordingBitmap(CanvasLayer* layer)
{
    int canvas_id = layer->uniqueId();
    std::map<int, SkBitmap*>::iterator bit_it = s_recording_bitmap.find(canvas_id);
    if(bit_it != s_recording_bitmap.end())
        return bit_it->second;
    else
        return NULL;
}

SkCanvas* CanvasLayer::getRecordingCanvas(CanvasLayer* layer)
{
    int canvas_id = layer->uniqueId();
    std::map<int, SkCanvas*>::iterator can_it = s_recording_canvas.find(canvas_id);
    if(can_it != s_recording_canvas.end())
        return can_it->second;
    else
        return NULL;
}

SkPicture* CanvasLayer::getRecordingPicture(CanvasLayer* layer)
{
    int canvas_id = layer->uniqueId();
    std::map<int, SkPicture*>::iterator can_it = s_recording_picture.find(canvas_id);
    if(can_it != s_recording_picture.end())
        return can_it->second;
    else
        return NULL;
}

void CanvasLayer::setRecordingBitmap(SkBitmap* bitmap, CanvasLayer* layer)
{
    int canvas_id = layer->uniqueId();
    std::map<int, SkBitmap*>::iterator bit_it = s_recording_bitmap.find(canvas_id);
    if(bit_it != s_recording_bitmap.end())
    {
        SkBitmap* oldBitmap = bit_it->second;
        if(oldBitmap != NULL)
        {
            oldBitmap->reset();
            delete oldBitmap;
            oldBitmap = NULL;
            s_recording_bitmap.erase(canvas_id);
        }
    }

    s_recording_bitmap.insert(std::make_pair(canvas_id, bitmap));
}

void CanvasLayer::setRecordingCanvas(SkCanvas* canvas, CanvasLayer* layer)
{
    int canvas_id = layer->uniqueId();
    std::map<int, SkCanvas*>::iterator can_it = s_recording_canvas.find(canvas_id);
    if(can_it != s_recording_canvas.end())
    {
        SkCanvas* oldCanvas = can_it->second;
        if(oldCanvas != NULL)
        {
            delete oldCanvas;
            oldCanvas = NULL;
            s_recording_canvas.erase(canvas_id);
        }
    }

    s_recording_canvas.insert(std::make_pair(canvas_id, canvas));
}

void CanvasLayer::setRecordingPicture(SkPicture* picture, int layer_id)
{
    int canvas_id = layer_id;
    std::map<int, SkPicture*>::iterator can_it = s_recording_picture.find(canvas_id);
    if(can_it != s_recording_picture.end())
    {
        SkPicture* oldPicture = can_it->second;
        if(oldPicture != NULL)
        {
            delete oldPicture;
            oldPicture = NULL;
            s_recording_picture.erase(canvas_id);
        }
    }

    s_recording_picture.insert(std::make_pair(canvas_id, picture));
}

void CanvasLayer::setGpuCanvasStatus(int canvas_id, bool val)
{
    MutexLocker locker(s_mutex);
    CanvasLayerAndroid* gpuCanvas = CanvasLayer::getGpuCanvas(canvas_id);
    if(gpuCanvas != NULL)
    {
        gpuCanvas->setGpuCanvasStatus(val);
    }
}

void CanvasLayer::copyRecordingToLayer(GraphicsContext* ctx, IntRect& r, int canvas_id)
{
    if(ctx)
    {
        SkPicture* canvasRecording = ctx->platformContext()->getRecordingPicture();
        SkPicture dstPicture(*canvasRecording);
        IntSize size = r.size();

        MutexLocker locker(s_mutex);
        CanvasLayerAndroid* gpuCanvas = CanvasLayer::getGpuCanvas(canvas_id);
        if(gpuCanvas)
        {
            gpuCanvas->setPicture(dstPicture, size);
        }
    }
}

void CanvasLayer::copyRecording(GraphicsContext* ctx, IntRect& r, int canvas_id)
{
    if(ctx)
    {
        SkPicture* canvasRecording = ctx->platformContext()->getRecordingPicture();
        SkPicture* dstPicture = new SkPicture(*canvasRecording);
        IntSize size = r.size();

        CanvasLayer::setRecordingPicture(dstPicture, canvas_id);
    }
}
//NOTE::USE OF THE FOLLOWING FUNCTIONS WITHOUT LOCKING s_mutex is THREAD UNSAFE

CanvasLayerAndroid* CanvasLayer::getGpuCanvas(int layerId)
{
    std::map<int, CanvasLayerAndroid*>::iterator can_it = s_gpu_canvas.find(layerId);
    if(can_it != s_gpu_canvas.end())
        return can_it->second;
    else
        return NULL;
}

CanvasLayerAndroid* CanvasLayer::getGpuCanvas(CanvasLayer* layer)
{
    int canvas_id = layer->uniqueId();
    std::map<int, CanvasLayerAndroid*>::iterator can_it = s_gpu_canvas.find(canvas_id);
    if(can_it != s_gpu_canvas.end())
        return can_it->second;
    else
        return NULL;
}

void CanvasLayer::setGpuCanvas(CanvasLayerAndroid* canvas, CanvasLayer* layer)
{
    int canvas_id = layer->uniqueId();
    std::map<int, CanvasLayerAndroid*>::iterator can_it = s_gpu_canvas.find(canvas_id);
    if(can_it != s_gpu_canvas.end())
    {
        CanvasLayerAndroid* oldCanvas = can_it->second;
        if(oldCanvas != NULL)
        {
            oldCanvas = NULL;
            s_gpu_canvas.erase(canvas_id);
        }
    }
    s_gpu_canvas.insert(std::make_pair(canvas_id, canvas));
}

void CanvasLayer::eraseGpuCanvas(CanvasLayer* layer)
{
    int canvas_id = layer->uniqueId();
    std::map<int, CanvasLayerAndroid*>::iterator can_it = s_gpu_canvas.find(canvas_id);
    if(can_it != s_gpu_canvas.end())
    {
        CanvasLayerAndroid* oldCanvas = can_it->second;
        oldCanvas = NULL;
        s_gpu_canvas.erase(canvas_id);
    }
}

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
