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

#include "TilePainter.h"
#include "Vector.h"

class SkCanvas;
class SkRegion;

namespace WebCore {

class BaseTile;
class DualTiledTexture;
class TexturesResult;
class LayerAndroid;

class LayerGroup : public TilePainter {
public:
    LayerGroup();
    virtual ~LayerGroup();

    void initializeGroup(LayerAndroid* newLayer, const SkRegion& newLayerInval,
                         LayerAndroid* oldLayer);

    void addLayer(LayerAndroid* layer);
    void prepareGL(bool layerTilesDisabled);
    bool drawGL(bool layerTilesDisabled);
    void swapTiles();
    bool isReady();

    IntRect computePrepareArea();
    void computeTexturesAmount(TexturesResult* result);

    // TilePainter methods
    virtual bool paint(BaseTile* tile, SkCanvas* canvas, unsigned int* pictureUsed);
    virtual const TransformationMatrix* transform();
    virtual float opacity();
private:
    bool m_hasText;
    DualTiledTexture* m_dualTiledTexture;
    Vector<LayerAndroid*> m_layers;
};

} // namespace WebCore

#endif //#define LayerGroup_h
