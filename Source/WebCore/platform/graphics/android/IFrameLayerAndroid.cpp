#include "config.h"
#include "IFrameLayerAndroid.h"

#if USE(ACCELERATED_COMPOSITING)

#include <cutils/log.h>
#include <wtf/CurrentTime.h>
#include <wtf/text/CString.h>

#include "DumpLayer.h"

#define XLOGC(...) android_printLog(ANDROID_LOG_DEBUG, "IFrameLayerAndroid", __VA_ARGS__)

namespace WebCore {

IFrameLayerAndroid* IFrameLayerAndroid::updatePosition(SkRect viewport,
                                                       IFrameLayerAndroid* parentIframeLayer)
{
    // As we are an iframe, accumulate the offset from the parent with
    // the current position, and change the parent pointer.

    // If this is the top level, take the current position
    SkPoint parentOffset;
    parentOffset.set(0,0);
    if (parentIframeLayer)
        parentOffset = parentIframeLayer->getPosition();

    SkPoint offset = parentOffset + getPosition();
    m_iframeOffset = IntPoint(offset.fX, offset.fY);

    return this;
}

void IFrameLayerAndroid::dumpLayer(FILE* file, int indentLevel) const
{
    writeIntVal(file, indentLevel + 1, "m_isIframe", true);
    writeIntPoint(file, indentLevel + 1, "m_iframeOffset", m_iframeOffset);
}

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
