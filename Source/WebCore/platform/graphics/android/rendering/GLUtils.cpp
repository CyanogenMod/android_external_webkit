/*
 * Copyright 2010, The Android Open Source Project
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

#define LOG_TAG "GLUtils"
#define LOG_NDEBUG 1

#include "config.h"
#include "GLUtils.h"

#if USE(ACCELERATED_COMPOSITING)

#include "AndroidLog.h"
#include "BaseRenderer.h"
#include "TextureInfo.h"
#include "Tile.h"
#include "TilesManager.h"
#include "TransferQueue.h"

#include <android/native_window.h>
#include <gui/GLConsumer.h>
#include <wtf/CurrentTime.h>

// We will limit GL error logging for LOG_VOLUME_PER_CYCLE times every
// LOG_VOLUME_PER_CYCLE seconds.
#define LOG_CYCLE 30.0
#define LOG_VOLUME_PER_CYCLE 20

struct ANativeWindowBuffer;

namespace WebCore {

using namespace android;

/////////////////////////////////////////////////////////////////////////////////////////
// Matrix utilities
/////////////////////////////////////////////////////////////////////////////////////////

void GLUtils::toGLMatrix(GLfloat* flattened, const TransformationMatrix& m)
{
    flattened[0] = m.m11(); // scaleX
    flattened[1] = m.m12(); // skewY
    flattened[2] = m.m13();
    flattened[3] = m.m14(); // persp0
    flattened[4] = m.m21(); // skewX
    flattened[5] = m.m22(); // scaleY
    flattened[6] = m.m23();
    flattened[7] = m.m24(); // persp1
    flattened[8] = m.m31();
    flattened[9] = m.m32();
    flattened[10] = m.m33();
    flattened[11] = m.m34();
    flattened[12] = m.m41(); // transX
    flattened[13] = m.m42(); // transY
    flattened[14] = m.m43();
    flattened[15] = m.m44(); // persp2
}

void GLUtils::toSkMatrix(SkMatrix& matrix, const TransformationMatrix& m)
{
    matrix[0] = m.m11(); // scaleX
    matrix[1] = m.m21(); // skewX
    matrix[2] = m.m41(); // transX
    matrix[3] = m.m12(); // skewY
    matrix[4] = m.m22(); // scaleY
    matrix[5] = m.m42(); // transY
    matrix[6] = m.m14(); // persp0
    matrix[7] = m.m24(); // persp1
    matrix[8] = m.m44(); // persp2
}

void GLUtils::setOrthographicMatrix(TransformationMatrix& ortho, float left, float top,
                                    float right, float bottom, float nearZ, float farZ)
{
    float deltaX = right - left;
    float deltaY = top - bottom;
    float deltaZ = farZ - nearZ;
    if (!deltaX || !deltaY || !deltaZ)
        return;

    ortho.setM11(2.0f / deltaX);
    ortho.setM41(-(right + left) / deltaX);
    ortho.setM22(2.0f / deltaY);
    ortho.setM42(-(top + bottom) / deltaY);
    ortho.setM33(-2.0f / deltaZ);
    ortho.setM43(-(nearZ + farZ) / deltaZ);
}

bool GLUtils::has3dTransform(const TransformationMatrix& matrix)
{
    return matrix.m13() != 0 || matrix.m23() != 0
        || matrix.m31() != 0 || matrix.m32() != 0
        || matrix.m33() != 1 || matrix.m34() != 0
        || matrix.m43() != 0;
}

/////////////////////////////////////////////////////////////////////////////////////////
// GL & EGL error checks
/////////////////////////////////////////////////////////////////////////////////////////

double GLUtils::m_previousLogTime = 0;
int GLUtils::m_currentLogCounter = 0;

bool GLUtils::allowGLLog()
{
    if (m_currentLogCounter < LOG_VOLUME_PER_CYCLE) {
        m_currentLogCounter++;
        return true;
    }

    // when we are in Log cycle and over the log limit, just return false
    double currentTime = WTF::currentTime();
    double delta = currentTime - m_previousLogTime;
    bool inLogCycle = (delta <= LOG_CYCLE) && (delta > 0);
    if (inLogCycle)
        return false;

    // When we are out of Log Cycle and over the log limit, we need to reset
    // the counter and timer.
    m_previousLogTime = currentTime;
    m_currentLogCounter = 0;
    return false;
}

static void crashIfOOM(GLint errorCode)
{
    const GLint OOM_ERROR_CODE = 0x505;
    if (errorCode == OOM_ERROR_CODE) {
        ALOGE("ERROR: Fatal OOM detected.");
        CRASH();
    }
}

void GLUtils::checkEglError(const char* op, EGLBoolean returnVal)
{
    if (returnVal != EGL_TRUE) {
#ifndef DEBUG
        if (allowGLLog())
#endif
        ALOGE("EGL ERROR - %s() returned %d\n", op, returnVal);
    }

    for (EGLint error = eglGetError(); error != EGL_SUCCESS; error = eglGetError()) {
#ifndef DEBUG
        if (allowGLLog())
#endif
        ALOGE("after %s() eglError (0x%x)\n", op, error);
        crashIfOOM(error);
    }
}

bool GLUtils::checkGlError(const char* op, bool crash /*= true*/)
{
    bool ret = false;
    for (GLint error = glGetError(); error; error = glGetError()) {
#ifndef DEBUG
        if (allowGLLog())
#endif
        ALOGE("GL ERROR - after %s() glError (0x%x)\n", op, error);
        if(crash)
            crashIfOOM(error);
        ret = true;
    }
    return ret;
}

bool GLUtils::checkGlErrorOn(void* p, const char* op, bool crash /*= true*/)
{
    bool ret = false;
    for (GLint error = glGetError(); error; error = glGetError()) {
#ifndef DEBUG
        if (allowGLLog())
#endif
        ALOGE("GL ERROR on %x - after %s() glError (0x%x)\n", p, op, error);
        if(crash)
            crashIfOOM(error);
        ret = true;
    }
    return ret;
}

void GLUtils::checkSurfaceTextureError(const char* functionName, int status)
{
    if (status !=  NO_ERROR) {
#ifndef DEBUG
        if (allowGLLog())
#endif
        ALOGE("ERROR at calling %s status is (%d)", functionName, status);
    }
}
/////////////////////////////////////////////////////////////////////////////////////////
// GL & EGL extension checks
/////////////////////////////////////////////////////////////////////////////////////////

bool GLUtils::isEGLImageSupported()
{
    const char* eglExtensions = eglQueryString(eglGetCurrentDisplay(), EGL_EXTENSIONS);
    const char* glExtensions = reinterpret_cast<const char*>(glGetString(GL_EXTENSIONS));

    return eglExtensions && glExtensions
        && strstr(eglExtensions, "EGL_KHR_image_base")
        && strstr(eglExtensions, "EGL_KHR_gl_texture_2D_image")
        && strstr(glExtensions, "GL_OES_EGL_image");
}

bool GLUtils::isEGLFenceSyncSupported()
{
    const char* eglExtensions = eglQueryString(eglGetCurrentDisplay(), EGL_EXTENSIONS);
    return eglExtensions && strstr(eglExtensions, "EGL_KHR_fence_sync");
}

/////////////////////////////////////////////////////////////////////////////////////////
// Textures utilities
/////////////////////////////////////////////////////////////////////////////////////////

static GLenum getInternalFormat(SkBitmap::Config config)
{
    switch (config) {
    case SkBitmap::kA8_Config:
        return GL_ALPHA;
    case SkBitmap::kARGB_4444_Config:
        return GL_RGBA;
    case SkBitmap::kARGB_8888_Config:
        return GL_RGBA;
    case SkBitmap::kRGB_565_Config:
        return GL_RGB;
    default:
        return -1;
    }
}

static GLenum getType(SkBitmap::Config config)
{
    switch (config) {
    case SkBitmap::kA8_Config:
        return GL_UNSIGNED_BYTE;
    case SkBitmap::kARGB_4444_Config:
        return GL_UNSIGNED_SHORT_4_4_4_4;
    case SkBitmap::kARGB_8888_Config:
        return GL_UNSIGNED_BYTE;
    case SkBitmap::kIndex8_Config:
        return -1; // No type for compressed data.
    case SkBitmap::kRGB_565_Config:
        return GL_UNSIGNED_SHORT_5_6_5;
    default:
        return -1;
    }
}

static EGLConfig defaultPbufferConfig(EGLDisplay display)
{
    EGLConfig config;
    EGLint numConfigs;

    static const EGLint configAttribs[] = {
        EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_NONE
    };

    eglChooseConfig(display, configAttribs, &config, 1, &numConfigs);
    GLUtils::checkEglError("eglPbufferConfig");
    if (numConfigs != 1)
        ALOGI("eglPbufferConfig failed (%d)\n", numConfigs);

    return config;
}

static EGLSurface createPbufferSurface(EGLDisplay display, const EGLConfig& config,
                                       EGLint* errorCode)
{
    const EGLint attribList[] = {
        EGL_WIDTH, 1,
        EGL_HEIGHT, 1,
        EGL_NONE
    };
    EGLSurface surface = eglCreatePbufferSurface(display, config, attribList);

    if (errorCode)
        *errorCode = eglGetError();
    else
        GLUtils::checkEglError("eglCreatePbufferSurface");

    if (surface == EGL_NO_SURFACE)
        return EGL_NO_SURFACE;

    return surface;
}

void GLUtils::deleteTexture(GLuint* texture)
{
    glDeleteTextures(1, texture);
    GLUtils::checkGlError("glDeleteTexture");
    *texture = 0;
}

GLuint GLUtils::createSampleColorTexture(int r, int g, int b)
{
    GLuint texture;
    glGenTextures(1, &texture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    GLubyte pixels[4 *3] = {
        r, g, b,
        r, g, b,
        r, g, b,
        r, g, b
    };
    glBindTexture(GL_TEXTURE_2D, texture);
    GLUtils::checkGlError("glBindTexture");
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 2, 2, 0, GL_RGB, GL_UNSIGNED_BYTE, pixels);
    GLUtils::checkGlError("glTexImage2D");
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    return texture;
}

GLuint GLUtils::createSampleTexture()
{
    GLuint texture;
    glGenTextures(1, &texture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    GLubyte pixels[4 *3] = {
        255, 0, 0,
        0, 255, 0,
        0, 0, 255,
        255, 255, 0
    };
    glBindTexture(GL_TEXTURE_2D, texture);
    GLUtils::checkGlError("glBindTexture");
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 2, 2, 0, GL_RGB, GL_UNSIGNED_BYTE, pixels);
    GLUtils::checkGlError("glTexImage2D");
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    return texture;
}

GLuint GLUtils::createTileGLTexture(int width, int height)
{
    GLuint texture;
    glGenTextures(1, &texture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    GLubyte* pixels = 0;
#ifdef DEBUG
    int length = width * height * 4;
    pixels = new GLubyte[length];
    for (int i = 0; i < length; i++)
        pixels[i] = i % 256;
#endif
    glBindTexture(GL_TEXTURE_2D, texture);
    GLUtils::checkGlError("glBindTexture");
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    GLUtils::checkGlError("glTexImage2D");
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
#ifdef DEBUG
    delete pixels;
#endif
    return texture;
}

bool GLUtils::isPureColorBitmap(const SkBitmap& bitmap, Color& pureColor)
{
    // If the bitmap is the pure color, skip the transfer step, and update the Tile Info.
    // This check is taking < 1ms if we do full bitmap check per tile.
    // TODO: use the SkPicture to determine whether or not a tile is single color.
    TRACE_METHOD();
    pureColor = Color(Color::transparent);
    bitmap.lockPixels();
    bool sameColor = true;
    int bitmapWidth = bitmap.width();

    // Create a row of pure color using the first pixel.
    // TODO: improve the perf here, by either picking a random pixel, or
    // creating an array of rows with pre-defined commonly used color, add
    // smart LUT to speed things up if possible.
    int* firstPixelPtr = static_cast<int*> (bitmap.getPixels());
    int* pixelsRow = new int[bitmapWidth];
    for (int i = 0; i < bitmapWidth; i++)
        pixelsRow[i] = (*firstPixelPtr);

    // Then compare the pure color row with each row of the bitmap.
    for (int j = 0; j < bitmap.height(); j++) {
        if (memcmp(pixelsRow, &firstPixelPtr[bitmapWidth * j], 4 * bitmapWidth)) {
            sameColor = false;
            break;
        }
    }
    delete pixelsRow;
    pixelsRow = 0;

    if (sameColor) {
        unsigned char* rgbaPtr = static_cast<unsigned char*>(bitmap.getPixels());
        pureColor = Color(rgbaPtr[0], rgbaPtr[1], rgbaPtr[2], rgbaPtr[3]);
        ALOGV("sameColor tile found , %x at (%d, %d, %d, %d)",
              *firstPixelPtr, rgbaPtr[0], rgbaPtr[1], rgbaPtr[2], rgbaPtr[3]);
    }
    bitmap.unlockPixels();

    return sameColor;
}

// Return true when the tile is pure color.
bool GLUtils::skipTransferForPureColor(const TileRenderInfo* renderInfo,
                                       const SkBitmap& bitmap)
{
    bool skipTransfer = false;
    Tile* tilePtr = renderInfo->baseTile;

    if (tilePtr) {
        TileTexture* tileTexture = tilePtr->backTexture();
        // Check the bitmap, and make everything ready here.
        if (tileTexture && renderInfo->isPureColor) {
            // update basetile's info
            // Note that we are skipping the whole TransferQueue.
            renderInfo->textureInfo->m_width = bitmap.width();
            renderInfo->textureInfo->m_height = bitmap.height();
            renderInfo->textureInfo->m_internalFormat = GL_RGBA;

            TilesManager::instance()->transferQueue()->addItemInPureColorQueue(renderInfo);

            skipTransfer = true;
        }
    }
    return skipTransfer;
}

void GLUtils::paintTextureWithBitmap(const TileRenderInfo* renderInfo,
                                     SkBitmap& bitmap)
{
    if (!renderInfo)
        return;
    const SkSize& requiredSize = renderInfo->tileSize;
    TextureInfo* textureInfo = renderInfo->textureInfo;

    if (skipTransferForPureColor(renderInfo, bitmap))
        return;

    if (requiredSize.equals(textureInfo->m_width, textureInfo->m_height))
        GLUtils::updateQueueWithBitmap(renderInfo, bitmap);
    else {
        if (!requiredSize.equals(bitmap.width(), bitmap.height())) {
            ALOGV("The bitmap size (%d,%d) does not equal the texture size (%d,%d)",
                  bitmap.width(), bitmap.height(),
                  requiredSize.width(), requiredSize.height());
        }
        GLUtils::updateQueueWithBitmap(renderInfo, bitmap);

        textureInfo->m_width = bitmap.width();
        textureInfo->m_height = bitmap.height();
        textureInfo->m_internalFormat = GL_RGBA;
    }
}

void GLUtils::updateQueueWithBitmap(const TileRenderInfo* renderInfo, SkBitmap& bitmap)
{
    if (!renderInfo
        || !renderInfo->textureInfo
        || !renderInfo->baseTile)
        return;

    TilesManager::instance()->transferQueue()->updateQueueWithBitmap(renderInfo, bitmap);
}

bool GLUtils::updateSharedSurfaceTextureWithBitmap(ANativeWindow* anw, const SkBitmap& bitmap)
{
    TRACE_METHOD();
    SkAutoLockPixels alp(bitmap);
    if (!bitmap.getPixels())
        return false;
    ANativeWindow_Buffer buffer;
    if (ANativeWindow_lock(anw, &buffer, 0))
        return false;
    if (buffer.width < bitmap.width() || buffer.height < bitmap.height()) {
        ALOGW("bitmap (%dx%d) too large for buffer (%dx%d)!",
                bitmap.width(), bitmap.height(),
                buffer.width, buffer.height);
        ANativeWindow_unlockAndPost(anw);
        return false;
    }
    uint8_t* img = (uint8_t*)buffer.bits;
    int row;
    int bpp = 4; // Now we only deal with RGBA8888 format.
    bitmap.lockPixels();
    uint8_t* bitmapOrigin = static_cast<uint8_t*>(bitmap.getPixels());

    if (buffer.stride != bitmap.width())
        // Copied line by line since we need to handle the offsets and stride.
        for (row = 0 ; row < bitmap.height(); row ++) {
            uint8_t* dst = &(img[buffer.stride * row * bpp]);
            uint8_t* src = &(bitmapOrigin[bitmap.width() * row * bpp]);
            memcpy(dst, src, bpp * bitmap.width());
        }
    else
        memcpy(img, bitmapOrigin, bpp * bitmap.width() * bitmap.height());

    bitmap.unlockPixels();
    ANativeWindow_unlockAndPost(anw);
    return true;
}

bool GLUtils::createTextureWithBitmapFailSafe(GLuint texture, const SkBitmap& bitmap, GLint filter)
{
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glBindTexture(GL_TEXTURE_2D, texture);

    bool val = GLUtils::checkGlError("glBindTexture", false);
    if(val)
        return false;

    SkBitmap::Config config = bitmap.getConfig();
    int internalformat = getInternalFormat(config);
    int type = getType(config);

    bitmap.lockPixels();
    glTexImage2D(GL_TEXTURE_2D, 0, internalformat, bitmap.width(), bitmap.height(),
                 0, internalformat, type, bitmap.getPixels());
    bitmap.unlockPixels();

    if (GLUtils::checkGlError("glTexImage2D", false)) {
        return false;
    }
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);

    // The following is a workaround -- remove when EGLImage texture upload is fixed.
    GLuint fboID;
    glGenFramebuffers(1, &fboID);
    glBindFramebuffer(GL_FRAMEBUFFER, fboID);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
    glCheckFramebufferStatus(GL_FRAMEBUFFER); // should return GL_FRAMEBUFFER_COMPLETE

    glBindFramebuffer(GL_FRAMEBUFFER, 0); // rebind the standard FBO
    glDeleteFramebuffers(1, &fboID);

    return true;
}

void GLUtils::createTextureWithBitmap(GLuint texture, const SkBitmap& bitmap, GLint filter)
{
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glBindTexture(GL_TEXTURE_2D, texture);
    GLUtils::checkGlError("glBindTexture");
    SkBitmap::Config config = bitmap.getConfig();
    int internalformat = getInternalFormat(config);
    int type = getType(config);
    bitmap.lockPixels();
    glTexImage2D(GL_TEXTURE_2D, 0, internalformat, bitmap.width(), bitmap.height(),
                 0, internalformat, type, bitmap.getPixels());
    bitmap.unlockPixels();
    if (GLUtils::checkGlError("glTexImage2D")) {
#ifndef DEBUG
        if (allowGLLog())
#endif
        ALOGE("GL ERROR: glTexImage2D parameters are : textureId %d,"
              " bitmap.width() %d, bitmap.height() %d,"
              " internalformat 0x%x, type 0x%x, bitmap.getPixels() %p",
              texture, bitmap.width(), bitmap.height(), internalformat, type,
              bitmap.getPixels());
    }
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
}

void GLUtils::updateTextureWithBitmap(GLuint texture, const SkBitmap& bitmap,
                                      const IntRect& inval, GLint filter)
{
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glBindTexture(GL_TEXTURE_2D, texture);
    GLUtils::checkGlError("glBindTexture");
    SkBitmap::Config config = bitmap.getConfig();
    int internalformat = getInternalFormat(config);
    int type = getType(config);
    bitmap.lockPixels();
    if (inval.isEmpty()) {
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, bitmap.width(), bitmap.height(),
                        internalformat, type, bitmap.getPixels());
    } else {
        glTexSubImage2D(GL_TEXTURE_2D, 0, inval.x(), inval.y(), inval.width(), inval.height(),
                        internalformat, type, bitmap.getPixels());
    }
    bitmap.unlockPixels();
    if (GLUtils::checkGlError("glTexSubImage2D")) {
#ifndef DEBUG
        if (allowGLLog())
#endif
        ALOGE("GL ERROR: glTexSubImage2D parameters are : textureId %d,"
              " bitmap.width() %d, bitmap.height() %d,"
              " internalformat 0x%x, type 0x%x, bitmap.getPixels() %p",
              texture, bitmap.width(), bitmap.height(), internalformat, type,
              bitmap.getPixels());
    }
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
}

void GLUtils::createEGLImageFromTexture(GLuint texture, EGLImageKHR* image)
{
    EGLClientBuffer buffer = reinterpret_cast<EGLClientBuffer>(texture);
    static const EGLint attr[] = { EGL_IMAGE_PRESERVED_KHR, EGL_TRUE, EGL_NONE };
    *image = eglCreateImageKHR(eglGetCurrentDisplay(), eglGetCurrentContext(),
                               EGL_GL_TEXTURE_2D_KHR, buffer, attr);
    GLUtils::checkEglError("eglCreateImage", (*image != EGL_NO_IMAGE_KHR));
}

void GLUtils::createTextureFromEGLImage(GLuint texture, EGLImageKHR image, GLint filter)
{
    glBindTexture(GL_TEXTURE_2D, texture);
    GLUtils::checkGlError("glBindTexture");
    glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, (GLeglImageOES)image);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
}

void GLUtils::convertToTransformationMatrix(const float* matrix, TransformationMatrix& transformMatrix)
{
    transformMatrix.setMatrix(
        matrix[0], matrix[1], matrix[2], matrix[3],
        matrix[4], matrix[5], matrix[6], matrix[7],
        matrix[8], matrix[9], matrix[10], matrix[11],
        matrix[12], matrix[13], matrix[14], matrix[15]);
}

void GLUtils::clearBackgroundIfOpaque(const Color* backgroundColor)
{
    if (!backgroundColor->hasAlpha()) {
        if (TilesManager::instance()->invertedScreen()) {
            float color = 1.0 - ((((float) backgroundColor->red() / 255.0) +
                          ((float) backgroundColor->green() / 255.0) +
                          ((float) backgroundColor->blue() / 255.0)) / 3.0);
            glClearColor(color, color, color, 1);
        } else {
            glClearColor((float)backgroundColor->red() / 255.0,
                         (float)backgroundColor->green() / 255.0,
                         (float)backgroundColor->blue() / 255.0, 1);
        }
        glClear(GL_COLOR_BUFFER_BIT);
    }
}

bool GLUtils::deepCopyBitmapSubset(const SkBitmap& sourceBitmap,
                                   SkBitmap& subset, int leftOffset, int topOffset)
{
    sourceBitmap.lockPixels();
    subset.lockPixels();
    char* srcPixels = (char*) sourceBitmap.getPixels();
    char* dstPixels = (char*) subset.getPixels();
    if (!dstPixels || !srcPixels || !subset.lockPixelsAreWritable()) {
        ALOGD("no pixels :( %p, %p (writable=%d)", srcPixels, dstPixels,
              subset.lockPixelsAreWritable());
        subset.unlockPixels();
        sourceBitmap.unlockPixels();
        return false;
    }
    int srcRowSize = sourceBitmap.rowBytes();
    int destRowSize = subset.rowBytes();
    for (int i = 0; i < subset.height(); i++) {
        int srcOffset = (i + topOffset) * srcRowSize;
        srcOffset += (leftOffset * sourceBitmap.bytesPerPixel());
        int dstOffset = i * destRowSize;
        memcpy(dstPixels + dstOffset, srcPixels + srcOffset, destRowSize);
    }
    subset.unlockPixels();
    sourceBitmap.unlockPixels();
    return true;
}

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
