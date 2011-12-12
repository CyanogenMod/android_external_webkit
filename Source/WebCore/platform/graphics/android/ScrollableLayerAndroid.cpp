#include "config.h"
#include "ScrollableLayerAndroid.h"

#include "GLWebViewState.h"

#if USE(ACCELERATED_COMPOSITING)

namespace WebCore {

bool ScrollableLayerAndroid::scrollTo(int x, int y)
{
    IntRect scrollBounds;
    getScrollBounds(&scrollBounds);
    if (!scrollBounds.width() && !scrollBounds.height())
        return false;
    SkScalar newX = SkScalarPin(x, scrollBounds.x(), scrollBounds.width());
    SkScalar newY = SkScalarPin(y, scrollBounds.y(), scrollBounds.height());
    // Check for no change.
    if (newX == m_offset.x() && newY == m_offset.y())
        return false;
    setScrollOffset(IntPoint(newX, newY));
    return true;
}

void ScrollableLayerAndroid::getScrollBounds(IntRect* out) const
{
    const SkPoint& pos = getPosition();
    out->setX(m_scrollLimits.fLeft - pos.fX);
    out->setY(m_scrollLimits.fTop - pos.fY);
    out->setWidth(getSize().width() - m_scrollLimits.width());
    out->setHeight(getSize().height() - m_scrollLimits.height());

}

void ScrollableLayerAndroid::getScrollRect(SkIRect* out) const
{
    const SkPoint& pos = getPosition();
    out->fLeft = m_scrollLimits.fLeft - pos.fX + m_offset.x();
    out->fTop = m_scrollLimits.fTop - pos.fY + m_offset.y();
    out->fRight = getSize().width() - m_scrollLimits.width();
    out->fBottom = getSize().height() - m_scrollLimits.height();
}

bool ScrollableLayerAndroid::scrollRectIntoView(const SkIRect& rect)
{
    // Apply the local transform to the rect to get it relative to the parent
    // layer.
    SkMatrix localTransform;
    getLocalTransform(&localTransform);
    SkRect transformedRect;
    transformedRect.set(rect);
    localTransform.mapRect(&transformedRect);

    // Test left before right to prioritize left alignment if transformedRect is wider than
    // visible area.
    int x = m_scrollLimits.fLeft;
    if (transformedRect.fLeft < m_scrollLimits.fLeft)
        x = transformedRect.fLeft;
    else if (transformedRect.fRight > m_scrollLimits.fRight)
        x = transformedRect.fRight - std::max(m_scrollLimits.width(), transformedRect.width());

    // Test top before bottom to prioritize top alignment if transformedRect is taller than
    // visible area.
    int y = m_scrollLimits.fTop;
    if (transformedRect.fTop < m_scrollLimits.fTop)
        y = transformedRect.fTop;
    else if (transformedRect.fBottom > m_scrollLimits.fBottom)
        y = transformedRect.fBottom - std::max(m_scrollLimits.height(), transformedRect.height());

    return scrollTo(x - getPosition().fX, y - getPosition().fY);
}

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
