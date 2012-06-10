/*
 * Copyright (C) 2009 The Android Open Source Project
 * Copyright (C) 2011, Sony Ericsson Mobile Communications AB
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

#include "config.h"
#include "GraphicsLayerAndroid.h"

#if USE(ACCELERATED_COMPOSITING)

#include "AndroidAnimation.h"
#include "Animation.h"
#include "FloatRect.h"
#include "GraphicsContext.h"
#include "Image.h"
#include "ImagesManager.h"
#include "Layer.h"
#include "Length.h"
#include "MediaLayer.h"
#include "PlatformBridge.h"
#include "PlatformGraphicsContext.h"
#include "RenderLayerBacking.h"
#include "RenderView.h"
#include "RotateTransformOperation.h"
#include "ScaleTransformOperation.h"
#include "ScrollableLayerAndroid.h"
#include "SkCanvas.h"
#include "SkRegion.h"
#include "TransformationMatrix.h"
#include "TranslateTransformOperation.h"

#include <cutils/log.h>
#include <wtf/CurrentTime.h>
#include <wtf/text/CString.h>

#undef LOG
#define LOG(...) android_printLog(ANDROID_LOG_DEBUG, "GraphicsLayer", __VA_ARGS__)
#define MLOG(...) android_printLog(ANDROID_LOG_DEBUG, "GraphicsLayer", __VA_ARGS__)
#define TLOG(...) android_printLog(ANDROID_LOG_DEBUG, "GraphicsLayer", __VA_ARGS__)

#undef LOG
#define LOG(...)
#undef MLOG
#define MLOG(...)
#undef TLOG
#define TLOG(...)
#undef LAYER_DEBUG

using namespace std;

static bool gPaused;
static double gPausedDelay;

namespace WebCore {

static int gDebugGraphicsLayerAndroidInstances = 0;
inline int GraphicsLayerAndroid::instancesCount()
{
    return gDebugGraphicsLayerAndroidInstances;
}

static String propertyIdToString(AnimatedPropertyID property)
{
    switch (property) {
    case AnimatedPropertyWebkitTransform:
        return "transform";
    case AnimatedPropertyOpacity:
        return "opacity";
    case AnimatedPropertyBackgroundColor:
        return "backgroundColor";
    case AnimatedPropertyInvalid:
        ASSERT_NOT_REACHED();
    }
    ASSERT_NOT_REACHED();
    return "";
}

PassOwnPtr<GraphicsLayer> GraphicsLayer::create(GraphicsLayerClient* client)
{
    return new GraphicsLayerAndroid(client);
}

SkLength convertLength(Length len)
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

static RenderLayer* renderLayerFromClient(GraphicsLayerClient* client)
{
    return client ? client->owningLayer() : 0;
}

GraphicsLayerAndroid::GraphicsLayerAndroid(GraphicsLayerClient* client) :
    GraphicsLayer(client),
    m_needsSyncChildren(false),
    m_needsSyncMask(false),
    m_needsRepaint(false),
    m_needsNotifyClient(false),
    m_haveContents(false),
    m_newImage(false),
    m_image(0),
#if ENABLE(WEBGL)
    m_is3DCanvas(false),
#endif
    m_foregroundLayer(0),
    m_foregroundClipLayer(0)
{
    RenderLayer* renderLayer = renderLayerFromClient(m_client);
    m_contentLayer = new LayerAndroid(renderLayer);
    m_dirtyRegion.setEmpty();
    gDebugGraphicsLayerAndroidInstances++;
}

GraphicsLayerAndroid::~GraphicsLayerAndroid()
{
    if (m_image)
        m_image->deref();

    m_contentLayer->unref();
    SkSafeUnref(m_foregroundLayer);
    SkSafeUnref(m_foregroundClipLayer);
    gDebugGraphicsLayerAndroidInstances--;
}

void GraphicsLayerAndroid::setName(const String& name)
{
    GraphicsLayer::setName(name);
}

NativeLayer GraphicsLayerAndroid::nativeLayer() const
{
    LOG("(%x) nativeLayer", this);
    return 0;
}

bool GraphicsLayerAndroid::setChildren(const Vector<GraphicsLayer*>& children)
{
    bool childrenChanged = GraphicsLayer::setChildren(children);
    if (childrenChanged) {
        m_needsSyncChildren = true;
        askForSync();
    }

    return childrenChanged;
}

void GraphicsLayerAndroid::addChild(GraphicsLayer* childLayer)
{
#ifndef NDEBUG
    const String& name = childLayer->name();
    LOG("(%x) addChild: %x (%s)", this, childLayer, name.latin1().data());
#endif
    GraphicsLayer::addChild(childLayer);
    m_needsSyncChildren = true;
    askForSync();
}

void GraphicsLayerAndroid::addChildAtIndex(GraphicsLayer* childLayer, int index)
{
    LOG("(%x) addChild %x AtIndex %d", this, childLayer, index);
    GraphicsLayer::addChildAtIndex(childLayer, index);
    m_needsSyncChildren = true;
    askForSync();
}

void GraphicsLayerAndroid::addChildBelow(GraphicsLayer* childLayer, GraphicsLayer* sibling)
{
    LOG("(%x) addChild %x Below %x", this, childLayer, sibling);
    GraphicsLayer::addChildBelow(childLayer, sibling);
    m_needsSyncChildren = true;
    askForSync();
}

void GraphicsLayerAndroid::addChildAbove(GraphicsLayer* childLayer, GraphicsLayer* sibling)
{
    LOG("(%x) addChild %x Above %x", this, childLayer, sibling);
    GraphicsLayer::addChildAbove(childLayer, sibling);
    m_needsSyncChildren = true;
    askForSync();
}

bool GraphicsLayerAndroid::replaceChild(GraphicsLayer* oldChild, GraphicsLayer* newChild)
{
    LOG("(%x) replaceChild %x by %x", this, oldChild, newChild);
    bool ret = GraphicsLayer::replaceChild(oldChild, newChild);
    if (ret) {
        m_needsSyncChildren = true;
        askForSync();
    }
    return ret;
}

void GraphicsLayerAndroid::removeFromParent()
{
    LOG("(%x) removeFromParent()", this);
    GraphicsLayerAndroid* parent = static_cast<GraphicsLayerAndroid*>(m_parent);
    GraphicsLayer::removeFromParent();
    // Update the parent's children.
    if (parent) {
        parent->m_needsSyncChildren = true;
        askForSync();
    }
}

void GraphicsLayerAndroid::updateFixedPosition()
{
    RenderLayer* renderLayer = renderLayerFromClient(m_client);
    if (!renderLayer)
        return;
    RenderView* view = static_cast<RenderView*>(renderLayer->renderer());

    if (!view)
        return;

    // We will need the Iframe flag in the LayerAndroid tree for fixed position
    if (view->isRenderIFrame())
        m_contentLayer->setIsIframe(true);
    // If we are a fixed position layer, just set it
    if (view->isPositioned() && view->style()->position() == FixedPosition) {
        // We need to get the passed CSS properties for the element
        SkLength left, top, right, bottom;
        left = convertLength(view->style()->left());
        top = convertLength(view->style()->top());
        right = convertLength(view->style()->right());
        bottom = convertLength(view->style()->bottom());

        // We also need to get the margin...
        SkLength marginLeft, marginTop, marginRight, marginBottom;
        marginLeft = convertLength(view->style()->marginLeft());
        marginTop = convertLength(view->style()->marginTop());
        marginRight = convertLength(view->style()->marginRight());
        marginBottom = convertLength(view->style()->marginBottom());

        // In order to compute the fixed element's position, we need the width
        // and height of the element when bottom or right is defined.
        // And here we should use the non-overflowed value, that means, the
        // overflowed content (e.g. outset shadow) will not be counted into the
        // width and height.
        int w = view->width();
        int h = view->height();

        int paintingOffsetX = - offsetFromRenderer().width();
        int paintingOffsetY = - offsetFromRenderer().height();

        SkRect viewRect;
        viewRect.set(paintingOffsetX, paintingOffsetY, paintingOffsetX + w, paintingOffsetY + h);
        IntPoint renderLayerPos(renderLayer->x(), renderLayer->y());
        m_contentLayer->setFixedPosition(left, top, right, bottom,
                                         marginLeft, marginTop,
                                         marginRight, marginBottom,
                                         renderLayerPos,
                                         viewRect);
    }
}

void GraphicsLayerAndroid::setPosition(const FloatPoint& point)
{
    if (point == m_position)
        return;

    GraphicsLayer::setPosition(point);

#ifdef LAYER_DEBUG_2
    LOG("(%x) setPosition(%.2f,%.2f) pos(%.2f, %.2f) anchor(%.2f,%.2f) size(%.2f, %.2f)",
        this, point.x(), point.y(), m_position.x(), m_position.y(),
        m_anchorPoint.x(), m_anchorPoint.y(), m_size.width(), m_size.height());
#endif
    m_contentLayer->setPosition(point.x(), point.y());
    askForSync();
}

void GraphicsLayerAndroid::setPreserves3D(bool preserves3D)
{
    if (preserves3D == m_preserves3D)
        return;

    GraphicsLayer::setPreserves3D(preserves3D);
    m_contentLayer->setPreserves3D(preserves3D);
    askForSync();
}

void GraphicsLayerAndroid::setAnchorPoint(const FloatPoint3D& point)
{
    if (point == m_anchorPoint)
        return;
    GraphicsLayer::setAnchorPoint(point);
    m_contentLayer->setAnchorPoint(point.x(), point.y());
    m_contentLayer->setAnchorPointZ(point.z());
    askForSync();
}

void GraphicsLayerAndroid::setSize(const FloatSize& size)
{
    if (size == m_size)
        return;
    MLOG("(%x) setSize (%.2f,%.2f)", this, size.width(), size.height());
    GraphicsLayer::setSize(size);

    // If it is a media layer the size may have changed as a result of the media
    // element (e.g. plugin) gaining focus. Therefore, we must sync the size of
    // the focus' outline so that our UI thread can draw accordingly.
    RenderLayer* layer = renderLayerFromClient(m_client);
    if (layer && m_contentLayer->isMedia()) {
        RenderBox* box = layer->renderBox();
        int outline = box->view()->maximalOutlineSize();
        static_cast<MediaLayer*>(m_contentLayer)->setOutlineSize(outline);
        LOG("Media Outline: %d %p %p %p", outline, m_client, layer, box);
        LOG("Media Size: %g,%g", size.width(), size.height());
    }

    m_contentLayer->setSize(size.width(), size.height());
    setNeedsDisplay();
    askForSync();
}

void GraphicsLayerAndroid::setBackfaceVisibility(bool b)
{
    if (b == m_backfaceVisibility)
        return;

    GraphicsLayer::setBackfaceVisibility(b);
    m_contentLayer->setBackfaceVisibility(b);
    askForSync();
}

void GraphicsLayerAndroid::setTransform(const TransformationMatrix& t)
{
    if (t == m_transform)
        return;

    GraphicsLayer::setTransform(t);
    m_contentLayer->setTransform(t);
    askForSync();
}

void GraphicsLayerAndroid::setChildrenTransform(const TransformationMatrix& t)
{
    if (t == m_childrenTransform)
       return;
    LOG("(%x) setChildrenTransform", this);

    GraphicsLayer::setChildrenTransform(t);
    m_contentLayer->setChildrenTransform(t);
    for (unsigned int i = 0; i < m_children.size(); i++) {
        GraphicsLayer* layer = m_children[i];
        layer->setTransform(t);
        if (layer->children().size())
            layer->setChildrenTransform(t);
    }
    askForSync();
}

void GraphicsLayerAndroid::setMaskLayer(GraphicsLayer* layer)
{
    if (layer == m_maskLayer)
        return;

    GraphicsLayer::setMaskLayer(layer);
    m_needsSyncMask = true;
    askForSync();
}

void GraphicsLayerAndroid::setMasksToBounds(bool masksToBounds)
{
    if (masksToBounds == m_masksToBounds)
        return;
    GraphicsLayer::setMasksToBounds(masksToBounds);
    m_needsSyncMask = true;
    askForSync();
}

void GraphicsLayerAndroid::setDrawsContent(bool drawsContent)
{
    if (drawsContent == m_drawsContent)
        return;
    GraphicsLayer::setDrawsContent(drawsContent);
    m_contentLayer->setVisible(drawsContent);
    if (m_drawsContent) {
        m_haveContents = true;
        setNeedsDisplay();
    }
    askForSync();
}

void GraphicsLayerAndroid::setBackgroundColor(const Color& color)
{
    if (color == m_backgroundColor && m_backgroundColorSet)
        return;
    LOG("(%x) setBackgroundColor", this);
    GraphicsLayer::setBackgroundColor(color);
    SkColor c = SkColorSetARGB(color.alpha(), color.red(), color.green(), color.blue());
    m_contentLayer->setBackgroundColor(c);
    m_haveContents = true;
    askForSync();
}

void GraphicsLayerAndroid::clearBackgroundColor()
{
    if (!m_backgroundColorSet)
        return;

    LOG("(%x) clearBackgroundColor", this);
    GraphicsLayer::clearBackgroundColor();
    askForSync();
}

void GraphicsLayerAndroid::setContentsOpaque(bool opaque)
{
    if (opaque == m_contentsOpaque)
        return;
    LOG("(%x) setContentsOpaque (%d)", this, opaque);
    GraphicsLayer::setContentsOpaque(opaque);
    m_haveContents = true;
    askForSync();
}

void GraphicsLayerAndroid::setOpacity(float opacity)
{
    LOG("(%x) setOpacity: %.2f", this, opacity);
    float clampedOpacity = max(0.0f, min(opacity, 1.0f));

    if (clampedOpacity == m_opacity)
        return;

    MLOG("(%x) setFinalOpacity: %.2f=>%.2f (%.2f)", this,
        opacity, clampedOpacity, m_opacity);
    GraphicsLayer::setOpacity(clampedOpacity);
    m_contentLayer->setOpacity(clampedOpacity);
    askForSync();
}

void GraphicsLayerAndroid::setNeedsDisplay()
{
    LOG("(%x) setNeedsDisplay()", this);
    FloatRect rect(0, 0, m_size.width(), m_size.height());
    setNeedsDisplayInRect(rect);
}

#if ENABLE(WEBGL)
void GraphicsLayerAndroid::setContentsNeedsDisplay()
{
    if (m_is3DCanvas)
        setNeedsDisplay();
}
#endif

// Helper to set and clear the painting phase as well as auto restore the
// original phase.
class PaintingPhase {
public:
    PaintingPhase(GraphicsLayer* layer)
        : m_layer(layer)
        , m_originalPhase(layer->paintingPhase()) {}

    ~PaintingPhase()
    {
        m_layer->setPaintingPhase(m_originalPhase);
    }

    void set(GraphicsLayerPaintingPhase phase)
    {
        m_layer->setPaintingPhase(phase);
    }

    void clear(GraphicsLayerPaintingPhase phase)
    {
        m_layer->setPaintingPhase(
                (GraphicsLayerPaintingPhase) (m_originalPhase & ~phase));
    }
private:
    GraphicsLayer* m_layer;
    GraphicsLayerPaintingPhase m_originalPhase;
};

void GraphicsLayerAndroid::updateScrollingLayers()
{
#if ENABLE(ANDROID_OVERFLOW_SCROLL)
    RenderLayer* layer = renderLayerFromClient(m_client);
    if (!layer || !m_haveContents)
        return;
    bool hasOverflowScroll = m_foregroundLayer || m_contentLayer->contentIsScrollable();
    bool layerNeedsOverflow = layer->hasOverflowScroll();
    bool iframeNeedsOverflow = layer->isRootLayer() &&
        layer->renderer()->frame()->ownerRenderer() &&
        layer->renderer()->frame()->view()->hasOverflowScroll();

    if (hasOverflowScroll && (layerNeedsOverflow || iframeNeedsOverflow)) {
        // Already has overflow layers.
        return;
    }
    if (!hasOverflowScroll && !layerNeedsOverflow && !iframeNeedsOverflow) {
        // Does not need overflow layers.
        return;
    }
    if (layerNeedsOverflow || iframeNeedsOverflow) {
        ASSERT(!hasOverflowScroll);
        if (layerNeedsOverflow) {
            ASSERT(!m_foregroundLayer && !m_foregroundClipLayer);
            m_foregroundLayer = new ScrollableLayerAndroid(layer);
            m_foregroundClipLayer = new LayerAndroid(layer);
            m_foregroundClipLayer->setMasksToBounds(true);
            m_foregroundClipLayer->addChild(m_foregroundLayer);
            m_contentLayer->addChild(m_foregroundClipLayer);
            m_contentLayer->setHasOverflowChildren(true);
        } else {
            ASSERT(iframeNeedsOverflow && !m_contentLayer->contentIsScrollable());
            // No need to copy the children as they will be removed and synced.
            m_contentLayer->removeChildren();
            // Replace the content layer with a scrollable layer.
            LayerAndroid* layer = new ScrollableLayerAndroid(*m_contentLayer);
            m_contentLayer->unref();
            m_contentLayer = layer;
            if (m_parent) {
                // The content layer has changed so the parent needs to sync
                // children.
                static_cast<GraphicsLayerAndroid*>(m_parent)->m_needsSyncChildren = true;
            }
        }
        // Need to rebuild our children based on the new structure.
        m_needsSyncChildren = true;
    } else {
        ASSERT(hasOverflowScroll && !layerNeedsOverflow && !iframeNeedsOverflow);
        ASSERT(m_contentLayer);
        // Remove the foreground layers.
        if (m_foregroundLayer) {
            m_foregroundLayer->unref();
            m_foregroundLayer = 0;
            m_foregroundClipLayer->unref();
            m_foregroundClipLayer = 0;
        }
        // No need to copy over children.
        m_contentLayer->removeChildren();
        LayerAndroid* layer = new LayerAndroid(*m_contentLayer);
        m_contentLayer->unref();
        m_contentLayer = layer;
        if (m_parent) {
            // The content layer has changed so the parent needs to sync
            // children.
            static_cast<GraphicsLayerAndroid*>(m_parent)->m_needsSyncChildren = true;
        }
        // Children are all re-parented.
        m_needsSyncChildren = true;
    }
#endif
}

bool GraphicsLayerAndroid::repaint()
{
    LOG("(%x) repaint(), gPaused(%d) m_needsRepaint(%d) m_haveContents(%d) ",
        this, gPaused, m_needsRepaint, m_haveContents);

    if (!gPaused && m_haveContents && m_needsRepaint && !m_image) {
        // with SkPicture, we request the entire layer's content.
        IntRect layerBounds(0, 0, m_size.width(), m_size.height());

        RenderLayer* layer = renderLayerFromClient(m_client);
        if (!layer)
            return false;
        if (m_foregroundLayer) {
            PaintingPhase phase(this);
            // Paint the background into a separate context.
            phase.set(GraphicsLayerPaintBackground);
            if (!paintContext(m_contentLayer->recordContext(), layerBounds))
                return false;
            m_contentLayer->checkTextPresence();

            // Construct the foreground layer and draw.
            RenderBox* box = layer->renderBox();
            int outline = box->view()->maximalOutlineSize();
            IntRect contentsRect(0, 0,
                                 box->borderLeft() + box->borderRight() + layer->scrollWidth(),
                                 box->borderTop() + box->borderBottom() + layer->scrollHeight());
            contentsRect.inflate(outline);
            // Update the foreground layer size.
            m_foregroundLayer->setSize(contentsRect.width(), contentsRect.height());
            // Paint everything else into the main recording canvas.
            phase.clear(GraphicsLayerPaintBackground);

            // Paint at 0,0.
            IntSize scroll = layer->scrolledContentOffset();
            layer->scrollToOffset(0, 0);
            // At this point, it doesn't matter if painting failed.
            (void) paintContext(m_foregroundLayer->recordContext(), contentsRect);
            m_foregroundLayer->checkTextPresence();
            layer->scrollToOffset(scroll.width(), scroll.height());

            // Construct the clip layer for masking the contents.
            IntRect clip = layer->renderer()->absoluteBoundingBoxRect();
            // absoluteBoundingBoxRect does not include the outline so we need
            // to offset the position.
            int x = box->borderLeft() + outline;
            int y = box->borderTop() + outline;
            int width = clip.width() - box->borderLeft() - box->borderRight();
            int height = clip.height() - box->borderTop() - box->borderBottom();
            m_foregroundClipLayer->setPosition(x, y);
            m_foregroundClipLayer->setSize(width, height);

            // Need to offset the foreground layer by the clip layer in order
            // for the contents to be in the correct position.
            m_foregroundLayer->setPosition(-x, -y);
            // Set the scrollable bounds of the layer.
            m_foregroundLayer->setScrollLimits(-x, -y, m_size.width(), m_size.height());

            // Invalidate the entire layer for now, as webkit will only send the
            // setNeedsDisplayInRect() for the visible (clipped) scrollable area,
            // offsetting the invals by the scroll position would not be enough.
            // TODO: have webkit send us invals even for non visible area
            SkRegion region;
            region.setRect(0, 0, contentsRect.width(), contentsRect.height());
            m_foregroundLayer->markAsDirty(region);
            m_foregroundLayer->needsRepaint();
        } else {
            // If there is no contents clip, we can draw everything into one
            // picture.
            if (!paintContext(m_contentLayer->recordContext(), layerBounds))
                return false;
            m_contentLayer->checkTextPresence();
            // Check for a scrollable iframe and report the scrolling
            // limits based on the view size.
            if (m_contentLayer->contentIsScrollable()) {
                FrameView* view = layer->renderer()->frame()->view();
                static_cast<ScrollableLayerAndroid*>(m_contentLayer)->setScrollLimits(
                    m_position.x(), m_position.y(), view->layoutWidth(), view->layoutHeight());
            }
        }

        LOG("(%x) repaint() on (%.2f,%.2f) contentlayer(%.2f,%.2f,%.2f,%.2f)paintGraphicsLayer called!",
            this, m_size.width(), m_size.height(),
            m_contentLayer->getPosition().fX,
            m_contentLayer->getPosition().fY,
            m_contentLayer->getSize().width(),
            m_contentLayer->getSize().height());

        m_contentLayer->markAsDirty(m_dirtyRegion);
        m_dirtyRegion.setEmpty();
        m_contentLayer->needsRepaint();
        m_needsRepaint = false;

        return true;
    }
    if (m_needsRepaint && m_image && m_newImage) {
        // We need to tell the GL thread that we will need to repaint the
        // texture. Only do so if we effectively have a new image!
        m_contentLayer->markAsDirty(m_dirtyRegion);
        m_dirtyRegion.setEmpty();
        m_contentLayer->needsRepaint();
        m_newImage = false;
        m_needsRepaint = false;
        return true;
    }
    return false;
}

bool GraphicsLayerAndroid::paintContext(SkPicture* context,
                                        const IntRect& rect)
{
    SkAutoPictureRecord arp(context, rect.width(), rect.height());
    SkCanvas* canvas = arp.getRecordingCanvas();

    if (!canvas)
        return false;

    PlatformGraphicsContext platformContext(canvas);
    GraphicsContext graphicsContext(&platformContext);

    paintGraphicsLayerContents(graphicsContext, rect);
    return true;
}

void GraphicsLayerAndroid::setNeedsDisplayInRect(const FloatRect& rect)
{
    // rect is in the render object coordinates

    if (!m_image && !drawsContent()) {
        LOG("(%x) setNeedsDisplay(%.2f,%.2f,%.2f,%.2f) doesn't have content, bypass...",
            this, rect.x(), rect.y(), rect.width(), rect.height());
        return;
    }

    SkRegion region;
    region.setRect(rect.x(), rect.y(),
                   rect.x() + rect.width(),
                   rect.y() + rect.height());
    m_dirtyRegion.op(region, SkRegion::kUnion_Op);

    m_needsRepaint = true;
    askForSync();
}

void GraphicsLayerAndroid::pauseDisplay(bool state)
{
    gPaused = state;
    if (gPaused)
        gPausedDelay = WTF::currentTime() + 1;
}

bool GraphicsLayerAndroid::addAnimation(const KeyframeValueList& valueList,
                                        const IntSize& boxSize,
                                        const Animation* anim,
                                        const String& keyframesName,
                                        double beginTime)
{
    if (!anim || anim->isEmptyOrZeroDuration() || valueList.size() < 2)
        return false;

    bool createdAnimations = false;
    if (valueList.property() == AnimatedPropertyWebkitTransform) {
        createdAnimations = createTransformAnimationsFromKeyframes(valueList,
                                                                   anim,
                                                                   keyframesName,
                                                                   beginTime,
                                                                   boxSize);
    } else {
        createdAnimations = createAnimationFromKeyframes(valueList,
                                                         anim,
                                                         keyframesName,
                                                         beginTime);
    }
    if (createdAnimations)
        askForSync();
    return createdAnimations;
}

bool GraphicsLayerAndroid::createAnimationFromKeyframes(const KeyframeValueList& valueList,
     const Animation* animation, const String& keyframesName, double beginTime)
{
    bool isKeyframe = valueList.size() > 2;
    TLOG("createAnimationFromKeyframes(%d), name(%s) beginTime(%.2f)",
        isKeyframe, keyframesName.latin1().data(), beginTime);

    switch (valueList.property()) {
    case AnimatedPropertyInvalid: break;
    case AnimatedPropertyWebkitTransform: break;
    case AnimatedPropertyBackgroundColor: break;
    case AnimatedPropertyOpacity: {
        MLOG("ANIMATEDPROPERTYOPACITY");

        KeyframeValueList* operationsList = new KeyframeValueList(AnimatedPropertyOpacity);
        for (unsigned int i = 0; i < valueList.size(); i++) {
            FloatAnimationValue* originalValue = (FloatAnimationValue*)valueList.at(i);
            PassRefPtr<TimingFunction> timingFunction(const_cast<TimingFunction*>(originalValue->timingFunction()));
            FloatAnimationValue* value = new FloatAnimationValue(originalValue->keyTime(),
                                                                 originalValue->value(),
                                                                 timingFunction);
            operationsList->insert(value);
        }

        RefPtr<AndroidOpacityAnimation> anim = AndroidOpacityAnimation::create(animation,
                                                                               operationsList,
                                                                               beginTime);
        if (keyframesName.isEmpty())
            anim->setName(propertyIdToString(valueList.property()));
        else
            anim->setName(keyframesName);

        m_contentLayer->addAnimation(anim.release());
        needsNotifyClient();
        return true;
    } break;
    }
    return false;
}

void GraphicsLayerAndroid::needsNotifyClient()
{
    m_needsNotifyClient = true;
    askForSync();
}

bool GraphicsLayerAndroid::createTransformAnimationsFromKeyframes(const KeyframeValueList& valueList,
                                                                  const Animation* animation,
                                                                  const String& keyframesName,
                                                                  double beginTime,
                                                                  const IntSize& boxSize)
{
    ASSERT(valueList.property() == AnimatedPropertyWebkitTransform);
    TLOG("createTransformAnimationFromKeyframes, name(%s) beginTime(%.2f)",
        keyframesName.latin1().data(), beginTime);

    KeyframeValueList* operationsList = new KeyframeValueList(AnimatedPropertyWebkitTransform);
    for (unsigned int i = 0; i < valueList.size(); i++) {
        TransformAnimationValue* originalValue = (TransformAnimationValue*)valueList.at(i);
        PassRefPtr<TimingFunction> timingFunction(const_cast<TimingFunction*>(originalValue->timingFunction()));
        TransformAnimationValue* value = new TransformAnimationValue(originalValue->keyTime(),
                                                                     originalValue->value(),
                                                                     timingFunction);
        operationsList->insert(value);
    }

    RefPtr<AndroidTransformAnimation> anim = AndroidTransformAnimation::create(animation,
                                                                               operationsList,
                                                                               beginTime);

    if (keyframesName.isEmpty())
        anim->setName(propertyIdToString(valueList.property()));
    else
        anim->setName(keyframesName);


    m_contentLayer->addAnimation(anim.release());

    needsNotifyClient();
    return true;
}

void GraphicsLayerAndroid::removeAnimationsForProperty(AnimatedPropertyID anID)
{
    TLOG("NRO removeAnimationsForProperty(%d)", anID);
    m_contentLayer->removeAnimationsForProperty(anID);
    askForSync();
}

void GraphicsLayerAndroid::removeAnimationsForKeyframes(const String& keyframesName)
{
    TLOG("NRO removeAnimationsForKeyframes(%s)", keyframesName.latin1().data());
    m_contentLayer->removeAnimationsForKeyframes(keyframesName);
    askForSync();
}

void GraphicsLayerAndroid::pauseAnimation(const String& keyframesName)
{
    TLOG("NRO pauseAnimation(%s)", keyframesName.latin1().data());
}

void GraphicsLayerAndroid::suspendAnimations(double time)
{
    TLOG("NRO suspendAnimations(%.2f)", time);
}

void GraphicsLayerAndroid::resumeAnimations()
{
    TLOG("NRO resumeAnimations()");
}

void GraphicsLayerAndroid::setContentsToImage(Image* image)
{
    TLOG("(%x) setContentsToImage", this, image);
    if (image && image != m_image) {
        image->ref();
        if (m_image)
            m_image->deref();
        m_image = image;

        SkBitmapRef* bitmap = image->nativeImageForCurrentFrame();
        m_contentLayer->setContentsImage(bitmap);

        m_haveContents = true;
        m_newImage = true;
    }
    if (!image && m_image) {
        m_contentLayer->setContentsImage(0);
        m_image->deref();
        m_image = 0;
    }

    setNeedsDisplay();
    askForSync();
}

void GraphicsLayerAndroid::setContentsToMedia(PlatformLayer* mediaLayer)
{
    // Only fullscreen video on Android, so media doesn't get it's own layer.
    // We might still have other layers though.
    if (m_contentLayer != mediaLayer && mediaLayer) {

        // TODO add a copy method to LayerAndroid to sync everything
        // copy data from the original content layer to the new one
        mediaLayer->setPosition(m_contentLayer->getPosition().fX,
                                m_contentLayer->getPosition().fY);
        mediaLayer->setSize(m_contentLayer->getWidth(), m_contentLayer->getHeight());
        mediaLayer->setDrawTransform(*m_contentLayer->drawTransform());

        mediaLayer->ref();
        m_contentLayer->unref();
        m_contentLayer = mediaLayer;

        // If the parent exists then notify it to re-sync it's children
        if (m_parent) {
            GraphicsLayerAndroid* parent = static_cast<GraphicsLayerAndroid*>(m_parent);
            parent->m_needsSyncChildren = true;
        }
        m_needsSyncChildren = true;

        setNeedsDisplay();
        askForSync();
    }
}

#if ENABLE(WEBGL)
void GraphicsLayerAndroid::setContentsToCanvas(PlatformLayer* canvasLayer)
{
    if (m_contentLayer != canvasLayer && canvasLayer) {
        // Copy data from the original content layer to the new one
        canvasLayer->setPosition(m_contentLayer->getPosition().fX,
                                 m_contentLayer->getPosition().fY);
        canvasLayer->setSize(m_contentLayer->getWidth(), m_contentLayer->getHeight());
        canvasLayer->setDrawTransform(*(m_contentLayer->drawTransform()));

        canvasLayer->ref();
        m_contentLayer->unref();
        m_contentLayer = canvasLayer;

        m_needsSyncChildren = true;
        m_is3DCanvas = true;

        setDrawsContent(true);
    }
}
#endif

PlatformLayer* GraphicsLayerAndroid::platformLayer() const
{
    LOG("platformLayer");
    return m_contentLayer;
}

#ifndef NDEBUG
void GraphicsLayerAndroid::setDebugBackgroundColor(const Color& color)
{
}

void GraphicsLayerAndroid::setDebugBorder(const Color& color, float borderWidth)
{
}
#endif

void GraphicsLayerAndroid::setZPosition(float position)
{
    if (position == m_zPosition)
        return;
    LOG("(%x) setZPosition: %.2f", this, position);
    GraphicsLayer::setZPosition(position);
    askForSync();
}

void GraphicsLayerAndroid::askForSync()
{
    if (!m_client)
        return;

    if (m_client)
        m_client->notifySyncRequired(this);
}

void GraphicsLayerAndroid::syncChildren()
{
    if (m_needsSyncChildren) {
        m_contentLayer->removeChildren();
        LayerAndroid* layer = m_contentLayer;
        if (m_foregroundClipLayer) {
            m_contentLayer->addChild(m_foregroundClipLayer);
            // Use the scrollable content layer as the parent of the children so
            // that they move with the content.
            layer = m_foregroundLayer;
            layer->removeChildren();
        }
        for (unsigned int i = 0; i < m_children.size(); i++)
            layer->addChild(m_children[i]->platformLayer());
        m_needsSyncChildren = false;
    }
}

void GraphicsLayerAndroid::syncMask()
{
    if (m_needsSyncMask) {
        if (m_maskLayer) {
            LayerAndroid* mask = m_maskLayer->platformLayer();
            m_contentLayer->setMaskLayer(mask);
        } else
            m_contentLayer->setMaskLayer(0);

        m_contentLayer->setMasksToBounds(m_masksToBounds);
        m_needsSyncMask = false;
    }
}

void GraphicsLayerAndroid::gatherRootLayers(Vector<const RenderLayer*>& list)
{
    RenderLayer* renderLayer = renderLayerFromClient(m_client);
    if (renderLayer) {
        const RenderLayer* rootLayer = renderLayer->root();
        bool found = false;
        for (unsigned int i = 0; i < list.size(); i++) {
            const RenderLayer* current = list[i];
            if (current == rootLayer) {
                found = true;
                break;
            }
        }
        if (!found)
            list.append(rootLayer);
    }

    for (unsigned int i = 0; i < m_children.size(); i++) {
        GraphicsLayerAndroid* layer = static_cast<GraphicsLayerAndroid*>(m_children[i]);
        layer->gatherRootLayers(list);
    }
}

void GraphicsLayerAndroid::syncCompositingStateForThisLayerOnly()
{
    updateScrollingLayers();
    updateFixedPosition();
    syncChildren();
    syncMask();

    if (!gPaused || WTF::currentTime() >= gPausedDelay)
        repaint();
}

void GraphicsLayerAndroid::syncCompositingState()
{
    for (unsigned int i = 0; i < m_children.size(); i++)
        m_children[i]->syncCompositingState();

    syncCompositingStateForThisLayerOnly();
}

void GraphicsLayerAndroid::notifyClientAnimationStarted()
{
    for (unsigned int i = 0; i < m_children.size(); i++)
        static_cast<GraphicsLayerAndroid*>(m_children[i])->notifyClientAnimationStarted();

    if (m_needsNotifyClient) {
        if (client())
            client()->notifyAnimationStarted(this, WTF::currentTime());
        m_needsNotifyClient = false;
    }
}

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
