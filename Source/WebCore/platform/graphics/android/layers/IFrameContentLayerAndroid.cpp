#define LOG_TAG "IFrameContentLayerAndroid"
#define LOG_NDEBUG 1

#include "config.h"
#include "IFrameContentLayerAndroid.h"

#include "AndroidLog.h"

#if USE(ACCELERATED_COMPOSITING)

namespace WebCore {

bool IFrameContentLayerAndroid::scrollTo(int x, int y)
{
    IntRect scrollBounds;
    getScrollBounds(&scrollBounds);
    if (!scrollBounds.width() && !scrollBounds.height())
        return false;
    SkScalar newX = SkScalarPin(x, scrollBounds.x(), scrollBounds.width());
    SkScalar newY = SkScalarPin(y, scrollBounds.y(), scrollBounds.height());
    // Check for no change.
    if (newX == m_iframeScrollOffset.x() && newY == m_iframeScrollOffset.y())
        return false;
    newX = newX - m_iframeScrollOffset.x();
    newY = newY - m_iframeScrollOffset.y();
    setScrollOffset(IntPoint(newX, newY));
    return true;
}

void IFrameContentLayerAndroid::getScrollRect(SkIRect* out) const
{
    const SkPoint& pos = getPosition();
    out->fLeft = m_scrollLimits.fLeft - pos.fX + m_iframeScrollOffset.x();
    out->fTop = m_scrollLimits.fTop - pos.fY + m_iframeScrollOffset.y();

    out->fRight = m_scrollLimits.width();
    out->fBottom = m_scrollLimits.height();
}

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
