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

#ifndef FixedPositioning_h
#define FixedPositioning_h

#if USE(ACCELERATED_COMPOSITING)

#include "LayerAndroid.h"
#include "Length.h"

namespace WebCore {

class IFrameLayerAndroid;

struct SkLength {
    enum SkLengthType { Undefined, Auto, Relative,
        Percent, Fixed, Static, Intrinsic, MinIntrinsic };
    SkLengthType type;
    SkScalar value;
    SkLength()
    {
        type = Undefined;
        value = 0;
    }
    bool defined() const
    {
        if (type == Undefined)
            return false;
        return true;
    }
    void setFixedValue(float v)
    {
        type = Fixed;
        value = v;
    }
    void setAuto()
    {
        type = Auto;
    }
    float calcFloatValue(float max) const
    {
        switch (type) {
        case Percent:
            return (max * value) / 100.0f;
        case Fixed:
            return value;
        default:
            return value;
        }
    }

    static SkLength convertLength(Length len)
    {
        SkLength length;
        length.type = SkLength::Undefined;
        length.value = 0;
        if (len.type() == WebCore::Percent) {
            length.type = SkLength::Percent;
            length.value = len.percent();
        }
        if (len.type() == WebCore::Fixed) {
            length.type = SkLength::Fixed;
            length.value = len.value();
        }
        return length;
    }

};

class FixedPositioning {

public:
    FixedPositioning(LayerAndroid* layer = 0) : m_layer(layer) {}
    FixedPositioning(LayerAndroid* layer, const FixedPositioning& position);
    virtual ~FixedPositioning() {};

    virtual bool isBackgroundImagePositioning() { return true; }
    virtual FixedPositioning* copy(LayerAndroid* layer) const {
        return new FixedPositioning(layer, *this);
    }

    void setFixedPosition(SkLength left, // CSS left property
                          SkLength top, // CSS top property
                          SkLength right, // CSS right property
                          SkLength bottom, // CSS bottom property
                          SkLength marginLeft, // CSS margin-left property
                          SkLength marginTop, // CSS margin-top property
                          SkLength marginRight, // CSS margin-right property
                          SkLength marginBottom, // CSS margin-bottom property
                          const IntPoint& renderLayerPos, // For undefined fixed position
                          SkRect viewRect) { // view rect, can be smaller than the layer's
        m_fixedLeft = left;
        m_fixedTop = top;
        m_fixedRight = right;
        m_fixedBottom = bottom;
        m_fixedMarginLeft = marginLeft;
        m_fixedMarginTop = marginTop;
        m_fixedMarginRight = marginRight;
        m_fixedMarginBottom = marginBottom;
        m_fixedRect = viewRect;
        m_renderLayerPos = renderLayerPos;
    }

    SkRect getViewport(SkRect viewport, IFrameLayerAndroid* parentIframeLayer);
    virtual IFrameLayerAndroid* updatePosition(SkRect viewPort,
                                               IFrameLayerAndroid* parentIframeLayer);

    void contentDraw(SkCanvas* canvas, Layer::PaintStyle style);

    void dumpLayer(FILE*, int indentLevel) const;

    // ViewStateSerializer friends
    friend void android::serializeLayer(LayerAndroid* layer, SkWStream* stream);
    friend LayerAndroid* android::deserializeLayer(int version, SkStream* stream);

protected:
    LayerAndroid* m_layer;

    SkLength m_fixedLeft;
    SkLength m_fixedTop;
    SkLength m_fixedRight;
    SkLength m_fixedBottom;
    SkLength m_fixedMarginLeft;
    SkLength m_fixedMarginTop;
    SkLength m_fixedMarginRight;
    SkLength m_fixedMarginBottom;
    SkRect m_fixedRect;

    // When fixed element is undefined or auto, the render layer's position
    // is needed for offset computation
    IntPoint m_renderLayerPos;
};

class BackgroundImagePositioning : public FixedPositioning {
public:
    BackgroundImagePositioning(LayerAndroid* layer)
        : FixedPositioning(layer)
        , m_repeatX(false)
        , m_repeatY(false)
        , m_nbRepeatX(0)
        , m_nbRepeatY(0)
        , m_offsetX(0)
        , m_offsetY(0)
    {}
    BackgroundImagePositioning(LayerAndroid* layer, const BackgroundImagePositioning& position);
    virtual bool isBackgroundImagePositioning() { return true; }
    virtual FixedPositioning* copy(LayerAndroid* layer) const {
        return new BackgroundImagePositioning(layer, *this);
    }
    void setPosition(SkLength left, SkLength top) {
        m_fixedLeft = left;
        m_fixedTop = top;
    }
    virtual IFrameLayerAndroid* updatePosition(SkRect viewPort,
                                               IFrameLayerAndroid* parentIframeLayer);

    // Measures the background image repetition
    void setRepeatX(bool repeat) { m_repeatX = repeat; }
    void setRepeatY(bool repeat) { m_repeatY = repeat; }
    bool repeatX() { return m_repeatX; }
    bool repeatY() { return m_repeatY; }
    int nbRepeatX() { return m_nbRepeatX; }
    int offsetX() { return m_offsetX; }
    int nbRepeatY() { return m_nbRepeatY; }
    int offsetY() { return m_offsetY; }

private:
    bool m_repeatX;
    bool m_repeatY;
    int  m_nbRepeatX;
    int  m_nbRepeatY;
    int  m_offsetX;
    int  m_offsetY;
};

}

#endif // USE(ACCELERATED_COMPOSITING)

#endif // FixedPositioning_h
