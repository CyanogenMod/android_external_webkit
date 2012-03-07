/*
 * Copyright (C) 2012 The Android Open Source Project
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

#ifndef FixedLayerAndroid_h
#define FixedLayerAndroid_h

#if USE(ACCELERATED_COMPOSITING)

#include "LayerAndroid.h"
#include "IFrameLayerAndroid.h"

namespace WebCore {

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
};

class FixedLayerAndroid : public LayerAndroid {

public:
    FixedLayerAndroid(RenderLayer* owner)
        : LayerAndroid(owner) {}
    FixedLayerAndroid(const LayerAndroid& layer)
        : LayerAndroid(layer) {}
    FixedLayerAndroid(const FixedLayerAndroid& layer);
    virtual ~FixedLayerAndroid() {};

    virtual LayerAndroid* copy() const { return new FixedLayerAndroid(*this); }
    virtual SubclassType subclassType() { return LayerAndroid::FixedLayer; }

    friend void android::serializeLayer(LayerAndroid* layer, SkWStream* stream);
    friend LayerAndroid* android::deserializeLayer(int version, SkStream* stream);

    virtual bool isFixed() const { return true; }

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
        setShouldInheritFromRootTransform(true);
    }

    virtual IFrameLayerAndroid* updatePosition(SkRect viewPort,
                                               IFrameLayerAndroid* parentIframeLayer);

    virtual void contentDraw(SkCanvas* canvas, PaintStyle style);

    virtual void dumpLayer(FILE*, int indentLevel) const;

private:
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

}

#endif // USE(ACCELERATED_COMPOSITING)

#endif // FixedLayerAndroid_h
