#define LOG_TAG "ScrollableLayerAndroid"
#define LOG_NDEBUG 1

#include "config.h"
#include "ScrollableLayerAndroid.h"

#include "GLWebViewState.h"

#if USE(ACCELERATED_COMPOSITING)

#include "AndroidLog.h"

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
    if (newX == getScrollOffset().x() && newY == getScrollOffset().y())
        return false;
    setScrollOffset(IntPoint(newX, newY));
    return true;
}

void ScrollableLayerAndroid::getScrollBounds(IntRect* out) const
{
    out->setX(m_scrollLimits.fLeft);
    out->setY(m_scrollLimits.fTop);
    out->setWidth(m_scrollLimits.width());
    out->setHeight(m_scrollLimits.height());
}

void ScrollableLayerAndroid::getScrollRect(SkIRect* out) const
{
    out->fLeft = getScrollOffset().x();
    out->fTop = getScrollOffset().y();

    out->fRight = m_scrollLimits.width();
    out->fBottom = m_scrollLimits.height();
}

void ScrollableLayerAndroid::setScrollLimits(float minX, float minY,
                                             float maxX, float maxY)
{
    if (minX < 0) minX = 0;
    if (minY < 0) minY = 0;
    if (maxX < 0) maxX = 0;
    if (maxY < 0) maxY = 0;
    if (minX > maxX) minX = maxX;
    if (minY > maxY) minY = maxY;
    m_scrollLimits.set(minX, minY, minX + maxX, minY + maxY);
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
