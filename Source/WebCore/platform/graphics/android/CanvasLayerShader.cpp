/*
Copyright (c) 2013, The Linux Foundation. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above
      copyright notice, this list of conditions and the following
      disclaimer in the documentation and/or other materials provided
      with the distribution.
    * Neither the name of The Linux Foundation nor the names of its
      contributors may be used to endorse or promote products derived
      from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "config.h"
#include "CanvasLayerShader.h"

#if USE(ACCELERATED_COMPOSITING)

#include "GLUtils.h"
#include <GLES2/gl2.h>
#include <cutils/log.h>

namespace WebCore{

static const char gCVertexShader[] =
    "attribute vec4 vPosition;\n"
    "attribute vec2 vTexCoord;\n"
    "varying vec2 outTexCoords;\n"
    "void main() {\n"
    "  outTexCoords = vTexCoord;\n"
    "  gl_Position = vPosition;\n"
    "}\n";

static const char gCFragmentShader[] =
    "precision mediump float;\n"
    "varying vec2 outTexCoords; \n"
    "uniform float alpha; \n"
    "uniform sampler2D s_texture; \n"
    "void main() {\n"
    "  gl_FragColor = texture2D(s_texture, outTexCoords); \n"
    "  //gl_FragColor = vec4(outTexCoords.x, outTexCoords.y, 0.0, 1.0); \n"
    "  gl_FragColor *= alpha; "
    "}\n";

//Code to load and compile a shader
GLuint CanvasLayerShader::loadShader(GLenum shaderType, const char* shaderSource)
{
    GLuint shader = glCreateShader(shaderType);
    if (shader)
    {
        glShaderSource(shader, 1, &shaderSource, 0);
        glCompileShader(shader);
        GLint compileStatus = 0;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &compileStatus);

        if (!compileStatus)
        {
            GLint info = 0;
            glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &info);
            if (info)
            {
                char* buffer = (char*) malloc(info);
                if (buffer)
                {
                    glGetShaderInfoLog(shader, info, 0, buffer);
                    SLOGD("Canvas shader compilation failed: %s", buffer);
                    free(buffer);
                }
            glDeleteShader(shader);
            shader = 0;
            }
        }
    }
    return shader;
}

GLuint CanvasLayerShader::createProgram(const char* vSource, const char* fSource)
{
    //Load and compile the vertex shader
    GLuint vertexShader = loadShader(GL_VERTEX_SHADER, vSource);
    if (!vertexShader)
    {
        SLOGD("Couldn't load the vertex shader");
        return -1;
    }

    //Load and compile the fragment shader
    GLuint fragmentShader = loadShader(GL_FRAGMENT_SHADER, fSource);
    if (!fragmentShader)
    {
        SLOGD("couldn't load the pixel shader!");
        return -1;
    }

    //Create the program
    GLuint program = glCreateProgram();

    if (program)
    {
        SLOGD("++++++++++++++++++++++++++++++++++++++++++++++Creating shader for canvas");

        glAttachShader(program, vertexShader);
        GLUtils::checkGlError("Attaching canvas vertex shader to program", false);

        glAttachShader(program, fragmentShader);
        GLUtils::checkGlError("Attaching canvas fragment shader to program", false);

        //Linking program
        glLinkProgram(program);
        GLint status = GL_FALSE;
        glGetProgramiv(program, GL_LINK_STATUS, &status);

        if (status != GL_TRUE)
        {
            GLint info = 0;
            glGetProgramiv(program, GL_INFO_LOG_LENGTH, &info);

            if (info)
            {
                char* buffer = (char*) malloc(info);
                if (buffer)
                {
                    glGetProgramInfoLog(program, info, 0, buffer);

                    SLOGD("Could not link canvas program: %s", buffer);
                    free(buffer);
                }
            }
            glDeleteProgram(program);
            program = -1;
        }
    }
    return program;
}

CanvasLayerShader::CanvasLayerShader()
{}

void CanvasLayerShader::initialize()
{
    m_program = createProgram(gCVertexShader, gCFragmentShader);

    if(m_program == -1)
        return;

    //Get uniforms
    m_suAlpha = glGetUniformLocation(m_program, "alpha");
    m_suSampler = glGetUniformLocation(m_program, "s_texture");

    //Get attribs
    m_saPos = glGetAttribLocation(m_program, "vPosition");
    m_saTexCoords = glGetAttribLocation(m_program, "vTexCoord");
}

//Drawing functions
/*
void CanvasLayerShader::setViewport(SkRect& viewport)
{
    TransformationMatrix matrix;
    GLUtils::setOrthographicMatrix(matrix, viewport.fLeft, viewport.fTop, viewport.fRight, viewport.fBottom, -1000, 1000);

    m_projectionMatrix = matrix;
    m_viewport = viewport;
}
*/

void CanvasLayerShader::resetBlending()
{
    glDisable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    glBlendEquation(GL_FUNC_ADD);
    m_blendingEnabled = false;
}

void CanvasLayerShader::setBlendingState(bool enableBlending)
{
    if(enableBlending == m_blendingEnabled)
        return;

    if(enableBlending)
        glEnable(GL_BLEND);
    else
        glDisable(GL_BLEND);

    m_blendingEnabled = enableBlending;
}

void CanvasLayerShader::cleanupData(int textureId)
{
    GLfloat* vertex_buffer_data = NULL;
    GLfloat* texture_buffer_data = NULL;
    GLuint gvertex_buffer = -1;
    GLuint gtexture_buffer = -1;
    int numVertices = -1;

    std::map<int, GLfloat*>::iterator v_it = m_vertex_buffer_data_map.find(textureId);
    std::map<int, GLfloat*>::iterator t_it = m_texture_buffer_data_map.find(textureId);
    std::map<int, GLuint>::iterator g_vit = m_gvertex_buffer_map.find(textureId);
    std::map<int, GLuint>::iterator g_tit = m_gtexture_buffer_map.find(textureId);
    std::map<int, int>::iterator num_it = m_numVertices_map.find(textureId);

    if( v_it != m_vertex_buffer_data_map.end() &&
        t_it != m_texture_buffer_data_map.end() &&
        g_vit != m_gvertex_buffer_map.end() &&
        g_tit != m_gtexture_buffer_map.end() &&
        num_it != m_numVertices_map.end())
    {
        vertex_buffer_data = v_it->second;
        texture_buffer_data = t_it->second;
        gvertex_buffer = g_vit->second;
        gtexture_buffer = g_tit->second;
        numVertices = num_it->second;
    }

    if(vertex_buffer_data != NULL)
        free(vertex_buffer_data);

    if(texture_buffer_data != NULL)
        free(texture_buffer_data);

    glDeleteBuffers(1, &gvertex_buffer);

    glDeleteBuffers(1, &gtexture_buffer);


    m_vertex_buffer_data_map.erase(v_it);
    m_texture_buffer_data_map.erase(t_it);
    m_gvertex_buffer_map.erase(g_vit);
    m_gtexture_buffer_map.erase(g_tit);
    m_numVertices_map.erase(num_it);
}

bool CanvasLayerShader::drawPrimitives(std::vector<SkRect>& primitives, std::vector<FloatRect>& texturecoords,
                                        std::vector<int>& primScaleX, std::vector<int>& primScaleY, std::vector<SkMatrix>& primMatrix,
                                        int textureId, TransformationMatrix& matrix, float opacity)
{
    int num_vertices_per_primitive = 6; //Later to be 4

    GLfloat* vertex_buffer_data = NULL;
    GLfloat* texture_buffer_data = NULL;
    GLuint gvertex_buffer = -1;
    GLuint gtexture_buffer = -1;
    int numVertices = -1;

    std::map<int, GLfloat*>::iterator v_it = m_vertex_buffer_data_map.find(textureId);
    std::map<int, GLfloat*>::iterator t_it = m_texture_buffer_data_map.find(textureId);
    std::map<int, GLuint>::iterator g_vit = m_gvertex_buffer_map.find(textureId);
    std::map<int, GLuint>::iterator g_tit = m_gtexture_buffer_map.find(textureId);
    std::map<int, int>::iterator num_it = m_numVertices_map.find(textureId);

    if( v_it != m_vertex_buffer_data_map.end() &&
        t_it != m_texture_buffer_data_map.end() &&
        g_vit != m_gvertex_buffer_map.end() &&
        g_tit != m_gtexture_buffer_map.end() &&
        num_it != m_numVertices_map.end())
    {
        vertex_buffer_data = v_it->second;
        texture_buffer_data = t_it->second;
        gvertex_buffer = g_vit->second;
        gtexture_buffer = g_tit->second;
        numVertices = num_it->second;
    }

    if(primitives.size() != texturecoords.size())
    {
        SLOGD("++++++++++++++++++ERROR");
        return false;
    }

    if(vertex_buffer_data == NULL)
    {
        glGenBuffers(2, m_vertexBuffer);
        numVertices = num_vertices_per_primitive * primitives.size();  //Later this will be 4 when switching to elements
        vertex_buffer_data = (GLfloat*)malloc(2 * numVertices * sizeof(GLfloat));
        texture_buffer_data = (GLfloat*)malloc(2 * numVertices * sizeof(GLfloat));
        ///TODO:: Error checking if memory is indeed allocated
        //Add it to the map and also set it to the variables
        gvertex_buffer = m_vertexBuffer[0];
        gtexture_buffer = m_vertexBuffer[1];

        m_vertex_buffer_data_map.insert(std::make_pair(textureId, vertex_buffer_data));
        m_texture_buffer_data_map.insert(std::make_pair(textureId, texture_buffer_data));
        m_numVertices_map.insert(std::make_pair(textureId, numVertices));
        m_gvertex_buffer_map.insert(std::make_pair(textureId, gvertex_buffer));
        m_gtexture_buffer_map.insert(std::make_pair(textureId, gtexture_buffer));
    }
    else if(numVertices != num_vertices_per_primitive * primitives.size())
    {
        numVertices = num_vertices_per_primitive * primitives.size();

        vertex_buffer_data = (GLfloat*)realloc((void*)vertex_buffer_data, 2 * numVertices * sizeof(GLfloat));
        texture_buffer_data = (GLfloat*)realloc((void*)texture_buffer_data, 2 * numVertices * sizeof(GLfloat));
        //TODO::Error checking if memory is allocated
        //Replace the data in the maps
        m_vertex_buffer_data_map.erase(v_it);
        m_texture_buffer_data_map.erase(t_it);
        m_numVertices_map.erase(num_it);

        m_vertex_buffer_data_map.insert(std::make_pair(textureId, vertex_buffer_data));
        m_texture_buffer_data_map.insert(std::make_pair(textureId, texture_buffer_data));
        m_numVertices_map.insert(std::make_pair(textureId, numVertices));
    }

    if(vertex_buffer_data == NULL || texture_buffer_data == NULL)
        return false;

    int totalcount = 0;

    //Populate the primitive vertices
    for(int jj =0; jj < primitives.size(); ++jj)
    {
        SkRect& geometry = primitives[jj];

        int& scaleX = primScaleX[jj];
        int& scaleY = primScaleY[jj];
        SkMatrix& tempMatrix = primMatrix[jj];

        //Convert to right format
        TransformationMatrix tempWebCoreMatrix;
        tempWebCoreMatrix.setM11(SkScalarToFloat(tempMatrix[0]));  //m11 scaleX
        tempWebCoreMatrix.setM12(SkScalarToFloat(tempMatrix[3]));  //m12 skewY
        tempWebCoreMatrix.setM21(SkScalarToFloat(tempMatrix[1]));  //m21 skewX
        tempWebCoreMatrix.setM22(SkScalarToFloat(tempMatrix[4]));  //m22 scaleY
        tempWebCoreMatrix.setM41(SkScalarToFloat(tempMatrix[2]));  //m41 transX
        tempWebCoreMatrix.setM42(SkScalarToFloat(tempMatrix[5]));  //m42 transY
        tempWebCoreMatrix.setM14(SkScalarToFloat(tempMatrix[6]));  //m14 persp0
        tempWebCoreMatrix.setM24(SkScalarToFloat(tempMatrix[7]));  //m24 persp1
        tempWebCoreMatrix.setM44(SkScalarToFloat(tempMatrix[8]));  //m44 persp2


        float fLeft = SkScalarRound(geometry.fLeft);
        float fTop = SkScalarRound(geometry.fTop);
        float fRight = SkScalarRound(geometry.fRight);
        float fBottom = SkScalarRound(geometry.fBottom);

        float width = fRight - fLeft;
        float height = fBottom - fTop;

        TransformationMatrix translate;
        translate.translate3d(fLeft, fTop, 0.0);
        TransformationMatrix scale;
        scale.scale3d(width, height, 1.0);

        //TransformationMatrix total = m_projectionMatrix;
        TransformationMatrix total = m_surfaceProjectionMatrix;
        total.multiply(matrix);
        total.multiply(tempWebCoreMatrix);
        total.multiply(translate);
        total.multiply(scale);

        FloatRect test(0.0f, 0.0f, 1.0f, 1.0f);

        FloatRect testfinal = total.mapRect(test);

        //Texture coords
        FloatRect& texval = texturecoords[jj];
        vertex_buffer_data[totalcount] = testfinal.location().x();//primitives[jj].fLeft;
        vertex_buffer_data[totalcount + 1] = testfinal.location().y();//primitives[jj].fTop;

        //Tex stuff
        if(scaleX < 0 && scaleY > 0)
        {
            texture_buffer_data[totalcount] = texval.location().x() + texval.width();
            texture_buffer_data[totalcount + 1] = texval.location().y() + texval.height();
        }
        else if(scaleX > 0 && scaleY > 0)
        {
            texture_buffer_data[totalcount] = texval.location().x();
            texture_buffer_data[totalcount + 1] = texval.location().y() + texval.height();
        }
        else if(scaleX < 0 && scaleY < 0)
        {
            texture_buffer_data[totalcount] = texval.location().x() + texval.width();
            texture_buffer_data[totalcount + 1] = texval.location().y();
        }
        else if(scaleX > 0 && scaleY < 0)
        {
            texture_buffer_data[totalcount] = texval.location().x();
            texture_buffer_data[totalcount + 1] = texval.location().y();
        }

        //2
        totalcount = totalcount + 2;
        vertex_buffer_data[totalcount] = testfinal.location().x() + testfinal.width();//primitives[jj].fLeft;
        vertex_buffer_data[totalcount + 1] = testfinal.location().y();//primitives[jj].fBottom;

        //Tex stuff
        if(scaleX < 0 && scaleY > 0)
        {
            texture_buffer_data[totalcount] = texval.location().x();
            texture_buffer_data[totalcount + 1] = texval.location().y() + texval.height();
        }
        else if(scaleX > 0 && scaleY > 0)
        {
            texture_buffer_data[totalcount] = texval.location().x() + texval.width();
            texture_buffer_data[totalcount + 1] = texval.location().y() + texval.height();
        }
        else if(scaleX < 0 && scaleY < 0)
        {
            texture_buffer_data[totalcount] = texval.location().x();
            texture_buffer_data[totalcount + 1] = texval.location().y();
        }
        else if(scaleX > 0 && scaleY < 0)
        {
            texture_buffer_data[totalcount] = texval.location().x() + texval.width();
            texture_buffer_data[totalcount + 1] = texval.location().y();
        }

        //3
        totalcount = totalcount + 2;
        vertex_buffer_data[totalcount] = testfinal.location().x();//primitives[jj].fRight;
        vertex_buffer_data[totalcount + 1] = testfinal.location().y() + testfinal.height();//primitives[jj].fBottom;

        //Tex stuff
        if(scaleX < 0 && scaleY > 0)
        {
            texture_buffer_data[totalcount] = texval.location().x() + texval.width();
            texture_buffer_data[totalcount + 1] = texval.location().y();
        }
        else if(scaleX > 0 && scaleY > 0)
        {
            texture_buffer_data[totalcount] = texval.location().x();
            texture_buffer_data[totalcount + 1] = texval.location().y();
        }
        else if(scaleX < 0 && scaleY < 0)
        {
            texture_buffer_data[totalcount] = texval.location().x() + texval.width();
            texture_buffer_data[totalcount + 1] = texval.location().y() + texval.height();
        }
        else if(scaleX > 0 && scaleY < 0)
        {
            texture_buffer_data[totalcount] = texval.location().x();
            texture_buffer_data[totalcount + 1] = texval.location().y() + texval.height();
        }

        //Triangle 2
        //1
        totalcount = totalcount + 2;
        vertex_buffer_data[totalcount] = testfinal.location().x();//primitives[jj].fLeft;
        vertex_buffer_data[totalcount + 1] = testfinal.location().y() + testfinal.height();//primitives[jj].fTop;

        //Tex stuff
        if(scaleX < 0 && scaleY > 0)
        {
            texture_buffer_data[totalcount] = texval.location().x() + texval.width();
            texture_buffer_data[totalcount + 1] = texval.location().y();
        }
        else if(scaleX > 0 && scaleY > 0)
        {
            texture_buffer_data[totalcount] = texval.location().x();
            texture_buffer_data[totalcount + 1] = texval.location().y();
        }
        else if(scaleX < 0 && scaleY < 0)
        {
            texture_buffer_data[totalcount] = texval.location().x() + texval.width();
            texture_buffer_data[totalcount + 1] = texval.location().y() + texval.height();
        }
        else if(scaleX > 0 && scaleY < 0)
        {
            texture_buffer_data[totalcount] = texval.location().x();
            texture_buffer_data[totalcount + 1] = texval.location().y() + texval.height();
        }

        //2
        totalcount = totalcount + 2;
        vertex_buffer_data[totalcount] = testfinal.location().x() + testfinal.width();//primitives[jj].fRight;
        vertex_buffer_data[totalcount + 1] = testfinal.location().y();//primitives[jj].fBottom;

        //Tex stuff
        if(scaleX < 0 && scaleY > 0)
        {
            texture_buffer_data[totalcount] = texval.location().x();
            texture_buffer_data[totalcount + 1] = texval.location().y() + texval.height();
        }
        else if(scaleX > 0 && scaleY > 0)
        {
            texture_buffer_data[totalcount] = texval.location().x() + texval.width();
            texture_buffer_data[totalcount + 1] = texval.location().y() + texval.height();
        }
        else if(scaleX < 0 && scaleY < 0)
        {
            texture_buffer_data[totalcount] = texval.location().x();
            texture_buffer_data[totalcount + 1] = texval.location().y();
        }
        else if(scaleX > 0 && scaleY < 0)
        {
            texture_buffer_data[totalcount] = texval.location().x() + texval.width();
            texture_buffer_data[totalcount + 1] = texval.location().y();
        }

        //3
        totalcount = totalcount + 2;
        vertex_buffer_data[totalcount] = testfinal.location().x() + testfinal.width();//primitives[jj].fRight;
        vertex_buffer_data[totalcount + 1] = testfinal.location().y() + testfinal.height();//primitives[jj].fTop;

        //Tex stuff
        if(scaleX < 0 && scaleY > 0)
        {
            texture_buffer_data[totalcount] = texval.location().x();
            texture_buffer_data[totalcount + 1] = texval.location().y();
        }
        else if(scaleX > 0 && scaleY > 0)
        {
            texture_buffer_data[totalcount] = texval.location().x() + texval.width();
            texture_buffer_data[totalcount + 1] = texval.location().y();
        }
        else if(scaleX <0 && scaleY < 0)
        {
            texture_buffer_data[totalcount] = texval.location().x();
            texture_buffer_data[totalcount + 1] = texval.location().y() + texval.height();
        }
        else if(scaleX > 0 && scaleY < 0)
        {
            texture_buffer_data[totalcount] = texval.location().x() + texval.width();
            texture_buffer_data[totalcount + 1] = texval.location().y() + texval.height();
        }

        totalcount = totalcount + 2;
    }

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textureId);
    bool filter = false;

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);//GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);//GL_REPEAT);
    if (filter) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    } else {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    }

    glBindBuffer(GL_ARRAY_BUFFER, gvertex_buffer);
    glBufferData(GL_ARRAY_BUFFER, 2 * numVertices * sizeof(GLfloat), vertex_buffer_data, GL_STREAM_DRAW);

    bool val = GLUtils::checkGlError("glBufferData", false);
    if(val)
        return false;

    glEnableVertexAttribArray(m_saPos);
    glVertexAttribPointer(m_saPos, 2, GL_FLOAT, GL_FALSE, 0, 0);

    glBindBuffer(GL_ARRAY_BUFFER, gtexture_buffer);
    glBufferData(GL_ARRAY_BUFFER, 2 * numVertices * sizeof(GLfloat), texture_buffer_data, GL_STREAM_DRAW);

    bool val2 = GLUtils::checkGlError("glBufferData", false);
    if(val2)
        return false;

    glEnableVertexAttribArray(m_saTexCoords);
    glVertexAttribPointer(m_saTexCoords, 2, GL_FLOAT, GL_FALSE, 0, 0);

    glUniform1f(getAlpha(), opacity);
    glEnable(GL_BLEND);GLUtils::checkGlError("glEnable(GL_BLEND)", false);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

    glDrawArrays(GL_TRIANGLES, 0, numVertices);
    return true;
}

}   // namespace WebCore

#endif  // if USE(ACCELERATED_COMPOSITING)
