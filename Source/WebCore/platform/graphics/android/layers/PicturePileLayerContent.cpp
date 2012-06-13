#define LOG_TAG "PicturePileLayerContent"
#define LOG_NDEBUG 1

#include "config.h"
#include "PicturePileLayerContent.h"

#include "AndroidLog.h"
#include "SkCanvas.h"
#include "SkPicture.h"

namespace WebCore {

PicturePileLayerContent::PicturePileLayerContent(const PicturePile& picturePile)
    : m_picturePile(picturePile)
{
}

void PicturePileLayerContent::draw(SkCanvas* canvas)
{
    TRACE_METHOD();
    android::Mutex::Autolock lock(m_drawLock);
    m_picturePile.draw(canvas);
}

void PicturePileLayerContent::serialize(SkWStream* stream)
{
    if (!stream)
       return;
    SkPicture picture;
    draw(picture.beginRecording(width(), height(),
                                SkPicture::kUsePathBoundsForClip_RecordingFlag));
    picture.endRecording();
    picture.serialize(stream);
}

PrerenderedInval* PicturePileLayerContent::prerenderForRect(const IntRect& dirty)
{
    return m_picturePile.prerenderedInvalForArea(dirty);
}

void PicturePileLayerContent::clearPrerenders()
{
    m_picturePile.clearPrerenders();
}

} // namespace WebCore
