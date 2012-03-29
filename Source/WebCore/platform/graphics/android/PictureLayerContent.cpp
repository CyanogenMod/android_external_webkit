#include "config.h"
#include "PictureLayerContent.h"

#include "InspectorCanvas.h"
#include "SkPicture.h"

namespace WebCore {

PictureLayerContent::PictureLayerContent(SkPicture* picture)
    : m_picture(picture)
    , m_checkedContent(false)
    , m_hasText(true)
{
    SkSafeRef(m_picture);
}

PictureLayerContent::PictureLayerContent(const PictureLayerContent& content)
    : m_picture(content.m_picture)
    , m_checkedContent(content.m_checkedContent)
    , m_hasText(content.m_hasText)
{
    SkSafeRef(m_picture);
}

PictureLayerContent::~PictureLayerContent()
{
    SkSafeUnref(m_picture);
}

int PictureLayerContent::width()
{
    if (!m_picture)
        return 0;
    return m_picture->width();
}

int PictureLayerContent::height()
{
    if (!m_picture)
        return 0;
    return m_picture->height();
}

bool PictureLayerContent::isEmpty()
{
    if (!m_picture)
        return true;
    if (m_picture->width() == 0
        || m_picture->height() == 0)
        return true;
    return false;
}

void PictureLayerContent::checkForOptimisations()
{
    if (!m_checkedContent)
        hasText(); // for now only check the presence of text
}

bool PictureLayerContent::hasText()
{
    if (m_checkedContent)
        return m_hasText;

    // Let's check if we have text or not. If we don't, we can limit
    // ourselves to scale 1!
    InspectorBounder inspectorBounder;
    InspectorCanvas checker(&inspectorBounder, m_picture);
    SkBitmap bitmap;
    bitmap.setConfig(SkBitmap::kARGB_8888_Config,
                     m_picture->width(),
                     m_picture->height());
    checker.setBitmapDevice(bitmap);
    checker.drawPicture(*m_picture);
    m_hasText = checker.hasText();
    if (!checker.hasContent()) {
        // no content to draw, discard picture so UI / tile generation
        // doesn't bother with it
        SkSafeUnref(m_picture);
        m_picture = 0;
    }

    m_checkedContent = true;

    return m_hasText;
}

void PictureLayerContent::draw(SkCanvas* canvas)
{
    if (!m_picture)
        return;

    android::Mutex::Autolock lock(m_drawLock);
    SkRect r = SkRect::MakeWH(width(), height());
    canvas->clipRect(r);
    canvas->drawPicture(*m_picture);
}

void PictureLayerContent::serialize(SkWStream* stream)
{
    if (!m_picture)
        return;
    m_picture->serialize(stream);
}

} // namespace WebCore
