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

#define LOG_TAG "ShaderProgram"
#define LOG_NDEBUG 1

#include "config.h"
#include "ShaderProgram.h"

#if USE(ACCELERATED_COMPOSITING)

#include "AndroidLog.h"
#include "DrawQuadData.h"
#include "FloatPoint3D.h"
#include "GLUtils.h"
#include "TilesManager.h"

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#define EPSILON 0.00001f

namespace WebCore {

// fillPortion.xy = starting UV coordinates.
// fillPortion.zw = UV coordinates width and height.
static const char gVertexShader[] =
    "attribute vec4 vPosition;\n"
    "uniform mat4 projectionMatrix;\n"
    "uniform vec4 fillPortion;\n"
    "varying vec2 v_texCoord;\n"
    "void main() {\n"
    "  gl_Position = projectionMatrix * vPosition;\n"
    "  v_texCoord = vPosition.xy * fillPortion.zw + fillPortion.xy;\n"
    "}\n";

static const char gRepeatTexFragmentShader[] =
    "precision mediump float;\n"
    "varying vec2 v_texCoord; \n"
    "uniform float alpha; \n"
    "uniform sampler2D s_texture; \n"
    "uniform vec2 repeatScale;\n"
    "void main() {\n"
    "  vec2 repeatedTexCoord; "
    "  repeatedTexCoord.x = v_texCoord.x - floor(v_texCoord.x); "
    "  repeatedTexCoord.y = v_texCoord.y - floor(v_texCoord.y); "
    "  repeatedTexCoord.x = repeatedTexCoord.x * repeatScale.x; "
    "  repeatedTexCoord.y = repeatedTexCoord.y * repeatScale.y; "
    "  gl_FragColor = texture2D(s_texture, repeatedTexCoord); \n"
    "  gl_FragColor *= alpha; "
    "}\n";

static const char gRepeatTexFragmentShaderInverted[] =
    "precision mediump float;\n"
    "varying vec2 v_texCoord; \n"
    "uniform float alpha; \n"
    "uniform float contrast; \n"
    "uniform sampler2D s_texture; \n"
    "uniform vec2 repeatScale;\n"
    "void main() {\n"
    "  vec2 repeatedTexCoord; "
    "  repeatedTexCoord.x = v_texCoord.x - floor(v_texCoord.x); "
    "  repeatedTexCoord.y = v_texCoord.y - floor(v_texCoord.y); "
    "  repeatedTexCoord.x = repeatedTexCoord.x * repeatScale.x; "
    "  repeatedTexCoord.y = repeatedTexCoord.y * repeatScale.y; "
    "  vec4 pixel = texture2D(s_texture, repeatedTexCoord); \n"
    "  float a = pixel.a; \n"
    "  float color = a - (0.2989 * pixel.r + 0.5866 * pixel.g + 0.1145 * pixel.b);\n"
    "  color = ((color - a/2.0) * contrast) + a/2.0; \n"
    "  pixel.rgb = vec3(color, color, color); \n "
    "  gl_FragColor = pixel; \n"
    "  gl_FragColor *= alpha; "
    "}\n";

static const char gFragmentShader[] =
    "precision mediump float;\n"
    "varying vec2 v_texCoord; \n"
    "uniform float alpha; \n"
    "uniform sampler2D s_texture; \n"
    "void main() {\n"
    "  gl_FragColor = texture2D(s_texture, v_texCoord); \n"
    "  gl_FragColor *= alpha; "
    "}\n";

// We could pass the pureColor into either Vertex or Frag Shader.
// The reason we passed the color into the Vertex Shader is that some driver
// might create redundant copy when uniforms in fragment shader changed.
static const char gPureColorVertexShader[] =
    "attribute vec4 vPosition;\n"
    "uniform mat4 projectionMatrix;\n"
    "uniform vec4 inputColor;\n"
    "varying vec4 v_color;\n"
    "void main() {\n"
    "  gl_Position = projectionMatrix * vPosition;\n"
    "  v_color = inputColor;\n"
    "}\n";

static const char gPureColorFragmentShader[] =
    "precision mediump float;\n"
    "varying vec4 v_color;\n"
    "void main() {\n"
    "  gl_FragColor = v_color;\n"
    "}\n";

static const char gFragmentShaderInverted[] =
    "precision mediump float;\n"
    "varying vec2 v_texCoord; \n"
    "uniform float alpha; \n"
    "uniform float contrast; \n"
    "uniform sampler2D s_texture; \n"
    "void main() {\n"
    "  vec4 pixel = texture2D(s_texture, v_texCoord); \n"
    "  float a = pixel.a; \n"
    "  float color = a - (0.2989 * pixel.r + 0.5866 * pixel.g + 0.1145 * pixel.b);\n"
    "  color = ((color - a/2.0) * contrast) + a/2.0; \n"
    "  pixel.rgb = vec3(color, color, color); \n "
    "  gl_FragColor = pixel; \n"
    "  gl_FragColor *= alpha; \n"
    "}\n";

static const char gVideoVertexShader[] =
    "attribute vec4 vPosition;\n"
    "uniform mat4 textureMatrix;\n"
    "uniform mat4 projectionMatrix;\n"
    "varying vec2 v_texCoord;\n"
    "void main() {\n"
    "  gl_Position = projectionMatrix * vPosition;\n"
    "  v_texCoord = vec2(textureMatrix * vec4(vPosition.x, 1.0 - vPosition.y, 0.0, 1.0));\n"
    "}\n";

static const char gVideoFragmentShader[] =
    "#extension GL_OES_EGL_image_external : require\n"
    "precision mediump float;\n"
    "uniform samplerExternalOES s_yuvTexture;\n"
    "varying vec2 v_texCoord;\n"
    "void main() {\n"
    "  gl_FragColor = texture2D(s_yuvTexture, v_texCoord);\n"
    "}\n";

static const char gSurfaceTextureOESFragmentShader[] =
    "#extension GL_OES_EGL_image_external : require\n"
    "precision mediump float;\n"
    "varying vec2 v_texCoord; \n"
    "uniform float alpha; \n"
    "uniform samplerExternalOES s_texture; \n"
    "void main() {\n"
    "  gl_FragColor = texture2D(s_texture, v_texCoord); \n"
    "  gl_FragColor *= alpha; "
    "}\n";

static const char gSurfaceTextureOESFragmentShaderInverted[] =
    "#extension GL_OES_EGL_image_external : require\n"
    "precision mediump float;\n"
    "varying vec2 v_texCoord; \n"
    "uniform float alpha; \n"
    "uniform float contrast; \n"
    "uniform samplerExternalOES s_texture; \n"
    "void main() {\n"
    "  vec4 pixel = texture2D(s_texture, v_texCoord); \n"
    "  float a = pixel.a; \n"
    "  float color = a - (0.2989 * pixel.r + 0.5866 * pixel.g + 0.1145 * pixel.b);\n"
    "  color = ((color - a/2.0) * contrast) + a/2.0; \n"
    "  pixel.rgb = vec3(color, color, color); \n "
    "  gl_FragColor = pixel; \n"
    "  gl_FragColor *= alpha; \n"
    "}\n";

GLuint ShaderProgram::loadShader(GLenum shaderType, const char* pSource)
{
    GLuint shader = glCreateShader(shaderType);
    if (shader) {
        glShaderSource(shader, 1, &pSource, 0);
        glCompileShader(shader);
        GLint compiled = 0;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
        if (!compiled) {
            GLint infoLen = 0;
            glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLen);
            if (infoLen) {
                char* buf = (char*) malloc(infoLen);
                if (buf) {
                glGetShaderInfoLog(shader, infoLen, 0, buf);
                ALOGE("could not compile shader %d:\n%s\n", shaderType, buf);
                free(buf);
            }
            glDeleteShader(shader);
            shader = 0;
            }
        }
    }
    return shader;
}

GLint ShaderProgram::createProgram(const char* pVertexSource, const char* pFragmentSource)
{
    GLuint vertexShader = loadShader(GL_VERTEX_SHADER, pVertexSource);
    if (!vertexShader) {
        ALOGE("couldn't load the vertex shader!");
        return -1;
    }

    GLuint pixelShader = loadShader(GL_FRAGMENT_SHADER, pFragmentSource);
    if (!pixelShader) {
        ALOGE("couldn't load the pixel shader!");
        return -1;
    }

    GLuint program = glCreateProgram();
    if (program) {
        glAttachShader(program, vertexShader);
        GLUtils::checkGlError("glAttachShader vertex");
        glAttachShader(program, pixelShader);
        GLUtils::checkGlError("glAttachShader pixel");
        glLinkProgram(program);
        GLint linkStatus = GL_FALSE;
        glGetProgramiv(program, GL_LINK_STATUS, &linkStatus);
        if (linkStatus != GL_TRUE) {
            GLint bufLength = 0;
            glGetProgramiv(program, GL_INFO_LOG_LENGTH, &bufLength);
            if (bufLength) {
                char* buf = (char*) malloc(bufLength);
                if (buf) {
                    glGetProgramInfoLog(program, bufLength, 0, buf);
                    ALOGE("could not link program:\n%s\n", buf);
                    free(buf);
                }
            }
            glDeleteProgram(program);
            program = -1;
        }
    }

    ShaderResource newResource(program, vertexShader, pixelShader);
    m_resources.append(newResource);
    return program;
}

ShaderProgram::ShaderProgram()
    : m_blendingEnabled(false)
    , m_contrast(1)
    , m_alphaLayer(false)
    , m_currentScale(1.0f)
    , m_needsInit(true)
{
}

void ShaderProgram::cleanupGLResources()
{
    for (unsigned int i = 0; i < m_resources.size(); i++) {
        glDetachShader(m_resources[i].program, m_resources[i].vertexShader);
        glDetachShader(m_resources[i].program, m_resources[i].fragmentShader);
        glDeleteShader(m_resources[i].vertexShader);
        glDeleteShader(m_resources[i].fragmentShader);
        glDeleteProgram(m_resources[i].program);
    }
    glDeleteBuffers(1, m_textureBuffer);

    m_resources.clear();
    m_needsInit = true;
    GLUtils::checkGlError("cleanupGLResources");

    return;
}

void ShaderProgram::initGLResources()
{
    // To detect whether or not resources for ShaderProgram allocated
    // successfully, we clean up pre-existing errors here and will check for
    // new errors at the end of this function.
    GLUtils::checkGlError("before initGLResources");

    GLint tex2DProgram = createProgram(gVertexShader, gFragmentShader);
    GLint pureColorProgram = createProgram(gPureColorVertexShader, gPureColorFragmentShader);
    GLint tex2DInvProgram = createProgram(gVertexShader, gFragmentShaderInverted);
    GLint videoProgram = createProgram(gVideoVertexShader, gVideoFragmentShader);
    GLint texOESProgram =
        createProgram(gVertexShader, gSurfaceTextureOESFragmentShader);
    GLint texOESInvProgram =
        createProgram(gVertexShader, gSurfaceTextureOESFragmentShaderInverted);
    GLint repeatTexProgram =
        createProgram(gVertexShader, gRepeatTexFragmentShader);
    GLint repeatTexInvProgram =
        createProgram(gVertexShader, gRepeatTexFragmentShaderInverted);

    if (tex2DProgram == -1
        || pureColorProgram == -1
        || tex2DInvProgram == -1
        || videoProgram == -1
        || texOESProgram == -1
        || texOESInvProgram == -1
        || repeatTexProgram == -1
        || repeatTexInvProgram == -1) {
        m_needsInit = true;
        return;
    }

    GLint pureColorPosition = glGetAttribLocation(pureColorProgram, "vPosition");
    GLint pureColorProjMtx = glGetUniformLocation(pureColorProgram, "projectionMatrix");
    GLint pureColorValue = glGetUniformLocation(pureColorProgram, "inputColor");
    m_handleArray[PureColor].init(-1, -1, pureColorPosition, pureColorProgram,
                                  pureColorProjMtx, pureColorValue, -1, -1, -1, -1);

    GLint tex2DAlpha = glGetUniformLocation(tex2DProgram, "alpha");
    GLint tex2DPosition = glGetAttribLocation(tex2DProgram, "vPosition");
    GLint tex2DProjMtx = glGetUniformLocation(tex2DProgram, "projectionMatrix");
    GLint tex2DTexSampler = glGetUniformLocation(tex2DProgram, "s_texture");
    GLint tex2DFillPortion = glGetUniformLocation(tex2DProgram, "fillPortion");
    m_handleArray[Tex2D].init(tex2DAlpha, -1, tex2DPosition, tex2DProgram,
                              tex2DProjMtx, -1, tex2DTexSampler, -1, tex2DFillPortion, -1);

    GLint tex2DInvAlpha = glGetUniformLocation(tex2DInvProgram, "alpha");
    GLint tex2DInvContrast = glGetUniformLocation(tex2DInvProgram, "contrast");
    GLint tex2DInvPosition = glGetAttribLocation(tex2DInvProgram, "vPosition");
    GLint tex2DInvProjMtx = glGetUniformLocation(tex2DInvProgram, "projectionMatrix");
    GLint tex2DInvTexSampler = glGetUniformLocation(tex2DInvProgram, "s_texture");
    GLint tex2DInvFillPortion = glGetUniformLocation(tex2DInvProgram, "fillPortion");
    m_handleArray[Tex2DInv].init(tex2DInvAlpha, tex2DInvContrast,
                                 tex2DInvPosition, tex2DInvProgram,
                                 tex2DInvProjMtx, -1,
                                 tex2DInvTexSampler, -1, tex2DInvFillPortion, -1);

    GLint repeatTexAlpha = glGetUniformLocation(repeatTexProgram, "alpha");
    GLint repeatTexPosition = glGetAttribLocation(repeatTexProgram, "vPosition");
    GLint repeatTexProjMtx = glGetUniformLocation(repeatTexProgram, "projectionMatrix");
    GLint repeatTexTexSampler = glGetUniformLocation(repeatTexProgram, "s_texture");
    GLint repeatTexFillPortion = glGetUniformLocation(repeatTexProgram, "fillPortion");
    GLint repeatTexScale = glGetUniformLocation(repeatTexProgram, "repeatScale");
    m_handleArray[RepeatTex].init(repeatTexAlpha, -1, repeatTexPosition,
                                  repeatTexProgram,repeatTexProjMtx, -1,
                                  repeatTexTexSampler, -1, repeatTexFillPortion,
                                  repeatTexScale);

    GLint repeatTexInvAlpha = glGetUniformLocation(repeatTexInvProgram, "alpha");
    GLint repeatTexInvContrast = glGetUniformLocation(tex2DInvProgram, "contrast");
    GLint repeatTexInvPosition = glGetAttribLocation(repeatTexInvProgram, "vPosition");
    GLint repeatTexInvProjMtx = glGetUniformLocation(repeatTexInvProgram, "projectionMatrix");
    GLint repeatTexInvTexSampler = glGetUniformLocation(repeatTexInvProgram, "s_texture");
    GLint repeatTexInvFillPortion = glGetUniformLocation(repeatTexInvProgram, "fillPortion");
    GLint repeatTexInvScale = glGetUniformLocation(repeatTexInvProgram, "repeatScale");
    m_handleArray[RepeatTexInv].init(repeatTexInvAlpha, repeatTexInvContrast,
                                     repeatTexInvPosition, repeatTexInvProgram,
                                     repeatTexInvProjMtx, -1,
                                     repeatTexInvTexSampler, -1,
                                     repeatTexInvFillPortion, repeatTexInvScale);

    GLint texOESAlpha = glGetUniformLocation(texOESProgram, "alpha");
    GLint texOESPosition = glGetAttribLocation(texOESProgram, "vPosition");
    GLint texOESProjMtx = glGetUniformLocation(texOESProgram, "projectionMatrix");
    GLint texOESTexSampler = glGetUniformLocation(texOESProgram, "s_texture");
    GLint texOESFillPortion = glGetUniformLocation(texOESProgram, "fillPortion");
    m_handleArray[TexOES].init(texOESAlpha, -1, texOESPosition, texOESProgram,
                               texOESProjMtx, -1, texOESTexSampler, -1, texOESFillPortion, -1);

    GLint texOESInvAlpha = glGetUniformLocation(texOESInvProgram, "alpha");
    GLint texOESInvContrast = glGetUniformLocation(texOESInvProgram, "contrast");
    GLint texOESInvPosition = glGetAttribLocation(texOESInvProgram, "vPosition");
    GLint texOESInvProjMtx = glGetUniformLocation(texOESInvProgram, "projectionMatrix");
    GLint texOESInvTexSampler = glGetUniformLocation(texOESInvProgram, "s_texture");
    GLint texOESInvFillPortion = glGetUniformLocation(texOESInvProgram, "fillPortion");
    m_handleArray[TexOESInv].init(texOESInvAlpha, texOESInvContrast,
                                  texOESInvPosition, texOESInvProgram,
                                  texOESInvProjMtx, -1,
                                  texOESInvTexSampler, -1, texOESInvFillPortion, -1);

    GLint videoPosition = glGetAttribLocation(videoProgram, "vPosition");
    GLint videoProjMtx = glGetUniformLocation(videoProgram, "projectionMatrix");
    GLint videoTexSampler = glGetUniformLocation(videoProgram, "s_yuvTexture");
    GLint videoTexMtx = glGetUniformLocation(videoProgram, "textureMatrix");
    m_handleArray[Video].init(-1, -1, videoPosition, videoProgram,
                              videoProjMtx, -1, videoTexSampler,
                              videoTexMtx, -1, -1);

    const GLfloat coord[] = {
        0.0f, 0.0f, // C
        1.0f, 0.0f, // D
        0.0f, 1.0f, // A
        1.0f, 1.0f // B
    };

    glGenBuffers(1, m_textureBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, m_textureBuffer[0]);
    glBufferData(GL_ARRAY_BUFFER, 2 * 4 * sizeof(GLfloat), coord, GL_STATIC_DRAW);

    TransformationMatrix matrix;
    // Map x,y from (0,1) to (-1, 1)
    matrix.scale3d(2, 2, 1);
    matrix.translate3d(-0.5, -0.5, 0);
    GLUtils::toGLMatrix(m_transferProjMtx, matrix);

    m_needsInit = GLUtils::checkGlError("initGLResources");
    return;
}

void ShaderProgram::resetBlending()
{
    glDisable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    glBlendEquation(GL_FUNC_ADD);
    m_blendingEnabled = false;
}

void ShaderProgram::setBlendingState(bool enableBlending)
{
    if (enableBlending == m_blendingEnabled)
        return;

    if (enableBlending)
        glEnable(GL_BLEND);
    else
        glDisable(GL_BLEND);

    m_blendingEnabled = enableBlending;
}

/////////////////////////////////////////////////////////////////////////////////////////
// Drawing
/////////////////////////////////////////////////////////////////////////////////////////

// We have multiple coordinates to deal with: first is the screen coordinates,
// second is the view coordinates and the last one is content(document) coordinates.
// Both screen and view coordinates are in pixels.
// All these coordinates start from upper left, but for the purpose of OpenGL
// operations, we may need a inverted Y version of such coordinates which
// start from lower left.
//
// invScreenRect - inv screen coordinates starting from lower left.
// visibleContentRect - local content(document) coordinates starting from upper left.
// screenRect - screen coordinates starting from upper left.
// screenClip - screen coordinates starting from upper left.
//    ------------------------------------------
//    |(origin of screen)                      |
//    |screen                                  |
//    |   ---------------------------------    |
//    |   | (origin of view)              |    |
//    |   | webview                       |    |
//    |   |        --------               |    |
//    |   |        | clip |               |    |
//    |   |        |      |               |    |
//    |   |        --------               |    |
//    |   |                               |    |
//    |   |(origin of inv view)           |    |
//    |   ---------------------------------    |
//    |(origin of inv screen)                  |
//    ------------------------------------------
void ShaderProgram::setupDrawing(const IntRect& invScreenRect,
                                 const SkRect& visibleContentRect,
                                 const IntRect& screenRect, int titleBarHeight,
                                 const IntRect& screenClip, float scale)
{
    m_screenRect = screenRect;
    m_titleBarHeight = titleBarHeight;

    //// viewport ////
    GLUtils::setOrthographicMatrix(m_visibleContentRectProjectionMatrix,
                                   visibleContentRect.fLeft,
                                   visibleContentRect.fTop,
                                   visibleContentRect.fRight,
                                   visibleContentRect.fBottom,
                                   -1000, 1000);

    ALOGV("set m_clipProjectionMatrix, %d, %d, %d, %d",
          screenClip.x(), screenClip.y(), screenClip.x() + screenClip.width(),
          screenClip.y() + screenClip.height());

    // In order to incorporate the animation delta X and Y, using the clip as
    // the GL viewport can save all the trouble of re-position from screenRect
    // to final position.
    GLUtils::setOrthographicMatrix(m_clipProjectionMatrix, screenClip.x(), screenClip.y(),
                                   screenClip.x() + screenClip.width(),
                                   screenClip.y() + screenClip.height(), -1000, 1000);

    glViewport(screenClip.x(), m_targetHeight - screenClip.y() - screenClip.height() ,
               screenClip.width(), screenClip.height());

    m_visibleContentRect = visibleContentRect;
    m_currentScale = scale;


    //// viewRect ////
    m_invScreenRect = invScreenRect;

    // The following matrices transform content coordinates into view coordinates
    // and inv view coordinates.
    // Note that GLUtils::setOrthographicMatrix is inverting the Y.
    TransformationMatrix viewTranslate;
    viewTranslate.translate(1.0, 1.0);

    TransformationMatrix viewScale;
    viewScale.scale3d(m_invScreenRect.width() * 0.5f, m_invScreenRect.height() * 0.5f, 1);

    m_contentToInvViewMatrix = viewScale * viewTranslate * m_visibleContentRectProjectionMatrix;

    viewTranslate.scale3d(1, -1, 1);
    m_contentToViewMatrix = viewScale * viewTranslate * m_visibleContentRectProjectionMatrix;

    IntRect invViewRect(0, 0, m_screenRect.width(), m_screenRect.height());
    m_contentViewport = m_contentToInvViewMatrix.inverse().mapRect(invViewRect);


    //// clipping ////
    IntRect viewClip = screenClip;

    // The incoming screenClip is in screen coordinates, we first
    // translate it into view coordinates.
    // Then we convert it into inverted view coordinates.
    // Therefore, in the clip() function, we need to convert things back from
    // inverted view coordinates to inverted screen coordinates which is used by GL.
    viewClip.setX(screenClip.x() - m_screenRect.x());
    viewClip.setY(screenClip.y() - m_screenRect.y() - m_titleBarHeight);
    FloatRect invViewClip = convertViewCoordToInvViewCoord(viewClip);
    m_invViewClip.setLocation(IntPoint(invViewClip.x(), invViewClip.y()));
    // use ceilf to handle view -> doc -> view coord rounding errors
    m_invViewClip.setSize(IntSize(ceilf(invViewClip.width()), ceilf(invViewClip.height())));

    resetBlending();

    // Set up m_clipProjectionMatrix, m_currentScale and m_webViewMatrix before
    // calling this function.
    setupSurfaceProjectionMatrix();
}

// Calculate the right color value sent into the shader considering the (0,1)
// clamp and alpha blending.
Color ShaderProgram::shaderColor(Color pureColor, float opacity)
{
    float r = pureColor.red() / 255.0;
    float g = pureColor.green() / 255.0;
    float b = pureColor.blue() / 255.0;
    float a = pureColor.alpha() / 255.0;

    if (TilesManager::instance()->invertedScreen()) {
        float intensity = a - (0.2989 * r + 0.5866 * g + 0.1145 * b);
        intensity = ((intensity - a / 2.0) * m_contrast) + a / 2.0;
        intensity *= opacity;
        return Color(intensity, intensity, intensity, a * opacity);
    }
    return Color(r * opacity, g * opacity, b * opacity, a * opacity);
}

// For shaders using texture, it is easy to get the type from the textureTarget.
ShaderType ShaderProgram::getTextureShaderType(GLenum textureTarget,
                                               bool hasRepeatScale)
{
    ShaderType type = UndefinedShader;
    if (textureTarget == GL_TEXTURE_2D) {
        if (!TilesManager::instance()->invertedScreen())
            type = hasRepeatScale ?  RepeatTex : Tex2D;
        else {
            // With the new GPU texture upload path, we do not use an FBO
            // to blit the texture we receive from the TexturesGenerator thread.
            // To implement inverted rendering, we thus have to do the rendering
            // live, by using a different shader.
            type = hasRepeatScale ?  RepeatTexInv : Tex2DInv;
        }
    } else if (textureTarget == GL_TEXTURE_EXTERNAL_OES) {
        if (!TilesManager::instance()->invertedScreen())
            type = TexOES;
        else
            type = TexOESInv;
    }
    return type;
}

// This function transform a clip rect extracted from the current layer
// into a clip rect in InvView coordinates -- used by the clipping rects
FloatRect ShaderProgram::rectInInvViewCoord(const TransformationMatrix& drawMatrix, const IntSize& size)
{
    FloatRect srect(0, 0, size.width(), size.height());
    TransformationMatrix renderMatrix = m_contentToInvViewMatrix * drawMatrix;
    return renderMatrix.mapRect(srect);
}

// used by the partial screen invals
FloatRect ShaderProgram::rectInViewCoord(const TransformationMatrix& drawMatrix, const IntSize& size)
{
    FloatRect srect(0, 0, size.width(), size.height());
    TransformationMatrix renderMatrix = m_contentToViewMatrix * drawMatrix;
    return renderMatrix.mapRect(srect);
}

FloatRect ShaderProgram::rectInViewCoord(const FloatRect& rect)
{
    return m_contentToViewMatrix.mapRect(rect);
}

FloatRect ShaderProgram::rectInInvViewCoord(const FloatRect& rect)
{
    return m_contentToInvViewMatrix.mapRect(rect);
}

FloatRect ShaderProgram::convertInvViewCoordToContentCoord(const FloatRect& rect)
{
    return m_contentToInvViewMatrix.inverse().mapRect(rect);
}

FloatRect ShaderProgram::convertViewCoordToInvViewCoord(const FloatRect& rect)
{
    FloatRect visibleContentRect = m_contentToViewMatrix.inverse().mapRect(rect);
    return rectInInvViewCoord(visibleContentRect);
}

FloatRect ShaderProgram::convertInvViewCoordToViewCoord(const FloatRect& rect)
{
    FloatRect visibleContentRect = m_contentToInvViewMatrix.inverse().mapRect(rect);
    return rectInViewCoord(visibleContentRect);
}

// clip is in screen coordinates
void ShaderProgram::clip(const FloatRect& clip)
{
    if (clip == m_clipRect)
        return;

    ALOGV("--clipping rect %f %f, %f x %f",
          clip.x(), clip.y(), clip.width(), clip.height());

    // we should only call glScissor in this function, so that we can easily
    // track the current clipping rect.

    IntRect screenClip(clip.x(),
                       clip.y(),
                       clip.width(), clip.height());

    if (!m_invViewClip.isEmpty())
        screenClip.intersect(m_invViewClip);

    // The previous intersection calculation is using local screen coordinates.
    // Now we need to convert things from local screen coordinates to global
    // screen coordinates and pass to the GL functions.
    screenClip.setX(screenClip.x() + m_invScreenRect.x());
    screenClip.setY(screenClip.y() + m_invScreenRect.y());
    if (screenClip.x() < 0) {
        int w = screenClip.width();
        w += screenClip.x();
        screenClip.setX(0);
        screenClip.setWidth(w);
    }
    if (screenClip.y() < 0) {
        int h = screenClip.height();
        h += screenClip.y();
        screenClip.setY(0);
        screenClip.setHeight(h);
    }

    glScissor(screenClip.x(), screenClip.y(), screenClip.width(), screenClip.height());

    m_clipRect = clip;
}

IntRect ShaderProgram::clippedRectWithVisibleContentRect(const IntRect& rect, int margin)
{
    IntRect viewport(m_visibleContentRect.fLeft - margin, m_visibleContentRect.fTop - margin,
                     m_visibleContentRect.width() + margin,
                     m_visibleContentRect.height() + margin);
    viewport.intersect(rect);
    return viewport;
}

float ShaderProgram::zValue(const TransformationMatrix& drawMatrix, float w, float h)
{
    TransformationMatrix modifiedDrawMatrix = drawMatrix;
    modifiedDrawMatrix.scale3d(w, h, 1);
    TransformationMatrix renderMatrix =
        m_visibleContentRectProjectionMatrix * modifiedDrawMatrix;
    FloatPoint3D point(0.5, 0.5, 0.0);
    FloatPoint3D result = renderMatrix.mapPoint(point);
    return result.z();
}

void ShaderProgram::drawQuadInternal(ShaderType type, const GLfloat* matrix,
                                     int textureId, float opacity,
                                     GLenum textureTarget, GLenum filter,
                                     const Color& pureColor, const FloatRect& fillPortion,
                                     const FloatSize& repeatScale)
{
    glUseProgram(m_handleArray[type].programHandle);
    glUniformMatrix4fv(m_handleArray[type].projMtxHandle, 1, GL_FALSE, matrix);

    if (type != PureColor) {
        glActiveTexture(GL_TEXTURE0);
        glUniform1i(m_handleArray[type].texSamplerHandle, 0);
        glBindTexture(textureTarget, textureId);
        glTexParameteri(textureTarget, GL_TEXTURE_MIN_FILTER, filter);
        glTexParameteri(textureTarget, GL_TEXTURE_MAG_FILTER, filter);
        glUniform1f(m_handleArray[type].alphaHandle, opacity);

        GLint contrastHandle = m_handleArray[type].contrastHandle;
        if (contrastHandle != -1)
            glUniform1f(contrastHandle, m_contrast);

        glUniform4f(m_handleArray[type].fillPortionHandle, fillPortion.x(), fillPortion.y(),
                    fillPortion.width(), fillPortion.height());

        // Only when we have repeat scale, this handle can be >= 0;
        if (m_handleArray[type].scaleHandle != -1) {
            glUniform2f(m_handleArray[type].scaleHandle,
                        repeatScale.width(), repeatScale.height());
        }
    } else {
        glUniform4f(m_handleArray[type].pureColorHandle,
                    pureColor.red() / 255.0, pureColor.green() / 255.0,
                    pureColor.blue() / 255.0, pureColor.alpha() / 255.0);
    }

    GLint positionHandle = m_handleArray[type].positionHandle;
    glBindBuffer(GL_ARRAY_BUFFER, m_textureBuffer[0]);
    glEnableVertexAttribArray(positionHandle);
    glVertexAttribPointer(positionHandle, 2, GL_FLOAT, GL_FALSE, 0, 0);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

// Put the common matrix computation at higher level to avoid redundancy.
void ShaderProgram::setupSurfaceProjectionMatrix()
{
    TransformationMatrix scaleMatrix;
    scaleMatrix.scale3d(m_currentScale, m_currentScale, 1);
    m_surfaceProjectionMatrix = m_clipProjectionMatrix * m_webViewMatrix * scaleMatrix;
}

// Calculate the matrix given the geometry.
GLfloat* ShaderProgram::getTileProjectionMatrix(const DrawQuadData* data)
{
    DrawQuadType type = data->type();
    if (type == Blit)
        return m_transferProjMtx;

    const TransformationMatrix* matrix = data->drawMatrix();
    const SkRect* geometry = data->geometry();
    FloatRect fillPortion = data->fillPortion();
    ALOGV("fillPortion " FLOAT_RECT_FORMAT, FLOAT_RECT_ARGS(fillPortion));

    // This modifiedDrawMatrix tranform (0,0)(1x1) to the final rect in screen
    // coordinates, before applying the m_webViewMatrix.
    // It first scale and translate the vertex array from (0,0)(1x1) to real
    // tile position and size. Then apply the transform from the layer's.
    // Finally scale to the currentScale to support zooming.
    // Note the geometry contains the tile zoom scale, so visually we will see
    // the tiles scale at a ratio as (m_currentScale/tile's scale).
    TransformationMatrix modifiedDrawMatrix;
    if (type == LayerQuad)
        modifiedDrawMatrix = *matrix;
    modifiedDrawMatrix.translate(geometry->fLeft + geometry->width() * fillPortion.x(),
                                 geometry->fTop + geometry->height() * fillPortion.y());
    modifiedDrawMatrix.scale3d(geometry->width() * fillPortion.width(),
                               geometry->height() * fillPortion.height(), 1);

    // Even when we are on a alpha layer or not, we need to respect the
    // m_webViewMatrix, it may contain the layout offset. Normally it is
    // identity.
    TransformationMatrix renderMatrix;
    renderMatrix = m_surfaceProjectionMatrix * modifiedDrawMatrix;

#if DEBUG_MATRIX
    debugMatrixInfo(m_currentScale, m_clipProjectionMatrix, m_webViewMatrix,
                    modifiedDrawMatrix, matrix);
#endif

    GLUtils::toGLMatrix(m_tileProjMatrix, renderMatrix);
    return m_tileProjMatrix;
}

void ShaderProgram::drawQuad(const DrawQuadData* data)
{
    GLfloat* matrix = getTileProjectionMatrix(data);

    float opacity = data->opacity();
    bool forceBlending = data->forceBlending();
    bool enableBlending = forceBlending || opacity < 1.0;

    ShaderType shaderType = UndefinedShader;
    int textureId = 0;
    GLint textureFilter = 0;
    GLenum textureTarget = 0;

    Color quadColor = data->quadColor();
    if (data->pureColor()) {
        shaderType = PureColor;
        quadColor = shaderColor(quadColor, opacity);
        enableBlending = enableBlending || quadColor.hasAlpha();
        if (!quadColor.alpha() && enableBlending)
            return;
    } else {
        textureId = data->textureId();
        textureFilter = data->textureFilter();
        textureTarget = data->textureTarget();
        shaderType = getTextureShaderType(textureTarget, data->hasRepeatScale());
    }
    setBlendingState(enableBlending);
    drawQuadInternal(shaderType, matrix, textureId, opacity,
                     textureTarget, textureFilter, quadColor, data->fillPortion(),
                     data->repeatScale());
}

void ShaderProgram::drawVideoLayerQuad(const TransformationMatrix& drawMatrix,
                                       float* textureMatrix, SkRect& geometry,
                                       int textureId)
{
    // switch to our custom yuv video rendering program
    glUseProgram(m_handleArray[Video].programHandle);
    // TODO: Merge drawVideoLayerQuad into drawQuad.
    TransformationMatrix modifiedDrawMatrix;
    modifiedDrawMatrix.scale3d(m_currentScale, m_currentScale, 1);
    modifiedDrawMatrix.multiply(drawMatrix);
    modifiedDrawMatrix.translate(geometry.fLeft, geometry.fTop);
    modifiedDrawMatrix.scale3d(geometry.width(), geometry.height(), 1);
    TransformationMatrix renderMatrix =
        m_clipProjectionMatrix * m_webViewMatrix * modifiedDrawMatrix;

    GLfloat projectionMatrix[16];
    GLUtils::toGLMatrix(projectionMatrix, renderMatrix);
    glUniformMatrix4fv(m_handleArray[Video].projMtxHandle, 1, GL_FALSE,
                       projectionMatrix);
    glUniformMatrix4fv(m_handleArray[Video].videoMtxHandle, 1, GL_FALSE,
                       textureMatrix);
    glActiveTexture(GL_TEXTURE0);
    glUniform1i(m_handleArray[Video].texSamplerHandle, 0);
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, textureId);

    GLint videoPosition = m_handleArray[Video].positionHandle;
    glBindBuffer(GL_ARRAY_BUFFER, m_textureBuffer[0]);
    glEnableVertexAttribArray(videoPosition);
    glVertexAttribPointer(videoPosition, 2, GL_FLOAT, GL_FALSE, 0, 0);

    setBlendingState(false);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

void ShaderProgram::setGLDrawInfo(const android::uirenderer::DrawGlInfo* info)
{
    GLUtils::convertToTransformationMatrix(info->transform, m_webViewMatrix);
    m_alphaLayer = info->isLayer;
    m_targetHeight = info->height;
}

// This function is called per tileGrid to minimize the computation overhead.
// The ortho projection and glViewport will map 1:1, so we don't need to
// worry about them here. Basically, if the current zoom scale / tile's scale
// plus the webview and layer transformation ends up at scale factor 1.0,
// then we can use point sampling.
bool ShaderProgram::usePointSampling(float tileScale,
                                     const TransformationMatrix* layerTransform)
{
    const float testSize = 1.0;
    FloatRect rect(0, 0, testSize, testSize);
    TransformationMatrix matrix;
    matrix.scale3d(m_currentScale, m_currentScale, 1);
    if (layerTransform)
        matrix.multiply(*layerTransform);
    matrix.scale3d(1.0 / tileScale, 1.0 / tileScale, 1);

    matrix = m_webViewMatrix * matrix;

    rect = matrix.mapRect(rect);

    float deltaWidth = abs(rect.width() - testSize);
    float deltaHeight = abs(rect.height() - testSize);

    if (deltaWidth < EPSILON && deltaHeight < EPSILON) {
        ALOGV("Point sampling : deltaWidth is %f, deltaHeight is %f", deltaWidth, deltaHeight);
        return true;
    }
    return false;
}

#if DEBUG_MATRIX
FloatRect ShaderProgram::debugMatrixTransform(const TransformationMatrix& matrix,
                                              const char* matrixName)
{
    FloatRect rect(0.0, 0.0, 1.0, 1.0);
    rect = matrix.mapRect(rect);
    ALOGV("After %s matrix:\n %f, %f rect.width() %f rect.height() %f",
          matrixName, rect.x(), rect.y(), rect.width(), rect.height());
    return rect;

}

void ShaderProgram::debugMatrixInfo(float currentScale,
                                    const TransformationMatrix& clipProjectionMatrix,
                                    const TransformationMatrix& webViewMatrix,
                                    const TransformationMatrix& modifiedDrawMatrix,
                                    const TransformationMatrix* layerMatrix)
{
    int viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);
    ALOGV("viewport %d, %d, %d, %d , currentScale %f",
          viewport[0], viewport[1], viewport[2], viewport[3], currentScale);
    IntRect currentGLViewport(viewport[0], viewport[1], viewport[2], viewport[3]);

    TransformationMatrix scaleMatrix;
    scaleMatrix.scale3d(currentScale, currentScale, 1.0);

    if (layerMatrix)
        debugMatrixTransform(*layerMatrix, "layerMatrix");

    TransformationMatrix debugMatrix = scaleMatrix * modifiedDrawMatrix;
    debugMatrixTransform(debugMatrix, "scaleMatrix * modifiedDrawMatrix");

    debugMatrix = webViewMatrix * debugMatrix;
    debugMatrixTransform(debugMatrix, "webViewMatrix * scaleMatrix * modifiedDrawMatrix");

    debugMatrix = clipProjectionMatrix * debugMatrix;
    FloatRect finalRect =
        debugMatrixTransform(debugMatrix, "all Matrix");
    // After projection, we will be in a (-1, 1) range and now we can map it back
    // to the (x,y) -> (x+width, y+height)
    ALOGV("final convert to screen coord x, y %f, %f width %f height %f , ",
          (finalRect.x() + 1) / 2 * currentGLViewport.width() + currentGLViewport.x(),
          (finalRect.y() + 1) / 2 * currentGLViewport.height() + currentGLViewport.y(),
          finalRect.width() * currentGLViewport.width() / 2,
          finalRect.height() * currentGLViewport.height() / 2);
}
#endif // DEBUG_MATRIX

} // namespace WebCore
#endif // USE(ACCELERATED_COMPOSITING)
