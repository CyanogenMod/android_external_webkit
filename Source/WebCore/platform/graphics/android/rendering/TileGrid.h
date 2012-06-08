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

#ifndef TileGrid_h
#define TileGrid_h

#include "IntRect.h"
#include "SkRegion.h"

#include <wtf/Vector.h>

namespace WebCore {

class Color;
class GLWebViewState;
class Tile;
class TilePainter;
class TransformationMatrix;

class TileGrid {
public:
    enum PrepareRegionFlags { EmptyRegion = 0x0, StandardRegion = 0x1, ExpandedRegion = 0x2 };

    TileGrid(bool isBaseSurface);
    virtual ~TileGrid();

    static IntRect computeTilesArea(const IntRect& contentArea, float scale);

    void prepareGL(GLWebViewState* state, float scale,
                   const IntRect& prepareArea, const IntRect& fullContentArea,
                   TilePainter* painter, int regionFlags = StandardRegion,
                   bool isLowResPrefetch = false, bool updateWithBlit = false);
    bool swapTiles();
    void drawGL(const IntRect& visibleContentArea, float opacity,
                const TransformationMatrix* transform, const Color* background = 0);

    void markAsDirty(const SkRegion& dirtyArea);

    Tile* getTile(int x, int y);

    void removeTiles();
    void discardTextures();

    bool isReady();
    bool isMissingContent();
    bool isDirty() { return !m_dirtyRegion.isEmpty(); }

    int nbTextures(const IntRect& area, float scale);
    unsigned int getImageTextureId();

private:
    void prepareTile(int x, int y, TilePainter* painter,
                     GLWebViewState* state, bool isLowResPrefetch,
                     bool isExpandPrefetch, bool shouldTryUpdateWithBlit);
    void drawMissingRegion(const SkRegion& region, float opacity, const Color* tileBackground);
    bool tryBlitFromContents(Tile* tile, TilePainter* painter);

    WTF::Vector<Tile*> m_tiles;

    IntRect m_area;

    SkRegion m_dirtyRegion;

    int m_prevTileY;
    float m_scale;

    bool m_isBaseSurface;
};

} // namespace WebCore

#endif // TileGrid_h
