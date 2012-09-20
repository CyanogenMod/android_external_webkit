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

#define LOG_TAG "Surface"
#define LOG_NDEBUG 1

#include "config.h"
#include "Surface.h"

#include "AndroidLog.h"
#include "BaseLayerAndroid.h"
#include "ClassTracker.h"
#include "LayerAndroid.h"
#include "LayerContent.h"
#include "GLWebViewState.h"
#include "PrerenderedInval.h"
#include "SkCanvas.h"
#include "SurfaceBacking.h"
#include "Tile.h"
#include "TileTexture.h"
#include "TilesManager.h"

#include <wtf/text/CString.h>

// Surfaces with an area larger than 2048*2048 should never be unclipped
#define MAX_FULL_CONTENT_AREA 4194304

namespace WebCore {

Surface::Surface()
    : m_surfaceBacking(0)
    , m_needsTexture(false)
    , m_maxZoomScale(1)
{
#ifdef DEBUG_COUNT
    ClassTracker::instance()->increment("Surface");
#endif
}

Surface::~Surface()
{
    for (unsigned int i = 0; i < m_layers.size(); i++)
        SkSafeUnref(m_layers[i]);
    if (m_surfaceBacking)
        SkSafeUnref(m_surfaceBacking);
#ifdef DEBUG_COUNT
    ClassTracker::instance()->decrement("Surface");
#endif
}

bool Surface::tryUpdateSurface(Surface* oldSurface)
{
    if (!needsTexture() || !oldSurface->needsTexture())
        return false;

    // merge surfaces based on first layer ID
    if (getFirstLayer()->uniqueId() != oldSurface->getFirstLayer()->uniqueId())
        return false;

    m_surfaceBacking = oldSurface->m_surfaceBacking;
    SkSafeRef(m_surfaceBacking);

    ALOGV("%p taking old SurfBack %p from surface %p, nt %d",
          this, m_surfaceBacking, oldSurface, oldSurface->needsTexture());

    if (!m_surfaceBacking) {
        // no SurfBack to inval, so don't worry about it.
        return true;
    }

    SkRegion invalRegion;
    bool fullInval = false;
    if (singleLayer() && oldSurface->singleLayer()) {
        // both are single matching layers, simply apply inval
        SkRegion* layerInval = getFirstLayer()->getInvalRegion();
        invalRegion = *layerInval;

        if (isBase()) {
            // the base layer paints outside it's content area to ensure the
            // viewport is convered, so fully invalidate all tiles if its size
            // changes to ensure no stale content remains
            LayerContent* newContent = getFirstLayer()->content();
            LayerContent* oldContent = oldSurface->getFirstLayer()->content();
            fullInval = newContent->width() != oldContent->width()
                || newContent->height() != oldContent->height();
        }
    } else {
        fullInval = m_layers.size() != oldSurface->m_layers.size();
        if (!fullInval) {
            for (unsigned int i = 0; i < m_layers.size(); i++) {
                if ((m_layers[i]->uniqueId() != oldSurface->m_layers[i]->uniqueId())
                    || (m_layers[i]->fullContentAreaMapped() != oldSurface->m_layers[i]->fullContentAreaMapped())) {
                    // layer list has changed, fully invalidate
                    // TODO: partially invalidate based on layer size/position
                    fullInval = true;
                    break;
                } else if (!m_layers[i]->getInvalRegion()->isEmpty()) {
                    // merge layer inval - translate the layer's inval region into surface coordinates
                    // TODO: handle scale/3d transform mapping
                    FloatRect layerPos = m_layers[i]->fullContentAreaMapped();
                    m_layers[i]->getInvalRegion()->translate(layerPos.x(), layerPos.y());
                    invalRegion.op(*(m_layers[i]->getInvalRegion()), SkRegion::kUnion_Op);
                }
            }
        }
    }

    if (fullInval)
        invalRegion.setRect(-1e8, -1e8, 2e8, 2e8);

    m_surfaceBacking->markAsDirty(invalRegion);
    return true;
}

void Surface::addLayer(LayerAndroid* layer, const TransformationMatrix& transform)
{
    m_layers.append(layer);
    SkSafeRef(layer);

    m_needsTexture |= layer->needsTexture();
    m_maxZoomScale = std::max(m_maxZoomScale, layer->maxZoomScale());

    // add this layer's size to the surface's area
    // TODO: handle scale/3d transform mapping
    IntRect rect = enclosingIntRect(layer->fullContentAreaMapped());

    if (layer->needsTexture()) {
        if (m_fullContentArea.isEmpty()) {
            m_drawTransform = transform;
            m_drawTransform.translate3d(-rect.x(), -rect.y(), 0);
            m_fullContentArea = rect;
        } else
            m_fullContentArea.unite(rect);
        ALOGV("Surf %p adding LA %p, size " INT_RECT_FORMAT
              " now fullContentArea " INT_RECT_FORMAT,
              this, layer, INT_RECT_ARGS(rect), INT_RECT_ARGS(m_fullContentArea));
    }

    if (isBase())
        m_background = static_cast<BaseLayerAndroid*>(layer)->getBackgroundColor();
}

IntRect Surface::visibleContentArea(bool force3dContentVisible) const
{
    if (singleLayer())
        return getFirstLayer()->visibleContentArea(force3dContentVisible);

    IntRect rect = m_fullContentArea;

    // clip with the viewport in content coordinate
    IntRect contentViewport(TilesManager::instance()->shader()->contentViewport());
    rect.intersect(contentViewport);

    // TODO: handle recursive layer clip

    return rect;
}

IntRect Surface::fullContentArea()
{
    if (singleLayer())
        return getFirstLayer()->fullContentArea();
    return m_fullContentArea;
}

bool Surface::useAggressiveRendering()
{
    // When the background is semi-opaque, 0 < alpha < 255, we had to turn off
    // low res to avoid artifacts from double drawing.
    // TODO: avoid double drawing for low res tiles.
    return isBase()
           && (!m_background.alpha()
           || !m_background.hasAlpha());
}

void Surface::prepareGL(bool layerTilesDisabled, bool updateWithBlit)
{
    bool tilesDisabled = layerTilesDisabled && !isBase();
    if (!m_surfaceBacking) {
        ALOGV("prepareGL on Surf %p, no SurfBack, needsTexture? %d",
              this, m_surfaceBacking, needsTexture());

        if (needsTexture() || (isBase() && layerTilesDisabled))
            m_surfaceBacking = new SurfaceBacking(isBase());
        else
            return;
    }

    if (tilesDisabled) {
        m_surfaceBacking->discardTextures();
    } else {
        IntRect prepareArea = computePrepareArea();
        IntRect fullArea = fullContentArea();

        ALOGV("prepareGL on Surf %p with SurfBack %p, %d layers, first layer %s (%d) "
              "prepareArea(%d, %d - %d x %d) fullArea(%d, %d - %d x %d)",
              this, m_surfaceBacking, m_layers.size(),
              getFirstLayer()->subclassName(),
              getFirstLayer()->uniqueId(),
              prepareArea.x(), prepareArea.y(), prepareArea.width(), prepareArea.height(),
              fullArea.x(), fullArea.y(), fullArea.width(), fullArea.height());

        m_surfaceBacking->prepareGL(getFirstLayer()->state(), m_maxZoomScale,
                                    prepareArea, fullArea,
                                    this, useAggressiveRendering(), updateWithBlit);
    }
    for (size_t i = 0; i < m_layers.size(); i++) {
        LayerContent* content = m_layers[i]->content();
        if (content)
            content->clearPrerenders();
    }
}

bool Surface::drawGL(bool layerTilesDisabled)
{
    bool tilesDisabled = layerTilesDisabled && !isBase();
    if (singleLayer() && !getFirstLayer()->visible())
        return false;

    if (!isBase()) {
        FloatRect drawClip = getFirstLayer()->drawClip();
        if (!singleLayer()) {
            for (unsigned int i = 1; i < m_layers.size(); i++)
                drawClip.unite(m_layers[i]->drawClip());
        }
        FloatRect clippingRect = TilesManager::instance()->shader()->rectInInvViewCoord(drawClip);
        TilesManager::instance()->shader()->clip(clippingRect);
    }

    bool askRedraw = false;
    if (m_surfaceBacking && !tilesDisabled) {
        ALOGV("drawGL on Surf %p with SurfBack %p, first layer %s (%d)", this, m_surfaceBacking,
              getFirstLayer()->subclassName(), getFirstLayer()->uniqueId());

        bool force3dContentVisible = true;
        IntRect drawArea = visibleContentArea(force3dContentVisible);
        m_surfaceBacking->drawGL(drawArea, opacity(), drawTransform(),
                                 useAggressiveRendering(), background());
    }

    // draw member layers (draws image textures, glextras)
    for (unsigned int i = 0; i < m_layers.size(); i++) {
        if (m_layers[i]->drawGL(tilesDisabled)) {
           m_layers[i]->addDirtyArea();
           askRedraw = true;
        }
    }

    return askRedraw;
}

void Surface::swapTiles(bool calculateFrameworkInvals)
{
    if (!m_surfaceBacking)
        return;

    if (m_surfaceBacking->swapTiles() && calculateFrameworkInvals)
        addFrameworkInvals();
}

void Surface::addFrameworkInvals()
{
    // Let's return an inval area to framework that will
    // contain all of our layers' areas
    for (unsigned int i = 0; i < m_layers.size(); i++)
        m_layers[i]->addDirtyArea();
}

bool Surface::isReady()
{
    if (!m_surfaceBacking)
        return true;

    return m_surfaceBacking->isReady();
}

bool Surface::isMissingContent()
{
    if (!m_surfaceBacking)
        return true;

    return m_surfaceBacking->isMissingContent();
}

bool Surface::canUpdateWithBlit()
{
    // If we don't have a texture, we have nothing to update and thus can take
    // the fast path
    if (!needsTexture())
        return true;
    // If we have a surface backing that isn't ready, we can't update with a blit
    // If it is ready, then check to see if it is dirty. We can only call isDirty()
    // if isReady() returns true
    if (!m_surfaceBacking)
        return false;
    if (!m_surfaceBacking->isReady())
        return false;
    if (!m_surfaceBacking->isDirty())
        return true;
    if (!singleLayer())
        return false;
    return getFirstLayer()->canUpdateWithBlit();
}

IntRect Surface::computePrepareArea()
{
    IntRect area;

    if (!getFirstLayer()->contentIsScrollable()
        && !isBase()
        && getFirstLayer()->state()->layersRenderingMode() == GLWebViewState::kAllTextures) {

        area = fullContentArea();

        double total = ((double) area.width()) * ((double) area.height());
        if (total > MAX_FULL_CONTENT_AREA)
            area = visibleContentArea();
    } else
        area = visibleContentArea();

    return area;
}

void Surface::computeTexturesAmount(TexturesResult* result)
{
    if (!m_surfaceBacking || isBase())
        return;


    LayerAndroid* layer = 0;
    if (singleLayer())
        layer = getFirstLayer();

    m_surfaceBacking->computeTexturesAmount(result, visibleContentArea(),
                                            fullContentArea(), layer);
}

bool Surface::isBase()
{
    // base layer surface
    // - doesn't use layer tiles (disables blending, doesn't compute textures amount)
    // - ignores clip rects
    // - only prepares clippedArea
    return getFirstLayer()->subclassType() == LayerAndroid::BaseLayer;
}

bool Surface::paint(SkCanvas* canvas)
{
    if (singleLayer()) {
        getFirstLayer()->contentDraw(canvas, Layer::UnmergedLayers);

        // TODO: double buffer by disabling SurfaceCollection swaps and position
        // updates until painting complete

        // In single surface mode, draw layer content onto the base layer
        if (isBase()
            && getFirstLayer()->countChildren()
            && getFirstLayer()->state()->isSingleSurfaceRenderingMode()) {
            for (int i = 0; i < getFirstLayer()->countChildren(); i++)
                getFirstLayer()->getChild(i)->drawCanvas(canvas, true, Layer::FlattenedLayers);
        }
    } else {
        SkAutoCanvasRestore acr(canvas, true);
        SkMatrix matrix;
        GLUtils::toSkMatrix(matrix, m_drawTransform);

        SkMatrix inverse;
        inverse.reset();
        matrix.invert(&inverse);

        SkMatrix canvasMatrix = canvas->getTotalMatrix();
        inverse.postConcat(canvasMatrix);
        canvas->setMatrix(inverse);

        for (unsigned int i=0; i<m_layers.size(); i++)
            m_layers[i]->drawCanvas(canvas, false, Layer::MergedLayers);
    }
    return true;
}

float Surface::opacity()
{
    if (singleLayer())
        return getFirstLayer()->drawOpacity();
    return 1.0;
}

Color* Surface::background()
{
    if (!isBase() || !m_background.isValid())
        return 0;
    return &m_background;
}

bool Surface::blitFromContents(Tile* tile)
{
    if (!singleLayer() || !tile || !getFirstLayer() || !getFirstLayer()->content())
        return false;
    LayerContent* content = getFirstLayer()->content();
    // Extract the dirty rect from the region. Note that this is *NOT* constrained
    // to this tile
    IntRect dirtyRect = tile->dirtyArea().getBounds();
    IntRect tileRect = IntRect(tile->x() * TilesManager::tileWidth(),
                               tile->y() * TilesManager::tileHeight(),
                               TilesManager::tileWidth(),
                               TilesManager::tileHeight());
    FloatRect tileRectInDoc = tileRect;
    tileRectInDoc.scale(1 / tile->scale());
    dirtyRect.intersect(enclosingIntRect(tileRectInDoc));
    PrerenderedInval* prerenderedInval = content->prerenderForRect(dirtyRect);
    if (!prerenderedInval || prerenderedInval->bitmap.isNull())
        return false;
    SkBitmap sourceBitmap = prerenderedInval->bitmap;
    // Calculate the screen rect that is dirty, then intersect it with the
    // tile's screen rect so that we end up with the pixels we need to blit
    FloatRect screenDirty = dirtyRect;
    screenDirty.scale(tile->scale());
    IntRect enclosingScreenDirty = enclosingIntRect(screenDirty);
    enclosingScreenDirty.intersect(tileRect);
    if (enclosingScreenDirty.isEmpty())
        return false;
    // Make sure the screen area we want to blit is contained by the
    // prerendered screen area
    if (!prerenderedInval->screenArea.contains(enclosingScreenDirty)) {
        ALOGD("prerendered->screenArea " INT_RECT_FORMAT " doesn't contain "
                "enclosingScreenDirty " INT_RECT_FORMAT,
                INT_RECT_ARGS(prerenderedInval->screenArea),
                INT_RECT_ARGS(enclosingScreenDirty));
        return false;
    }
    IntPoint origin = prerenderedInval->screenArea.location();
    SkBitmap subset;
    subset.setConfig(sourceBitmap.config(), enclosingScreenDirty.width(),
            enclosingScreenDirty.height());
    subset.allocPixels();

    int topOffset = enclosingScreenDirty.y() - prerenderedInval->screenArea.y();
    int leftOffset = enclosingScreenDirty.x() - prerenderedInval->screenArea.x();
    if (!GLUtils::deepCopyBitmapSubset(sourceBitmap, subset, leftOffset, topOffset))
        return false;
    // Now upload
    SkIRect textureInval = SkIRect::MakeXYWH(enclosingScreenDirty.x() - tileRect.x(),
                                             enclosingScreenDirty.y() - tileRect.y(),
                                             enclosingScreenDirty.width(),
                                             enclosingScreenDirty.height());
    GLUtils::updateTextureWithBitmap(tile->frontTexture()->m_ownTextureId,
                                     subset, textureInval);
    tile->onBlitUpdate();
    return true;
}

const TransformationMatrix* Surface::drawTransform()
{
    // single layer surfaces query the layer's draw transform, while multi-layer
    // surfaces copy the draw transform once, during initialization
    // TODO: support fixed multi-layer surfaces by querying the changing drawTransform
    if (singleLayer())
        return getFirstLayer()->drawTransform();

    return &m_drawTransform;
}

} // namespace WebCore
