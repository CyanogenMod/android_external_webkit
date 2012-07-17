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
#include "private/hwui/DrawGlInfo.h"
#include <GLES2/gl2.h>

#define MAX_CONTRAST 5
#define DEBUG_MATRIX 0

namespace WebCore {

class DrawQuadData;
class PureColorQuadData;
class TextureQuadData;

enum ShaderType {
    UndefinedShader = -1,
    PureColor,
    Tex2D,
    Tex2DInv,
    TexOES,
    TexOESInv,
    Video,
    RepeatTex,
    RepeatTexInv,
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
        , fillPortionHandle(-1)
        , scaleHandle(-1)
    {
    }

    void init(GLint alphaHdl, GLint contrastHdl, GLint posHdl, GLint pgmHdl,
              GLint projMtxHdl, GLint colorHdl, GLint texSamplerHdl,
              GLint videoMtxHdl, GLint fillPortionHdl, GLint scaleHdl)
    {
        alphaHandle = alphaHdl;
        contrastHandle = contrastHdl;
        positionHandle = posHdl;
        programHandle = pgmHdl;
        projMtxHandle = projMtxHdl;
        pureColorHandle = colorHdl;
        texSamplerHandle = texSamplerHdl;
        videoMtxHandle = videoMtxHdl;
        fillPortionHandle = fillPortionHdl;
        scaleHandle = scaleHdl;
    }

    GLint alphaHandle;
    GLint contrastHandle;
    GLint positionHandle;
    GLint programHandle;
    GLint projMtxHandle;
    GLint pureColorHandle;
    GLint texSamplerHandle;
    GLint videoMtxHandle;
    GLint fillPortionHandle;
    GLint scaleHandle;
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
    void setupDrawing(const IntRect& invScreenRect, const SkRect& visibleContentRect,
                      const IntRect& screenRect, int titleBarHeight,
                      const IntRect& screenClip, float scale);
    float zValue(const TransformationMatrix& drawMatrix, float w, float h);

    // For drawQuad and drawLayerQuad, they can handle 3 cases for now:
    // 1) textureTarget == GL_TEXTURE_2D
    // Normal texture in GL_TEXTURE_2D target.
    // 2) textureTarget == GL_TEXTURE_EXTERNAL_OES
    // Surface texture in GL_TEXTURE_EXTERNAL_OES target.
    // 3) textureId == 0
    // No texture needed, just a pureColor quad.
    void drawQuad(const DrawQuadData* data);
    void drawVideoLayerQuad(const TransformationMatrix& drawMatrix,
                     float* textureMatrix, SkRect& geometry, int textureId);
    FloatRect rectInInvViewCoord(const TransformationMatrix& drawMatrix,
                                const IntSize& size);
    FloatRect rectInViewCoord(const TransformationMatrix& drawMatrix,
                                const IntSize& size);

    FloatRect rectInViewCoord(const FloatRect& rect);
    FloatRect rectInInvViewCoord(const FloatRect& rect);
    FloatRect convertInvViewCoordToContentCoord(const FloatRect& rect);
    FloatRect convertViewCoordToInvViewCoord(const FloatRect& rect);
    FloatRect convertInvViewCoordToViewCoord(const FloatRect& rect);

    void clip(const FloatRect& rect);
    IntRect clippedRectWithVisibleContentRect(const IntRect& rect, int margin = 0);
    FloatRect contentViewport() { return m_contentViewport; }

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
    void setGLDrawInfo(const android::uirenderer::DrawGlInfo* info);
    void forceNeedsInit() { m_needsInit = true; }
    bool needsInit() { return m_needsInit; }
    bool usePointSampling(float tileScale, const TransformationMatrix* layerTransform);

private:
    GLuint loadShader(GLenum shaderType, const char* pSource);
    GLint createProgram(const char* vertexSource, const char* fragmentSource);
    void initProgram(ShaderType type);
    GLfloat* getTileProjectionMatrix(const DrawQuadData* data);
    void setBlendingState(bool enableBlending);
    void drawQuadInternal(ShaderType type, const GLfloat* matrix, int textureId,
                         float opacity, GLenum textureTarget, GLenum filter,
                         const Color& pureColor,  const FloatRect& fillPortion,
                         const FloatSize& repeatScale);
    Color shaderColor(Color pureColor, float opacity);
    ShaderType getTextureShaderType(GLenum textureTarget, bool hasRepeatScale);
    void resetBlending();
    void setupSurfaceProjectionMatrix();
#if DEBUG_MATRIX
    FloatRect debugMatrixTransform(const TransformationMatrix& matrix, const char* matrixName);
    void debugMatrixInfo(float currentScale,
                         const TransformationMatrix& clipProjectionMatrix,
                         const TransformationMatrix& webViewMatrix,
                         const TransformationMatrix& modifiedDrawMatrix,
                         const TransformationMatrix* layerMatrix);
#endif // DEBUG_MATRIX

    bool m_blendingEnabled;

    TransformationMatrix m_surfaceProjectionMatrix;
    TransformationMatrix m_clipProjectionMatrix;
    TransformationMatrix m_visibleContentRectProjectionMatrix;
    GLuint m_textureBuffer[1];

    TransformationMatrix m_contentToInvViewMatrix;
    TransformationMatrix m_contentToViewMatrix;
    SkRect m_visibleContentRect;
    IntRect m_invScreenRect;
    FloatRect m_clipRect;
    IntRect m_invViewClip;
    int m_titleBarHeight;
    // This is the layout position in screen coordinate and didn't contain the
    // animation offset.
    IntRect m_screenRect;

    FloatRect m_contentViewport;

    float m_contrast;

    // The height of the render target, either FBO or screen.
    int m_targetHeight;
    bool m_alphaLayer;
    TransformationMatrix m_webViewMatrix;
    float m_currentScale;

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

    GLfloat m_tileProjMatrix[16];

    Vector<ShaderResource> m_resources;

    ShaderType m_cachedProgramType;
    GLfloat m_cachedOpacity;
    FloatRect m_cachedFillPortion;
    Color m_cachedPureColor;
};

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
#endif // ShaderProgram_h
