/*
 * Copyright (C) 2010 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef ShaderProgram_h
#define ShaderProgram_h

#if USE(ACCELERATED_COMPOSITING)

#include "Color.h"
#include "FloatRect.h"
#include "IntRect.h"
#include "SkRect.h"
#include "TransformationMatrix.h"
#include <GLES2/gl2.h>

#define MAX_CONTRAST 5

namespace WebCore {

enum ShaderType {
    UndefinedShader = -1,
    PureColor,
    Tex2D,
    Tex2DInv,
    TexOES,
    TexOESInv,
    Video,
    // When growing this enum list, make sure to insert before the
    // MaxShaderNumber and init the m_handleArray accordingly.
    MaxShaderNumber
};

struct ShaderHandles {
    ShaderHandles()
    : alphaHandle(-1)
    , contrastHandle(-1)
    , positionHandle(-1)
    , programHandle(-1)
    , projMtxHandle(-1)
    , pureColorHandle(-1)
    , texSamplerHandle(-1)
    , videoMtxHandle(-1)
    {
    }

    void init(GLint alphaHdl, GLint contrastHdl, GLint posHdl, GLint pgmHdl,
              GLint projMtxHdl, GLint colorHdl, GLint texSamplerHdl,
              GLint videoMtxHdl)
    {
        alphaHandle = alphaHdl;
        contrastHandle = contrastHdl;
        positionHandle = posHdl;
        programHandle = pgmHdl;
        projMtxHandle = projMtxHdl;
        pureColorHandle = colorHdl;
        texSamplerHandle = texSamplerHdl;
        videoMtxHandle = videoMtxHdl;
    }

    GLint alphaHandle;
    GLint contrastHandle;
    GLint positionHandle;
    GLint programHandle;
    GLint projMtxHandle;
    GLint pureColorHandle;
    GLint texSamplerHandle;
    GLint videoMtxHandle;
};

struct ShaderResource {
    ShaderResource()
    : program(-1)
    , vertexShader(-1)
    , fragmentShader(-1)
    {
    };

    ShaderResource(GLuint prog, GLuint vertex, GLuint fragment)
    : program(prog)
    , vertexShader(vertex)
    , fragmentShader(fragment)
    {
    };

    GLuint program;
    GLuint vertexShader;
    GLuint fragmentShader;
};

class ShaderProgram {
public:
    ShaderProgram();
    void initGLResources();
    void cleanupGLResources();
    // Drawing
    void setupDrawing(const IntRect& viewRect, const SkRect& visibleRect,
                      const IntRect& webViewRect, int titleBarHeight,
                      const IntRect& screenClip, float scale);
    float zValue(const TransformationMatrix& drawMatrix, float w, float h);

    // For drawQuad and drawLayerQuad, they can handle 3 cases for now:
    // 1) textureTarget == GL_TEXTURE_2D
    // Normal texture in GL_TEXTURE_2D target.
    // 2) textureTarget == GL_TEXTURE_EXTERNAL_OES
    // Surface texture in GL_TEXTURE_EXTERNAL_OES target.
    // 3) textureId == 0
    // No texture needed, just a pureColor quad.
    void drawQuad(const SkRect& geometry, int textureId, float opacity, Color pureColor = Color(),
                  GLenum textureTarget = GL_TEXTURE_2D,
                  GLint texFilter = GL_LINEAR);
    void drawLayerQuad(const TransformationMatrix& drawMatrix,
                       const SkRect& geometry, int textureId, float opacity,
                       bool forceBlending = false,
                       GLenum textureTarget = GL_TEXTURE_2D,
                       Color pureColor = Color());
    void drawVideoLayerQuad(const TransformationMatrix& drawMatrix,
                     float* textureMatrix, SkRect& geometry, int textureId);
    FloatRect rectInScreenCoord(const TransformationMatrix& drawMatrix,
                                const IntSize& size);
    FloatRect rectInInvScreenCoord(const TransformationMatrix& drawMatrix,
                                const IntSize& size);

    FloatRect rectInInvScreenCoord(const FloatRect& rect);
    FloatRect rectInScreenCoord(const FloatRect& rect);
    FloatRect convertScreenCoordToDocumentCoord(const FloatRect& rect);
    FloatRect convertInvScreenCoordToScreenCoord(const FloatRect& rect);
    FloatRect convertScreenCoordToInvScreenCoord(const FloatRect& rect);

    void clip(const FloatRect& rect);
    IntRect clippedRectWithViewport(const IntRect& rect, int margin = 0);
    FloatRect documentViewport() { return m_documentViewport; }

    float contrast() { return m_contrast; }
    void setContrast(float c)
    {
        float contrast = c;
        if (contrast < 0)
            contrast = 0;
        if (contrast > MAX_CONTRAST)
            contrast = MAX_CONTRAST;
        m_contrast = contrast;
    }
    void setWebViewMatrix(const float* matrix, bool alphaLayer);

    // This delta is the delta from the layout pos and the current animation pos.
    // Basically, in terms of layout, the webview is still in the original layout
    // pos, as without animation. Such that the viewport and visible rect etc are
    // still in that pos, too, except the clipping info.
    // Our rendering approach is after applying all the matrix, webView is
    // rendered as if it was at the original layout pos, but then offset the
    // glViewport to match the animation.
    void calculateAnimationDelta();
    int getAnimationDeltaX() { return m_animationDelta.x(); }
    int getAnimationDeltaY() { return m_animationDelta.y(); }
    bool needsInit() { return m_needsInit; }

private:
    GLuint loadShader(GLenum shaderType, const char* pSource);
    GLint createProgram(const char* vertexSource, const char* fragmentSource);
    void setProjectionMatrix(const SkRect& geometry, GLfloat* mtxPtr);
    void setBlendingState(bool enableBlending);
    void drawQuadInternal(ShaderType type, const GLfloat* matrix, int textureId,
                         float opacity, GLenum textureTarget, GLenum filter,
                         const Color& pureColor);
    Color shaderColor(Color pureColor, float opacity);
    ShaderType getTextureShaderType(GLenum textureTarget);
    void resetBlending();

    bool m_blendingEnabled;

    TransformationMatrix m_projectionMatrix;
    GLuint m_textureBuffer[1];

    TransformationMatrix m_documentToScreenMatrix;
    TransformationMatrix m_documentToInvScreenMatrix;
    SkRect m_viewport;
    IntRect m_viewRect;
    FloatRect m_clipRect;
    IntRect m_screenClip;
    int m_titleBarHeight;
    IntRect m_webViewRect;

    FloatRect m_documentViewport;

    float m_contrast;

    bool m_alphaLayer;
    TransformationMatrix m_webViewMatrix;
    float m_currentScale;

    // After the webViewTranform, we need to reposition the rect to match our viewport.
    // Basically, the webViewTransformMatrix should apply on the screen resolution.
    // So we start by doing the scale and translate to get each tile into screen coordinates.
    // After applying the webViewTransformMatrix, b/c the way it currently set up
    // for scroll and titlebar, we need to offset both of them.
    // Finally, map everything back to (-1, 1) by using the m_projectionMatrix.
    // TODO: Given that m_webViewMatrix contains most of the tranformation
    // information, we should be able to get rid of some parameter we got from
    // Java side and simplify our code.
    TransformationMatrix m_repositionMatrix;
    IntPoint m_animationDelta;

    // Put all the uniform location (handle) info into an array, and group them
    // by the shader's type, this can help to clean up the interface.
    // TODO: use the type and data comparison to skip GL call if possible.
    ShaderHandles m_handleArray[MaxShaderNumber];

    // If there is any GL error happens such that the Shaders are not initialized
    // successfully at the first time, then we need to init again when we draw.
    bool m_needsInit;

    // For transfer queue blitting, we need a special matrix map from (0,1) to
    // (-1,1)
    GLfloat m_transferProjMtx[16];

    Vector<ShaderResource> m_resources;
};

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
#endif // ShaderProgram_h
