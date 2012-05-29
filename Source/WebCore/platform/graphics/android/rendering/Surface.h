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

#ifndef Surface_h
#define Surface_h

#include "Color.h"
#include "IntRect.h"
#include "TilePainter.h"
#include "Vector.h"

class SkCanvas;
class SkRegion;

namespace WebCore {

class Tile;
class SurfaceBacking;
class LayerAndroid;
class TexturesResult;

class Surface : public TilePainter {
public:
    Surface();
    virtual ~Surface();

    bool tryUpdateSurface(Surface* oldSurface);

    void addLayer(LayerAndroid* layer, const TransformationMatrix& transform);
    void prepareGL(bool layerTilesDisabled, bool updateWithBlit);
    bool drawGL(bool layerTilesDisabled);
    void swapTiles(bool calculateFrameworkInvals);
    void addFrameworkInvals();
    bool isReady();
    bool isMissingContent();
    bool canUpdateWithBlit();

    void computeTexturesAmount(TexturesResult* result);

    LayerAndroid* getFirstLayer() const { return m_layers[0]; }
    bool needsTexture() { return m_needsTexture; }
    bool hasText() { return m_hasText; }
    bool isBase();

    // don't allow transform fudging for merged layers - they need the transform
    // static at paint time, and are always aligned to 0,0 doc coordinates.
    bool allowTransformFudging() const { return singleLayer(); }

    // TilePainter methods
    virtual bool paint(SkCanvas* canvas);
    virtual float opacity();
    virtual Color* background();
    virtual bool blitFromContents(Tile* tile);

private:
    IntRect computePrepareArea();
    IntRect visibleContentArea(bool force3dContentVisible = false) const;
    IntRect fullContentArea();
    bool singleLayer() const { return m_layers.size() == 1; }
    bool useAggressiveRendering();

    const TransformationMatrix* drawTransform();
    IntRect m_fullContentArea;
    TransformationMatrix m_drawTransform;

    SurfaceBacking* m_surfaceBacking;
    bool m_needsTexture;
    bool m_hasText;
    Vector<LayerAndroid*> m_layers;

    Color m_background;
};

class LayerMergeState {
public:
    LayerMergeState(Vector<Surface*>* const allGroups)
        : surfaceList(allGroups)
        , currentSurface(0)
        , nonMergeNestedLevel(0)
        , depth(0)
        {}

    // vector storing all generated layer groups
    Vector<Surface*>* const surfaceList;

    // currently merging group. if cleared, no more layers may join
    Surface* currentSurface;

    // records depth within non-mergeable parents (clipping, fixed, scrolling)
    // and disable merging therein.
    int nonMergeNestedLevel;

    // counts layer tree depth for debugging
    int depth;
};

} // namespace WebCore

#endif //#define Surface_h
