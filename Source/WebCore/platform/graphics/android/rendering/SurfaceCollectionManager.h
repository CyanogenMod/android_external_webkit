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

#ifndef SurfaceCollectionManager_h
#define SurfaceCollectionManager_h

#include "TestExport.h"
#include <utils/threads.h>

class SkRect;
class SkCanvas;

namespace WebCore {

class GLWebViewState;
class IntRect;
class TexturesResult;
class SurfaceCollection;

class TEST_EXPORT SurfaceCollectionManager {
public:
    SurfaceCollectionManager();

    ~SurfaceCollectionManager();

    bool updateWithSurfaceCollection(SurfaceCollection* collection, bool brandNew);

    void updateScrollableLayer(int layerId, int x, int y);

    int drawGL(double currentTime, IntRect& viewRect,
                SkRect& visibleContentRect, float scale,
                bool scrolling, bool singleSurfaceMode,
                bool* collectionsSwappedPtr, bool* newCollectionHasAnimPtr,
                TexturesResult* texturesResultPtr, bool shouldDraw);

private:
    void swap();
    void clearCollections();
    void updatePaintingCollection(SurfaceCollection* newCollection);
    int singleSurfaceModeInvalidation(bool hasRunningAnimation, bool scrolling, bool shouldDraw);
    SurfaceCollection* m_drawingCollection;
    SurfaceCollection* m_paintingCollection;
    SurfaceCollection* m_queuedCollection;

    bool m_fastSwapMode;
    // Used in single surface mode only. True if the previous frame is scrolling.
    bool m_previouslyScrolling;
    // Used in single surface mode only. True if there is a new painting tree
    // added for the current frame.
    bool m_newPaintingCollection;
};

} // namespace WebCore

#endif //#define SurfaceCollectionManager_h
