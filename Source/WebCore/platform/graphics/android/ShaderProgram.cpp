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
#include "FloatPoint3D.h"
#include "GLUtils.h"
#include "TilesManager.h"

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

namespace WebCore {

static const char gVertexShader[] =
    "attribute vec4 vPosition;\n"
    "uniform mat4 projectionMatrix;\n"
    "varying vec2 v_texCoord;\n"
    "void main() {\n"
    "  gl_Position = projectionMatrix * vPosition;\n"
    "  v_texCoord = vec2(vPosition);\n"
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

    if (tex2DProgram == -1
        || pureColorProgram == -1
        || tex2DInvProgram == -1
        || videoProgram == -1
        || texOESProgram == -1
        || texOESInvProgram == -1) {
        m_needsInit = true;
        return;
    }

    GLint pureColorPosition = glGetAttribLocation(pureColorProgram, "vPosition");
    GLint pureColorProjMtx = glGetUniformLocation(pureColorProgram, "projectionMatrix");
    GLint pureColorValue = glGetUniformLocation(pureColorProgram, "inputColor");
    m_handleArray[PureColor].init(-1, -1, pureColorPosition, pureColorProgram,
                                  pureColorProjMtx, pureColorValue, -1, -1);

    GLint tex2DAlpha = glGetUniformLocation(tex2DProgram, "alpha");
    GLint tex2DPosition = glGetAttribLocation(tex2DProgram, "vPosition");
    GLint tex2DProjMtx = glGetUniformLocation(tex2DProgram, "projectionMatrix");
    GLint tex2DTexSampler = glGetUniformLocation(tex2DProgram, "s_texture");
    m_handleArray[Tex2D].init(tex2DAlpha, -1, tex2DPosition, tex2DProgram,
                              tex2DProjMtx, -1, tex2DTexSampler, -1);

    GLint tex2DInvAlpha = glGetUniformLocation(tex2DInvProgram, "alpha");
    GLint tex2DInvContrast = glGetUniformLocation(tex2DInvProgram, "contrast");
    GLint tex2DInvPosition = glGetAttribLocation(tex2DInvProgram, "vPosition");
    GLint tex2DInvProjMtx = glGetUniformLocation(tex2DInvProgram, "projectionMatrix");
    GLint tex2DInvTexSampler = glGetUniformLocation(tex2DInvProgram, "s_texture");
    m_handleArray[Tex2DInv].init(tex2DInvAlpha, tex2DInvContrast,
                                 tex2DInvPosition, tex2DInvProgram,
                                 tex2DInvProjMtx, -1,
                                 tex2DInvTexSampler, -1);

    GLint texOESAlpha = glGetUniformLocation(texOESProgram, "alpha");
    GLint texOESPosition = glGetAttribLocation(texOESProgram, "vPosition");
    GLint texOESProjMtx = glGetUniformLocation(texOESProgram, "projectionMatrix");
    GLint texOESTexSampler = glGetUniformLocation(texOESProgram, "s_texture");
    m_handleArray[TexOES].init(texOESAlpha, -1, texOESPosition, texOESProgram,
                               texOESProjMtx, -1, texOESTexSampler, -1);

    GLint texOESInvAlpha = glGetUniformLocation(texOESInvProgram, "alpha");
    GLint texOESInvContrast = glGetUniformLocation(texOESInvProgram, "contrast");
    GLint texOESInvPosition = glGetAttribLocation(texOESInvProgram, "vPosition");
    GLint texOESInvProjMtx = glGetUniformLocation(texOESInvProgram, "projectionMatrix");
    GLint texOESInvTexSampler = glGetUniformLocation(texOESInvProgram, "s_texture");
    m_handleArray[TexOESInv].init(texOESInvAlpha, texOESInvContrast,
                                  texOESInvPosition, texOESInvProgram,
                                  texOESInvProjMtx, -1,
                                  texOESInvTexSampler, -1);

    GLint videoPosition = glGetAttribLocation(videoProgram, "vPosition");
    GLint videoProjMtx = glGetUniformLocation(videoProgram, "projectionMatrix");
    GLint videoTexSampler = glGetUniformLocation(videoProgram, "s_yuvTexture");
    GLint videoTexMtx = glGetUniformLocation(videoProgram, "textureMatrix");
    m_handleArray[Video].init(-1, -1, videoPosition, videoProgram,
                              videoProjMtx, -1, videoTexSampler,
                              videoTexMtx);

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

void ShaderProgram::setupDrawing(const IntRect& viewRect, const SkRect& visibleRect,
                                 const IntRect& webViewRect, int titleBarHeight,
                                 const IntRect& screenClip, float scale)
{
    m_webViewRect = webViewRect;
    m_titleBarHeight = titleBarHeight;

    //// viewport ////
    TransformationMatrix ortho;
    GLUtils::setOrthographicMatrix(ortho, visibleRect.fLeft, visibleRect.fTop,
                                   visibleRect.fRight, visibleRect.fBottom, -1000, 1000);
    // In most case , visibleRect / viewRect * scale should 1.0, but for the
    // translation case, the scale factor can be 1 but visibleRect is smaller
    // than viewRect, we need to tune in this factor to make sure we scale them
    // right. Conceptually, that means, no matter how animation affects the
    // visibleRect, the scaling should respect the viewRect if zoomScale is 1.0.
    // Note that at TiledPage, we already scale the tile size inversely to make
    // zooming animation right.
    float orthoScaleX = scale * visibleRect.width() / viewRect.width();
    float orthoScaleY = scale * visibleRect.height() / viewRect.height();

    TransformationMatrix orthoScale;
    orthoScale.scale3d(orthoScaleX, orthoScaleY, 1.0);

    m_projectionMatrix = ortho * orthoScale;
    m_viewport = visibleRect;
    m_currentScale = scale;


    //// viewRect ////
    m_viewRect = viewRect;

    // We do clipping using glScissor, which needs to take
    // coordinates in screen space. The following matrix transform
    // content coordinates in screen coordinates.
    TransformationMatrix viewTranslate;
    viewTranslate.translate(1.0, 1.0);

    TransformationMatrix viewScale;
    viewScale.scale3d(m_viewRect.width() * 0.5f, m_viewRect.height() * 0.5f, 1);

    m_documentToScreenMatrix = viewScale * viewTranslate * m_projectionMatrix;

    viewTranslate.scale3d(1, -1, 1);
    m_documentToInvScreenMatrix = viewScale * viewTranslate * m_projectionMatrix;

    IntRect rect(0, 0, m_webViewRect.width(), m_webViewRect.height());
    m_documentViewport = m_documentToScreenMatrix.inverse().mapRect(rect);


    //// clipping ////
    IntRect mclip = screenClip;

    // the clip from frameworks is in full screen coordinates
    mclip.setY(screenClip.y() - m_webViewRect.y() - m_titleBarHeight);
    FloatRect tclip = convertInvScreenCoordToScreenCoord(mclip);
    m_screenClip.setLocation(IntPoint(tclip.x(), tclip.y()));
    // use ceilf to handle view -> doc -> view coord rounding errors
    m_screenClip.setSize(IntSize(ceilf(tclip.width()), ceilf(tclip.height())));

    resetBlending();
}

// Calculate the matrix given the geometry.
void ShaderProgram::setProjectionMatrix(const SkRect& geometry, GLfloat* mtxPtr)
{
    TransformationMatrix translate;
    translate.translate3d(geometry.fLeft, geometry.fTop, 0.0);
    TransformationMatrix scale;
    scale.scale3d(geometry.width(), geometry.height(), 1.0);

    TransformationMatrix total;
    if (!m_alphaLayer)
        total = m_projectionMatrix * m_repositionMatrix * m_webViewMatrix
                * translate * scale;
    else
        total = m_projectionMatrix * translate * scale;

    GLUtils::toGLMatrix(mtxPtr, total);
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
ShaderType ShaderProgram::getTextureShaderType(GLenum textureTarget)
{
    ShaderType type = UndefinedShader;
    if (textureTarget == GL_TEXTURE_2D) {
        if (!TilesManager::instance()->invertedScreen())
            type = Tex2D;
        else {
            // With the new GPU texture upload path, we do not use an FBO
            // to blit the texture we receive from the TexturesGenerator thread.
            // To implement inverted rendering, we thus have to do the rendering
            // live, by using a different shader.
            type = Tex2DInv;
        }
    } else if (textureTarget == GL_TEXTURE_EXTERNAL_OES) {
        if (!TilesManager::instance()->invertedScreen())
            type = TexOES;
        else
            type = TexOESInv;
    }
    return type;
}

void ShaderProgram::drawQuad(const SkRect& geometry, int textureId, float opacity,
                             Color pureColor, GLenum textureTarget, GLint texFilter)
{
    ShaderType type = UndefinedShader;
    if (!textureId) {
        pureColor = shaderColor(pureColor, opacity);
        if (pureColor.rgb() == Color::transparent && opacity < 1.0)
            return;
        type = PureColor;
    } else
        type = getTextureShaderType(textureTarget);

    if (type != UndefinedShader) {
        // The matrix is either for the transfer queue or the tiles
        GLfloat* finalMatrix = m_transferProjMtx;
        GLfloat projectionMatrix[16];
        if (!geometry.isEmpty()) {
            setProjectionMatrix(geometry, projectionMatrix);
            finalMatrix = projectionMatrix;
        }
        setBlendingState(opacity < 1.0 || pureColor.hasAlpha());
        drawQuadInternal(type, finalMatrix, textureId, opacity, textureTarget,
                        texFilter, pureColor);
    }
    GLUtils::checkGlError("drawQuad");
}

// This function transform a clip rect extracted from the current layer
// into a clip rect in screen coordinates -- used by the clipping rects
FloatRect ShaderProgram::rectInScreenCoord(const TransformationMatrix& drawMatrix, const IntSize& size)
{
    FloatRect srect(0, 0, size.width(), size.height());
    TransformationMatrix renderMatrix = m_documentToScreenMatrix * drawMatrix;
    return renderMatrix.mapRect(srect);
}

// used by the partial screen invals
FloatRect ShaderProgram::rectInInvScreenCoord(const TransformationMatrix& drawMatrix, const IntSize& size)
{
    FloatRect srect(0, 0, size.width(), size.height());
    TransformationMatrix renderMatrix = m_documentToInvScreenMatrix * drawMatrix;
    return renderMatrix.mapRect(srect);
}

FloatRect ShaderProgram::rectInInvScreenCoord(const FloatRect& rect)
{
    return m_documentToInvScreenMatrix.mapRect(rect);
}

FloatRect ShaderProgram::rectInScreenCoord(const FloatRect& rect)
{
    return m_documentToScreenMatrix.mapRect(rect);
}

FloatRect ShaderProgram::convertScreenCoordToDocumentCoord(const FloatRect& rect)
{
    return m_documentToScreenMatrix.inverse().mapRect(rect);
}

FloatRect ShaderProgram::convertInvScreenCoordToScreenCoord(const FloatRect& rect)
{
    FloatRect documentRect = m_documentToInvScreenMatrix.inverse().mapRect(rect);
    return rectInScreenCoord(documentRect);
}

FloatRect ShaderProgram::convertScreenCoordToInvScreenCoord(const FloatRect& rect)
{
    FloatRect documentRect = m_documentToScreenMatrix.inverse().mapRect(rect);
    return rectInInvScreenCoord(documentRect);
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

    if (!m_screenClip.isEmpty())
        screenClip.intersect(m_screenClip);

    screenClip.setY(screenClip.y() + m_viewRect.y());
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

IntRect ShaderProgram::clippedRectWithViewport(const IntRect& rect, int margin)
{
    IntRect viewport(m_viewport.fLeft - margin, m_viewport.fTop - margin,
                     m_viewport.width() + margin, m_viewport.height() + margin);
    viewport.intersect(rect);
    return viewport;
}

float ShaderProgram::zValue(const TransformationMatrix& drawMatrix, float w, float h)
{
    TransformationMatrix modifiedDrawMatrix = drawMatrix;
    modifiedDrawMatrix.scale3d(w, h, 1);
    TransformationMatrix renderMatrix = m_projectionMatrix * modifiedDrawMatrix;
    FloatPoint3D point(0.5, 0.5, 0.0);
    FloatPoint3D result = renderMatrix.mapPoint(point);
    return result.z();
}

void ShaderProgram::drawQuadInternal(ShaderType type, const GLfloat* matrix,
                                    int textureId, float opacity,
                                    GLenum textureTarget, GLenum filter,
                                    const Color& pureColor)
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

void ShaderProgram::drawLayerQuad(const TransformationMatrix& drawMatrix,
                                  const SkRect& geometry, int textureId,
                                  float opacity, bool forceBlending,
                                  GLenum textureTarget,
                                  Color pureColor)
{
    TransformationMatrix modifiedDrawMatrix = drawMatrix;
    // move the drawing depending on where the texture is on the layer
    modifiedDrawMatrix.translate(geometry.fLeft, geometry.fTop);
    modifiedDrawMatrix.scale3d(geometry.width(), geometry.height(), 1);

    TransformationMatrix renderMatrix;
    if (!m_alphaLayer)
        renderMatrix = m_projectionMatrix * m_repositionMatrix
                       * m_webViewMatrix * modifiedDrawMatrix;
    else
        renderMatrix = m_projectionMatrix * modifiedDrawMatrix;

    GLfloat projectionMatrix[16];
    GLUtils::toGLMatrix(projectionMatrix, renderMatrix);
    bool enableBlending = forceBlending || opacity < 1.0;

    ShaderType type = UndefinedShader;
    if (!textureId) {
        pureColor = shaderColor(pureColor, opacity);
        if (pureColor.rgb() == Color::transparent && enableBlending)
            return;
        type = PureColor;
    } else
        type = getTextureShaderType(textureTarget);

    if (type != UndefinedShader) {
        setBlendingState(enableBlending);
        drawQuadInternal(type, projectionMatrix, textureId, opacity,
                        textureTarget, GL_LINEAR, pureColor);
    }

    GLUtils::checkGlError("drawLayerQuad");
}

void ShaderProgram::drawVideoLayerQuad(const TransformationMatrix& drawMatrix,
                                       float* textureMatrix, SkRect& geometry,
                                       int textureId)
{
    // switch to our custom yuv video rendering program
    glUseProgram(m_handleArray[Video].programHandle);

    TransformationMatrix modifiedDrawMatrix = drawMatrix;
    modifiedDrawMatrix.translate(geometry.fLeft, geometry.fTop);
    modifiedDrawMatrix.scale3d(geometry.width(), geometry.height(), 1);
    TransformationMatrix renderMatrix = m_projectionMatrix * modifiedDrawMatrix;

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

void ShaderProgram::setWebViewMatrix(const float* matrix, bool alphaLayer)
{
    GLUtils::convertToTransformationMatrix(matrix, m_webViewMatrix);
    m_alphaLayer = alphaLayer;
}

void ShaderProgram::calculateAnimationDelta()
{
    // The matrix contains the scrolling info, so this rect is starting from
    // the m_viewport.
    // So we just need to map the webview's visible rect using the matrix,
    // calculate the difference b/t transformed rect and the webViewRect,
    // then we can get the delta x , y caused by the animation.
    // Note that the Y is for reporting back to GL viewport, so it is inverted.
    // When it is alpha animation, then we rely on the framework implementation
    // such that there is no matrix applied in native webkit.
    if (!m_alphaLayer) {
        FloatRect rect(m_viewport.fLeft * m_currentScale,
                       m_viewport.fTop * m_currentScale,
                       m_webViewRect.width(),
                       m_webViewRect.height());
        rect = m_webViewMatrix.mapRect(rect);
        m_animationDelta.setX(rect.x() - m_webViewRect.x() );
        m_animationDelta.setY(rect.y() + rect.height() - m_webViewRect.y()
                              - m_webViewRect.height() - m_titleBarHeight);

        m_repositionMatrix.makeIdentity();
        m_repositionMatrix.translate3d(-m_webViewRect.x(), -m_webViewRect.y() - m_titleBarHeight, 0);
        m_repositionMatrix.translate3d(m_viewport.fLeft * m_currentScale, m_viewport.fTop * m_currentScale, 0);
        m_repositionMatrix.translate3d(-m_animationDelta.x(), -m_animationDelta.y(), 0);
    } else {
        m_animationDelta.setX(0);
        m_animationDelta.setY(0);
        m_repositionMatrix.makeIdentity();
    }

}

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
