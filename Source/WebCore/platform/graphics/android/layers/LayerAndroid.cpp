#define LOG_TAG "LayerAndroid"
#define LOG_NDEBUG 1

#include "config.h"
#include "LayerAndroid.h"

#if USE(ACCELERATED_COMPOSITING)

#include "AndroidLog.h"
#include "AndroidAnimation.h"
#include "ClassTracker.h"
#include "DrawExtra.h"
#include "DumpLayer.h"
#include "FixedPositioning.h"
#include "GLUtils.h"
#include "GLWebViewState.h"
#include "ImagesManager.h"
#include "InspectorCanvas.h"
#include "LayerContent.h"
#include "MediaLayer.h"
#include "ParseCanvas.h"
#include "PictureLayerContent.h"
#include "PrerenderedInval.h"
#include "SkBitmapRef.h"
#include "SkDevice.h"
#include "SkDrawFilter.h"
#include "SkPaint.h"
#include "SkPicture.h"
#include "SkTypeface.h"
#include "Surface.h"
#include "TilesManager.h"

#include <wtf/CurrentTime.h>
#include <wtf/text/CString.h>
#include <math.h>

#define DISABLE_LAYER_MERGE
#undef DISABLE_LAYER_MERGE

#define LAYER_MERGING_DEBUG
#undef LAYER_MERGING_DEBUG

namespace WebCore {

static int gUniqueId;

class OpacityDrawFilter : public SkDrawFilter {
public:
    OpacityDrawFilter(int opacity) : m_opacity(opacity) { }
    virtual void filter(SkPaint* paint, Type)
    {
        paint->setAlpha(m_opacity);
    }
private:
    int m_opacity;
};

///////////////////////////////////////////////////////////////////////////////

LayerAndroid::LayerAndroid(RenderLayer* owner) : Layer(),
    m_uniqueId(++gUniqueId),
    m_haveClip(false),
    m_backfaceVisibility(true),
    m_visible(true),
    m_backgroundColor(0),
    m_preserves3D(false),
    m_anchorPointZ(0),
    m_isPositionAbsolute(false),
    m_fixedPosition(0),
    m_zValue(0),
    m_content(0),
    m_imageCRC(0),
    m_scale(1),
    m_lastComputeTextureSize(0),
    m_owningLayer(owner),
    m_type(LayerAndroid::WebCoreLayer),
    m_intrinsicallyComposited(true),
    m_surface(0),
    m_replicatedLayer(0),
    m_originalLayer(0),
    m_maskLayer(0)
{
    m_dirtyRegion.setEmpty();
#ifdef DEBUG_COUNT
    ClassTracker::instance()->increment("LayerAndroid");
    ClassTracker::instance()->add(this);
#endif
}

LayerAndroid::LayerAndroid(const LayerAndroid& layer) : Layer(layer),
    m_uniqueId(layer.m_uniqueId),
    m_haveClip(layer.m_haveClip),
    m_backfaceVisibility(layer.m_backfaceVisibility),
    m_visible(layer.m_visible),
    m_backgroundColor(layer.m_backgroundColor),
    m_preserves3D(layer.m_preserves3D),
    m_anchorPointZ(layer.m_anchorPointZ),
    m_isPositionAbsolute(layer.m_isPositionAbsolute),
    m_fixedPosition(0),
    m_zValue(layer.m_zValue),
    m_content(layer.m_content),
    m_imageCRC(layer.m_imageCRC),
    m_scale(layer.m_scale),
    m_lastComputeTextureSize(0),
    m_owningLayer(layer.m_owningLayer),
    m_type(LayerAndroid::UILayer),
    m_intrinsicallyComposited(layer.m_intrinsicallyComposited),
    m_surface(0),
    m_replicatedLayer(0),
    m_originalLayer(0),
    m_maskLayer(0)
{
    if (m_imageCRC)
        ImagesManager::instance()->retainImage(m_imageCRC);

    SkSafeRef(m_content);

    if (layer.m_fixedPosition) {
        m_fixedPosition = layer.m_fixedPosition->copy(this);
        Layer::setShouldInheritFromRootTransform(true);
    }

    m_transform = layer.m_transform;
    m_drawTransform = layer.m_drawTransform;
    m_drawTransformUnfudged = layer.m_drawTransformUnfudged;
    m_childrenTransform = layer.m_childrenTransform;
    m_dirtyRegion = layer.m_dirtyRegion;

    m_replicatedLayerPosition = layer.m_replicatedLayerPosition;

#ifdef ABSOLUTE_POSITION
    // If we have absolute elements, we may need to reorder them if they
    // are followed by another layer that is not also absolutely positioned.
    // (as absolutely positioned elements are out of the normal flow)
    bool hasAbsoluteChildren = false;
    bool hasOnlyAbsoluteFollowers = true;

    for (int i = 0; i < layer.countChildren(); i++) {
        if (layer.getChild(i)->isPositionAbsolute()) {
            hasAbsoluteChildren = true;
            continue;
        }
        if (hasAbsoluteChildren
            && !layer.getChild(i)->isPositionAbsolute()) {
            hasOnlyAbsoluteFollowers = false;
            break;
        }
    }

    if (hasAbsoluteChildren && !hasOnlyAbsoluteFollowers) {
        Vector<LayerAndroid*> normalLayers;
        Vector<LayerAndroid*> absoluteLayers;
        for (int i = 0; i < layer.countChildren(); i++) {
            LayerAndroid* child = layer.getChild(i);
            if (child->isPositionAbsolute()
                || child->isPositionFixed())
                absoluteLayers.append(child);
            else
                normalLayers.append(child);
        }
        for (unsigned int i = 0; i < normalLayers.size(); i++)
            addChild(normalLayers[i]->copy())->unref();
        for (unsigned int i = 0; i < absoluteLayers.size(); i++)
            addChild(absoluteLayers[i]->copy())->unref();
    } else {
        for (int i = 0; i < layer.countChildren(); i++)
            addChild(layer.getChild(i)->copy())->unref();
    }
#else
    for (int i = 0; i < layer.countChildren(); i++)
        addChild(layer.getChild(i)->copy())->unref();
#endif

    KeyframesMap::const_iterator end = layer.m_animations.end();
    for (KeyframesMap::const_iterator it = layer.m_animations.begin(); it != end; ++it) {
        // Deep copy the key's string, to avoid cross-thread refptr use
        pair<String, int> newKey(it->first.first.threadsafeCopy(), it->first.second);
        m_animations.add(newKey, it->second);
    }

    if (layer.m_replicatedLayer) {
        // The replicated layer is always the first child
        m_replicatedLayer = getChild(0);
        m_replicatedLayer->setOriginalLayer(this);
    }

    if (layer.m_maskLayer)
        m_maskLayer = layer.m_maskLayer->copy();

#ifdef DEBUG_COUNT
    ClassTracker::instance()->increment("LayerAndroid - recopy (UI)");
    ClassTracker::instance()->add(this);
#endif
}

LayerAndroid::~LayerAndroid()
{
    if (m_imageCRC)
        ImagesManager::instance()->releaseImage(m_imageCRC);
    if (m_fixedPosition)
        delete m_fixedPosition;

    SkSafeUnref(m_maskLayer);
    SkSafeUnref(m_content);
    // Don't unref m_surface, owned by BaseLayerAndroid
    m_animations.clear();
#ifdef DEBUG_COUNT
    ClassTracker::instance()->remove(this);
    if (m_type == LayerAndroid::WebCoreLayer)
        ClassTracker::instance()->decrement("LayerAndroid");
    else if (m_type == LayerAndroid::UILayer)
        ClassTracker::instance()->decrement("LayerAndroid - recopy (UI)");
#endif
}

float LayerAndroid::maxZoomScale() const
{
    return m_content ? m_content->maxZoomScale() : 1.0f;
}

static int gDebugNbAnims = 0;

bool LayerAndroid::evaluateAnimations()
{
    double time = WTF::currentTime();
    gDebugNbAnims = 0;
    return evaluateAnimations(time);
}

bool LayerAndroid::hasAnimations() const
{
    for (int i = 0; i < countChildren(); i++) {
        if (getChild(i)->hasAnimations())
            return true;
    }
    return !!m_animations.size();
}

bool LayerAndroid::evaluateAnimations(double time)
{
    bool hasRunningAnimations = false;
    for (int i = 0; i < countChildren(); i++) {
        if (getChild(i)->evaluateAnimations(time))
            hasRunningAnimations = true;
    }

    m_hasRunningAnimations = false;
    int nbAnims = 0;
    KeyframesMap::const_iterator end = m_animations.end();
    for (KeyframesMap::const_iterator it = m_animations.begin(); it != end; ++it) {
        gDebugNbAnims++;
        nbAnims++;
        LayerAndroid* currentLayer = const_cast<LayerAndroid*>(this);
        m_hasRunningAnimations |= (it->second)->evaluate(currentLayer, time);
    }

    return hasRunningAnimations || m_hasRunningAnimations;
}

void LayerAndroid::initAnimations() {
    // tell auto-initializing animations to start now
    for (int i = 0; i < countChildren(); i++)
        getChild(i)->initAnimations();

    KeyframesMap::const_iterator localBegin = m_animations.begin();
    KeyframesMap::const_iterator localEnd = m_animations.end();
    for (KeyframesMap::const_iterator localIt = localBegin; localIt != localEnd; ++localIt)
        (localIt->second)->suggestBeginTime(WTF::currentTime());
}

void LayerAndroid::addDirtyArea()
{
    if (m_drawTransform.hasPerspective()) {
        state()->doFrameworkFullInval();
        return;
    }

    // TODO: rewrite this to handle partial invalidate, and to handle base
    // layer's large clip correctly

    IntSize layerSize(getSize().width(), getSize().height());

    FloatRect area =
        TilesManager::instance()->shader()->rectInViewCoord(m_drawTransform, layerSize);
    FloatRect clippingRect =
        TilesManager::instance()->shader()->rectInInvViewCoord(m_clippingRect);
    FloatRect clip =
        TilesManager::instance()->shader()->convertInvViewCoordToViewCoord(clippingRect);

    area.intersect(clip);
    IntRect dirtyArea(area.x(), area.y(), area.width(), area.height());

    state()->addDirtyArea(dirtyArea);

    for (int i = 0; i < countChildren(); i++)
        getChild(i)->addDirtyArea();
}

void LayerAndroid::addAnimation(PassRefPtr<AndroidAnimation> prpAnim)
{
    RefPtr<AndroidAnimation> anim = prpAnim;
    pair<String, int> key(anim->nameCopy(), anim->type());
    removeAnimationsForProperty(anim->type());
    m_animations.add(key, anim);
}

void LayerAndroid::removeAnimationsForProperty(AnimatedPropertyID property)
{
    KeyframesMap::const_iterator end = m_animations.end();
    Vector<pair<String, int> > toDelete;
    for (KeyframesMap::const_iterator it = m_animations.begin(); it != end; ++it) {
        if ((it->second)->type() == property)
            toDelete.append(it->first);
    }

    for (unsigned int i = 0; i < toDelete.size(); i++)
        m_animations.remove(toDelete[i]);
}

void LayerAndroid::removeAnimationsForKeyframes(const String& name)
{
    KeyframesMap::const_iterator end = m_animations.end();
    Vector<pair<String, int> > toDelete;
    for (KeyframesMap::const_iterator it = m_animations.begin(); it != end; ++it) {
        if ((it->second)->isNamed(name))
            toDelete.append(it->first);
    }

    for (unsigned int i = 0; i < toDelete.size(); i++)
        m_animations.remove(toDelete[i]);
}

// We only use the bounding rect of the layer as mask...
// FIXME: use a real mask?
void LayerAndroid::setMaskLayer(LayerAndroid* layer)
{
    SkSafeRef(layer);
    SkSafeUnref(m_maskLayer);
    m_maskLayer = layer;
    if (layer)
        m_haveClip = true;
}

void LayerAndroid::setBackgroundColor(SkColor color)
{
    m_backgroundColor = color;
}

FloatPoint LayerAndroid::translation() const
{
    TransformationMatrix::DecomposedType tDecomp;
    m_transform.decompose(tDecomp);
    FloatPoint p(tDecomp.translateX, tDecomp.translateY);
    return p;
}

IFrameLayerAndroid* LayerAndroid::updatePosition(SkRect viewport,
                                                 IFrameLayerAndroid* parentIframeLayer)
{
    // subclasses can implement this virtual function to modify their position
    if (m_fixedPosition)
        return m_fixedPosition->updatePosition(viewport, parentIframeLayer);
    return parentIframeLayer;
}

void LayerAndroid::updateLayerPositions(SkRect viewport, IFrameLayerAndroid* parentIframeLayer)
{
    ALOGV("updating fixed positions, using viewport %fx%f - %fx%f",
          viewport.fLeft, viewport.fTop,
          viewport.width(), viewport.height());

    IFrameLayerAndroid* iframeLayer = updatePosition(viewport, parentIframeLayer);

    int count = this->countChildren();
    for (int i = 0; i < count; i++)
        this->getChild(i)->updateLayerPositions(viewport, iframeLayer);
}

void LayerAndroid::updatePositions()
{
    // apply the viewport to us
    if (!isPositionFixed()) {
        // turn our fields into a matrix.
        //
        // FIXME: this should happen in the caller, and we should remove these
        // fields from our subclass
        SkMatrix matrix;
        GLUtils::toSkMatrix(matrix, m_transform);
        this->setMatrix(matrix);
    }

    // now apply it to our children
    int count = this->countChildren();
    for (int i = 0; i < count; i++)
        this->getChild(i)->updatePositions();
}

void LayerAndroid::updateLocalTransformAndClip(const TransformationMatrix& parentMatrix,
                                               const FloatRect& clipping)
{
    FloatPoint position(getPosition().x() + m_replicatedLayerPosition.x() - getScrollOffset().x(),
                        getPosition().y() + m_replicatedLayerPosition.y() - getScrollOffset().y());

    if (isPositionFixed())
        m_drawTransform.makeIdentity();
    else
        m_drawTransform = parentMatrix;

    if (m_transform.isIdentity()) {
        m_drawTransform.translate3d(position.x(),
                                    position.y(),
                                    0);
    } else {
        float originX = getAnchorPoint().x() * getWidth();
        float originY = getAnchorPoint().y() * getHeight();
        m_drawTransform.translate3d(originX + position.x(),
                                    originY + position.y(),
                                    anchorPointZ());
        m_drawTransform.multiply(m_transform);
        m_drawTransform.translate3d(-originX,
                                    -originY,
                                    -anchorPointZ());
    }

    m_drawTransformUnfudged = m_drawTransform;
    if (m_drawTransform.isIdentityOrTranslation()
        && surface() && surface()->allowTransformFudging()) {
        // adjust the translation coordinates of the draw transform matrix so
        // that layers (defined in content coordinates) will align to display/view pixels

        // the surface may not allow fudging if it uses the draw transform at paint time
        float desiredContentX = round(m_drawTransform.m41() * m_scale) / m_scale;
        float desiredContentY = round(m_drawTransform.m42() * m_scale) / m_scale;
        ALOGV("fudging translation from %f, %f to %f, %f",
              m_drawTransform.m41(), m_drawTransform.m42(),
              desiredContentX, desiredContentY);
        m_drawTransform.setM41(desiredContentX);
        m_drawTransform.setM42(desiredContentY);
    }

    m_zValue = TilesManager::instance()->shader()->zValue(m_drawTransform,
                                                          getSize().width(),
                                                          getSize().height());

    if (m_haveClip) {
        // The clipping rect calculation and intersetion will be done in content
        // coordinates.
        FloatRect rect(0, 0, getWidth(), getHeight());
        FloatRect clip = m_drawTransform.mapRect(rect);
        clip.intersect(clipping);
        setDrawClip(clip);
    } else {
        setDrawClip(clipping);
    }
    ALOGV("%s - %d %f %f %f %f",
          subclassType() == BaseLayer ? "BASE" : "nonbase",
          m_haveClip, m_clippingRect.x(), m_clippingRect.y(),
          m_clippingRect.width(), m_clippingRect.height());

    setVisible(m_backfaceVisibility || m_drawTransform.inverse().m33() >= 0);
}

void LayerAndroid::updateGLPositionsAndScale(const TransformationMatrix& parentMatrix,
                                             const FloatRect& clipping, float opacity,
                                             float scale, bool forceCalculation,
                                             bool disableFixedElemUpdate)
{
    m_scale = scale;

    opacity *= getOpacity();
    setDrawOpacity(opacity);

    // constantly recalculate the draw transform of layers that may require it (and their children)
    forceCalculation |= hasDynamicTransform();

    forceCalculation &= !(disableFixedElemUpdate && isPositionFixed());
    if (forceCalculation)
        updateLocalTransformAndClip(parentMatrix, clipping);

    if (!countChildren() || !m_visible)
        return;

    TransformationMatrix childMatrix = m_drawTransformUnfudged;
    // Flatten to 2D if the layer doesn't preserve 3D.
    if (!preserves3D()) {
        childMatrix.setM13(0);
        childMatrix.setM23(0);
        childMatrix.setM31(0);
        childMatrix.setM32(0);
        childMatrix.setM33(1);
        childMatrix.setM34(0);
        childMatrix.setM43(0);
    }

    // now apply it to our children
    childMatrix.translate3d(getScrollOffset().x(), getScrollOffset().y(), 0);
    if (!m_childrenTransform.isIdentity()) {
        childMatrix.translate(getSize().width() * 0.5f, getSize().height() * 0.5f);
        childMatrix.multiply(m_childrenTransform);
        childMatrix.translate(-getSize().width() * 0.5f, -getSize().height() * 0.5f);
    }
    for (int i = 0; i < countChildren(); i++)
        this->getChild(i)->updateGLPositionsAndScale(childMatrix, drawClip(),
                                                     opacity, scale, forceCalculation,
                                                     disableFixedElemUpdate);
}

bool LayerAndroid::visible() {
    // TODO: avoid climbing tree each access
    LayerAndroid* current = this;
    while (current->getParent()) {
        if (!current->m_visible)
            return false;
        current = static_cast<LayerAndroid*>(current->getParent());
    }
    return true;
}

void LayerAndroid::setContentsImage(SkBitmapRef* img)
{
    ImageTexture* image = ImagesManager::instance()->setImage(img);
    ImagesManager::instance()->releaseImage(m_imageCRC);
    m_imageCRC = image ? image->imageCRC() : 0;
}

void LayerAndroid::setContent(LayerContent* content)
{
    SkSafeRef(content);
    SkSafeUnref(m_content);
    m_content = content;
}

bool LayerAndroid::canUpdateWithBlit()
{
    if (!m_content || !m_scale)
        return false;
    IntRect clip = clippedRect();
    IntRect dirty = m_dirtyRegion.getBounds();
    dirty.intersect(clip);
    PrerenderedInval* prerendered = m_content->prerenderForRect(dirty);
    if (!prerendered)
        return false;
    // Check that the scales are "close enough" to produce the same rects
    FloatRect screenArea = prerendered->screenArea;
    screenArea.scale(1 / m_scale);
    IntRect enclosingDocArea = enclosingIntRect(screenArea);
    return enclosingDocArea == prerendered->area;
}

bool LayerAndroid::needsTexture()
{
    return (m_content && !m_content->isEmpty())
            || (m_originalLayer && m_originalLayer->needsTexture());
}

IntRect LayerAndroid::clippedRect() const
{
    IntRect r(0, 0, getWidth(), getHeight());
    IntRect tr = m_drawTransform.mapRect(r);
    IntRect cr = TilesManager::instance()->shader()->clippedRectWithVisibleContentRect(tr);
    IntRect rect = m_drawTransform.inverse().mapRect(cr);
    return rect;
}

int LayerAndroid::nbLayers()
{
    int nb = 0;
    int count = this->countChildren();
    for (int i = 0; i < count; i++)
        nb += this->getChild(i)->nbLayers();
    return nb+1;
}

int LayerAndroid::nbTexturedLayers()
{
    int nb = 0;
    int count = this->countChildren();
    for (int i = 0; i < count; i++)
        nb += this->getChild(i)->nbTexturedLayers();
    if (needsTexture())
        nb++;
    return nb;
}

void LayerAndroid::showLayer(int indent)
{
    char spaces[256];
    memset(spaces, 0, 256);
    for (int i = 0; i < indent; i++)
        spaces[i] = ' ';

    if (!indent) {
        ALOGD("\n\n--- LAYERS TREE ---");
        IntRect contentViewport(TilesManager::instance()->shader()->contentViewport());
        ALOGD("contentViewport(%d, %d, %d, %d)",
              contentViewport.x(), contentViewport.y(),
              contentViewport.width(), contentViewport.height());
    }

    IntRect r(0, 0, getWidth(), getHeight());
    IntRect tr = m_drawTransform.mapRect(r);
    IntRect visible = visibleContentArea();
    IntRect clip(m_clippingRect.x(), m_clippingRect.y(),
                 m_clippingRect.width(), m_clippingRect.height());
    ALOGD("%s s:%x %s %s (%d) [%d:%x - 0x%x] - %s %s - area (%d, %d, %d, %d) - visible (%d, %d, %d, %d) "
          "clip (%d, %d, %d, %d) %s %s m_content(%x), pic w: %d h: %d originalLayer: %x %d",
          spaces, m_surface, m_haveClip ? "CLIP LAYER" : "", subclassName(),
          subclassType(), uniqueId(), this, m_owningLayer,
          needsTexture() ? "needsTexture" : "",
          m_imageCRC ? "hasImage" : "",
          tr.x(), tr.y(), tr.width(), tr.height(),
          visible.x(), visible.y(), visible.width(), visible.height(),
          clip.x(), clip.y(), clip.width(), clip.height(),
          contentIsScrollable() ? "SCROLLABLE" : "",
          isPositionFixed() ? "FIXED" : "",
          m_content,
          m_content ? m_content->width() : -1,
          m_content ? m_content->height() : -1,
          m_originalLayer, m_originalLayer ? m_originalLayer->uniqueId() : -1);

    int count = this->countChildren();
    for (int i = 0; i < count; i++)
        this->getChild(i)->showLayer(indent + 2);
}

void LayerAndroid::mergeInvalsInto(LayerAndroid* replacementTree)
{
    int count = this->countChildren();
    for (int i = 0; i < count; i++)
        this->getChild(i)->mergeInvalsInto(replacementTree);

    LayerAndroid* replacementLayer = replacementTree->findById(uniqueId());
    if (replacementLayer)
        replacementLayer->markAsDirty(m_dirtyRegion);
}

static inline bool compareLayerZ(const LayerAndroid* a, const LayerAndroid* b)
{
    return a->zValue() > b->zValue();
}

bool LayerAndroid::canJoinSurface(Surface* surface)
{
#ifdef DISABLE_LAYER_MERGE
    return false;
#else
    // returns true if the layer can be merged onto the surface (group of layers)
    if (!surface)
        return false;

    LayerAndroid* lastLayer = surface->getFirstLayer();

    // isolate intrinsically composited layers
    if (needsIsolatedSurface() || lastLayer->needsIsolatedSurface())
        return false;

    // TODO: investigate potential for combining transformed layers
    if (!m_drawTransform.isIdentityOrTranslation()
        || !lastLayer->m_drawTransform.isIdentityOrTranslation())
        return false;

    // TODO: compare other layer properties - fixed? overscroll? transformed?
    return true;
#endif
}

void LayerAndroid::assignSurfaces(LayerMergeState* mergeState)
{
    // recurse through layers in draw order, and merge layers when able

    bool needNewSurface = !mergeState->currentSurface
        || mergeState->nonMergeNestedLevel > 0
        || !canJoinSurface(mergeState->currentSurface);

    if (needNewSurface) {
        mergeState->currentSurface = new Surface();
        mergeState->surfaceList->append(mergeState->currentSurface);
    }

#ifdef LAYER_MERGING_DEBUG
    ALOGD("%*slayer %p(%d) rl %p %s surface %p lvl: %d, fixed %d, anim %d, intCom %d, haveClip %d scroll %d hasText (layer: %d surface: %d) hasContent %d size %.2f x %.2f",
          4*mergeState->depth, "", this, m_uniqueId, m_owningLayer,
          needNewSurface ? "NEW" : "joins", mergeState->currentSurface,
          mergeState->nonMergeNestedLevel,
          isPositionFixed(), m_animations.size() != 0,
          m_intrinsicallyComposited,
          m_haveClip,
          contentIsScrollable(), m_content ? m_content->hasText() : -1,
          mergeState->currentSurface ? mergeState->currentSurface->hasText() : -1,
          needsTexture(), getWidth(), getHeight());
#endif

    mergeState->currentSurface->addLayer(this, m_drawTransform);
    m_surface = mergeState->currentSurface;

    if (hasDynamicTransform()) {
        // disable layer merging within the children of these layer types
        mergeState->nonMergeNestedLevel++;
    }

    // pass the surface through children in drawing order, so that they may
    // attach themselves (and paint on it) if possible, or ignore it and create
    // a new one if not
    int count = this->countChildren();
    if (count > 0) {
        mergeState->depth++;
        Vector <LayerAndroid*> sublayers;
        for (int i = 0; i < count; i++)
            sublayers.append(getChild(i));

        // sort for the transparency
        std::stable_sort(sublayers.begin(), sublayers.end(), compareLayerZ);
        for (int i = 0; i < count; i++)
            sublayers[i]->assignSurfaces(mergeState);
        mergeState->depth--;
    }

    if (hasDynamicTransform()) {
        // re-enable joining
        mergeState->nonMergeNestedLevel--;

        // disallow layers painting after to join with this surface
        mergeState->currentSurface = 0;
    }

    if (needsIsolatedSurface())
        mergeState->currentSurface = 0;

}

// We call this in WebViewCore, when copying the tree of layers.
// As we construct a new tree that will be passed on the UI,
// we mark the webkit-side tree as having no more dirty region
// (otherwise we would continuously have those dirty region UI-side)
void LayerAndroid::clearDirtyRegion()
{
    int count = this->countChildren();
    for (int i = 0; i < count; i++)
        this->getChild(i)->clearDirtyRegion();

    m_dirtyRegion.setEmpty();
}

int LayerAndroid::setHwAccelerated(bool hwAccelerated)
{
    int flags = InvalidateNone;
    int count = this->countChildren();
    for (int i = 0; i < count; i++)
        flags |= this->getChild(i)->setHwAccelerated(hwAccelerated);

    return flags | onSetHwAccelerated(hwAccelerated);
}

FloatRect LayerAndroid::fullContentAreaMapped() const
{
    FloatRect area(0,0, getWidth(), getHeight());
    FloatRect globalArea = m_drawTransform.mapRect(area);
    return globalArea;
}

IntRect LayerAndroid::fullContentArea() const
{
    IntRect area(0,0, getWidth(), getHeight());
    return area;
}

IntRect LayerAndroid::visibleContentArea(bool force3dContentVisible) const
{
    IntRect area = fullContentArea();
    if (subclassType() == LayerAndroid::FixedBackgroundImageLayer)
       return area;

    // If transform isn't limited to 2D space, return the entire content area.
    // Transforming from layers to content coordinates and back doesn't
    // preserve 3D.
    if (force3dContentVisible && GLUtils::has3dTransform(m_drawTransform))
            return area;

    // First, we get the transformed area of the layer,
    // in content coordinates
    IntRect rect = m_drawTransform.mapRect(area);

    // Then we apply the clipping
    IntRect clip(m_clippingRect);
    rect.intersect(clip);

    // Now clip with the viewport in content coordinate
    IntRect contentViewport(TilesManager::instance()->shader()->contentViewport());
    rect.intersect(contentViewport);

    // Finally, let's return the visible area, in layers coordinate
    return m_drawTransform.inverse().mapRect(rect);
}

bool LayerAndroid::drawCanvas(SkCanvas* canvas, bool drawChildren, PaintStyle style)
{
    if (!m_visible)
        return false;

    bool askScreenUpdate = false;

    {
        SkAutoCanvasRestore acr(canvas, true);
        SkRect r;
        r.set(m_clippingRect.x(), m_clippingRect.y(),
              m_clippingRect.x() + m_clippingRect.width(),
              m_clippingRect.y() + m_clippingRect.height());
        if (canvas->clipRect(r)) {
            SkMatrix matrix;
            GLUtils::toSkMatrix(matrix, m_drawTransform);
            SkMatrix canvasMatrix = canvas->getTotalMatrix();
            matrix.postConcat(canvasMatrix);
            canvas->setMatrix(matrix);
            onDraw(canvas, m_drawOpacity, 0, style);
        }
    }

    if (!drawChildren)
        return false;

    // When the layer is dirty, the UI thread should be notified to redraw.
    askScreenUpdate |= drawChildrenCanvas(canvas, style);
    return askScreenUpdate;
}

void LayerAndroid::collect3dRenderingContext(Vector<LayerAndroid*>& layersInContext)
{
    layersInContext.append(this);
    if (preserves3D()) {
        int count = countChildren();
        for (int i = 0; i < count; i++)
            getChild(i)->collect3dRenderingContext(layersInContext);
    }
}

bool LayerAndroid::drawSurfaceAndChildrenGL()
{
    bool askScreenUpdate = false;
    if (surface()->getFirstLayer() == this)
        askScreenUpdate |= surface()->drawGL(false);

    // return early, since children will be painted directly by drawTreeSurfacesGL
    if (preserves3D())
        return askScreenUpdate;

    int count = countChildren();
    Vector <LayerAndroid*> sublayers;
    for (int i = 0; i < count; i++)
        sublayers.append(getChild(i));

    std::stable_sort(sublayers.begin(), sublayers.end(), compareLayerZ);
    for (int i = 0; i < count; i++)
        askScreenUpdate |= sublayers[i]->drawTreeSurfacesGL();

    return askScreenUpdate;
}

bool LayerAndroid::drawTreeSurfacesGL()
{
    bool askScreenUpdate = false;
    if (preserves3D()) {
        // hit a preserve-3d layer, so render the entire 3D rendering context in z order
        Vector<LayerAndroid*> contextLayers;
        collect3dRenderingContext(contextLayers);
        std::stable_sort(contextLayers.begin(), contextLayers.end(), compareLayerZ);

        for (unsigned int i = 0; i < contextLayers.size(); i++)
            askScreenUpdate |= contextLayers[i]->drawSurfaceAndChildrenGL();
    } else
        askScreenUpdate |= drawSurfaceAndChildrenGL();

    return askScreenUpdate;
}

bool LayerAndroid::drawGL(bool layerTilesDisabled)
{
    if (!layerTilesDisabled && m_imageCRC) {
        ImageTexture* imageTexture = ImagesManager::instance()->retainImage(m_imageCRC);
        if (imageTexture)
            imageTexture->drawGL(this, getOpacity());
        ImagesManager::instance()->releaseImage(m_imageCRC);
    }

    state()->glExtras()->drawGL(this);
    bool askScreenUpdate = false;

    if (m_hasRunningAnimations)
        askScreenUpdate = true;

    return askScreenUpdate;
}

bool LayerAndroid::drawChildrenCanvas(SkCanvas* canvas, PaintStyle style)
{
    bool askScreenUpdate = false;
    int count = this->countChildren();
    if (count > 0) {
        Vector <LayerAndroid*> sublayers;
        for (int i = 0; i < count; i++)
            sublayers.append(this->getChild(i));

        // now we sort for the transparency
        std::stable_sort(sublayers.begin(), sublayers.end(), compareLayerZ);
        for (int i = 0; i < count; i++) {
            LayerAndroid* layer = sublayers[i];
            askScreenUpdate |= layer->drawCanvas(canvas, true, style);
        }
    }

    return askScreenUpdate;
}

void LayerAndroid::contentDraw(SkCanvas* canvas, PaintStyle style)
{
    if (m_maskLayer && m_maskLayer->m_content) {
        // TODO: we should use a shader instead of doing
        // the masking in software

        if (m_originalLayer)
            m_originalLayer->m_content->draw(canvas);
        else if (m_content)
            m_content->draw(canvas);

        SkPaint maskPaint;
        maskPaint.setXfermodeMode(SkXfermode::kDstIn_Mode);
        int count = canvas->saveLayer(0, &maskPaint, SkCanvas::kHasAlphaLayer_SaveFlag);
        m_maskLayer->m_content->draw(canvas);
        canvas->restoreToCount(count);

    } else if (m_content)
        m_content->draw(canvas);

    if (TilesManager::instance()->getShowVisualIndicator()) {
        float w = getSize().width();
        float h = getSize().height();
        SkPaint paint;

        if (style == MergedLayers)
            paint.setARGB(255, 255, 255, 0);
        else if (style == UnmergedLayers)
            paint.setARGB(255, 255, 0, 0);
        else if (style == FlattenedLayers)
            paint.setARGB(255, 255, 0, 255);

        canvas->drawLine(0, 0, w, h, paint);
        canvas->drawLine(0, h, w, 0, paint);

        canvas->drawLine(0, 0, 0, h-1, paint);
        canvas->drawLine(0, h-1, w-1, h-1, paint);
        canvas->drawLine(w-1, h-1, w-1, 0, paint);
        canvas->drawLine(w-1, 0, 0, 0, paint);

        static SkTypeface* s_typeface = 0;
        if (!s_typeface)
            s_typeface = SkTypeface::CreateFromName("", SkTypeface::kBold);
        paint.setARGB(255, 0, 0, 255);
        paint.setTextSize(17);
        char str[256];
        snprintf(str, 256, "%d", uniqueId());
        paint.setTypeface(s_typeface);
        canvas->drawText(str, strlen(str), 2, h - 2, paint);
    }

    if (m_fixedPosition)
        return m_fixedPosition->contentDraw(canvas, style);
}

void LayerAndroid::onDraw(SkCanvas* canvas, SkScalar opacity,
                          android::DrawExtra* extra, PaintStyle style)
{
    if (m_haveClip) {
        SkRect r;
        r.set(0, 0, getSize().width(), getSize().height());
        canvas->clipRect(r);
        return;
    }

    // only continue drawing if layer is drawable
    if (!m_content && !m_imageCRC)
        return;

    // we just have this save/restore for opacity...
    SkAutoCanvasRestore restore(canvas, true);

    int canvasOpacity = SkScalarRound(opacity * 255);
    if (canvasOpacity < 255)
        canvas->setDrawFilter(new OpacityDrawFilter(canvasOpacity));

    if (m_imageCRC) {
        ImageTexture* imageTexture = ImagesManager::instance()->retainImage(m_imageCRC);
        m_dirtyRegion.setEmpty();
        if (imageTexture) {
            SkRect dest;
            dest.set(0, 0, getSize().width(), getSize().height());
            imageTexture->drawCanvas(canvas, dest);
        }
        ImagesManager::instance()->releaseImage(m_imageCRC);
    }
    contentDraw(canvas, style);
    if (extra)
        extra->draw(canvas, this);
}

void LayerAndroid::setFixedPosition(FixedPositioning* position) {
    if (m_fixedPosition && m_fixedPosition != position)
        delete m_fixedPosition;
    m_fixedPosition = position;
}

void LayerAndroid::dumpLayer(LayerDumper* dumper) const
{
    dumper->writeIntVal("layerId", m_uniqueId);
    dumper->writeIntVal("haveClip", m_haveClip);
    dumper->writeIntVal("isFixed", isPositionFixed());

    dumper->writeFloatVal("opacity", getOpacity());
    dumper->writeSize("size", getSize());
    dumper->writePoint("position", getPosition());
    dumper->writePoint("anchor", getAnchorPoint());

    dumper->writeMatrix("drawMatrix", m_drawTransform);
    dumper->writeMatrix("transformMatrix", m_transform);
    dumper->writeRect("clippingRect", SkRect(m_clippingRect));

    if (m_content) {
        dumper->writeIntVal("m_content.width", m_content->width());
        dumper->writeIntVal("m_content.height", m_content->height());
    }

    if (m_fixedPosition)
        m_fixedPosition->dumpLayer(dumper);
}

void LayerAndroid::dumpLayers(LayerDumper* dumper) const
{
    dumper->beginLayer(subclassName(), this);
    dumpLayer(dumper);

    dumper->beginChildren(countChildren());
    if (countChildren()) {
        for (int i = 0; i < countChildren(); i++)
            getChild(i)->dumpLayers(dumper);
    }
    dumper->endChildren();
    dumper->endLayer();
}

LayerAndroid* LayerAndroid::findById(int match)
{
    if (m_uniqueId == match)
        return this;
    for (int i = 0; i < countChildren(); i++) {
        LayerAndroid* result = getChild(i)->findById(match);
        if (result)
            return result;
    }
    return 0;
}

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
