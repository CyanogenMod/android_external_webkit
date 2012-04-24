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

#define LOG_TAG "BaseLayerAndroid"
#define LOG_NDEBUG 1

#include "config.h"
#include "BaseLayerAndroid.h"

#include "AndroidLog.h"
#include "FixedPositioning.h"
#include "GLWebViewState.h"
#include "LayerContent.h"

namespace WebCore {

// Note: this must match the use of ID 0 specifying the base layer in DrawExtra
#define BASE_UNIQUE_ID 0

BaseLayerAndroid::BaseLayerAndroid(LayerContent* content)
    : LayerAndroid((RenderLayer*)0)
    , m_color(Color::white)
{
    if (content) {
        setContent(content);
        setSize(content->width(), content->height());
    }
    m_uniqueId = BASE_UNIQUE_ID;
}

void BaseLayerAndroid::getLocalTransform(SkMatrix* matrix) const
{
    // base layer doesn't use size in transform calculation
    matrix->preConcat(getMatrix());
}

IFrameLayerAndroid* BaseLayerAndroid::updatePosition(SkRect viewport,
                                                     IFrameLayerAndroid* parentIframeLayer)
{
    if (viewport.fRight > getWidth() || viewport.fBottom > getHeight()) {
        // To handle the viewport expanding past the layer's size with HW accel,
        // expand the size of the layer, so that tiles will cover the viewport.
        setSize(std::max(viewport.fRight, getWidth()),
                std::max(viewport.fBottom, getHeight()));
    }

    return LayerAndroid::updatePosition(viewport, parentIframeLayer);
}

ForegroundBaseLayerAndroid::ForegroundBaseLayerAndroid(LayerContent* content)
    : LayerAndroid((RenderLayer*)0)
{
    setIntrinsicallyComposited(true);
}

FixedBackgroundBaseLayerAndroid::FixedBackgroundBaseLayerAndroid(LayerContent* content)
    : LayerAndroid((RenderLayer*)0)
{
    if (content) {
        setContent(content);
        setSize(content->width(), content->height());
    }
    setIntrinsicallyComposited(true);

    // TODO: add support for fixed positioning attributes
    SkRect viewRect;
    SkLength left, top, right, bottom;
    left.setFixedValue(0);
    top.setFixedValue(0);
    right.setAuto();
    bottom.setAuto();
    SkLength marginLeft, marginTop, marginRight, marginBottom;
    marginLeft.setAuto();
    marginTop.setAuto();
    marginRight.setAuto();
    marginBottom.setAuto();

    viewRect.set(0, 0, content->width(), content->height());
    FixedPositioning* fixedPosition = new FixedPositioning(this);
    setFixedPosition(fixedPosition);
    fixedPosition->setFixedPosition(left, top, right, bottom,
                                    marginLeft, marginTop,
                                    marginRight, marginBottom,
                                    IntPoint(0, 0), viewRect);
}

} // namespace WebCore
