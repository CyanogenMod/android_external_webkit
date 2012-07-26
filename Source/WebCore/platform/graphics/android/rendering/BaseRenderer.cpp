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

#define LOG_TAG "BaseRenderer"
#define LOG_NDEBUG 1

#include "config.h"
#include "BaseRenderer.h"

#if USE(ACCELERATED_COMPOSITING)

#include "AndroidLog.h"
#include "GaneshRenderer.h"
#include "GLUtils.h"
#include "InstrumentedPlatformCanvas.h"
#include "RasterRenderer.h"
#include "SkBitmap.h"
#include "SkBitmapRef.h"
#include "SkCanvas.h"
#include "SkDevice.h"
#include "SkPicture.h"
#include "SkTypeface.h"
#include "Tile.h"
#include "TilesManager.h"

#include <wtf/text/CString.h>

#define UPDATE_COUNT_MASK 0xFF // displayed count wraps at 256
#define UPDATE_COUNT_ALPHA_MASK 0x1F // alpha wraps at 32

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
        const TileRenderInfo& renderInfo, int updateCount, double renderDuration)
{
    static SkTypeface* s_typeface = 0;
    if (!s_typeface)
        s_typeface = SkTypeface::CreateFromName("", SkTypeface::kBold);
    SkPaint paint;
    paint.setTextSize(17);
    char str[256];
    snprintf(str, 256, " (%d,%d)   %.2fx   %d   %.1fms", renderInfo.x, renderInfo.y,
            renderInfo.scale, updateCount, renderDuration);
    paint.setARGB(128, 255, 255, 255);
    canvas->drawRectCoords(0, 0, renderInfo.tileSize.fWidth, 17, paint);
    paint.setARGB(255, 255, 0, 0);
    paint.setTypeface(s_typeface);
    canvas->drawText(str, strlen(str), 20, 15, paint);
}

void BaseRenderer::renderTiledContent(TileRenderInfo& renderInfo)
{
    const bool visualIndicator = TilesManager::instance()->getShowVisualIndicator();
    const SkSize& tileSize = renderInfo.tileSize;

    Color *background = renderInfo.tilePainter->background();
    InstrumentedPlatformCanvas canvas(TilesManager::instance()->tileWidth(),
                                      TilesManager::instance()->tileHeight(),
                                      background ? *background : Color::transparent);
    setupCanvas(renderInfo, &canvas);

    if (!canvas.getDevice()) {
        // TODO: consider ALOGE
        ALOGV("Error: No Device");
        return;
    }

    double before;
    if (visualIndicator) {
        canvas.save();
        before = currentTimeMS();
    }

    canvas.translate(-renderInfo.x * tileSize.width(), -renderInfo.y * tileSize.height());
    canvas.scale(renderInfo.scale, renderInfo.scale);
    renderInfo.tilePainter->paint(&canvas);

    checkForPureColor(renderInfo, canvas);

    if (visualIndicator) {
        double after = currentTimeMS();
        canvas.restore();
        unsigned int updateCount = renderInfo.tilePainter->getUpdateCount() & UPDATE_COUNT_MASK;
        const int color = updateCount & UPDATE_COUNT_ALPHA_MASK;

        // only color the invalidated area
        SkPaint paint;
        paint.setARGB(color, 0, 255, 0);
        SkIRect rect;
        rect.set(0, 0, tileSize.width(), tileSize.height());
        canvas.drawIRect(rect, paint);

        drawTileInfo(&canvas, renderInfo, updateCount, after - before);

        // paint the tile boundaries
        paint.setARGB(64, 255, 0, 0);
        paint.setStrokeWidth(3);
        canvas.drawLine(0, 0, tileSize.width(), tileSize.height(), paint);
        paint.setARGB(64, 0, 255, 0);
        canvas.drawLine(0, tileSize.height(), tileSize.width(), 0, paint);
        paint.setARGB(128, 0, 0, 255);
        canvas.drawLine(tileSize.width(), 0, tileSize.width(), tileSize.height(), paint);
    }
    renderingComplete(renderInfo, &canvas);
}

void BaseRenderer::checkForPureColor(TileRenderInfo& renderInfo, InstrumentedPlatformCanvas& canvas)
{
    renderInfo.isPureColor = canvas.isSolidColor();
    renderInfo.pureColor = canvas.solidColor();
    deviceCheckForPureColor(renderInfo, &canvas);
}

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
