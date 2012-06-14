#define LOG_TAG "FixedPositioning"
#define LOG_NDEBUG 1

#include "config.h"
#include "FixedPositioning.h"

#include "AndroidLog.h"
#include "DumpLayer.h"
#include "IFrameLayerAndroid.h"
#include "TilesManager.h"
#include "SkCanvas.h"

#if USE(ACCELERATED_COMPOSITING)

namespace WebCore {

// Called when copying the layer tree to the UI
FixedPositioning::FixedPositioning(LayerAndroid* layer, const FixedPositioning& position)
        : m_layer(layer)
        , m_fixedLeft(position.m_fixedLeft)
        , m_fixedTop(position.m_fixedTop)
        , m_fixedRight(position.m_fixedRight)
        , m_fixedBottom(position.m_fixedBottom)
        , m_fixedMarginLeft(position.m_fixedMarginLeft)
        , m_fixedMarginTop(position.m_fixedMarginTop)
        , m_fixedMarginRight(position.m_fixedMarginRight)
        , m_fixedMarginBottom(position.m_fixedMarginBottom)
        , m_fixedRect(position.m_fixedRect)
        , m_renderLayerPos(position.m_renderLayerPos)
{
}

SkRect FixedPositioning::getViewport(SkRect aViewport, IFrameLayerAndroid* parentIframeLayer)
{
    // So if this is a fixed layer inside a iframe, use the iframe offset
    // and the iframe's size as the viewport and pass to the children
    if (parentIframeLayer)
        return SkRect::MakeXYWH(parentIframeLayer->iframeOffset().x(),
                                parentIframeLayer->iframeOffset().y(),
                                parentIframeLayer->getSize().width(),
                                parentIframeLayer->getSize().height());
    return aViewport;
}

// Executed on the UI
IFrameLayerAndroid* FixedPositioning::updatePosition(SkRect aViewport,
                                                     IFrameLayerAndroid* parentIframeLayer)
{
    SkRect viewport = getViewport(aViewport, parentIframeLayer);

    float w = viewport.width();
    float h = viewport.height();
    float dx = viewport.fLeft;
    float dy = viewport.fTop;
    float x = dx;
    float y = dy;

    // It turns out that when it is 'auto', we should use the webkit value
    // from the original render layer's X,Y, that will take care of alignment
    // with the parent's layer and fix Margin etc.
    if (!(m_fixedLeft.defined() || m_fixedRight.defined()))
        x += m_renderLayerPos.x();
    else if (m_fixedLeft.defined() || !m_fixedRight.defined())
        x += m_fixedMarginLeft.calcFloatValue(w) + m_fixedLeft.calcFloatValue(w) - m_fixedRect.fLeft;
    else
        x += w - m_fixedMarginRight.calcFloatValue(w) - m_fixedRight.calcFloatValue(w) - m_fixedRect.fRight;

    if (!(m_fixedTop.defined() || m_fixedBottom.defined()))
        y += m_renderLayerPos.y();
    else if (m_fixedTop.defined() || !m_fixedBottom.defined())
        y += m_fixedMarginTop.calcFloatValue(h) + m_fixedTop.calcFloatValue(h) - m_fixedRect.fTop;
    else
        y += h - m_fixedMarginBottom.calcFloatValue(h) - m_fixedBottom.calcFloatValue(h) - m_fixedRect.fBottom;

    m_layer->setPosition(x, y);

    return parentIframeLayer;
}

void FixedPositioning::contentDraw(SkCanvas* canvas, Layer::PaintStyle style)
{
    if (TilesManager::instance()->getShowVisualIndicator()) {
        SkPaint paint;
        paint.setARGB(80, 255, 0, 0);
        canvas->drawRect(m_fixedRect, paint);
    }
}

void FixedPositioning::dumpLayer(LayerDumper* dumper) const
{
    dumper->writeLength("fixedLeft", m_fixedLeft);
    dumper->writeLength("fixedTop", m_fixedTop);
    dumper->writeLength("fixedRight", m_fixedRight);
    dumper->writeLength("fixedBottom", m_fixedBottom);
    dumper->writeLength("fixedMarginLeft", m_fixedMarginLeft);
    dumper->writeLength("fixedMarginTop", m_fixedMarginTop);
    dumper->writeLength("fixedMarginRight", m_fixedMarginRight);
    dumper->writeLength("fixedMarginBottom", m_fixedMarginBottom);
    dumper->writeRect("fixedRect", m_fixedRect);
}

BackgroundImagePositioning::BackgroundImagePositioning(LayerAndroid* layer, const BackgroundImagePositioning& position)
        : FixedPositioning(layer, position)
        , m_repeatX(position.m_repeatX)
        , m_repeatY(position.m_repeatY)
        , m_nbRepeatX(position.m_nbRepeatX)
        , m_nbRepeatY(position.m_nbRepeatY)
        , m_offsetX(position.m_offsetX)
        , m_offsetY(position.m_offsetY)
{
}

// Executed on the UI
IFrameLayerAndroid* BackgroundImagePositioning::updatePosition(SkRect aViewport,
                                                               IFrameLayerAndroid* parentIframeLayer)
{
    SkRect viewport = getViewport(aViewport, parentIframeLayer);

    float w = viewport.width() - m_layer->getWidth();
    float h = viewport.height() - m_layer->getHeight();
    float x = 0;
    float y = 0;

    if (m_fixedLeft.defined())
        x += m_fixedLeft.calcFloatValue(w);
    if (m_fixedTop.defined())
        y += m_fixedTop.calcFloatValue(h);

    m_nbRepeatX = ceilf((viewport.width() / m_layer->getWidth()) + 1);
    m_offsetX = ceilf(x / m_layer->getWidth());

    m_nbRepeatY = ceilf((viewport.height() / m_layer->getHeight()) + 1);
    m_offsetY = ceilf(y / m_layer->getHeight());

    x += viewport.fLeft;
    y += viewport.fTop;

    m_layer->setPosition(x, y);

    return parentIframeLayer;
}

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
