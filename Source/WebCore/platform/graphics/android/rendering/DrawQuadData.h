/*
 * Copyright 2012, The Android Open Source Project
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

#ifndef DrawQuadData_h
#define DrawQuadData_h

#if USE(ACCELERATED_COMPOSITING)

#include "Color.h"
#include "FloatRect.h"
#include "SkRect.h"
#include <GLES2/gl2.h>

namespace WebCore {

class TransformationMatrix;

enum DrawQuadType {
    BaseQuad,
    LayerQuad,
    Blit        // 1:1 straight pixel blit
};

// Both PureColorQuadData and TextureQuadData share the data from DrawQuadData.
class DrawQuadData {
public:
    DrawQuadData(DrawQuadType type = BaseQuad,
                 const TransformationMatrix* drawMatrix = 0,
                 const SkRect* geometry = 0,
                 float opacity = 1.0f,
                 bool forceBlending = true,
                 FloatRect fillPortion = FloatRect(0.0f, 0.0f, 1.0f, 1.0f))
        : m_type(type)
        , m_drawMatrix(drawMatrix)
        , m_geometry(geometry)
        , m_opacity(opacity)
        , m_forceBlending(forceBlending)
        , m_fillPortion(fillPortion.x(), fillPortion.y(),
                        fillPortion.width(), fillPortion.height())
    {
    }

    DrawQuadData(const DrawQuadData& data)
        : m_type(data.m_type)
        , m_drawMatrix(data.m_drawMatrix)
        , m_geometry(data.m_geometry)
        , m_opacity(data.m_opacity)
        , m_forceBlending(data.m_forceBlending)
        , m_fillPortion(data.m_fillPortion.x(), data.m_fillPortion.y(),
                        data.m_fillPortion.width(), data.m_fillPortion.height())
    {
    }

    virtual ~DrawQuadData() {};

    DrawQuadType type() const { return m_type; }
    const TransformationMatrix* drawMatrix() const { return m_drawMatrix; }
    const SkRect* geometry() const { return m_geometry; }
    float opacity() const { return m_opacity; }
    bool forceBlending() const { return m_forceBlending; }

    void updateDrawMatrix(TransformationMatrix* matrix) { m_drawMatrix = matrix; }
    void updateGeometry(SkRect* rect) { m_geometry = rect; }
    void updateOpacity(float opacity) { m_opacity = opacity; }

    virtual bool pureColor() const { return false; }

    virtual Color quadColor() const { return Color(); }

    virtual int textureId() const { return 0; }
    virtual GLint textureFilter() const { return 0; }
    virtual GLenum textureTarget() const { return 0; }
    virtual FloatRect fillPortion() const { return m_fillPortion; }
    virtual bool hasRepeatScale() const { return false; }
    virtual FloatSize repeatScale() const { return FloatSize(); }

private:
    DrawQuadType m_type;
    const TransformationMatrix* m_drawMatrix;
    const SkRect* m_geometry;
    float m_opacity;
    bool m_forceBlending;
    FloatRect m_fillPortion;
};

class PureColorQuadData : public DrawQuadData {
public:
    PureColorQuadData(Color color,
                      DrawQuadType type = BaseQuad,
                      const TransformationMatrix* drawMatrix = 0,
                      const SkRect* geometry = 0,
                      float opacity = 1.0f,
                      bool forceBlending = true)
        : DrawQuadData(type, drawMatrix, geometry, opacity, forceBlending)
    {
        m_quadColor = color;
    }

    PureColorQuadData(const DrawQuadData& data, Color color)
        : DrawQuadData(data)
    {
        m_quadColor = color;
    }

    virtual ~PureColorQuadData() {};
    virtual bool pureColor() const { return true; }
    virtual Color quadColor() const { return m_quadColor; }
    void updateColor(const Color& color) { m_quadColor = color; }

private:
    Color m_quadColor;
};

class TextureQuadData : public DrawQuadData {
public:
    TextureQuadData(int textureId,
                    GLenum textureTarget = GL_TEXTURE_2D,
                    GLint textureFilter = GL_LINEAR,
                    DrawQuadType type = BaseQuad,
                    const TransformationMatrix* drawMatrix = 0,
                    const SkRect* geometry = 0,
                    float opacity = 1.0f,
                    bool forceBlending = true,
                    FloatRect fillPortion = FloatRect(0.0f, 0.0f, 1.0f, 1.0f),
                    FloatSize repeatScale = FloatSize())
        : DrawQuadData(type, drawMatrix, geometry, opacity, forceBlending, fillPortion)
    {
        m_textureId = textureId;
        m_textureTarget = textureTarget;
        m_textureFilter = textureFilter;
        m_repeatScale = repeatScale;
    }

    TextureQuadData(const DrawQuadData& data,
                    int textureId,
                    GLenum textureTarget = GL_TEXTURE_2D,
                    GLint textureFilter = GL_LINEAR)
        : DrawQuadData(data)
    {
        m_textureId = textureId;
        m_textureTarget = textureTarget;
        m_textureFilter = textureFilter;
    }

    virtual ~TextureQuadData() {};
    virtual bool pureColor() const { return false; }

    virtual int textureId() const { return m_textureId; }
    virtual GLint textureFilter() const { return m_textureFilter; }
    virtual GLenum textureTarget() const { return m_textureTarget; }

    void updateTextureId(int newId) { m_textureId = newId; }
    virtual bool hasRepeatScale() const { return !m_repeatScale.isEmpty(); }
    virtual FloatSize repeatScale() const { return m_repeatScale; }
private:
    int m_textureId;
    GLint m_textureFilter;
    GLenum m_textureTarget;
    FloatSize m_repeatScale;
};

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
#endif // DrawQuadData_h
