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
#include "CachedImage.h"
#include "DrawQuadData.h"
#include "FixedPositioning.h"
#include "GLWebViewState.h"
#include "ImagesManager.h"
#include "LayerContent.h"
#include "RenderStyle.h"
#include "StyleCachedImage.h"
#include "TilesManager.h"

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

void BaseLayerAndroid::updatePositionsRecursive(const SkRect& visibleContentRect)
{
    updateLayerPositions(visibleContentRect);
    TransformationMatrix ident;
    FloatRect clip(0, 0, 1e10, 1e10);
    updateGLPositionsAndScale(ident, clip, 1, state()->scale());
}

ForegroundBaseLayerAndroid::ForegroundBaseLayerAndroid(LayerContent* content)
    : LayerAndroid((RenderLayer*)0)
{
    setIntrinsicallyComposited(true);
}

FixedBackgroundImageLayerAndroid::FixedBackgroundImageLayerAndroid(PassRefPtr<RenderStyle> aStyle,
                                                                   int w, int h)
    : LayerAndroid((RenderLayer*)0)
    , m_width(w)
    , m_height(h)
{
    RefPtr<RenderStyle> style = aStyle;
    FillLayer* layers = style->accessBackgroundLayers();
    StyleImage* styleImage = layers->image();
    CachedImage* cachedImage = static_cast<StyleCachedImage*>(styleImage)->cachedImage();
    WebCore::Image* image = cachedImage->image();
    setContentsImage(image->nativeImageForCurrentFrame());
    setSize(image->width(), image->height());

    setIntrinsicallyComposited(true);

    SkLength left, top;
    left = SkLength::convertLength(style->backgroundXPosition());
    top = SkLength::convertLength(style->backgroundYPosition());

    BackgroundImagePositioning* position = new BackgroundImagePositioning(this);
    position->setRepeatX(style->backgroundRepeatX() != WebCore::NoRepeatFill);
    position->setRepeatY(style->backgroundRepeatY() != WebCore::NoRepeatFill);

    setFixedPosition(position);
    position->setPosition(left, top);
}

FixedBackgroundImageLayerAndroid::FixedBackgroundImageLayerAndroid(const FixedBackgroundImageLayerAndroid& layer)
    : LayerAndroid(layer)
    , m_width(layer.m_width)
    , m_height(layer.m_height)
{
}

static bool needToDisplayImage(bool repeatX, bool repeatY, float dx, float dy)
{
    // handles the repeat attribute for the background image
    if (repeatX && repeatY)
        return true;
    if (repeatX && !repeatY && dy == 0)
        return true;
    if (!repeatX && repeatY && dx == 0)
        return true;
    if (dx == 0 && dy == 0)
        return true;

    return false;
}

bool FixedBackgroundImageLayerAndroid::drawGL(bool layerTilesDisabled)
{
    if (layerTilesDisabled)
        return false;
    if (!m_imageCRC)
        return false;

    ImageTexture* imageTexture = ImagesManager::instance()->retainImage(m_imageCRC);
    if (!imageTexture) {
        ImagesManager::instance()->releaseImage(m_imageCRC);
        return false;
    }

    // We have a fixed background image, let's draw it
    if (m_fixedPosition && m_fixedPosition->isBackgroundImagePositioning()) {
        BackgroundImagePositioning* position =
            static_cast<BackgroundImagePositioning*>(m_fixedPosition);

        int nbX = position->nbRepeatX();
        int nbY = position->nbRepeatY();
        float startX = position->offsetX() * getWidth();
        float startY = position->offsetY() * getHeight();

        FloatPoint origin;
        origin = drawTransform()->mapPoint(origin);

        Color backgroundColor = Color((int)SkColorGetR(m_backgroundColor),
                                      (int)SkColorGetG(m_backgroundColor),
                                      (int)SkColorGetB(m_backgroundColor),
                                      (int)SkColorGetA(m_backgroundColor));

        // Cover the entire background
        for (int i = 0; i < nbY; i++) {
            float dy = (i * getHeight()) - startY;
            for (int j = 0; j < nbX; j++) {
                float dx = (j * getWidth()) - startX;
                if (needToDisplayImage(position->repeatX(),
                                       position->repeatY(),
                                       dx, dy)) {
                    FloatPoint p(dx, dy);
                    imageTexture->drawGL(this, getOpacity(), &p);
                } else {
                    // If the image is not displayed, we still need to fill
                    // with the background color
                    SkRect rect;
                    rect.fLeft = origin.x() + dx;
                    rect.fTop = origin.y() + dy;
                    rect.fRight = rect.fLeft + getWidth();
                    rect.fBottom = rect.fTop + getHeight();
                    PureColorQuadData backgroundData(backgroundColor, BaseQuad,
                                                     0, &rect, 1.0);
                    TilesManager::instance()->shader()->drawQuad(&backgroundData);
                }
            }
        }
    } else
        imageTexture->drawGL(this, getOpacity());

    ImagesManager::instance()->releaseImage(m_imageCRC);

    return false;
}

Image* FixedBackgroundImageLayerAndroid::GetCachedImage(PassRefPtr<RenderStyle> aStyle)
{
    RefPtr<RenderStyle> style = aStyle;
    if (!style)
        return 0;

    if (!style->hasFixedBackgroundImage())
        return 0;

    FillLayer* layers = style->accessBackgroundLayers();
    StyleImage* styleImage = layers->image();

    if (!styleImage)
        return 0;

    if (!styleImage->isLoaded())
        return 0;

    if (!styleImage->isCachedImage())
        return 0;

    CachedImage* cachedImage = static_cast<StyleCachedImage*>(styleImage)->cachedImage();

    Image* image = cachedImage->image();

    if (image == Image::nullImage())
        return 0;

    return image;
}

} // namespace WebCore
