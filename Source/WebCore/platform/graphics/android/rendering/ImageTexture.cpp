/*
 * Copyright 2011, The Android Open Source Project
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

#define LOG_TAG "ImageTexture"
#define LOG_NDEBUG 1

#include "config.h"
#include "ImageTexture.h"

#include "AndroidLog.h"
#include "ClassTracker.h"
#include "ImagesManager.h"
#include "LayerAndroid.h"
#include "SkDevice.h"
#include "SkPicture.h"
#include "TileGrid.h"
#include "TilesManager.h"

namespace WebCore {

// CRC computation adapted from Tools/DumpRenderTree/CyclicRedundancyCheck.cpp
static void makeCrcTable(unsigned crcTable[256])
{
    for (unsigned i = 0; i < 256; i++) {
        unsigned c = i;
        for (int k = 0; k < 8; k++) {
            if (c & 1)
                c = -306674912 ^ ((c >> 1) & 0x7fffffff);
            else
                c = c >> 1;
        }
        crcTable[i] = c;
    }
}

unsigned computeCrc(uint8_t* buffer, size_t size)
{
    static unsigned crcTable[256];
    static bool crcTableComputed = false;
    if (!crcTableComputed) {
        makeCrcTable(crcTable);
        crcTableComputed = true;
    }

    unsigned crc = 0xffffffffL;
    for (size_t i = 0; i < size; ++i)
        crc = crcTable[(crc ^ buffer[i]) & 0xff] ^ ((crc >> 8) & 0x00ffffffL);
    return crc ^ 0xffffffffL;
}

ImageTexture::ImageTexture(SkBitmap* bmp, unsigned crc)
    : m_image(bmp)
    , m_tileGrid(0)
    , m_layer(0)
    , m_picture(0)
    , m_crc(crc)
{
#ifdef DEBUG_COUNT
    ClassTracker::instance()->increment("ImageTexture");
#endif
    if (!m_image)
        return;

    // NOTE: This constructor is called on the webcore thread

    // Create a picture containing the image (needed for TileGrid)
    m_picture = new SkPicture();
    SkCanvas* pcanvas = m_picture->beginRecording(m_image->width(), m_image->height());
    pcanvas->clear(SkColorSetARGBInline(0, 0, 0, 0));
    pcanvas->drawBitmap(*m_image, 0, 0);
    m_picture->endRecording();
}

ImageTexture::~ImageTexture()
{
#ifdef DEBUG_COUNT
    ClassTracker::instance()->decrement("ImageTexture");
#endif
    delete m_image;
    delete m_tileGrid;
    SkSafeUnref(m_picture);
    ImagesManager::instance()->onImageTextureDestroy(m_crc);
}

SkBitmap* ImageTexture::convertBitmap(SkBitmap* bitmap)
{
    SkBitmap* img = new SkBitmap();
    int w = bitmap->width();
    int h = bitmap->height();

    // Create a copy of the image
    img->setConfig(SkBitmap::kARGB_8888_Config, w, h);
    img->allocPixels();
    SkDevice* device = new SkDevice(*img);
    SkCanvas canvas;
    canvas.setDevice(device);
    device->unref();
    SkRect dest;
    dest.set(0, 0, w, h);
    img->setIsOpaque(false);
    img->eraseARGB(0, 0, 0, 0);
    canvas.drawBitmapRect(*bitmap, 0, dest);

    return img;
}

unsigned ImageTexture::computeCRC(const SkBitmap* bitmap)
{
    if (!bitmap)
        return 0;
    bitmap->lockPixels();
    uint8_t* img = static_cast<uint8_t*>(bitmap->getPixels());
    unsigned crc = 0;
    if (img)
        crc = computeCrc(img, bitmap->getSize());
    bitmap->unlockPixels();
    return crc;
}

bool ImageTexture::equalsCRC(unsigned crc)
{
    return m_crc == crc;
}

// Return 0 if the image does not meet the repeatable criteria.
unsigned int ImageTexture::getImageTextureId()
{
    return m_tileGrid ? m_tileGrid->getImageTextureId() : 0;
}

int ImageTexture::nbTextures()
{
    if (!hasContentToShow())
        return 0;
    if (!m_tileGrid)
        return 0;

    // TODO: take in account the visible clip (need to maintain
    // a list of the clients layer, etc.)
    IntRect visibleContentArea(0, 0, m_image->width(), m_image->height());
    int nbTextures = m_tileGrid->nbTextures(visibleContentArea, 1.0);
    ALOGV("ImageTexture %p, %d x %d needs %d textures",
          this, m_image->width(), m_image->height(),
          nbTextures);
    return nbTextures;
}

bool ImageTexture::hasContentToShow()
{
    // Don't display 1x1 image -- no need to allocate a full texture for this
    if (!m_image)
        return false;
    if (m_image->width() == 1 && m_image->height() == 1)
        return false;
    return true;
}

bool ImageTexture::prepareGL(GLWebViewState* state)
{
    if (!hasContentToShow())
        return false;

    if (!m_tileGrid && m_picture) {
        bool isBaseSurface = false;
        m_tileGrid = new TileGrid(isBaseSurface);
        SkRegion region;
        region.setRect(0, 0, m_image->width(), m_image->height());
        m_tileGrid->markAsDirty(region);
    }

    if (!m_tileGrid)
        return false;

    IntRect fullContentArea(0, 0, m_image->width(), m_image->height());
    m_tileGrid->prepareGL(state, 1.0, fullContentArea, fullContentArea, this);
    if (m_tileGrid->isReady()) {
        m_tileGrid->swapTiles();
        return false;
    }
    return true;
}

const TransformationMatrix* ImageTexture::transform()
{
    if (!m_layer)
        return 0;

    TransformationMatrix d = *(m_layer->drawTransform());
    TransformationMatrix m;
    float scaleW = 1.0f;
    float scaleH = 1.0f;
    getImageToLayerScale(&scaleW, &scaleH);

    m.scaleNonUniform(scaleW, scaleH);
    m_layerMatrix = d.multiply(m);
    return &m_layerMatrix;
}

float ImageTexture::opacity()
{
    if (!m_layer)
        return 1.0;
    return m_layer->drawOpacity();
}

bool ImageTexture::paint(SkCanvas* canvas)
{
    if (!m_picture) {
        ALOGV("IT %p COULDNT PAINT, NO PICTURE", this);
        return false;
    }

    ALOGV("IT %p painting with picture %p", this, m_picture);
    canvas->drawPicture(*m_picture);

    return true;
}

void ImageTexture::getImageToLayerScale(float* scaleW, float* scaleH) const
{
    if (!scaleW || !scaleH)
        return;


    IntRect layerArea = m_layer->fullContentArea();

    if (layerArea.width() == 0 || layerArea.height() == 0)
        return;

    // calculate X, Y scale difference between image pixel coordinates and layer
    // content coordinates

    *scaleW = static_cast<float>(layerArea.width()) / static_cast<float>(m_image->width());
    *scaleH = static_cast<float>(layerArea.height()) / static_cast<float>(m_image->height());
}

void ImageTexture::drawGL(LayerAndroid* layer,
                         float opacity, FloatPoint* offset)
{
    if (!layer)
        return;
    if (!hasContentToShow())
        return;

    // TileGrid::draw() will call us back to know the
    // transform and opacity, so we need to set m_layer
    m_layer = layer;
    if (m_tileGrid) {
        bool force3dContentVisible = true;
        IntRect visibleContentArea = m_layer->visibleContentArea(force3dContentVisible);

        // transform visibleContentArea size to image size
        float scaleW = 1.0f;
        float scaleH = 1.0f;
        getImageToLayerScale(&scaleW, &scaleH);
        visibleContentArea.setX(visibleContentArea.x() / scaleW);
        visibleContentArea.setWidth(visibleContentArea.width() / scaleW);
        visibleContentArea.setY(visibleContentArea.y() / scaleH);
        visibleContentArea.setHeight(visibleContentArea.height() / scaleH);

        const TransformationMatrix* transformation = transform();
        if (offset)
            m_layerMatrix.translate(offset->x(), offset->y());
        m_tileGrid->drawGL(visibleContentArea, opacity, transformation);
    }
    m_layer = 0;
}

void ImageTexture::drawCanvas(SkCanvas* canvas, SkRect& rect)
{
    if (canvas && m_image)
        canvas->drawBitmapRect(*m_image, 0, rect);
}

} // namespace WebCore
