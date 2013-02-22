#define LOG_TAG "PictureLayerContent"
#define LOG_NDEBUG 1

#include "config.h"
#include "PictureLayerContent.h"

#include "AndroidLog.h"
#include "InspectorCanvas.h"
#include "SkPicture.h"

#include <dlfcn.h>
#include "SkDevice.h"

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

void PictureLayerContent::checkForOptimisations()
{
    if (!m_checkedContent)
        maxZoomScale(); // for now only check the maximum scale for painting
}

float PictureLayerContent::maxZoomScale()
{
    if (m_checkedContent)
        return m_hasText ? 1e6 : 1.0;

    // Let's check if we have text or not. If we don't, we can limit
    // ourselves to scale 1!
    SkBitmap bitmap;
    bitmap.setConfig(SkBitmap::kARGB_8888_Config,
                     m_picture->width(),
                     m_picture->height());
    InspectorBounder inspectorBounder;
    InspectorCanvas checker(&inspectorBounder, m_picture, bitmap);
    checker.drawPicture(*m_picture);
    m_hasText = checker.hasText();
    if (!checker.hasContent()) {
        // no content to draw, discard picture so UI / tile generation
        // doesn't bother with it
        SkSafeUnref(m_picture);
        m_picture = 0;
    }

    m_checkedContent = true;

    return m_hasText ? 1e6 : 1.0;
}

void PictureLayerContent::draw(SkCanvas* canvas)
{
    if (!m_picture)
        return;

    TRACE_METHOD();
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


LegacyPictureLayerContent::LegacyPictureLayerContent(SkMemoryStream* pictureStream) {
    m_legacyPicture = NULL;
    m_width = 0;
    m_height = 0;

    // load legacy skia lib (all functions hidden except ones defined below)
    m_legacyLib = dlopen("libskia_legacy.so", RTLD_LAZY);
    *reinterpret_cast<void**>(&m_createPictureProc) = dlsym(m_legacyLib, "legacy_skia_create_picture");
    *reinterpret_cast<void**>(&m_deletePictureProc) = dlsym(m_legacyLib, "legacy_skia_delete_picture");
    *reinterpret_cast<void**>(&m_drawPictureProc) = dlsym(m_legacyLib, "legacy_skia_draw_picture");

    const char* error = dlerror();
    if (error) {
      SkDebugf("Unable to load legacy lib: %s", error);
      sk_throw();
    }

    // call into library to create picture and set width and height
    const int streamLength = pictureStream->getLength() - pictureStream->peek();
    int bytesRead = m_createPictureProc(pictureStream->getAtPos(), streamLength,
                                        &m_legacyPicture, &m_width, &m_height);
    pictureStream->skip(bytesRead);
}

LegacyPictureLayerContent::~LegacyPictureLayerContent() {
    if (m_legacyLib) {
        if (m_legacyPicture) {
          m_deletePictureProc(m_legacyPicture);
        }
        dlclose(m_legacyLib);
    }
}

void LegacyPictureLayerContent::draw(SkCanvas* canvas) {
    if (!m_legacyPicture) {
      return;
    }

    // if this is an InspectorCanvas we need to at least draw something to
    // ensure that the canvas is not discarded. (We perform a no-op text
    // draw in order to trigger the InspectorCanvas into performing high
    // fidelity rendering while zooming.
    SkPaint paint;
    canvas->drawText(NULL, 0, 0, 0, paint);

    // decompose the canvas into basics
    void* matrixStorage = malloc(canvas->getTotalMatrix().writeToMemory(NULL));
    void* clipStorage = malloc(canvas->getTotalClip().writeToMemory(NULL));

    canvas->getTotalMatrix().writeToMemory(matrixStorage);
    canvas->getTotalClip().writeToMemory(clipStorage);

    const SkBitmap& bitmap = canvas->getDevice()->accessBitmap(true);
    bitmap.lockPixels();

    // pass picture, matrix, clip, and bitmap
    m_drawPictureProc(m_legacyPicture, matrixStorage, clipStorage,
                      bitmap.width(), bitmap.height(), bitmap.getConfig(),
                      bitmap.rowBytes(), bitmap.getPixels());


    bitmap.unlockPixels();
    free(matrixStorage);
    free(clipStorage);
}

} // namespace WebCore
