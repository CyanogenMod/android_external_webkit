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

#ifndef SurfaceBacking_h
#define SurfaceBacking_h

#include "SkRefCnt.h"
#include "TileGrid.h"

namespace WebCore {

class LayerAndroid;
class TexturesResult;
class TilePainter;

class SurfaceBacking : public SkRefCnt {
// TODO: investigate webkit threadsafe ref counting
public:
    SurfaceBacking(bool isBaseSurface);
    ~SurfaceBacking();
    void prepareGL(GLWebViewState* state, float maxZoomScale,
                   const IntRect& prepareArea, const IntRect& fullContentArea,
                   TilePainter* painter, bool aggressiveRendering,
                   bool updateWithBlit);
    bool swapTiles();
    void drawGL(const IntRect& visibleContentArea, float opacity,
                const TransformationMatrix* transform, bool aggressiveRendering,
                const Color* background);
    void markAsDirty(const SkRegion& dirtyArea);
    void computeTexturesAmount(TexturesResult* result,
                               const IntRect& visibleContentArea,
                               const IntRect& fullContentArea,
                               LayerAndroid* layer);
    void discardTextures()
    {
        m_frontTileGrid->discardTextures();
        m_backTileGrid->discardTextures();
    }
    bool isReady()
    {
        return !m_zooming && m_frontTileGrid->isReady() && m_scale > 0;
    }

    bool isDirty()
    {
        return m_frontTileGrid->isDirty();
    }

    bool isMissingContent()
    {
        return m_zooming || m_frontTileGrid->isMissingContent();
    }

    int nbTextures(IntRect& area, float scale)
    {
        // TODO: consider the zooming case for the backTileGrid
        if (!m_frontTileGrid)
            return 0;
        return m_frontTileGrid->nbTextures(area, scale);
    }

private:
    void swapTileGrids();

    // Delay before we schedule a new tile at the new scale factor
    static const double s_zoomUpdateDelay = 0.1; // 100 ms

    TileGrid* m_frontTileGrid;
    TileGrid* m_backTileGrid;
    TileGrid* m_lowResTileGrid;

    float m_scale;
    float m_futureScale;
    double m_zoomUpdateTime;
    bool m_zooming;
    float m_maxZoomScale;
};

} // namespace WebCore

#endif // SurfaceBacking_h
