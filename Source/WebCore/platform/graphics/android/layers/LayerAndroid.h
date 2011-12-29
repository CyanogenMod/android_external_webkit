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
#include "PlatformString.h"
#include "RefPtr.h"
#include "SkBitmap.h"
#include "SkColor.h"
#include "SkRegion.h"
#include "SkStream.h"
#include "TransformationMatrix.h"

#include <utils/threads.h>
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
class ImageTexture;
class Surface;
}

namespace android {
class DrawExtra;
void serializeLayer(WebCore::LayerAndroid* layer, SkWStream* stream);
WebCore::LayerAndroid* deserializeLayer(int version, SkMemoryStream* stream);
void cleanupImageRefs(WebCore::LayerAndroid* layer);
}

using namespace android;

namespace WebCore {

class AndroidAnimation;
class FixedPositioning;
class GLWebViewState;
class IFrameLayerAndroid;
class LayerMergeState;
class RenderLayer;
class PaintedSurface;
class LayerDumper;

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
                   FixedBackgroundLayer,
                   FixedBackgroundImageLayer,
                   ForegroundBaseLayer,
                   CanvasLayer, BaseLayer } SubclassType;
    typedef enum { InvalidateNone = 0, InvalidateLayers } InvalidateFlags;

    const char* subclassName() const
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
            case LayerAndroid::FixedBackgroundLayer:
                return "FixedBackgroundLayer";
            case LayerAndroid::FixedBackgroundImageLayer:
                return "FixedBackgroundImageLayer";
            case LayerAndroid::ForegroundBaseLayer:
                return "ForegroundBaseLayer";
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
    IntRect clippedRect() const;
    bool outsideViewport();

    // Returns the full area of the layer mapped into global content coordinates
    FloatRect fullContentAreaMapped() const;

    IntRect fullContentArea() const;
    IntRect visibleContentArea(bool force3dContentVisible = false) const;

    virtual bool needsTexture();

    // Debug helper methods
    int nbLayers();
    int nbTexturedLayers();
    void showLayer(int indent = 0);

    float getScale() { return m_scale; }

    // draw the layer tree recursively in draw order, grouping and sorting 3d rendering contexts
    bool drawTreeSurfacesGL();

    virtual bool drawGL(bool layerTilesDisabled);
    virtual bool drawCanvas(SkCanvas* canvas, bool drawChildren, PaintStyle style);
    bool drawChildrenCanvas(SkCanvas* canvas, PaintStyle style);

    void updateGLPositionsAndScale(const TransformationMatrix& parentMatrix,
                                   const FloatRect& clip, float opacity, float scale,
                                   bool forceCalculations, bool disableFixedElemUpdate);
    void setDrawOpacity(float opacity) { m_drawOpacity = opacity; }
    float drawOpacity() { return m_drawOpacity; }
    bool visible();
    void setVisible(bool value) { m_visible = value; }

    bool preserves3D() { return m_preserves3D; }
    void setPreserves3D(bool value) { m_preserves3D = value; }
    void setAnchorPointZ(float z) { m_anchorPointZ = z; }
    float anchorPointZ() { return m_anchorPointZ; }
    void setDrawTransform(const TransformationMatrix& transform) { m_drawTransform = m_drawTransformUnfudged = transform; }
    virtual const TransformationMatrix* drawTransform() const { return &m_drawTransform; }
    void setChildrenTransform(const TransformationMatrix& t) { m_childrenTransform = t; }
    void setDrawClip(const FloatRect& rect) { m_clippingRect = rect; }
    const FloatRect& drawClip() { return m_clippingRect; }

    void setBackgroundColor(SkColor color);
    void setMaskLayer(LayerAndroid*);
    void setMasksToBounds(bool masksToBounds)
    {
        m_haveClip = masksToBounds;
    }
    bool masksToBounds() const { return m_haveClip; }

    LayerContent* content() { return m_content; }
    void setContent(LayerContent* content);
    // Check to see if the dirty area of this layer can be updated with a blit
    // from the prerender instead of needing to generate tiles from the LayerContent
    bool canUpdateWithBlit();

    void addAnimation(PassRefPtr<AndroidAnimation> anim);
    void removeAnimationsForProperty(AnimatedPropertyID property);
    void removeAnimationsForKeyframes(const String& name);
    bool evaluateAnimations();
    bool evaluateAnimations(double time);
    void initAnimations();
    bool hasAnimations() const;
    void addDirtyArea();

    void dumpLayers(LayerDumper*) const;

    virtual IFrameLayerAndroid* updatePosition(SkRect viewport,
                                               IFrameLayerAndroid* parentIframeLayer);

    /** Call this to update the position attribute, so that later calls
        like bounds() will report the corrected position.

        This call is recursive, so it should be called on the root of the
        hierarchy.
     */
    void updatePositions();

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

    virtual LayerAndroid* copy() const { return new LayerAndroid(*this); }

    virtual void clearDirtyRegion();

    virtual void contentDraw(SkCanvas* canvas, PaintStyle style);

    virtual bool isMedia() const { return false; }
    virtual bool isVideo() const { return false; }
    virtual bool isIFrame() const { return false; }
    virtual bool isIFrameContent() const { return false; }
    virtual bool isFixedBackground() const { return false; }
    virtual bool isWebGL() const { return false; }

    bool isPositionFixed() const { return m_fixedPosition; }
    void setAbsolutePosition(bool isAbsolute) { m_isPositionAbsolute = isAbsolute; }
    bool isPositionAbsolute() { return m_isPositionAbsolute; }
    void setFixedPosition(FixedPositioning* position);
    FixedPositioning* fixedPosition() { return m_fixedPosition; }
    virtual bool isCanvas() const { return false; }

    RenderLayer* owningLayer() const { return m_owningLayer; }

    float zValue() const { return m_zValue; }

    // ViewStateSerializer friends
    friend void android::serializeLayer(LayerAndroid* layer, SkWStream* stream);
    friend LayerAndroid* android::deserializeLayer(int version, SkMemoryStream* stream);
    friend void android::cleanupImageRefs(LayerAndroid* layer);

    LayerType type() { return m_type; }
    virtual SubclassType subclassType() const { return LayerAndroid::StandardLayer; }

    float maxZoomScale() const;

    void copyAnimationStartTimesRecursive(LayerAndroid* oldTree);

// rendering asset management
    SkRegion* getInvalRegion() { return &m_dirtyRegion; }
    void mergeInvalsInto(LayerAndroid* replacementTree);

    bool canJoinSurface(Surface* surface);
    void assignSurfaces(LayerMergeState* mergeState);
    Surface* surface() { return m_surface; }

    void setIntrinsicallyComposited(bool intCom) { m_intrinsicallyComposited = intCom; }
    virtual bool needsIsolatedSurface() {
        return (needsTexture() && m_intrinsicallyComposited)
            || m_animations.size()
            || m_imageCRC;
    }

    int setHwAccelerated(bool hwAccelerated);

    void setReplicatedLayer(LayerAndroid* layer) { m_replicatedLayer = layer; }
    void setReplicatedLayerPosition(const FloatPoint& p) { m_replicatedLayerPosition = p; }
    void setOriginalLayer(LayerAndroid* layer) { m_originalLayer = layer; }
    bool hasReplicatedLayer() { return m_replicatedLayer; }
    const TransformationMatrix* replicatedLayerDrawTransform() {
        if (m_replicatedLayer)
            return m_replicatedLayer->drawTransform();
        return 0;
    }

protected:
    virtual void dumpLayer(LayerDumper*) const;
    /** Call this with the current viewport (scrolling, zoom) to update
        the position of the fixed layers.

        This call is recursive, so it should be called on the root of the
        hierarchy.
    */
    void updateLayerPositions(SkRect viewPort, IFrameLayerAndroid* parentIframeLayer = 0);
    virtual void onDraw(SkCanvas*, SkScalar opacity, android::DrawExtra* extra, PaintStyle style);
    virtual InvalidateFlags onSetHwAccelerated(bool hwAccelerated) { return InvalidateNone; }
    TransformationMatrix m_drawTransform;
    TransformationMatrix m_drawTransformUnfudged;
    int m_uniqueId;

private:
    void updateLocalTransformAndClip(const TransformationMatrix& parentMatrix,
                                     const FloatRect& clip);
    bool hasDynamicTransform() {
        return contentIsScrollable() || isPositionFixed() || (m_animations.size() != 0);
    }

    // recurse through the current 3d rendering context, adding layers in the context to the vector
    void collect3dRenderingContext(Vector<LayerAndroid*>& layersInContext);
    bool drawSurfaceAndChildrenGL();

#if DUMP_NAV_CACHE
    friend class CachedLayer::Debug; // debugging access only
#endif

    // -------------------------------------------------------------------
    // Fields to be serialized
    // -------------------------------------------------------------------

    bool m_haveClip;
    bool m_backgroundColorSet;

    bool m_backfaceVisibility;
    bool m_visible;

protected:
    SkColor m_backgroundColor;

private:

    bool m_preserves3D;
    float m_anchorPointZ;
    float m_drawOpacity;

    bool m_isPositionAbsolute;

protected:
    FixedPositioning* m_fixedPosition;

private:

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

protected:
    unsigned m_imageCRC;

private:

    // used to signal the framework we need a repaint
    bool m_hasRunningAnimations;

    float m_scale;

    // We try to not always compute the texture size, as this is quite heavy
    static const double s_computeTextureDelay = 0.2; // 200 ms
    double m_lastComputeTextureSize;

    RenderLayer* m_owningLayer;

    LayerType m_type;
    SubclassType m_subclassType;

    bool m_intrinsicallyComposited;

    Surface* m_surface;

    // link to a replicated layer (used e.g. for reflections)
    LayerAndroid* m_replicatedLayer;
    FloatPoint    m_replicatedLayerPosition;
    LayerAndroid* m_originalLayer;
    // link to a mask layer
    LayerAndroid* m_maskLayer;

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
