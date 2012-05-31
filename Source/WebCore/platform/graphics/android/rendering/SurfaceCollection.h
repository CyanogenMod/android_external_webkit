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

#ifndef SurfaceCollection_h
#define SurfaceCollection_h

#include "Color.h"
#include "SkRect.h"
#include "SkRefCnt.h"

#include <wtf/Vector.h>

class SkCanvas;
class SkRegion;

namespace WebCore {

class BaseLayerAndroid;
class Surface;
class TexturesResult;

class SurfaceCollection : public SkRefCnt {
// TODO: investigate webkit threadsafe ref counting
public:
    SurfaceCollection(BaseLayerAndroid* compositedRoot);
    virtual ~SurfaceCollection();

    // Tiled painting methods (executed on Surfaces)
    void prepareGL(const SkRect& visibleContentRect, bool tryToFastBlit = false);
    bool drawGL(const SkRect& visibleContentRect);
    Color getBackgroundColor();
    void swapTiles();
    void addFrameworkInvals();
    bool isReady();
    bool isMissingBackgroundContent();
    void removePainterOperations();
    void computeTexturesAmount(TexturesResult* result);

    // Recursive tree methods (animations, invals, etc)
    void setIsPainting(SurfaceCollection* drawingSurfaceCollection);
    void setIsDrawing();
    void mergeInvalsInto(SurfaceCollection* replacementSurfaceCollection);
    bool evaluateAnimations(double currentTime);

    bool hasCompositedLayers();
    bool hasCompositedAnimations();
    void updateScrollableLayer(int layerId, int x, int y);

    int size() { return m_surfaces.size(); }
    int backedSize();

private:
    void updateLayerPositions(const SkRect& visibleContentRect);
    BaseLayerAndroid* const m_compositedRoot;
    WTF::Vector<Surface*> m_surfaces;
};

} // namespace WebCore

#endif //#define SurfaceCollection_h
