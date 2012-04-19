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

#define LOG_TAG "GaneshContext"
#define LOG_NDEBUG 1

#include "config.h"
#include "GaneshContext.h"

#include "AndroidLog.h"
#include "GLUtils.h"
#include "TextureInfo.h"
#include "TilesManager.h"
#include "TransferQueue.h"

#include "android/native_window.h"

#if USE(ACCELERATED_COMPOSITING)

namespace WebCore {

GaneshContext::GaneshContext()
    : m_grContext(0)
    , m_tileDeviceSurface(0)
    , m_surfaceConfig(0)
    , m_surfaceContext(EGL_NO_CONTEXT)
{
}

GaneshContext* GaneshContext::gInstance = 0;

GaneshContext* GaneshContext::instance()
{
    if (!gInstance)
        gInstance = new GaneshContext();
    return gInstance;
}

GrContext* GaneshContext::getGrContext()
{
    if (!m_grContext)
        m_grContext = GrContext::Create(kOpenGL_Shaders_GrEngine, 0);
    return m_grContext;
}

void GaneshContext::flush()
{
    if (m_grContext)
        m_grContext->flush();
}

SkDevice* GaneshContext::getDeviceForTile(const TileRenderInfo& renderInfo)
{
    // Ganesh should be the only code in the rendering thread that is using GL
    // and setting the EGLContext.  If this is not the case then we need to
    // reset the Ganesh context to prevent rendering issues.
    bool contextNeedsReset = false;
    if (eglGetCurrentContext() != m_surfaceContext) {
        ALOGV("Warning: EGLContext has Changed! %p, %p",
              m_surfaceContext, eglGetCurrentContext());
        contextNeedsReset = true;
    }

    EGLDisplay display;

    if (!m_surfaceContext) {

        if(eglGetCurrentContext() != EGL_NO_CONTEXT)
            ALOGV("ERROR: should not have a context yet");

        display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
        GLUtils::checkEglError("eglGetDisplay");

        EGLint majorVersion;
        EGLint minorVersion;
        EGLBoolean returnValue = eglInitialize(display, &majorVersion, &minorVersion);
        GLUtils::checkEglError("eglInitialize", returnValue);

        EGLint numConfigs;
        static const EGLint configAttribs[] = {
            EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
            EGL_RED_SIZE, 8,
            EGL_GREEN_SIZE, 8,
            EGL_BLUE_SIZE, 8,
            EGL_ALPHA_SIZE, 8,
            EGL_STENCIL_SIZE, 8,
            EGL_NONE
        };

        eglChooseConfig(display, configAttribs, &m_surfaceConfig, 1, &numConfigs);
        GLUtils::checkEglError("eglChooseConfig");

        static const EGLint contextAttribs[] = {
            EGL_CONTEXT_CLIENT_VERSION, 2,
            EGL_NONE
        };

        m_surfaceContext = eglCreateContext(display, m_surfaceConfig, NULL, contextAttribs);
        GLUtils::checkEglError("eglCreateContext");
    } else {
        display = eglGetCurrentDisplay();
        GLUtils::checkEglError("eglGetCurrentDisplay");
    }

    TransferQueue* tileQueue = TilesManager::instance()->transferQueue();
    if (tileQueue->m_eglSurface == EGL_NO_SURFACE) {

        const float tileWidth = renderInfo.tileSize.width();
        const float tileHeight = renderInfo.tileSize.height();

        ANativeWindow* anw = tileQueue->m_ANW.get();

        int result = ANativeWindow_setBuffersGeometry(anw, (int)tileWidth,
                (int)tileHeight, WINDOW_FORMAT_RGBA_8888);

        renderInfo.textureInfo->m_width = tileWidth;
        renderInfo.textureInfo->m_height = tileHeight;
        tileQueue->m_eglSurface = eglCreateWindowSurface(display, m_surfaceConfig, anw, NULL);

        GLUtils::checkEglError("eglCreateWindowSurface");
        ALOGV("eglCreateWindowSurface");
    }

    EGLBoolean returnValue = eglMakeCurrent(display, tileQueue->m_eglSurface, tileQueue->m_eglSurface, m_surfaceContext);
    GLUtils::checkEglError("eglMakeCurrent", returnValue);
    ALOGV("eglMakeCurrent");

    if (!m_tileDeviceSurface) {

        GrPlatformRenderTargetDesc renderTargetDesc;
        renderTargetDesc.fWidth = TilesManager::tileWidth();
        renderTargetDesc.fHeight = TilesManager::tileHeight();
        renderTargetDesc.fConfig = kRGBA_8888_PM_GrPixelConfig;
        renderTargetDesc.fSampleCnt = 0;
        renderTargetDesc.fStencilBits = 8;
        renderTargetDesc.fRenderTargetHandle = 0;

        GrContext* grContext = getGrContext();
        GrRenderTarget* renderTarget = grContext->createPlatformRenderTarget(renderTargetDesc);

        m_tileDeviceSurface = new SkGpuDevice(grContext, renderTarget);
        renderTarget->unref();
        ALOGV("generated device %p", m_tileDeviceSurface);
    }

    GLUtils::checkGlError("getDeviceForTile");

    // We must reset the Ganesh context only after we are sure we have
    // re-established our EGLContext as the current context.
    if (m_tileDeviceSurface && contextNeedsReset)
        getGrContext()->resetContext();

    return m_tileDeviceSurface;
}



} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
