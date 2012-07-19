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

#define LOG_TAG "RasterRenderer"
#define LOG_NDEBUG 1

#include "config.h"
#include "RasterRenderer.h"

#if USE(ACCELERATED_COMPOSITING)

#include "AndroidLog.h"
#include "GLUtils.h"
#include "SkBitmap.h"
#include "SkBitmapRef.h"
#include "SkCanvas.h"
#include "SkDevice.h"
#include "Tile.h"
#include "TilesManager.h"

namespace WebCore {

RasterRenderer::RasterRenderer()
  : BaseRenderer(BaseRenderer::Raster)
  , m_bitmapIsPureColor(false)
{
    m_bitmap.setConfig(SkBitmap::kARGB_8888_Config,
                       TilesManager::instance()->tileWidth(),
                       TilesManager::instance()->tileHeight());
    m_bitmap.allocPixels();
#ifdef DEBUG_COUNT
    ClassTracker::instance()->increment("RasterRenderer");
#endif
}

RasterRenderer::~RasterRenderer()
{
#ifdef DEBUG_COUNT
    ClassTracker::instance()->decrement("RasterRenderer");
#endif
}

void RasterRenderer::setupCanvas(const TileRenderInfo& renderInfo, SkCanvas* canvas)
{
    TRACE_METHOD();

    if (renderInfo.baseTile->isLayerTile()) {
        m_bitmap.setIsOpaque(false);

        // clear bitmap if necessary
        if (!m_bitmapIsPureColor || m_bitmapPureColor != Color::transparent)
            m_bitmap.eraseARGB(0, 0, 0, 0);
    } else {
        Color defaultBackground = Color::white;
        Color* background = renderInfo.tilePainter->background();
        if (!background) {
            ALOGV("No background color for base layer!");
            background = &defaultBackground;
        }
        ALOGV("setupCanvas use background on Base Layer %x", background->rgb());
        m_bitmap.setIsOpaque(!background->hasAlpha());

        // fill background color if necessary
        if (!m_bitmapIsPureColor || m_bitmapPureColor != *background)
            m_bitmap.eraseARGB(background->alpha(), background->red(),
                               background->green(), background->blue());
    }

    SkDevice* device = new SkDevice(m_bitmap);

    canvas->setDevice(device);

    device->unref();
}

void RasterRenderer::renderingComplete(const TileRenderInfo& renderInfo, SkCanvas* canvas)
{
    // We may swap the content of m_bitmap with the bitmap in the transfer queue.
    GLUtils::paintTextureWithBitmap(&renderInfo, m_bitmap);
}

void RasterRenderer::deviceCheckForPureColor(TileRenderInfo& renderInfo, SkCanvas* canvas)
{
    if (!renderInfo.isPureColor) {
        // base renderer may have already determined isPureColor, so only do the
        // brute force check if needed
        renderInfo.isPureColor = GLUtils::isPureColorBitmap(m_bitmap, renderInfo.pureColor);
    }

    m_bitmapIsPureColor = renderInfo.isPureColor;
    m_bitmapPureColor = renderInfo.pureColor;
}

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
