#include "config.h"
#include "FixedLayerAndroid.h"
#include "DumpLayer.h"
#include "TilesManager.h"

#include "GLWebViewState.h"

#if USE(ACCELERATED_COMPOSITING)

#include <wtf/CurrentTime.h>
#include <cutils/log.h>
#include <wtf/text/CString.h>
#define XLOGC(...) android_printLog(ANDROID_LOG_DEBUG, "FixedLayerAndroid", __VA_ARGS__)

namespace WebCore {

FixedLayerAndroid::FixedLayerAndroid(const FixedLayerAndroid& layer)
        : LayerAndroid(layer, LayerAndroid::FixedLayer)
        , m_fixedLeft(layer.m_fixedLeft)
        , m_fixedTop(layer.m_fixedTop)
        , m_fixedRight(layer.m_fixedRight)
        , m_fixedBottom(layer.m_fixedBottom)
        , m_fixedMarginLeft(layer.m_fixedMarginLeft)
        , m_fixedMarginTop(layer.m_fixedMarginTop)
        , m_fixedMarginRight(layer.m_fixedMarginRight)
        , m_fixedMarginBottom(layer.m_fixedMarginBottom)
        , m_fixedRect(layer.m_fixedRect)
        , m_renderLayerPos(layer.m_renderLayerPos)
{
}

LayerAndroid* FixedLayerAndroid::updateFixedLayerPosition(SkRect viewport,
                                                          LayerAndroid* parentIframeLayer)
{
    LayerAndroid* iframeLayer = LayerAndroid::updateFixedLayerPosition(viewport, parentIframeLayer);

    // So if this is a fixed layer inside a iframe, use the iframe offset
    // and the iframe's size as the viewport and pass to the children
    if (iframeLayer) {
        viewport = SkRect::MakeXYWH(iframeLayer->iframeOffset().x(),
                             iframeLayer->iframeOffset().y(),
                             iframeLayer->getSize().width(),
                             iframeLayer->getSize().height());
    }
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

    this->setPosition(x, y);

    return iframeLayer;
}

void FixedLayerAndroid::contentDraw(SkCanvas* canvas, PaintStyle style)
{
    LayerAndroid::contentDraw(canvas, style);
    if (TilesManager::instance()->getShowVisualIndicator()) {
        SkPaint paint;
        paint.setARGB(80, 255, 0, 0);
        canvas->drawRect(m_fixedRect, paint);
    }
}

void writeLength(FILE* file, int indentLevel, const char* str, SkLength length)
{
    if (!length.defined())
        return;
    writeIndent(file, indentLevel);
    fprintf(file, "%s = { type = %d; value = %.2f; };\n", str, length.type, length.value);
}

void FixedLayerAndroid::dumpLayer(FILE* file, int indentLevel) const
{
    writeLength(file, indentLevel + 1, "fixedLeft", m_fixedLeft);
    writeLength(file, indentLevel + 1, "fixedTop", m_fixedTop);
    writeLength(file, indentLevel + 1, "fixedRight", m_fixedRight);
    writeLength(file, indentLevel + 1, "fixedBottom", m_fixedBottom);
    writeLength(file, indentLevel + 1, "fixedMarginLeft", m_fixedMarginLeft);
    writeLength(file, indentLevel + 1, "fixedMarginTop", m_fixedMarginTop);
    writeLength(file, indentLevel + 1, "fixedMarginRight", m_fixedMarginRight);
    writeLength(file, indentLevel + 1, "fixedMarginBottom", m_fixedMarginBottom);
    writeRect(file, indentLevel + 1, "fixedRect", m_fixedRect);
}

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
