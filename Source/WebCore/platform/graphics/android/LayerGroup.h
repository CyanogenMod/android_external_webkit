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

#ifndef LayerGroup_h
#define LayerGroup_h

#include "Color.h"
#include "IntRect.h"
#include "TilePainter.h"
#include "Vector.h"

class SkCanvas;
class SkRegion;

namespace WebCore {

class BaseTile;
class DualTiledTexture;
class LayerAndroid;
class TexturesResult;

class LayerGroup : public TilePainter {
public:
    LayerGroup();
    virtual ~LayerGroup();

    bool tryUpdateLayerGroup(LayerGroup* oldLayerGroup);

    void addLayer(LayerAndroid* layer, const TransformationMatrix& transform);
    void prepareGL(bool layerTilesDisabled);
    bool drawGL(bool layerTilesDisabled);
    void swapTiles();
    bool isReady();

    void computeTexturesAmount(TexturesResult* result);

    LayerAndroid* getFirstLayer() { return m_layers[0]; }
    bool needsTexture() { return m_needsTexture; }
    bool hasText() { return m_hasText; }
    bool isBase();
    void setBackground(Color background) { m_background = background; }

    // TilePainter methods
    virtual bool paint(BaseTile* tile, SkCanvas* canvas);
    virtual float opacity();
    virtual Color* background();

private:
    IntRect computePrepareArea();
    IntRect visibleArea();
    IntRect unclippedArea();
    bool singleLayer() { return m_layers.size() == 1; }
    void updateBackground(const Color& background);
    bool useAggressiveRendering();

    const TransformationMatrix* drawTransform();
    IntRect m_unclippedArea;
    TransformationMatrix m_drawTransform;

    DualTiledTexture* m_dualTiledTexture;
    bool m_needsTexture;
    bool m_hasText;
    Vector<LayerAndroid*> m_layers;

    Color m_background;
};

class LayerMergeState {
public:
    LayerMergeState(Vector<LayerGroup*>* const allGroups)
        : groupList(allGroups)
        , currentLayerGroup(0)
        , nonMergeNestedLevel(-1) // start at -1 to ignore first LayerAndroid's clipping
        , depth(0)
        {}

    // vector storing all generated layer groups
    Vector<LayerGroup*>* const groupList;

    // currently merging group. if cleared, no more layers may join
    LayerGroup* currentLayerGroup;

    // records depth within non-mergeable parents (clipping, fixed, scrolling)
    // and disable merging therein.
    int nonMergeNestedLevel;

    // counts layer tree depth for debugging
    int depth;
};

} // namespace WebCore

#endif //#define LayerGroup_h
