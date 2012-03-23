/*
 * Copyright (C) 2009 The Android Open Source Project
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

#ifndef LayerAndroid_h
#define LayerAndroid_h

#if USE(ACCELERATED_COMPOSITING)

#include "FloatPoint.h"
#include "FloatPoint3D.h"
#include "FloatRect.h"
#include "GraphicsLayerClient.h"
#include "ImageTexture.h"
#include "Layer.h"
#include "RefPtr.h"
#include "SkBitmap.h"
#include "SkColor.h"
#include "SkRegion.h"
#include "SkStream.h"
#include "TransformationMatrix.h"

#include <wtf/HashMap.h>

#ifndef BZERO_DEFINED
#define BZERO_DEFINED
// http://www.opengroup.org/onlinepubs/000095399/functions/bzero.html
// For maximum portability, it is recommended to replace the function call to bzero() as follows:
#define bzero(b, len) (memset((b), '\0', (len)), (void) 0)
#endif

class SkBitmapRef;
class SkCanvas;
class SkMatrix;
class SkPicture;

namespace WebCore {
class LayerAndroid;
class LayerContent;
class LayerGroup;
class ImageTexture;
}

namespace android {
class DrawExtra;
void serializeLayer(WebCore::LayerAndroid* layer, SkWStream* stream);
WebCore::LayerAndroid* deserializeLayer(int version, SkStream* stream);
void cleanupImageRefs(WebCore::LayerAndroid* layer);
}

using namespace android;

namespace WebCore {

class AndroidAnimation;
class BaseTileTexture;
class FixedPositioning;
class GLWebViewState;
class IFrameLayerAndroid;
class LayerMergeState;
class RenderLayer;
class PaintedSurface;

class TexturesResult {
public:
    TexturesResult()
        : fixed(0)
        , scrollable(0)
        , clipped(0)
        , full(0)
    {}

    int fixed;
    int scrollable;
    int clipped;
    int full;
};

class TEST_EXPORT LayerAndroid : public Layer {
public:
    typedef enum { UndefinedLayer, WebCoreLayer, UILayer } LayerType;
    typedef enum { StandardLayer, ScrollableLayer,
                   IFrameLayer, IFrameContentLayer,
                   CanvasLayer, BaseLayer } SubclassType;
    typedef enum { InvalidateNone = 0, InvalidateLayers } InvalidateFlags;

    String subclassName()
    {
        switch (subclassType()) {
            case LayerAndroid::StandardLayer:
                return "StandardLayer";
            case LayerAndroid::ScrollableLayer:
                return "ScrollableLayer";
            case LayerAndroid::IFrameLayer:
                return "IFrameLayer";
            case LayerAndroid::IFrameContentLayer:
                return "IFrameContentLayer";
            case LayerAndroid::CanvasLayer:
                return "CanvasLayer";
            case LayerAndroid::BaseLayer:
                return "BaseLayer";
        }
        return "Undefined";
    }

    LayerAndroid(RenderLayer* owner);
    LayerAndroid(const LayerAndroid& layer);
    virtual ~LayerAndroid();

    void setBackfaceVisibility(bool value) { m_backfaceVisibility = value; }
    void setTransform(const TransformationMatrix& matrix) { m_transform = matrix; }
    FloatPoint translation() const;
    // Returns a rect describing the bounds of the layer with the local
    // transformation applied, expressed relative to the parent layer.
    // FIXME: Currently we use only the translation component of the local
    // transformation.
    SkRect bounds() const;
    IntRect clippedRect() const;
    bool outsideViewport();

    IntRect unclippedArea();
    IntRect visibleArea();

    virtual bool needsTexture();

    // Debug helper methods
    int nbLayers();
    int nbTexturedLayers();
    void showLayer(int indent = 0);

    float getScale() { return m_scale; }

    virtual bool drawGL(bool layerTilesDisabled);
    virtual bool drawCanvas(SkCanvas* canvas, bool drawChildren, PaintStyle style);
    bool drawChildrenCanvas(SkCanvas* canvas, PaintStyle style);

    void updateGLPositionsAndScale(const TransformationMatrix& parentMatrix,
                                   const FloatRect& clip, float opacity, float scale);
    void setDrawOpacity(float opacity) { m_drawOpacity = opacity; }
    float drawOpacity() { return m_drawOpacity; }
    bool visible();
    void setVisible(bool value) { m_visible = value; }

    bool preserves3D() { return m_preserves3D; }
    void setPreserves3D(bool value) { m_preserves3D = value; }
    void setAnchorPointZ(float z) { m_anchorPointZ = z; }
    float anchorPointZ() { return m_anchorPointZ; }
    void setDrawTransform(const TransformationMatrix& transform) { m_drawTransform = transform; }
    virtual const TransformationMatrix* drawTransform() const { return &m_drawTransform; }
    void setChildrenTransform(const TransformationMatrix& t) { m_childrenTransform = t; }
    void setDrawClip(const FloatRect& rect) { m_clippingRect = rect; }
    const FloatRect& drawClip() { return m_clippingRect; }

    const IntPoint& scrollOffset() const { return m_offset; }
    void setScrollOffset(IntPoint offset) { m_offset = offset; }
    void setBackgroundColor(SkColor color);
    void setMaskLayer(LayerAndroid*);
    void setMasksToBounds(bool masksToBounds)
    {
        m_haveClip = masksToBounds;
    }
    bool masksToBounds() const { return m_haveClip; }

    LayerContent* content() { return m_content; }
    void setContent(LayerContent* content);

    void addAnimation(PassRefPtr<AndroidAnimation> anim);
    void removeAnimationsForProperty(AnimatedPropertyID property);
    void removeAnimationsForKeyframes(const String& name);
    bool evaluateAnimations();
    bool evaluateAnimations(double time);
    void initAnimations();
    bool hasAnimations() const;
    void addDirtyArea();

    virtual void dumpLayer(FILE*, int indentLevel) const;
    void dumpLayers(FILE*, int indentLevel) const;
    void dumpToLog() const;

    /** Call this with the current viewport (scrolling, zoom) to update
        the position of the fixed layers.

        This call is recursive, so it should be called on the root of the
        hierarchy.
    */
    void updateLayerPositions(SkRect viewPort, IFrameLayerAndroid* parentIframeLayer = 0);
    virtual IFrameLayerAndroid* updatePosition(SkRect viewport,
                                               IFrameLayerAndroid* parentIframeLayer);

    /** Call this to update the position attribute, so that later calls
        like bounds() will report the corrected position.

        This call is recursive, so it should be called on the root of the
        hierarchy.
     */
    void updatePositions();

    void clipArea(SkTDArray<SkRect>* region) const;
    const LayerAndroid* find(int* xPtr, int* yPtr, SkPicture* root) const;
    const LayerAndroid* findById(int uniqueID) const
    {
        return const_cast<LayerAndroid*>(this)->findById(uniqueID);
    }
    LayerAndroid* findById(int uniqueID);
    LayerAndroid* getChild(int index) const
    {
        return static_cast<LayerAndroid*>(this->INHERITED::getChild(index));
    }
    int uniqueId() const { return m_uniqueId; }

    /** This sets a content image -- calling it means we will use
        the image directly when drawing the layer instead of using
        the content painted by WebKit.
        Images are handled in ImagesManager, as they can be shared
        between layers.
    */
    void setContentsImage(SkBitmapRef* img);

    void bounds(SkRect*) const;

    virtual LayerAndroid* copy() const { return new LayerAndroid(*this); }

    virtual void clearDirtyRegion();

    virtual void contentDraw(SkCanvas* canvas, PaintStyle style);

    virtual bool isMedia() const { return false; }
    virtual bool isVideo() const { return false; }
    virtual bool isIFrame() const { return false; }
    virtual bool isIFrameContent() const { return false; }

    bool isPositionFixed() const { return m_fixedPosition; }
    void setAbsolutePosition(bool isAbsolute) { m_isPositionAbsolute = isAbsolute; }
    bool isPositionAbsolute() { return m_isPositionAbsolute; }
    void setFixedPosition(FixedPositioning* position);
    FixedPositioning* fixedPosition() { return m_fixedPosition; }

    RenderLayer* owningLayer() const { return m_owningLayer; }

    float zValue() const { return m_zValue; }

    // ViewStateSerializer friends
    friend void android::serializeLayer(LayerAndroid* layer, SkWStream* stream);
    friend LayerAndroid* android::deserializeLayer(int version, SkStream* stream);
    friend void android::cleanupImageRefs(LayerAndroid* layer);

    // Update layers using another tree. Only works for basic properties
    // such as the position, the transform. Return true if anything more
    // complex is needed.
    bool updateWithTree(LayerAndroid*);
    virtual bool updateWithLayer(LayerAndroid*);

    LayerType type() { return m_type; }
    virtual SubclassType subclassType() { return LayerAndroid::StandardLayer; }

    bool hasText();

    void copyAnimationStartTimesRecursive(LayerAndroid* oldTree);

// rendering asset management
    SkRegion* getInvalRegion() { return &m_dirtyRegion; }
    void mergeInvalsInto(LayerAndroid* replacementTree);

    bool canJoinGroup(LayerGroup* group);
    void assignGroups(LayerMergeState* mergeState);
    LayerGroup* group() { return m_layerGroup; }

    void setIntrinsicallyComposited(bool intCom) { m_intrinsicallyComposited = intCom; }

    int setHwAccelerated(bool hwAccelerated);

protected:
    virtual void onDraw(SkCanvas*, SkScalar opacity, android::DrawExtra* extra, PaintStyle style);
    virtual InvalidateFlags onSetHwAccelerated(bool hwAccelerated) { return InvalidateNone; }
    IntPoint m_offset;
    TransformationMatrix m_drawTransform;
    int m_uniqueId;

private:
#if DUMP_NAV_CACHE
    friend class CachedLayer::Debug; // debugging access only
#endif

    void copyAnimationStartTimes(LayerAndroid* oldLayer);
    bool prepareContext(bool force = false);
    void clipInner(SkTDArray<SkRect>* region, const SkRect& local) const;

    // -------------------------------------------------------------------
    // Fields to be serialized
    // -------------------------------------------------------------------

    bool m_haveClip;
    bool m_backgroundColorSet;

    bool m_backfaceVisibility;
    bool m_visible;

    SkColor m_backgroundColor;

    bool m_preserves3D;
    float m_anchorPointZ;
    float m_drawOpacity;

    bool m_isPositionAbsolute;
    FixedPositioning* m_fixedPosition;

    typedef HashMap<pair<String, int>, RefPtr<AndroidAnimation> > KeyframesMap;
    KeyframesMap m_animations;

    TransformationMatrix m_transform;
    TransformationMatrix m_childrenTransform;

    // -------------------------------------------------------------------
    // Fields that are not serialized (generated, cached, or non-serializable)
    // -------------------------------------------------------------------

    float m_zValue;

    FloatRect m_clippingRect;

    // Note that m_content and m_imageCRC are mutually exclusive;
    // m_content is used when WebKit is asked to paint the layer's
    // content, while m_imageCRC references an image that we directly
    // composite, using the layer's dimensions as a destination rect.
    // We do this as if the layer only contains an image, directly compositing
    // it is a much faster method than using m_content.
    LayerContent* m_content;
    unsigned m_imageCRC;

    // used to signal the framework we need a repaint
    bool m_hasRunningAnimations;

    float m_scale;

    // We try to not always compute the texture size, as this is quite heavy
    static const double s_computeTextureDelay = 0.2; // 200 ms
    double m_lastComputeTextureSize;

    // This mutex serves two purposes. (1) It ensures that certain operations
    // happen atomically and (2) it makes sure those operations are synchronized
    // across all threads and cores.
    android::Mutex m_atomicSync;

    RenderLayer* m_owningLayer;

    LayerType m_type;
    SubclassType m_subclassType;

    bool m_intrinsicallyComposited;

    LayerGroup* m_layerGroup;

    typedef Layer INHERITED;
};

}

#else

class SkPicture;

namespace WebCore {

class LayerAndroid {
public:
    LayerAndroid(SkPicture* picture) :
        m_recordingPicture(picture), // does not assign ownership
        m_uniqueId(-1)
    {}
    SkPicture* picture() const { return m_recordingPicture; }
    int uniqueId() const { return m_uniqueId; }
private:
    SkPicture* m_recordingPicture;
    int m_uniqueId;
};

}

#endif // USE(ACCELERATED_COMPOSITING)

#endif // LayerAndroid_h
