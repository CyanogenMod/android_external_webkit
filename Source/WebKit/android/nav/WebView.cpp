/*
 * Copyright 2007, The Android Open Source Project
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

#define LOG_TAG "webviewglue"

#include "config.h"

#include "AndroidAnimation.h"
#include "AndroidLog.h"
#include "BaseLayerAndroid.h"
#include "BaseRenderer.h"
#include "DrawExtra.h"
#include "DumpLayer.h"
#include "Frame.h"
#include "GLWebViewState.h"
#include "GraphicsJNI.h"
#include "HTMLInputElement.h"
#include "IntPoint.h"
#include "IntRect.h"
#include "LayerAndroid.h"
#include "LayerContent.h"
#include "Node.h"
#include "utils/Functor.h"
#include "private/hwui/DrawGlInfo.h"
#include "PlatformGraphicsContext.h"
#include "PlatformString.h"
#include "ScrollableLayerAndroid.h"
#include "SelectText.h"
#include "SkCanvas.h"
#include "SkDumpCanvas.h"
#include "SkPicture.h"
#include "SkRect.h"
#include "SkTime.h"
#include "TilesManager.h"
#include "TransferQueue.h"
#include "WebCoreJni.h"
#include "WebRequestContext.h"
#include "WebViewCore.h"

#ifdef GET_NATIVE_VIEW
#undef GET_NATIVE_VIEW
#endif

#define GET_NATIVE_VIEW(env, obj) ((WebView*)env->GetIntField(obj, gWebViewField))

#include <JNIUtility.h>
#include <JNIHelp.h>
#include <jni.h>
#include <androidfw/KeycodeLabels.h>
#include <wtf/text/AtomicString.h>
#include <wtf/text/CString.h>

// Free as much as we possible can
#define TRIM_MEMORY_COMPLETE 80
// Free a lot (all textures gone)
#define TRIM_MEMORY_MODERATE 60
// More moderate free (keep bare minimum to restore quickly-ish - possibly clear all textures)
#define TRIM_MEMORY_BACKGROUND 40
// Moderate free (clear cached tiles, keep visible ones)
#define TRIM_MEMORY_UI_HIDDEN 20
// Duration to show the pressed cursor ring
#define PRESSED_STATE_DURATION 400

namespace android {

static jfieldID gWebViewField;

//-------------------------------------

static jmethodID GetJMethod(JNIEnv* env, jclass clazz, const char name[], const char signature[])
{
    jmethodID m = env->GetMethodID(clazz, name, signature);
    ALOG_ASSERT(m, "Could not find method %s", name);
    return m;
}

//-------------------------------------
// This class provides JNI for making calls into native code from the UI side
// of the multi-threaded WebView.
class WebView
{
public:
enum FrameCachePermission {
    DontAllowNewer,
    AllowNewer
};

#define DRAW_EXTRAS_SIZE 2
enum DrawExtras { // keep this in sync with WebView.java
    DrawExtrasNone = 0,
    DrawExtrasSelection = 1,
    DrawExtrasCursorRing = 2
};

struct JavaGlue {
    jweak       m_obj;
    jmethodID   m_scrollBy;
    jmethodID   m_getScaledMaxXScroll;
    jmethodID   m_getScaledMaxYScroll;
    jmethodID   m_updateRectsForGL;
    jmethodID   m_viewInvalidate;
    jmethodID   m_viewInvalidateRect;
    jmethodID   m_postInvalidateDelayed;
    jmethodID   m_pageSwapCallback;
    jfieldID    m_rectLeft;
    jfieldID    m_rectTop;
    jmethodID   m_rectWidth;
    jmethodID   m_rectHeight;
    jfieldID    m_quadFP1;
    jfieldID    m_quadFP2;
    jfieldID    m_quadFP3;
    jfieldID    m_quadFP4;
    AutoJObject object(JNIEnv* env) {
        return getRealObject(env, m_obj);
    }
} m_javaGlue;

WebView(JNIEnv* env, jobject javaWebView, int viewImpl, WTF::String drawableDir,
        bool isHighEndGfx)
    : m_isHighEndGfx(isHighEndGfx)
{
    memset(m_extras, 0, DRAW_EXTRAS_SIZE * sizeof(DrawExtra*));
    jclass clazz = env->FindClass("android/webkit/WebViewClassic");
    m_javaGlue.m_obj = env->NewWeakGlobalRef(javaWebView);
    m_javaGlue.m_scrollBy = GetJMethod(env, clazz, "setContentScrollBy", "(IIZ)Z");
    m_javaGlue.m_getScaledMaxXScroll = GetJMethod(env, clazz, "getScaledMaxXScroll", "()I");
    m_javaGlue.m_getScaledMaxYScroll = GetJMethod(env, clazz, "getScaledMaxYScroll", "()I");
    m_javaGlue.m_updateRectsForGL = GetJMethod(env, clazz, "updateRectsForGL", "()V");
    m_javaGlue.m_viewInvalidate = GetJMethod(env, clazz, "viewInvalidate", "()V");
    m_javaGlue.m_viewInvalidateRect = GetJMethod(env, clazz, "viewInvalidate", "(IIII)V");
    m_javaGlue.m_postInvalidateDelayed = GetJMethod(env, clazz,
        "viewInvalidateDelayed", "(JIIII)V");
    m_javaGlue.m_pageSwapCallback = GetJMethod(env, clazz, "pageSwapCallback", "(Z)V");
    env->DeleteLocalRef(clazz);

    jclass rectClass = env->FindClass("android/graphics/Rect");
    ALOG_ASSERT(rectClass, "Could not find Rect class");
    m_javaGlue.m_rectLeft = env->GetFieldID(rectClass, "left", "I");
    m_javaGlue.m_rectTop = env->GetFieldID(rectClass, "top", "I");
    m_javaGlue.m_rectWidth = GetJMethod(env, rectClass, "width", "()I");
    m_javaGlue.m_rectHeight = GetJMethod(env, rectClass, "height", "()I");
    env->DeleteLocalRef(rectClass);

    jclass quadFClass = env->FindClass("android/webkit/QuadF");
    ALOG_ASSERT(quadFClass, "Could not find QuadF class");
    m_javaGlue.m_quadFP1 = env->GetFieldID(quadFClass, "p1", "Landroid/graphics/PointF;");
    m_javaGlue.m_quadFP2 = env->GetFieldID(quadFClass, "p2", "Landroid/graphics/PointF;");
    m_javaGlue.m_quadFP3 = env->GetFieldID(quadFClass, "p3", "Landroid/graphics/PointF;");
    m_javaGlue.m_quadFP4 = env->GetFieldID(quadFClass, "p4", "Landroid/graphics/PointF;");
    env->DeleteLocalRef(quadFClass);

    env->SetIntField(javaWebView, gWebViewField, (jint)this);
    m_viewImpl = (WebViewCore*) viewImpl;
    m_generation = 0;
    m_heightCanMeasure = false;
    m_lastDx = 0;
    m_lastDxTime = 0;
    m_baseLayer = 0;
    m_glDrawFunctor = 0;
    m_isDrawingPaused = false;
#if USE(ACCELERATED_COMPOSITING)
    m_glWebViewState = 0;
#endif
}

~WebView()
{
    if (m_javaGlue.m_obj)
    {
        JNIEnv* env = JSC::Bindings::getJNIEnv();
        env->DeleteWeakGlobalRef(m_javaGlue.m_obj);
        m_javaGlue.m_obj = 0;
    }
#if USE(ACCELERATED_COMPOSITING)
    // We must remove the m_glWebViewState prior to deleting m_baseLayer. If we
    // do not remove it here, we risk having BaseTiles trying to paint using a
    // deallocated base layer.
    stopGL();
#endif
    SkSafeUnref(m_baseLayer);
    delete m_glDrawFunctor;
    for (int i = 0; i < DRAW_EXTRAS_SIZE; i++)
        delete m_extras[i];
}

DrawExtra* getDrawExtra(DrawExtras extras)
{
    if (extras == DrawExtrasNone)
        return 0;
    return m_extras[extras - 1];
}

void stopGL()
{
#if USE(ACCELERATED_COMPOSITING)
    delete m_glWebViewState;
    m_glWebViewState = 0;
#endif
}

WebViewCore* getWebViewCore() const {
    return m_viewImpl;
}

void scrollRectOnScreen(const IntRect& rect)
{
    if (rect.isEmpty())
        return;
    int dx = 0;
    int left = rect.x();
    int right = rect.maxX();
    if (left < m_visibleContentRect.fLeft)
        dx = left - m_visibleContentRect.fLeft;
    // Only scroll right if the entire width can fit on screen.
    else if (right > m_visibleContentRect.fRight
            && right - left < m_visibleContentRect.width())
        dx = right - m_visibleContentRect.fRight;
    int dy = 0;
    int top = rect.y();
    int bottom = rect.maxY();
    if (top < m_visibleContentRect.fTop)
        dy = top - m_visibleContentRect.fTop;
    // Only scroll down if the entire height can fit on screen
    else if (bottom > m_visibleContentRect.fBottom
            && bottom - top < m_visibleContentRect.height())
        dy = bottom - m_visibleContentRect.fBottom;
    if ((dx|dy) == 0 || !scrollBy(dx, dy))
        return;
    viewInvalidate();
}

int drawGL(WebCore::IntRect& invScreenRect, WebCore::IntRect* invalRect,
        WebCore::IntRect& screenRect, int titleBarHeight,
        WebCore::IntRect& screenClip, float scale, int extras, bool shouldDraw)
{
#if USE(ACCELERATED_COMPOSITING)
    if (!m_baseLayer)
        return 0;

    if (m_viewImpl)
        m_viewImpl->setPrerenderingEnabled(!m_isDrawingPaused);

    if (!m_glWebViewState) {
        TilesManager::instance()->setHighEndGfx(m_isHighEndGfx);
        m_glWebViewState = new GLWebViewState();
        m_glWebViewState->setBaseLayer(m_baseLayer, false, true);
    }

    DrawExtra* extra = getDrawExtra((DrawExtras) extras);

    m_glWebViewState->glExtras()->setDrawExtra(extra);

    // Make sure we have valid coordinates. We might not have valid coords
    // if the zoom manager is still initializing. We will be redrawn
    // once the correct scale is set
    if (!m_visibleContentRect.isFinite())
        return 0;
    bool treesSwapped = false;
    bool newTreeHasAnim = false;
    int ret = m_glWebViewState->drawGL(invScreenRect, m_visibleContentRect, invalRect,
                                        screenRect, titleBarHeight, screenClip, scale,
                                        &treesSwapped, &newTreeHasAnim, shouldDraw);
    if (treesSwapped) {
        ALOG_ASSERT(m_javaGlue.m_obj, "A java object was not associated with this native WebView!");
        JNIEnv* env = JSC::Bindings::getJNIEnv();
        AutoJObject javaObject = m_javaGlue.object(env);
        if (javaObject.get()) {
            env->CallVoidMethod(javaObject.get(), m_javaGlue.m_pageSwapCallback, newTreeHasAnim);
            checkException(env);
        }
    }
    return m_isDrawingPaused ? 0 : ret;
#endif
    return 0;
}

void draw(SkCanvas* canvas, SkColor bgColor, DrawExtras extras)
{
    if (!m_baseLayer) {
        canvas->drawColor(bgColor);
        return;
    }

    // draw the content of the base layer first
    LayerContent* content = m_baseLayer->content();
    int sc = canvas->save(SkCanvas::kClip_SaveFlag);
    if (content) {
        canvas->clipRect(SkRect::MakeLTRB(0, 0, content->width(), content->height()),
                         SkRegion::kDifference_Op);
    }
    Color c = m_baseLayer->getBackgroundColor();
    canvas->drawColor(SkColorSetARGBInline(c.alpha(), c.red(), c.green(), c.blue()));
    canvas->restoreToCount(sc);

    // call this to be sure we've adjusted for any scrolling or animations
    // before we actually draw
    m_baseLayer->updatePositionsRecursive(m_visibleContentRect);
    m_baseLayer->updatePositions();

    // We have to set the canvas' matrix on the base layer
    // (to have fixed layers work as intended)
    SkAutoCanvasRestore restore(canvas, true);
    m_baseLayer->setMatrix(canvas->getTotalMatrix());
    canvas->resetMatrix();
    m_baseLayer->draw(canvas, getDrawExtra(extras));
}

int getScaledMaxXScroll()
{
    ALOG_ASSERT(m_javaGlue.m_obj, "A java object was not associated with this native WebView!");
    JNIEnv* env = JSC::Bindings::getJNIEnv();
    AutoJObject javaObject = m_javaGlue.object(env);
    if (!javaObject.get())
        return 0;
    int result = env->CallIntMethod(javaObject.get(), m_javaGlue.m_getScaledMaxXScroll);
    checkException(env);
    return result;
}

int getScaledMaxYScroll()
{
    ALOG_ASSERT(m_javaGlue.m_obj, "A java object was not associated with this native WebView!");
    JNIEnv* env = JSC::Bindings::getJNIEnv();
    AutoJObject javaObject = m_javaGlue.object(env);
    if (!javaObject.get())
        return 0;
    int result = env->CallIntMethod(javaObject.get(), m_javaGlue.m_getScaledMaxYScroll);
    checkException(env);
    return result;
}

// Call through JNI to ask Java side to update the rectangles for GL functor.
// This is called at every draw when it is not in process mode, so we should
// keep this route as efficient as possible. Currently, its average cost on Xoom
// is about 0.1ms - 0.2ms.
// Alternatively, this can be achieved by adding more listener on Java side, but
// that will be more likely causing jank when triggering GC.
void updateRectsForGL()
{
    JNIEnv* env = JSC::Bindings::getJNIEnv();
    AutoJObject javaObject = m_javaGlue.object(env);
    if (!javaObject.get())
        return;
    env->CallVoidMethod(javaObject.get(), m_javaGlue.m_updateRectsForGL);
    checkException(env);
}

#if USE(ACCELERATED_COMPOSITING)
static const ScrollableLayerAndroid* findScrollableLayer(
    const LayerAndroid* parent, int x, int y, SkIRect* foundBounds) {
    IntRect bounds = enclosingIntRect(parent->fullContentAreaMapped());

    // Check the parent bounds first; this will clip to within a masking layer's
    // bounds.
    if (parent->masksToBounds() && !bounds.contains(x, y))
        return 0;

    int count = parent->countChildren();
    while (count--) {
        const LayerAndroid* child = parent->getChild(count);
        const ScrollableLayerAndroid* result = findScrollableLayer(child, x, y, foundBounds);
        if (result) {
            if (parent->masksToBounds()) {
                if (bounds.width() < foundBounds->width())
                    foundBounds->fRight = foundBounds->fLeft + bounds.width();
                if (bounds.height() < foundBounds->height())
                    foundBounds->fBottom = foundBounds->fTop + bounds.height();
            }
            return result;
        }
    }
    if (parent->contentIsScrollable()) {
        foundBounds->set(bounds.x(), bounds.y(), bounds.width(), bounds.height());
        return static_cast<const ScrollableLayerAndroid*>(parent);
    }
    return 0;
}
#endif

int scrollableLayer(int x, int y, SkIRect* layerRect, SkIRect* bounds)
{
#if USE(ACCELERATED_COMPOSITING)
    if (!m_baseLayer)
        return 0;
    const ScrollableLayerAndroid* result = findScrollableLayer(m_baseLayer, x, y, bounds);
    if (result) {
        result->getScrollRect(layerRect);
        return result->uniqueId();
    }
#endif
    return 0;
}

void scrollLayer(int layerId, int x, int y)
{
    if (m_glWebViewState)
        m_glWebViewState->scrollLayer(layerId, x, y);
}

void setHeightCanMeasure(bool measure)
{
    m_heightCanMeasure = measure;
}

String getSelection()
{
    SelectText* select = static_cast<SelectText*>(
            getDrawExtra(WebView::DrawExtrasSelection));
    if (select)
        return select->getText();
    return String();
}

bool scrollBy(int dx, int dy)
{
    ALOG_ASSERT(m_javaGlue.m_obj, "A java object was not associated with this native WebView!");

    JNIEnv* env = JSC::Bindings::getJNIEnv();
    AutoJObject javaObject = m_javaGlue.object(env);
    if (!javaObject.get())
        return false;
    bool result = env->CallBooleanMethod(javaObject.get(), m_javaGlue.m_scrollBy, dx, dy, true);
    checkException(env);
    return result;
}

void setIsScrolling(bool isScrolling)
{
#if USE(ACCELERATED_COMPOSITING)
    if (m_glWebViewState)
        m_glWebViewState->setIsScrolling(isScrolling);
#endif
}

void viewInvalidate()
{
    JNIEnv* env = JSC::Bindings::getJNIEnv();
    AutoJObject javaObject = m_javaGlue.object(env);
    if (!javaObject.get())
        return;
    env->CallVoidMethod(javaObject.get(), m_javaGlue.m_viewInvalidate);
    checkException(env);
}

void viewInvalidateRect(int l, int t, int r, int b)
{
    JNIEnv* env = JSC::Bindings::getJNIEnv();
    AutoJObject javaObject = m_javaGlue.object(env);
    if (!javaObject.get())
        return;
    env->CallVoidMethod(javaObject.get(), m_javaGlue.m_viewInvalidateRect, l, r, t, b);
    checkException(env);
}

void postInvalidateDelayed(int64_t delay, const WebCore::IntRect& bounds)
{
    JNIEnv* env = JSC::Bindings::getJNIEnv();
    AutoJObject javaObject = m_javaGlue.object(env);
    if (!javaObject.get())
        return;
    env->CallVoidMethod(javaObject.get(), m_javaGlue.m_postInvalidateDelayed,
        delay, bounds.x(), bounds.y(), bounds.maxX(), bounds.maxY());
    checkException(env);
}

#if ENABLE(ANDROID_OVERFLOW_SCROLL)
static void copyScrollPosition(const LayerAndroid* fromRoot,
                               LayerAndroid* toRoot, int layerId)
{
    if (!fromRoot || !toRoot)
        return;
    const LayerAndroid* from = fromRoot->findById(layerId);
    LayerAndroid* to = toRoot->findById(layerId);
    if (!from || !to || !from->contentIsScrollable() || !to->contentIsScrollable())
        return;
    // TODO: Support this for iframes.
    if (to->isIFrameContent() || from->isIFrameContent())
        return;
    to->setScrollOffset(from->getScrollOffset());
}
#endif

BaseLayerAndroid* getBaseLayer() const { return m_baseLayer; }

bool setBaseLayer(BaseLayerAndroid* newBaseLayer, bool showVisualIndicator,
                  bool isPictureAfterFirstLayout, int scrollingLayer)
{
    bool queueFull = false;
#if USE(ACCELERATED_COMPOSITING)
    if (m_glWebViewState)
        queueFull = m_glWebViewState->setBaseLayer(newBaseLayer, showVisualIndicator,
                                                   isPictureAfterFirstLayout);
#endif

#if ENABLE(ANDROID_OVERFLOW_SCROLL)
    copyScrollPosition(m_baseLayer, newBaseLayer, scrollingLayer);
#endif
    SkSafeUnref(m_baseLayer);
    m_baseLayer = newBaseLayer;

    return queueFull;
}

void copyBaseContentToPicture(SkPicture* picture)
{
    if (!m_baseLayer || !m_baseLayer->content())
        return;
    LayerContent* content = m_baseLayer->content();
    SkCanvas* canvas = picture->beginRecording(content->width(), content->height(),
                                              SkPicture::kUsePathBoundsForClip_RecordingFlag);

    // clear the BaseLayerAndroid's previous matrix (set at each draw)
    SkMatrix baseMatrix;
    baseMatrix.reset();
    m_baseLayer->setMatrix(baseMatrix);

    m_baseLayer->draw(canvas, 0);

    picture->endRecording();
}

bool hasContent() {
    if (!m_baseLayer || !m_baseLayer->content())
        return false;
    return !m_baseLayer->content()->isEmpty();
}

void setFunctor(Functor* functor) {
    delete m_glDrawFunctor;
    m_glDrawFunctor = functor;
}

Functor* getFunctor() {
    return m_glDrawFunctor;
}

void setVisibleContentRect(SkRect& visibleContentRect) {
    m_visibleContentRect = visibleContentRect;
}

void setDrawExtra(DrawExtra *extra, DrawExtras type)
{
    if (type == DrawExtrasNone)
        return;
    DrawExtra* old = m_extras[type - 1];
    m_extras[type - 1] = extra;
    if (old != extra) {
        delete old;
    }
}

void setTextSelection(SelectText *selection) {
    setDrawExtra(selection, DrawExtrasSelection);
}

const TransformationMatrix* getLayerTransform(int layerId) {
    if (layerId != -1 && m_baseLayer) {
        LayerAndroid* layer = m_baseLayer->findById(layerId);
        // We need to make sure the drawTransform is up to date as this is
        // called before a draw() or drawGL()
        if (layer) {
            m_baseLayer->updatePositionsRecursive(m_visibleContentRect);
            return layer->drawTransform();
        }
    }
    return 0;
}

int getHandleLayerId(SelectText::HandleId handleId, SkIPoint& cursorPoint,
        FloatQuad& textBounds) {
    SelectText* selectText = static_cast<SelectText*>(getDrawExtra(DrawExtrasSelection));
    if (!selectText || !m_baseLayer)
        return -1;
    int layerId = selectText->caretLayerId(handleId);
    IntRect cursorRect = selectText->caretRect(handleId);
    IntRect textRect = selectText->textRect(handleId);
    // Rects exclude the last pixel on right/bottom. We want only included pixels.
    cursorPoint.set(cursorRect.x(), cursorRect.maxY() - 1);
    textRect.setHeight(std::max(1, textRect.height() - 1));
    textRect.setWidth(std::max(1, textRect.width() - 1));
    textBounds = FloatQuad(textRect);

    const TransformationMatrix* transform = getLayerTransform(layerId);
    if (transform) {
        // We're overloading the concept of Rect to be just the two
        // points (bottom-left and top-right.
        cursorPoint = transform->mapPoint(cursorPoint);
        textBounds = transform->mapQuad(textBounds);
    }
    return layerId;
}

void mapLayerRect(int layerId, SkIRect& rect) {
    const TransformationMatrix* transform = getLayerTransform(layerId);
    if (transform)
        rect = transform->mapRect(rect);
}

void floatQuadToQuadF(JNIEnv* env, const FloatQuad& nativeTextQuad,
        jobject textQuad)
{
    jobject p1 = env->GetObjectField(textQuad, m_javaGlue.m_quadFP1);
    jobject p2 = env->GetObjectField(textQuad, m_javaGlue.m_quadFP2);
    jobject p3 = env->GetObjectField(textQuad, m_javaGlue.m_quadFP3);
    jobject p4 = env->GetObjectField(textQuad, m_javaGlue.m_quadFP4);
    GraphicsJNI::point_to_jpointf(nativeTextQuad.p1(), env, p1);
    GraphicsJNI::point_to_jpointf(nativeTextQuad.p2(), env, p2);
    GraphicsJNI::point_to_jpointf(nativeTextQuad.p3(), env, p3);
    GraphicsJNI::point_to_jpointf(nativeTextQuad.p4(), env, p4);
    env->DeleteLocalRef(p1);
    env->DeleteLocalRef(p2);
    env->DeleteLocalRef(p3);
    env->DeleteLocalRef(p4);
}

// This is called when WebView switches rendering modes in a more permanent fashion
// such as when the layer type is set or the view is attached/detached from the window
int setHwAccelerated(bool hwAccelerated) {
    if (!m_glWebViewState)
        return 0;
    LayerAndroid* root = m_baseLayer;
    if (root)
        return root->setHwAccelerated(hwAccelerated);
    return 0;
}

void setDrawingPaused(bool isPaused)
{
    m_isDrawingPaused = isPaused;
    if (m_viewImpl)
        m_viewImpl->setPrerenderingEnabled(!isPaused);
}

// Finds the rectangles within world to the left, right, top, and bottom
// of rect and adds them to rects. If no intersection exists, false is returned.
static bool findMaskedRects(const FloatRect& world,
        const FloatRect& rect, Vector<FloatRect>& rects) {
    if (!world.intersects(rect))
        return false; // nothing to subtract

    // left rectangle
    if (rect.x() > world.x())
        rects.append(FloatRect(world.x(), world.y(),
                rect.x() - world.x(), world.height()));
    // top rectangle
    if (rect.y() > world.y())
        rects.append(FloatRect(world.x(), world.y(),
                world.width(), rect.y() - world.y()));
    // right rectangle
    if (rect.maxX() < world.maxX())
        rects.append(FloatRect(rect.maxX(), world.y(),
                world.maxX() - rect.maxX(), world.height()));
    // bottom rectangle
    if (rect.maxY() < world.maxY())
        rects.append(FloatRect(world.x(), rect.maxY(),
                world.width(), world.maxY() - rect.maxY()));
    return true;
}

// Returns false if layerId is a fixed position layer, otherwise
// all fixed position layer rectangles are subtracted from those within
// rects. Rects will be modified to contain rectangles that don't include
// the fixed position layer rectangles.
static bool findMaskedRectsForLayer(LayerAndroid* layer,
        Vector<FloatRect>& rects, int layerId)
{
    if (layer->isPositionFixed()) {
        if (layerId == layer->uniqueId())
            return false;
        FloatRect layerRect = layer->fullContentAreaMapped();
        for (int i = rects.size() - 1; i >= 0; i--)
            if (findMaskedRects(rects[i], layerRect, rects))
                rects.remove(i);
    }

    int childIndex = 0;
    while (LayerAndroid* child = layer->getChild(childIndex++))
        if (!findMaskedRectsForLayer(child, rects, layerId))
            return false;

    return true;
}

// Finds the largest rectangle not masked by any fixed layer.
void findMaxVisibleRect(int movingLayerId, SkIRect& visibleContentRect)
{
    if (!m_baseLayer)
        return;

    FloatRect visibleContentFloatRect(visibleContentRect);
    m_baseLayer->updatePositionsRecursive(visibleContentFloatRect);
    Vector<FloatRect> rects;
    rects.append(visibleContentFloatRect);
    if (findMaskedRectsForLayer(m_baseLayer, rects, movingLayerId)) {
        float maxSize = 0.0;
        const FloatRect* largest = 0;
        for (unsigned int i = 0; i < rects.size(); i++) {
            const FloatRect& rect = rects[i];
            float size = rect.width() * rect.height();
            if (size > maxSize) {
                maxSize = size;
                largest = &rect;
            }
        }
        if (largest) {
            SkRect largeRect = *largest;
            largeRect.round(&visibleContentRect);
        }
    }
}

bool isHandleLeft(SelectText::HandleId handleId)
{
    SelectText* selectText = static_cast<SelectText*>(getDrawExtra(DrawExtrasSelection));
    if (!selectText)
        return (handleId == SelectText::BaseHandle);

    return (selectText->getHandleType(handleId) == SelectText::LeftHandle);
}

bool isPointVisible(int layerId, int contentX, int contentY)
{
    bool isVisible = true;
    const TransformationMatrix* transform = getLayerTransform(layerId);
    if (transform) {
        // layer is guaranteed to be non-NULL because of getLayerTransform
        LayerAndroid* layer = m_baseLayer->findById(layerId);
        IntRect rect = layer->visibleContentArea();
        rect = transform->mapRect(rect);
        isVisible = rect.contains(contentX, contentY);
    }
    return isVisible;
}

private: // local state for WebView
    bool m_isDrawingPaused;
    // private to getFrameCache(); other functions operate in a different thread
    WebViewCore* m_viewImpl;
    int m_generation; // associate unique ID with sent kit focus to match with ui
    // Corresponds to the same-named boolean on the java side.
    bool m_heightCanMeasure;
    int m_lastDx;
    SkMSec m_lastDxTime;
    DrawExtra* m_extras[DRAW_EXTRAS_SIZE];
    BaseLayerAndroid* m_baseLayer;
    Functor* m_glDrawFunctor;
#if USE(ACCELERATED_COMPOSITING)
    GLWebViewState* m_glWebViewState;
#endif
    SkRect m_visibleContentRect;
    bool m_isHighEndGfx;
}; // end of WebView class


/**
 * This class holds a function pointer and parameters for calling drawGL into a specific
 * viewport. The pointer to the Functor will be put on a framework display list to be called
 * when the display list is replayed.
 */
class GLDrawFunctor : Functor {
    public:
    GLDrawFunctor(WebView* _wvInstance,
            int (WebView::*_funcPtr)(WebCore::IntRect&, WebCore::IntRect*,
                    WebCore::IntRect&, int, WebCore::IntRect&, jfloat, jint, bool),
            WebCore::IntRect _invScreenRect, float _scale, int _extras) {
        wvInstance = _wvInstance;
        funcPtr = _funcPtr;
        invScreenRect = _invScreenRect;
        scale = _scale;
        extras = _extras;
    };

    status_t operator()(int messageId, void* data) {
        TRACE_METHOD();
        bool shouldDraw = (messageId == uirenderer::DrawGlInfo::kModeDraw);
        if (shouldDraw)
            wvInstance->updateRectsForGL();

        if (invScreenRect.isEmpty()) {
            // NOOP operation if viewport is empty
            return 0;
        }

        WebCore::IntRect inval;
        int titlebarHeight = screenRect.height() - invScreenRect.height();

        uirenderer::DrawGlInfo* info = reinterpret_cast<uirenderer::DrawGlInfo*>(data);
        WebCore::IntRect screenClip(info->clipLeft, info->clipTop,
                                    info->clipRight - info->clipLeft,
                                    info->clipBottom - info->clipTop);

        WebCore::IntRect localInvScreenRect = invScreenRect;
        if (info->isLayer) {
            // When webview is on a layer, we need to use the viewport relative
            // to the FBO, rather than the screen(which will use invScreenRect).
            localInvScreenRect.setX(screenClip.x());
            localInvScreenRect.setY(info->height - screenClip.y() - screenClip.height());
        }
        // Send the necessary info to the shader.
        TilesManager::instance()->shader()->setGLDrawInfo(info);

        int returnFlags = (*wvInstance.*funcPtr)(localInvScreenRect, &inval, screenRect,
                titlebarHeight, screenClip, scale, extras, shouldDraw);
        if ((returnFlags & uirenderer::DrawGlInfo::kStatusDraw) != 0) {
            IntRect finalInval;
            if (inval.isEmpty())
                finalInval = screenRect;
            else {
                finalInval.setX(screenRect.x() + inval.x());
                finalInval.setY(screenRect.y() + titlebarHeight + inval.y());
                finalInval.setWidth(inval.width());
                finalInval.setHeight(inval.height());
            }
            info->dirtyLeft = finalInval.x();
            info->dirtyTop = finalInval.y();
            info->dirtyRight = finalInval.maxX();
            info->dirtyBottom = finalInval.maxY();
        }
        // return 1 if invalidation needed, 2 to request non-drawing functor callback, 0 otherwise
        ALOGV("returnFlags are %d, shouldDraw %d", returnFlags, shouldDraw);
        return returnFlags;
    }
    void updateScreenRect(WebCore::IntRect& _screenRect) {
        screenRect = _screenRect;
    }
    void updateInvScreenRect(WebCore::IntRect& _invScreenRect) {
        invScreenRect = _invScreenRect;
    }
    void updateScale(float _scale) {
        scale = _scale;
    }
    void updateExtras(jint _extras) {
        extras = _extras;
    }
    private:
    WebView* wvInstance;
    int (WebView::*funcPtr)(WebCore::IntRect&, WebCore::IntRect*,
            WebCore::IntRect&, int, WebCore::IntRect&, float, int, bool);
    WebCore::IntRect invScreenRect;
    WebCore::IntRect screenRect;
    jfloat scale;
    jint extras;
};

/*
 * Native JNI methods
 */

static void nativeCreate(JNIEnv *env, jobject obj, int viewImpl,
                         jstring drawableDir, jboolean isHighEndGfx)
{
    WTF::String dir = jstringToWtfString(env, drawableDir);
    new WebView(env, obj, viewImpl, dir, isHighEndGfx);
    // NEED THIS OR SOMETHING LIKE IT!
    //Release(obj);
}

static WebCore::IntRect jrect_to_webrect(JNIEnv* env, jobject obj)
{
    if (obj) {
        int L, T, R, B;
        GraphicsJNI::get_jrect(env, obj, &L, &T, &R, &B);
        return WebCore::IntRect(L, T, R - L, B - T);
    } else
        return WebCore::IntRect();
}

static SkRect jrectf_to_rect(JNIEnv* env, jobject obj)
{
    SkRect rect = SkRect::MakeEmpty();
    if (obj)
        GraphicsJNI::jrectf_to_rect(env, obj, &rect);
    return rect;
}

static void nativeDraw(JNIEnv *env, jobject obj, jobject canv,
        jobject visible, jint color,
        jint extras) {
    SkCanvas* canvas = GraphicsJNI::getNativeCanvas(env, canv);
    WebView* webView = GET_NATIVE_VIEW(env, obj);
    SkRect visibleContentRect = jrectf_to_rect(env, visible);
    webView->setVisibleContentRect(visibleContentRect);
    webView->draw(canvas, color, static_cast<WebView::DrawExtras>(extras));
}

static jint nativeCreateDrawGLFunction(JNIEnv *env, jobject obj, jint nativeView,
                                       jobject jinvscreenrect, jobject jscreenrect,
                                       jobject jvisiblecontentrect,
                                       jfloat scale, jint extras) {
    WebCore::IntRect invScreenRect = jrect_to_webrect(env, jinvscreenrect);
    WebView *wvInstance = reinterpret_cast<WebView*>(nativeView);
    SkRect visibleContentRect = jrectf_to_rect(env, jvisiblecontentrect);
    wvInstance->setVisibleContentRect(visibleContentRect);

    GLDrawFunctor* functor = (GLDrawFunctor*) wvInstance->getFunctor();
    if (!functor) {
        functor = new GLDrawFunctor(wvInstance, &android::WebView::drawGL,
                                    invScreenRect, scale, extras);
        wvInstance->setFunctor((Functor*) functor);
    } else {
        functor->updateInvScreenRect(invScreenRect);
        functor->updateScale(scale);
        functor->updateExtras(extras);
    }

    WebCore::IntRect rect = jrect_to_webrect(env, jscreenrect);
    functor->updateScreenRect(rect);

    return (jint)functor;
}

static jint nativeGetDrawGLFunction(JNIEnv *env, jobject obj, jint nativeView) {
    WebView *wvInstance = reinterpret_cast<WebView*>(nativeView);
    if (!wvInstance)
        return 0;

    return (jint) wvInstance->getFunctor();
}

static void nativeUpdateDrawGLFunction(JNIEnv *env, jobject obj, jint nativeView,
                                       jobject jinvscreenrect, jobject jscreenrect,
                                       jobject jvisiblecontentrect, jfloat scale) {
    WebView *wvInstance = reinterpret_cast<WebView*>(nativeView);
    if (wvInstance) {
        GLDrawFunctor* functor = (GLDrawFunctor*) wvInstance->getFunctor();
        if (functor) {
            WebCore::IntRect invScreenRect = jrect_to_webrect(env, jinvscreenrect);
            functor->updateInvScreenRect(invScreenRect);

            SkRect visibleContentRect = jrectf_to_rect(env, jvisiblecontentrect);
            wvInstance->setVisibleContentRect(visibleContentRect);

            WebCore::IntRect screenRect = jrect_to_webrect(env, jscreenrect);
            functor->updateScreenRect(screenRect);

            functor->updateScale(scale);
        }
    }
}

static bool nativeEvaluateLayersAnimations(JNIEnv *env, jobject obj, jint nativeView)
{
    // only call in software rendering, initialize and evaluate animations
#if USE(ACCELERATED_COMPOSITING)
    BaseLayerAndroid* baseLayer = reinterpret_cast<WebView*>(nativeView)->getBaseLayer();
    if (baseLayer) {
        baseLayer->initAnimations();
        return baseLayer->evaluateAnimations();
    }
#endif
    return false;
}

static bool nativeSetBaseLayer(JNIEnv *env, jobject obj, jint nativeView, jint layer,
                               jboolean showVisualIndicator,
                               jboolean isPictureAfterFirstLayout,
                               jint scrollingLayer)
{
    BaseLayerAndroid* layerImpl = reinterpret_cast<BaseLayerAndroid*>(layer);
    return reinterpret_cast<WebView*>(nativeView)->setBaseLayer(layerImpl, showVisualIndicator,
                                                                isPictureAfterFirstLayout,
                                                                scrollingLayer);
}

static BaseLayerAndroid* nativeGetBaseLayer(JNIEnv *env, jobject obj, jint nativeView)
{
    return reinterpret_cast<WebView*>(nativeView)->getBaseLayer();
}

static void nativeCopyBaseContentToPicture(JNIEnv *env, jobject obj, jobject pict)
{
    SkPicture* picture = GraphicsJNI::getNativePicture(env, pict);
    GET_NATIVE_VIEW(env, obj)->copyBaseContentToPicture(picture);
}

static jboolean nativeDumpLayerContentToPicture(JNIEnv *env, jobject obj, jint instance,
                                                jstring jclassName, jint layerId, jobject pict)
{
    bool success = false;
    SkPicture* picture = GraphicsJNI::getNativePicture(env, pict);
    std::string classname = jstringToStdString(env, jclassName);
    BaseLayerAndroid* baseLayer = reinterpret_cast<WebView*>(instance)->getBaseLayer();
    LayerAndroid* layer = baseLayer->findById(layerId);
    SkSafeRef(layer);
    if (layer && layer->subclassName() == classname) {
        LayerContent* content = layer->content();
        if (content) {
            SkCanvas* canvas = picture->beginRecording(content->width(), content->height());
            content->draw(canvas);
            picture->endRecording();
            success = true;
        }
    }
    SkSafeUnref(layer);
    return success;
}

static bool nativeHasContent(JNIEnv *env, jobject obj)
{
    return GET_NATIVE_VIEW(env, obj)->hasContent();
}

static void nativeSetHeightCanMeasure(JNIEnv *env, jobject obj, bool measure)
{
    WebView* view = GET_NATIVE_VIEW(env, obj);
    ALOG_ASSERT(view, "view not set in nativeSetHeightCanMeasure");
    view->setHeightCanMeasure(measure);
}

static void nativeDestroy(JNIEnv *env, jobject obj, jint ptr)
{
    WebView* view = reinterpret_cast<WebView*>(ptr);
    ALOGD("nativeDestroy view: %p", view);
    ALOG_ASSERT(view, "view not set in nativeDestroy");
    delete view;
}

static void nativeStopGL(JNIEnv *env, jobject obj, jint ptr)
{
    if (ptr)
        reinterpret_cast<WebView*>(ptr)->stopGL();
}

static jobject nativeGetSelection(JNIEnv *env, jobject obj)
{
    WebView* view = GET_NATIVE_VIEW(env, obj);
    ALOG_ASSERT(view, "view not set in %s", __FUNCTION__);
    String selection = view->getSelection();
    return wtfStringToJstring(env, selection);
}

static void nativeDiscardAllTextures(JNIEnv *env, jobject obj)
{
    //discard all textures for debugging/test purposes, but not gl backing memory
    bool allTextures = true, deleteGLTextures = false;
    TilesManager::instance()->discardTextures(allTextures, deleteGLTextures);
}

static void nativeTileProfilingStart(JNIEnv *env, jobject obj)
{
    TilesManager::instance()->getProfiler()->start();
}

static float nativeTileProfilingStop(JNIEnv *env, jobject obj)
{
    return TilesManager::instance()->getProfiler()->stop();
}

static void nativeTileProfilingClear(JNIEnv *env, jobject obj)
{
    TilesManager::instance()->getProfiler()->clear();
}

static int nativeTileProfilingNumFrames(JNIEnv *env, jobject obj)
{
    return TilesManager::instance()->getProfiler()->numFrames();
}

static int nativeTileProfilingNumTilesInFrame(JNIEnv *env, jobject obj, int frame)
{
    return TilesManager::instance()->getProfiler()->numTilesInFrame(frame);
}

static int nativeTileProfilingGetInt(JNIEnv *env, jobject obj, int frame, int tile, jstring jkey)
{
    WTF::String key = jstringToWtfString(env, jkey);
    TileProfileRecord* record = TilesManager::instance()->getProfiler()->getTile(frame, tile);

    if (key == "left")
        return record->left;
    if (key == "top")
        return record->top;
    if (key == "right")
        return record->right;
    if (key == "bottom")
        return record->bottom;
    if (key == "level")
        return record->level;
    if (key == "isReady")
        return record->isReady ? 1 : 0;
    return -1;
}

static float nativeTileProfilingGetFloat(JNIEnv *env, jobject obj, int frame, int tile, jstring jkey)
{
    TileProfileRecord* record = TilesManager::instance()->getProfiler()->getTile(frame, tile);
    return record->scale;
}

#ifdef ANDROID_DUMP_DISPLAY_TREE
static void dumpToFile(const char text[], void* file) {
    fwrite(text, 1, strlen(text), reinterpret_cast<FILE*>(file));
    fwrite("\n", 1, 1, reinterpret_cast<FILE*>(file));
}
#endif

// Return true to view invalidate WebView
static bool nativeSetProperty(JNIEnv *env, jobject obj, jstring jkey, jstring jvalue)
{
    WTF::String key = jstringToWtfString(env, jkey);
    WTF::String value = jstringToWtfString(env, jvalue);
    if (key == "inverted") {
        bool shouldInvert = (value == "true");
        TilesManager::instance()->setInvertedScreen(shouldInvert);
        return true;
    }
    else if (key == "inverted_contrast") {
        float contrast = value.toFloat();
        TilesManager::instance()->setInvertedScreenContrast(contrast);
        return true;
    }
    else if (key == "enable_cpu_upload_path") {
        TilesManager::instance()->transferQueue()->setTextureUploadType(
            value == "true" ? CpuUpload : GpuUpload);
    }
    else if (key == "use_minimal_memory") {
        TilesManager::instance()->setUseMinimalMemory(value == "true");
    }
    else if (key == "use_double_buffering") {
        TilesManager::instance()->setUseDoubleBuffering(value == "true");
    }
    else if (key == "tree_updates") {
        TilesManager::instance()->clearContentUpdates();
    }
    return false;
}

static jstring nativeGetProperty(JNIEnv *env, jobject obj, jstring jkey)
{
    WTF::String key = jstringToWtfString(env, jkey);
    if (key == "tree_updates") {
        int updates = TilesManager::instance()->getContentUpdates();
        WTF::String wtfUpdates = WTF::String::number(updates);
        return wtfStringToJstring(env, wtfUpdates);
    }
    return 0;
}

static void nativeOnTrimMemory(JNIEnv *env, jobject obj, jint level)
{
    if (TilesManager::hardwareAccelerationEnabled()) {
        // When we got TRIM_MEMORY_MODERATE or TRIM_MEMORY_COMPLETE, we should
        // make sure the transfer queue is empty and then abandon the Surface
        // Texture to avoid ANR b/c framework may destroy the EGL context.
        // Refer to WindowManagerImpl.java for conditions we followed.
        TilesManager* tilesManager = TilesManager::instance();
        if ((level >= TRIM_MEMORY_MODERATE
            && !tilesManager->highEndGfx())
            || level >= TRIM_MEMORY_COMPLETE) {
            ALOGD("OnTrimMemory with EGL Context %p", eglGetCurrentContext());
            tilesManager->cleanupGLResources();
        }

        bool freeAllTextures = (level > TRIM_MEMORY_UI_HIDDEN), glTextures = true;
        tilesManager->discardTextures(freeAllTextures, glTextures);
    }
}

static void nativeDumpDisplayTree(JNIEnv* env, jobject jwebview, jstring jurl)
{
#ifdef ANDROID_DUMP_DISPLAY_TREE
    WebView* view = GET_NATIVE_VIEW(env, jwebview);
    ALOG_ASSERT(view, "view not set in %s", __FUNCTION__);

    if (view && view->getWebViewCore()) {
        FILE* file = fopen(DISPLAY_TREE_LOG_FILE, "w");
        if (file) {
            SkFormatDumper dumper(dumpToFile, file);
            // dump the URL
            if (jurl) {
                const char* str = env->GetStringUTFChars(jurl, 0);
                SkDebugf("Dumping %s to %s\n", str, DISPLAY_TREE_LOG_FILE);
                dumpToFile(str, file);
                env->ReleaseStringUTFChars(jurl, str);
            }
            // now dump the display tree
            SkDumpCanvas canvas(&dumper);
            // this will playback the picture into the canvas, which will
            // spew its contents to the dumper
            view->draw(&canvas, 0, WebView::DrawExtrasNone);
            // we're done with the file now
            fwrite("\n", 1, 1, file);
            fclose(file);
        }
#if USE(ACCELERATED_COMPOSITING)
        const LayerAndroid* baseLayer = view->getBaseLayer();
        if (baseLayer) {
          FILE* file = fopen(LAYERS_TREE_LOG_FILE,"w");
          if (file) {
              WebCore::FileLayerDumper dumper(file);
              baseLayer->dumpLayers(&dumper);
              fclose(file);
          }
        }
#endif
    }
#endif
}

static int nativeScrollableLayer(JNIEnv* env, jobject jwebview, jint nativeView,
    jint x, jint y, jobject rect, jobject bounds)
{
    WebView* webview = reinterpret_cast<WebView*>(nativeView);
    ALOG_ASSERT(webview, "webview not set in %s", __FUNCTION__);
    SkIRect nativeRect, nativeBounds;
    int id = webview->scrollableLayer(x, y, &nativeRect, &nativeBounds);
    if (rect)
        GraphicsJNI::irect_to_jrect(nativeRect, env, rect);
    if (bounds)
        GraphicsJNI::irect_to_jrect(nativeBounds, env, bounds);
    return id;
}

static bool nativeScrollLayer(JNIEnv* env, jobject obj,
    jint nativeView, jint layerId, jint x, jint y)
{
#if ENABLE(ANDROID_OVERFLOW_SCROLL)
    WebView* webview = reinterpret_cast<WebView*>(nativeView);
    webview->scrollLayer(layerId, x, y);

    //TODO: the below only needed for the SW rendering path
    LayerAndroid* baseLayer = webview->getBaseLayer();
    if (!baseLayer)
        return false;
    LayerAndroid* layer = baseLayer->findById(layerId);
    if (!layer || !layer->contentIsScrollable())
        return false;
    return static_cast<ScrollableLayerAndroid*>(layer)->scrollTo(x, y);
#endif
    return false;
}

static void nativeSetIsScrolling(JNIEnv* env, jobject jwebview, jboolean isScrolling)
{
    // TODO: Pass in the native pointer instead
    WebView* view = GET_NATIVE_VIEW(env, jwebview);
    if (view)
        view->setIsScrolling(isScrolling);
}

static void nativeUseHardwareAccelSkia(JNIEnv*, jobject, jboolean enabled)
{
    BaseRenderer::setCurrentRendererType(enabled ? BaseRenderer::Ganesh : BaseRenderer::Raster);
}

static int nativeGetBackgroundColor(JNIEnv* env, jobject obj, jint nativeView)
{
    WebView* view = reinterpret_cast<WebView*>(nativeView);
    BaseLayerAndroid* baseLayer = view->getBaseLayer();
    if (baseLayer) {
        WebCore::Color color = baseLayer->getBackgroundColor();
        if (color.isValid())
            return SkColorSetARGB(color.alpha(), color.red(),
                                  color.green(), color.blue());
    }
    return SK_ColorWHITE;
}

static void nativeSetPauseDrawing(JNIEnv *env, jobject obj, jint nativeView,
                                      jboolean pause)
{
    reinterpret_cast<WebView*>(nativeView)->setDrawingPaused(pause);
}

static void nativeSetTextSelection(JNIEnv *env, jobject obj, jint nativeView,
                                   jint selectionPtr)
{
    SelectText* selection = reinterpret_cast<SelectText*>(selectionPtr);
    reinterpret_cast<WebView*>(nativeView)->setTextSelection(selection);
}

static jint nativeGetHandleLayerId(JNIEnv *env, jobject obj, jint nativeView,
                                     jint handleIndex, jobject cursorPoint,
                                     jobject textQuad)
{
    WebView* webview = reinterpret_cast<WebView*>(nativeView);
    SkIPoint nativePoint;
    FloatQuad nativeTextQuad;
    int layerId = webview->getHandleLayerId((SelectText::HandleId) handleIndex,
            nativePoint, nativeTextQuad);
    if (cursorPoint)
        GraphicsJNI::ipoint_to_jpoint(nativePoint, env, cursorPoint);
    if (textQuad)
        webview->floatQuadToQuadF(env, nativeTextQuad, textQuad);
    return layerId;
}

static void nativeMapLayerRect(JNIEnv *env, jobject obj, jint nativeView,
        jint layerId, jobject rect)
{
    WebView* webview = reinterpret_cast<WebView*>(nativeView);
    SkIRect nativeRect;
    GraphicsJNI::jrect_to_irect(env, rect, &nativeRect);
    webview->mapLayerRect(layerId, nativeRect);
    GraphicsJNI::irect_to_jrect(nativeRect, env, rect);
}

static jint nativeSetHwAccelerated(JNIEnv *env, jobject obj, jint nativeView,
        jboolean hwAccelerated)
{
    WebView* webview = reinterpret_cast<WebView*>(nativeView);
    return webview->setHwAccelerated(hwAccelerated);
}

static void nativeFindMaxVisibleRect(JNIEnv *env, jobject obj, jint nativeView,
        jint movingLayerId, jobject visibleContentRect)
{
    WebView* webview = reinterpret_cast<WebView*>(nativeView);
    SkIRect nativeRect;
    GraphicsJNI::jrect_to_irect(env, visibleContentRect, &nativeRect);
    webview->findMaxVisibleRect(movingLayerId, nativeRect);
    GraphicsJNI::irect_to_jrect(nativeRect, env, visibleContentRect);
}

static bool nativeIsHandleLeft(JNIEnv *env, jobject obj, jint nativeView,
        jint handleId)
{
    WebView* webview = reinterpret_cast<WebView*>(nativeView);
    return webview->isHandleLeft(static_cast<SelectText::HandleId>(handleId));
}

static bool nativeIsPointVisible(JNIEnv *env, jobject obj, jint nativeView,
        jint layerId, jint contentX, jint contentY)
{
    WebView* webview = reinterpret_cast<WebView*>(nativeView);
    return webview->isPointVisible(layerId, contentX, contentY);
}

/*
 * JNI registration
 */
static JNINativeMethod gJavaWebViewMethods[] = {
    { "nativeCreate", "(ILjava/lang/String;Z)V",
        (void*) nativeCreate },
    { "nativeDestroy", "(I)V",
        (void*) nativeDestroy },
    { "nativeDraw", "(Landroid/graphics/Canvas;Landroid/graphics/RectF;II)V",
        (void*) nativeDraw },
    { "nativeCreateDrawGLFunction", "(ILandroid/graphics/Rect;Landroid/graphics/Rect;Landroid/graphics/RectF;FI)I",
        (void*) nativeCreateDrawGLFunction },
    { "nativeGetDrawGLFunction", "(I)I",
        (void*) nativeGetDrawGLFunction },
    { "nativeUpdateDrawGLFunction", "(ILandroid/graphics/Rect;Landroid/graphics/Rect;Landroid/graphics/RectF;F)V",
        (void*) nativeUpdateDrawGLFunction },
    { "nativeDumpDisplayTree", "(Ljava/lang/String;)V",
        (void*) nativeDumpDisplayTree },
    { "nativeEvaluateLayersAnimations", "(I)Z",
        (void*) nativeEvaluateLayersAnimations },
    { "nativeGetSelection", "()Ljava/lang/String;",
        (void*) nativeGetSelection },
    { "nativeSetHeightCanMeasure", "(Z)V",
        (void*) nativeSetHeightCanMeasure },
    { "nativeSetBaseLayer", "(IIZZI)Z",
        (void*) nativeSetBaseLayer },
    { "nativeGetBaseLayer", "(I)I",
        (void*) nativeGetBaseLayer },
    { "nativeCopyBaseContentToPicture", "(Landroid/graphics/Picture;)V",
        (void*) nativeCopyBaseContentToPicture },
    { "nativeDumpLayerContentToPicture", "(ILjava/lang/String;ILandroid/graphics/Picture;)Z",
        (void*) nativeDumpLayerContentToPicture },
    { "nativeHasContent", "()Z",
        (void*) nativeHasContent },
    { "nativeDiscardAllTextures", "()V",
        (void*) nativeDiscardAllTextures },
    { "nativeTileProfilingStart", "()V",
        (void*) nativeTileProfilingStart },
    { "nativeTileProfilingStop", "()F",
        (void*) nativeTileProfilingStop },
    { "nativeTileProfilingClear", "()V",
        (void*) nativeTileProfilingClear },
    { "nativeTileProfilingNumFrames", "()I",
        (void*) nativeTileProfilingNumFrames },
    { "nativeTileProfilingNumTilesInFrame", "(I)I",
        (void*) nativeTileProfilingNumTilesInFrame },
    { "nativeTileProfilingGetInt", "(IILjava/lang/String;)I",
        (void*) nativeTileProfilingGetInt },
    { "nativeTileProfilingGetFloat", "(IILjava/lang/String;)F",
        (void*) nativeTileProfilingGetFloat },
    { "nativeStopGL", "(I)V",
        (void*) nativeStopGL },
    { "nativeScrollableLayer", "(IIILandroid/graphics/Rect;Landroid/graphics/Rect;)I",
        (void*) nativeScrollableLayer },
    { "nativeScrollLayer", "(IIII)Z",
        (void*) nativeScrollLayer },
    { "nativeSetIsScrolling", "(Z)V",
        (void*) nativeSetIsScrolling },
    { "nativeUseHardwareAccelSkia", "(Z)V",
        (void*) nativeUseHardwareAccelSkia },
    { "nativeGetBackgroundColor", "(I)I",
        (void*) nativeGetBackgroundColor },
    { "nativeSetProperty", "(Ljava/lang/String;Ljava/lang/String;)Z",
        (void*) nativeSetProperty },
    { "nativeGetProperty", "(Ljava/lang/String;)Ljava/lang/String;",
        (void*) nativeGetProperty },
    { "nativeOnTrimMemory", "(I)V",
        (void*) nativeOnTrimMemory },
    { "nativeSetPauseDrawing", "(IZ)V",
        (void*) nativeSetPauseDrawing },
    { "nativeSetTextSelection", "(II)V",
        (void*) nativeSetTextSelection },
    { "nativeGetHandleLayerId", "(IILandroid/graphics/Point;Landroid/webkit/QuadF;)I",
        (void*) nativeGetHandleLayerId },
    { "nativeMapLayerRect", "(IILandroid/graphics/Rect;)V",
        (void*) nativeMapLayerRect },
    { "nativeSetHwAccelerated", "(IZ)I",
        (void*) nativeSetHwAccelerated },
    { "nativeFindMaxVisibleRect", "(IILandroid/graphics/Rect;)V",
        (void*) nativeFindMaxVisibleRect },
    { "nativeIsHandleLeft", "(II)Z",
        (void*) nativeIsHandleLeft },
    { "nativeIsPointVisible", "(IIII)Z",
        (void*) nativeIsPointVisible },
};

int registerWebView(JNIEnv* env)
{
    jclass clazz = env->FindClass("android/webkit/WebViewClassic");
    ALOG_ASSERT(clazz, "Unable to find class android/webkit/WebViewClassic");
    gWebViewField = env->GetFieldID(clazz, "mNativeClass", "I");
    ALOG_ASSERT(gWebViewField, "Unable to find android/webkit/WebViewClassic.mNativeClass");
    env->DeleteLocalRef(clazz);

    return jniRegisterNativeMethods(env, "android/webkit/WebViewClassic", gJavaWebViewMethods, NELEM(gJavaWebViewMethods));
}

} // namespace android
