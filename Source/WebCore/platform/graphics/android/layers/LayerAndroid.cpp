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
#include "SkBitmapRef.h"
#include "SkDrawFilter.h"
#include "SkPaint.h"
#include "SkPicture.h"
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
    m_surface(0)
{
    m_backgroundColor = 0;

    m_preserves3D = false;
    m_dirtyRegion.setEmpty();
#ifdef DEBUG_COUNT
    ClassTracker::instance()->increment("LayerAndroid");
    ClassTracker::instance()->add(this);
#endif
}

LayerAndroid::LayerAndroid(const LayerAndroid& layer) : Layer(layer),
    m_uniqueId(layer.m_uniqueId),
    m_haveClip(layer.m_haveClip),
    m_isPositionAbsolute(layer.m_isPositionAbsolute),
    m_fixedPosition(0),
    m_zValue(layer.m_zValue),
    m_owningLayer(layer.m_owningLayer),
    m_type(LayerAndroid::UILayer),
    m_intrinsicallyComposited(layer.m_intrinsicallyComposited),
    m_surface(0)
{
    m_imageCRC = layer.m_imageCRC;
    if (m_imageCRC)
        ImagesManager::instance()->retainImage(m_imageCRC);

    m_transform = layer.m_transform;
    m_backfaceVisibility = layer.m_backfaceVisibility;
    m_visible = layer.m_visible;
    m_backgroundColor = layer.m_backgroundColor;

    m_offset = layer.m_offset;

    m_content = layer.m_content;
    SkSafeRef(m_content);

    m_preserves3D = layer.m_preserves3D;
    m_anchorPointZ = layer.m_anchorPointZ;

    if (layer.m_fixedPosition) {
        m_fixedPosition = new FixedPositioning(this, *layer.m_fixedPosition);
        Layer::setShouldInheritFromRootTransform(true);
    }

    m_drawTransform = layer.m_drawTransform;
    m_childrenTransform = layer.m_childrenTransform;
    m_dirtyRegion = layer.m_dirtyRegion;
    m_scale = layer.m_scale;
    m_lastComputeTextureSize = 0;

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

    KeyframesMap::const_iterator end = layer.m_animations.end();
    for (KeyframesMap::const_iterator it = layer.m_animations.begin(); it != end; ++it) {
        m_animations.add(it->first, it->second);
    }

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

bool LayerAndroid::hasText()
{
    return m_content && m_content->hasText();
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
    IntSize layerSize(getSize().width(), getSize().height());

    FloatRect area = TilesManager::instance()->shader()->rectInInvScreenCoord(m_drawTransform, layerSize);
    FloatRect clippingRect = TilesManager::instance()->shader()->rectInScreenCoord(m_clippingRect);
    FloatRect clip = TilesManager::instance()->shader()->convertScreenCoordToInvScreenCoord(clippingRect);

    area.intersect(clip);
    IntRect dirtyArea(area.x(), area.y(), area.width(), area.height());
    state()->addDirtyArea(dirtyArea);
}

void LayerAndroid::addAnimation(PassRefPtr<AndroidAnimation> prpAnim)
{
    RefPtr<AndroidAnimation> anim = prpAnim;
    pair<String, int> key(anim->name(), anim->type());
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
        if ((it->second)->name() == name)
            toDelete.append(it->first);
    }

    for (unsigned int i = 0; i < toDelete.size(); i++)
        m_animations.remove(toDelete[i]);
}

// We only use the bounding rect of the layer as mask...
// FIXME: use a real mask?
void LayerAndroid::setMaskLayer(LayerAndroid* layer)
{
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

SkRect LayerAndroid::bounds() const
{
    SkRect rect;
    bounds(&rect);
    return rect;
}

void LayerAndroid::bounds(SkRect* rect) const
{
    const SkPoint& pos = this->getPosition();
    const SkSize& size = this->getSize();

    // The returned rect has the translation applied
    // FIXME: apply the full transform to the rect,
    // and fix the text selection accordingly
    FloatPoint p(pos.fX, pos.fY);
    p = m_transform.mapPoint(p);
    rect->fLeft = p.x();
    rect->fTop = p.y();
    rect->fRight = p.x() + size.width();
    rect->fBottom = p.y() + size.height();
}

static bool boundsIsUnique(const SkTDArray<SkRect>& region,
                           const SkRect& local)
{
    for (int i = 0; i < region.count(); i++) {
        if (region[i].contains(local))
            return false;
    }
    return true;
}

void LayerAndroid::clipArea(SkTDArray<SkRect>* region) const
{
    SkRect local;
    local.set(0, 0, std::numeric_limits<float>::max(),
        std::numeric_limits<float>::max());
    clipInner(region, local);
}

void LayerAndroid::clipInner(SkTDArray<SkRect>* region,
                             const SkRect& local) const
{
    SkRect localBounds;
    bounds(&localBounds);
    localBounds.intersect(local);
    if (localBounds.isEmpty())
        return;
    if (m_content && boundsIsUnique(*region, localBounds))
        *region->append() = localBounds;
    for (int i = 0; i < countChildren(); i++)
        getChild(i)->clipInner(region, m_haveClip ? localBounds : local);
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

void LayerAndroid::updateGLPositionsAndScale(const TransformationMatrix& parentMatrix,
                                             const FloatRect& clipping, float opacity, float scale)
{
    IntSize layerSize(getSize().width(), getSize().height());
    FloatPoint anchorPoint(getAnchorPoint().fX, getAnchorPoint().fY);
    FloatPoint position(getPosition().fX - m_offset.x(), getPosition().fY - m_offset.y());
    float originX = anchorPoint.x() * layerSize.width();
    float originY = anchorPoint.y() * layerSize.height();
    TransformationMatrix localMatrix;
    if (!isPositionFixed())
        localMatrix = parentMatrix;
    localMatrix.translate3d(originX + position.x(),
                            originY + position.y(),
                            anchorPointZ());
    localMatrix.multiply(m_transform);
    localMatrix.translate3d(-originX,
                            -originY,
                            -anchorPointZ());

    setDrawTransform(localMatrix);
    if (m_drawTransform.isIdentityOrTranslation()) {
        // adjust the translation coordinates of the draw transform matrix so
        // that layers (defined in content coordinates) will align to display/view pixels
        float desiredContentX = round(m_drawTransform.m41() * scale) / scale;
        float desiredContentY = round(m_drawTransform.m42() * scale) / scale;
        ALOGV("fudging translation from %f, %f to %f, %f",
              m_drawTransform.m41(), m_drawTransform.m42(),
              desiredContentX, desiredContentY);
        m_drawTransform.setM41(desiredContentX);
        m_drawTransform.setM42(desiredContentY);
    }

    m_zValue = TilesManager::instance()->shader()->zValue(m_drawTransform, getSize().width(), getSize().height());

    m_atomicSync.lock();
    m_scale = scale;
    m_atomicSync.unlock();

    opacity *= getOpacity();
    setDrawOpacity(opacity);

    if (m_haveClip) {
        // The clipping rect calculation and intersetion will be done in documents coordinates.
        FloatRect rect(0, 0, layerSize.width(), layerSize.height());
        FloatRect clip = m_drawTransform.mapRect(rect);
        clip.intersect(clipping);
        setDrawClip(clip);
    } else {
        setDrawClip(clipping);
    }
    ALOGV("%s - %d %f %f %f %f",
          subclassType() == BaseLayer ? "BASE" : "nonbase",
          m_haveClip, m_clippingRect.x(), m_clippingRect.y(), m_clippingRect.width(), m_clippingRect.height());

    if (!m_backfaceVisibility
         && m_drawTransform.inverse().m33() < 0) {
         setVisible(false);
         return;
    } else {
         setVisible(true);
    }

    int count = this->countChildren();
    if (!count)
        return;

    // Flatten to 2D if the layer doesn't preserve 3D.
    if (!preserves3D()) {
        localMatrix.setM13(0);
        localMatrix.setM23(0);
        localMatrix.setM31(0);
        localMatrix.setM32(0);
        localMatrix.setM33(1);
        localMatrix.setM34(0);
        localMatrix.setM43(0);
    }

    // now apply it to our children

    TransformationMatrix childMatrix;
    childMatrix = localMatrix;
    childMatrix.translate3d(m_offset.x(), m_offset.y(), 0);
    if (!m_childrenTransform.isIdentity()) {
        childMatrix.translate(getSize().width() * 0.5f, getSize().height() * 0.5f);
        childMatrix.multiply(m_childrenTransform);
        childMatrix.translate(-getSize().width() * 0.5f, -getSize().height() * 0.5f);
    }
    for (int i = 0; i < count; i++)
        this->getChild(i)->updateGLPositionsAndScale(childMatrix, drawClip(), opacity, scale);
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

bool LayerAndroid::needsTexture()
{
    return m_content && !m_content->isEmpty();
}

IntRect LayerAndroid::clippedRect() const
{
    IntRect r(0, 0, getWidth(), getHeight());
    IntRect tr = m_drawTransform.mapRect(r);
    IntRect cr = TilesManager::instance()->shader()->clippedRectWithViewport(tr);
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
        IntRect documentViewport(TilesManager::instance()->shader()->documentViewport());
        ALOGD("documentViewport(%d, %d, %d, %d)",
              documentViewport.x(), documentViewport.y(),
              documentViewport.width(), documentViewport.height());
    }

    IntRect r(0, 0, getWidth(), getHeight());
    IntRect tr = m_drawTransform.mapRect(r);
    IntRect visible = visibleArea();
    IntRect clip(m_clippingRect.x(), m_clippingRect.y(),
                 m_clippingRect.width(), m_clippingRect.height());
    ALOGD("%s %s (%d) [%d:0x%x] - %s %s - area (%d, %d, %d, %d) - visible (%d, %d, %d, %d) "
          "clip (%d, %d, %d, %d) %s %s m_content(%x), pic w: %d h: %d",
          spaces, subclassName().latin1().data(), subclassType(), uniqueId(), m_owningLayer,
          needsTexture() ? "needs a texture" : "no texture",
          m_imageCRC ? "has an image" : "no image",
          tr.x(), tr.y(), tr.width(), tr.height(),
          visible.x(), visible.y(), visible.width(), visible.height(),
          clip.x(), clip.y(), clip.width(), clip.height(),
          contentIsScrollable() ? "SCROLLABLE" : "",
          isPositionFixed() ? "FIXED" : "",
          m_content,
          m_content ? m_content->width() : -1,
          m_content ? m_content->height() : -1);

    int count = this->countChildren();
    for (int i = 0; i < count; i++)
        this->getChild(i)->showLayer(indent + 1);
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

    // isolate non-tiled layers
    // TODO: remove this check so that multiple tiled layers with a invisible
    // one inbetween can be merged
    if (!needsTexture() || !lastLayer->needsTexture())
        return false;

    // isolate clipped layers
    // TODO: paint correctly with clip when merged
    if (m_haveClip || lastLayer->m_haveClip)
        return false;

    // isolate intrinsically composited layers
    if (m_intrinsicallyComposited || lastLayer->m_intrinsicallyComposited)
        return false;

    // TODO: investigate potential for combining transformed layers
    if (!m_drawTransform.isIdentityOrTranslation()
        || !lastLayer->m_drawTransform.isIdentityOrTranslation())
        return false;

    // currently, we don't surface zoomable with non-zoomable layers (unless the
    // surface or the layer doesn't need a texture)
    if (surface->needsTexture() && needsTexture() && m_content->hasText() != surface->hasText())
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
    ALOGD("%*slayer %p(%d) rl %p %s surface %p, fixed %d, anim %d, intCom %d, haveClip %d scroll %d",
          4*mergeState->depth, "", this, m_uniqueId, m_owningLayer,
          needNewSurface ? "NEW" : "joins", mergeState->currentSurface,
          isPositionFixed(), m_animations.size() != 0,
          m_intrinsicallyComposited,
          m_haveClip,
          contentIsScrollable());
#endif

    mergeState->currentSurface->addLayer(this, m_drawTransform);
    m_surface = mergeState->currentSurface;

    if (m_haveClip || contentIsScrollable() || isPositionFixed()) {
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

    if (m_haveClip || contentIsScrollable() || isPositionFixed()) {
        // re-enable joining
        mergeState->nonMergeNestedLevel--;

        // disallow layers painting after to join with this surface
        mergeState->currentSurface = 0;
    }
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

IntRect LayerAndroid::unclippedArea()
{
    IntRect area;
    area.setX(0);
    area.setY(0);
    area.setWidth(getSize().width());
    area.setHeight(getSize().height());
    return area;
}

IntRect LayerAndroid::visibleArea()
{
    IntRect area = unclippedArea();
    // First, we get the transformed area of the layer,
    // in document coordinates
    IntRect rect = m_drawTransform.mapRect(area);
    int dx = rect.x();
    int dy = rect.y();

    // Then we apply the clipping
    IntRect clip(m_clippingRect);
    rect.intersect(clip);

    // Now clip with the viewport in documents coordinate
    IntRect documentViewport(TilesManager::instance()->shader()->documentViewport());
    rect.intersect(documentViewport);

    // Finally, let's return the visible area, in layers coordinate
    rect.move(-dx, -dy);
    return rect;
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
        canvas->clipRect(r);
        SkMatrix matrix;
        GLUtils::toSkMatrix(matrix, m_drawTransform);
        SkMatrix canvasMatrix = canvas->getTotalMatrix();
        matrix.postConcat(canvasMatrix);
        canvas->setMatrix(matrix);
        onDraw(canvas, m_drawOpacity, 0, style);
    }

    if (!drawChildren)
        return false;

    // When the layer is dirty, the UI thread should be notified to redraw.
    askScreenUpdate |= drawChildrenCanvas(canvas, style);
    m_atomicSync.lock();
    if (askScreenUpdate || m_hasRunningAnimations || m_drawTransform.hasPerspective())
        addDirtyArea();

    m_atomicSync.unlock();
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

    m_atomicSync.lock();
    if (m_hasRunningAnimations || m_drawTransform.hasPerspective()) {
        askScreenUpdate = true;
        addDirtyArea();
    }

    m_atomicSync.unlock();
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
    if (m_content)
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

void LayerAndroid::dumpLayer(FILE* file, int indentLevel) const
{
    writeHexVal(file, indentLevel + 1, "layer", (int)this);
    writeIntVal(file, indentLevel + 1, "layerId", m_uniqueId);
    writeIntVal(file, indentLevel + 1, "haveClip", m_haveClip);
    writeIntVal(file, indentLevel + 1, "isFixed", isPositionFixed());

    writeFloatVal(file, indentLevel + 1, "opacity", getOpacity());
    writeSize(file, indentLevel + 1, "size", getSize());
    writePoint(file, indentLevel + 1, "position", getPosition());
    writePoint(file, indentLevel + 1, "anchor", getAnchorPoint());

    writeMatrix(file, indentLevel + 1, "drawMatrix", m_drawTransform);
    writeMatrix(file, indentLevel + 1, "transformMatrix", m_transform);
    writeRect(file, indentLevel + 1, "clippingRect", SkRect(m_clippingRect));

    if (m_content) {
        writeIntVal(file, indentLevel + 1, "m_content.width", m_content->width());
        writeIntVal(file, indentLevel + 1, "m_content.height", m_content->height());
    }

    if (m_fixedPosition)
        return m_fixedPosition->dumpLayer(file, indentLevel);
}

void LayerAndroid::dumpLayers(FILE* file, int indentLevel) const
{
    writeln(file, indentLevel, "{");

    dumpLayer(file, indentLevel);

    if (countChildren()) {
        writeln(file, indentLevel + 1, "children = [");
        for (int i = 0; i < countChildren(); i++) {
            if (i > 0)
                writeln(file, indentLevel + 1, ", ");
            getChild(i)->dumpLayers(file, indentLevel + 1);
        }
        writeln(file, indentLevel + 1, "];");
    }
    writeln(file, indentLevel, "}");
}

void LayerAndroid::dumpToLog() const
{
    FILE* file = fopen("/data/data/com.android.browser/layertmp", "w");
    dumpLayers(file, 0);
    fclose(file);
    file = fopen("/data/data/com.android.browser/layertmp", "r");
    char buffer[512];
    bzero(buffer, sizeof(buffer));
    while (fgets(buffer, sizeof(buffer), file))
        SkDebugf("%s", buffer);
    fclose(file);
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
