/*
 * Copyright 2012, The Android Open Source Project
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

namespace WebCore {

CanvasLayer::CanvasLayer(RenderLayer* owner, HTMLCanvasElement* canvas)
    : LayerAndroid(owner)
    , m_canvas(canvas)
    , m_dirtyCanvas()
    , m_bitmap(0)
{
    init();
    m_canvas->addObserver(this);
    // Make sure we initialize in case the canvas has already been laid out
    canvasResized(m_canvas);
}

CanvasLayer::CanvasLayer(const CanvasLayer& layer)
    : LayerAndroid(layer)
    , m_canvas(0)
    , m_bitmap(0)
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
    if (!previousState && layer.m_dirtyCanvas.isEmpty()) {
        // We were previously in software and don't have anything new to draw,
        // so stay in software
        m_bitmap = layer.bitmap();
        SkSafeRef(m_bitmap);
    } else {
        // Attempt to upload to a surface texture
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
        m_canvas->removeObserver(this);
    SkSafeUnref(m_bitmap);
}

void CanvasLayer::init()
{
    m_texture = CanvasTexture::getCanvasTexture(this);
}

void CanvasLayer::canvasChanged(HTMLCanvasElement*, const FloatRect& changedRect)
{
    if (!m_texture->hasValidTexture()) {
        // We only need to track invals if we aren't using a SurfaceTexture.
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

LayerAndroid::InvalidateFlags CanvasLayer::onSetHwAccelerated(bool hwAccelerated)
{
    if (m_texture->setHwAccelerated(hwAccelerated))
        return LayerAndroid::InvalidateLayers;
    return LayerAndroid::InvalidateNone;
}

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
