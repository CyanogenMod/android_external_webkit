/*
 * Copyright 2010, The Android Open Source Project
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

#ifndef BaseLayerAndroid_h
#define BaseLayerAndroid_h

#include <utils/threads.h>

#include "Color.h"
#include "Layer.h"
#include "PictureSet.h"
#include "LayerContent.h"

namespace WebCore {

class TiledPage;

class BaseLayerAndroid : public Layer {

public:
    enum ScrollState {
        NotScrolling = 0,
        Scrolling = 1,
        ScrollingFinishPaint = 2
    };

    BaseLayerAndroid();
    virtual ~BaseLayerAndroid();

#if USE(ACCELERATED_COMPOSITING)
    void setBackgroundColor(Color& color) { m_color = color; }
    Color getBackgroundColor() { return m_color; }
#endif
    void setContent(LayerContent* content);
    LayerContent* content() { return m_content; }

    // This method will paint using the current PictureSet onto
    // the passed canvas. We used it to paint the GL tiles as well as
    // WebView::copyBaseContentToPicture(), so a lock is necessary as
    // we are running in different threads.
    virtual bool drawCanvas(SkCanvas* canvas);

    void updateLayerPositions(const SkRect& visibleRect);
    void prepareGL(const SkRect& visibleRect, float scale, double currentTime);
    void drawGL(float scale);

    // rendering asset management
    void swapTiles();
    void setIsDrawing(bool isDrawing);
    void setIsPainting();
    void mergeInvalsInto(BaseLayerAndroid* replacementLayer);
    bool isReady();

private:
#if USE(ACCELERATED_COMPOSITING)
    void prefetchBasePicture(const SkRect& viewport, float currentScale,
                             TiledPage* prefetchTiledPage, bool draw);
    void drawBasePictureInGL();

    android::Mutex m_drawLock;
    Color m_color;
#endif
    LayerContent* m_content;

    ScrollState m_scrollState;
};

} // namespace WebCore

#endif // BaseLayerAndroid_h
