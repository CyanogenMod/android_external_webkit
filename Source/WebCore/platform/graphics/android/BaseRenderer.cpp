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


#include "config.h"
#include "BaseRenderer.h"

#if USE(ACCELERATED_COMPOSITING)

#include "GaneshRenderer.h"
#include "GLUtils.h"
#include "RasterRenderer.h"
#include "SkBitmap.h"
#include "SkBitmapRef.h"
#include "SkCanvas.h"
#include "SkDevice.h"
#include "SkPicture.h"
#include "TilesManager.h"

#include <wtf/text/CString.h>

#include <cutils/log.h>
#include <wtf/CurrentTime.h>

#undef XLOGC
#define XLOGC(...) android_printLog(ANDROID_LOG_DEBUG, "BaseRenderer", __VA_ARGS__)

#ifdef DEBUG

#undef XLOG
#define XLOG(...) android_printLog(ANDROID_LOG_DEBUG, "BaseRenderer", __VA_ARGS__)

#else

#undef XLOG
#define XLOG(...)

#endif // DEBUG

#define UPDATE_COUNT_MASK 0xFF // displayed count wraps at 256
#define UPDATE_COUNT_ALPHA_MASK 0x3F // alpha wraps at 64

namespace WebCore {

BaseRenderer::RendererType BaseRenderer::g_currentType = BaseRenderer::Raster;

BaseRenderer* BaseRenderer::createRenderer()
{
    if (g_currentType == Raster)
        return new RasterRenderer();
    else if (g_currentType == Ganesh)
        return new GaneshRenderer();
    return NULL;
}

void BaseRenderer::swapRendererIfNeeded(BaseRenderer*& renderer)
{
    if (renderer->getType() == g_currentType)
        return;

    delete renderer;
    renderer = createRenderer();
}

void BaseRenderer::drawTileInfo(SkCanvas* canvas,
        const TileRenderInfo& renderInfo, int updateCount)
{
    SkPaint paint;
    char str[256];
    snprintf(str, 256, "(%d,%d) %.2f, tl%x p%x c%d", renderInfo.x, renderInfo.y,
            renderInfo.scale, this, renderInfo.tilePainter, updateCount);
    paint.setARGB(255, 0, 0, 0);
    canvas->drawText(str, strlen(str), 0, 10, paint);
    paint.setARGB(255, 255, 0, 0);
    canvas->drawText(str, strlen(str), 0, 11, paint);

    int tagCount = 0;
    const String* tags = getPerformanceTags(tagCount);

    float total = 0;
    for (int i = 0; i < tagCount; i++) {
        float tagDuration = m_perfMon.getAverageDuration(tags[i]);
        total += tagDuration;
        snprintf(str, 256, "%s: %.2f", tags[i].utf8().data(), tagDuration);
        paint.setARGB(255, 0, 0, 0);
        int textY = (i * 12) + 25;
        canvas->drawText(str, strlen(str), 0, textY, paint);
        paint.setARGB(255, 255, 0, 0);
        canvas->drawText(str, strlen(str), 0, textY + 1, paint);
    }
    snprintf(str, 256, "total: %.2f", total);
    paint.setARGB(255, 0, 0, 0);
    int textY = (tagCount * 12) + 30;
    canvas->drawText(str, strlen(str), 0, textY, paint);
    paint.setARGB(255, 255, 0, 0);
    canvas->drawText(str, strlen(str), 0, textY + 1, paint);
}

void BaseRenderer::renderTiledContent(const TileRenderInfo& renderInfo)
{
    const bool visualIndicator = TilesManager::instance()->getShowVisualIndicator();
    const SkSize& tileSize = renderInfo.tileSize;

    SkCanvas canvas;
    setupCanvas(renderInfo, &canvas);

    if (!canvas.getDevice()) {
        XLOG("Error: No Device");
        return;
    }

    if (visualIndicator)
        canvas.save();

    setupPartialInval(renderInfo, &canvas);
    canvas.translate(-renderInfo.x * tileSize.width(), -renderInfo.y * tileSize.height());
    canvas.scale(renderInfo.scale, renderInfo.scale);
    renderInfo.tilePainter->paint(renderInfo.baseTile, &canvas);

    if (visualIndicator) {
        canvas.restore();
        unsigned int updateCount = renderInfo.tilePainter->getUpdateCount() & UPDATE_COUNT_MASK;
        const int color = updateCount & UPDATE_COUNT_ALPHA_MASK;

        // only color the invalidated area
        SkPaint invalPaint;
        invalPaint.setARGB(color, 0, 255, 0);
        if (renderInfo.invalRect)
            canvas.drawIRect(*renderInfo.invalRect, invalPaint);
        else {
            SkIRect rect;
            rect.set(0, 0, tileSize.width(), tileSize.height());
            canvas.drawIRect(rect, invalPaint);
        }

        // paint the tile boundaries
        SkPaint paint;
        paint.setARGB(128, 255, 0, 0);
        paint.setStrokeWidth(3);
        canvas.drawLine(0, 0, tileSize.width(), tileSize.height(), paint);
        paint.setARGB(128, 0, 255, 0);
        canvas.drawLine(0, tileSize.height(), tileSize.width(), 0, paint);
        paint.setARGB(128, 0, 0, 255);
        canvas.drawLine(0, 0, tileSize.width(), 0, paint);
        canvas.drawLine(tileSize.width(), 0, tileSize.width(), tileSize.height(), paint);

        if (renderInfo.invalRect) {
            // if partial inval...
            int x = renderInfo.invalRect->fLeft;
            int y = renderInfo.invalRect->fTop;
            int w = renderInfo.invalRect->width();
            int h = renderInfo.invalRect->height();

            paint.setARGB(128, 255, 255, 0);
            canvas.drawLine(x, y, x + w, y + h, paint);
            canvas.drawLine(x, y + h, x + w, y, paint);
        }

        if (renderInfo.measurePerf)
            drawTileInfo(&canvas, renderInfo, updateCount);
    }
    renderingComplete(renderInfo, &canvas);
}

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
