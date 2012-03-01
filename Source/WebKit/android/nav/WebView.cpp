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
#include "DrawExtra.h"
#include "Frame.h"
#include "GraphicsJNI.h"
#include "HTMLInputElement.h"
#include "IntPoint.h"
#include "IntRect.h"
#include "LayerAndroid.h"
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
#include "WebCoreJni.h"
#include "WebRequestContext.h"
#include "WebViewCore.h"
#include "android_graphics.h"

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
    jmethodID   m_getVisibleRect;
    jmethodID   m_viewInvalidate;
    jmethodID   m_viewInvalidateRect;
    jmethodID   m_postInvalidateDelayed;
    jmethodID   m_pageSwapCallback;
    jfieldID    m_rectLeft;
    jfieldID    m_rectTop;
    jmethodID   m_rectWidth;
    jmethodID   m_rectHeight;
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
    m_javaGlue.m_getVisibleRect = GetJMethod(env, clazz, "sendOurVisibleRect", "()Landroid/graphics/Rect;");
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
    if (left < m_visibleRect.fLeft)
        dx = left - m_visibleRect.fLeft;
    // Only scroll right if the entire width can fit on screen.
    else if (right > m_visibleRect.fRight
            && right - left < m_visibleRect.width())
        dx = right - m_visibleRect.fRight;
    int dy = 0;
    int top = rect.y();
    int bottom = rect.maxY();
    if (top < m_visibleRect.fTop)
        dy = top - m_visibleRect.fTop;
    // Only scroll down if the entire height can fit on screen
    else if (bottom > m_visibleRect.fBottom
            && bottom - top < m_visibleRect.height())
        dy = bottom - m_visibleRect.fBottom;
    if ((dx|dy) == 0 || !scrollBy(dx, dy))
        return;
    viewInvalidate();
}

bool drawGL(WebCore::IntRect& viewRect, WebCore::IntRect* invalRect,
        WebCore::IntRect& webViewRect, int titleBarHeight,
        WebCore::IntRect& clip, float scale, int extras)
{
#if USE(ACCELERATED_COMPOSITING)
    if (!m_baseLayer)
        return false;

    if (!m_glWebViewState) {
        TilesManager::instance()->setHighEndGfx(m_isHighEndGfx);
        m_glWebViewState = new GLWebViewState();
        if (m_baseLayer->content()) {
            SkRegion region;
            SkIRect rect;
            rect.set(0, 0, m_baseLayer->content()->width(), m_baseLayer->content()->height());
            region.setRect(rect);
            m_baseLayer->markAsDirty(region);
            m_glWebViewState->setBaseLayer(m_baseLayer, false, true);
        }
    }

    DrawExtra* extra = getDrawExtra((DrawExtras) extras);

    unsigned int pic = m_glWebViewState->currentPictureCounter();
    m_glWebViewState->glExtras()->setDrawExtra(extra);

    // Make sure we have valid coordinates. We might not have valid coords
    // if the zoom manager is still initializing. We will be redrawn
    // once the correct scale is set
    if (!m_visibleRect.isFinite())
        return false;
    bool treesSwapped = false;
    bool newTreeHasAnim = false;
    bool ret = m_glWebViewState->drawGL(viewRect, m_visibleRect, invalRect,
                                        webViewRect, titleBarHeight, clip, scale,
                                        &treesSwapped, &newTreeHasAnim);
    if (treesSwapped) {
        ALOG_ASSERT(m_javaGlue.m_obj, "A java object was not associated with this native WebView!");
        JNIEnv* env = JSC::Bindings::getJNIEnv();
        AutoJObject javaObject = m_javaGlue.object(env);
        if (javaObject.get()) {
            env->CallVoidMethod(javaObject.get(), m_javaGlue.m_pageSwapCallback, newTreeHasAnim);
            checkException(env);
        }
    }
    if (ret || m_glWebViewState->currentPictureCounter() != pic)
        return !m_isDrawingPaused;
#endif
    return false;
}

PictureSet* draw(SkCanvas* canvas, SkColor bgColor, DrawExtras extras, bool split)
{
    PictureSet* ret = 0;
    if (!m_baseLayer) {
        canvas->drawColor(bgColor);
        return ret;
    }

    // draw the content of the base layer first
    PictureSet* content = m_baseLayer->content();
    int sc = canvas->save(SkCanvas::kClip_SaveFlag);
    canvas->clipRect(SkRect::MakeLTRB(0, 0, content->width(),
                content->height()), SkRegion::kDifference_Op);
    canvas->drawColor(bgColor);
    canvas->restoreToCount(sc);
    if (content->draw(canvas))
        ret = split ? new PictureSet(*content) : 0;

    DrawExtra* extra = getDrawExtra(extras);
    if (extra)
        extra->draw(canvas, 0);

#if USE(ACCELERATED_COMPOSITING)
    LayerAndroid* compositeLayer = compositeRoot();
    if (compositeLayer) {
        // call this to be sure we've adjusted for any scrolling or animations
        // before we actually draw
        compositeLayer->updateLayerPositions(m_visibleRect);
        compositeLayer->updatePositions();
        // We have to set the canvas' matrix on the base layer
        // (to have fixed layers work as intended)
        SkAutoCanvasRestore restore(canvas, true);
        m_baseLayer->setMatrix(canvas->getTotalMatrix());
        canvas->resetMatrix();
        m_baseLayer->draw(canvas, extra);
    }
    if (extra) {
        IntRect dummy; // inval area, unused for now
        extra->drawLegacy(canvas, compositeLayer, &dummy);
    }
#endif
    return ret;
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

IntRect getVisibleRect()
{
    IntRect rect;
    ALOG_ASSERT(m_javaGlue.m_obj, "A java object was not associated with this native WebView!");
    JNIEnv* env = JSC::Bindings::getJNIEnv();
    AutoJObject javaObject = m_javaGlue.object(env);
    if (!javaObject.get())
        return rect;
    jobject jRect = env->CallObjectMethod(javaObject.get(), m_javaGlue.m_getVisibleRect);
    checkException(env);
    rect.setX(env->GetIntField(jRect, m_javaGlue.m_rectLeft));
    checkException(env);
    rect.setY(env->GetIntField(jRect, m_javaGlue.m_rectTop));
    checkException(env);
    rect.setWidth(env->CallIntMethod(jRect, m_javaGlue.m_rectWidth));
    checkException(env);
    rect.setHeight(env->CallIntMethod(jRect, m_javaGlue.m_rectHeight));
    checkException(env);
    env->DeleteLocalRef(jRect);
    checkException(env);
    return rect;
}

#if USE(ACCELERATED_COMPOSITING)
static const ScrollableLayerAndroid* findScrollableLayer(
    const LayerAndroid* parent, int x, int y, SkIRect* foundBounds) {
    SkRect bounds;
    parent->bounds(&bounds);
    // Check the parent bounds first; this will clip to within a masking layer's
    // bounds.
    if (parent->masksToBounds() && !bounds.contains(x, y))
        return 0;
    // Move the hit test local to parent.
    x -= bounds.fLeft;
    y -= bounds.fTop;
    int count = parent->countChildren();
    while (count--) {
        const LayerAndroid* child = parent->getChild(count);
        const ScrollableLayerAndroid* result = findScrollableLayer(child, x, y,
            foundBounds);
        if (result) {
            foundBounds->offset(bounds.fLeft, bounds.fTop);
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
        foundBounds->set(0, 0, bounds.width(), bounds.height());
        return static_cast<const ScrollableLayerAndroid*>(parent);
    }
    return 0;
}
#endif

int scrollableLayer(int x, int y, SkIRect* layerRect, SkIRect* bounds)
{
#if USE(ACCELERATED_COMPOSITING)
    const LayerAndroid* layerRoot = compositeRoot();
    if (!layerRoot)
        return 0;
    const ScrollableLayerAndroid* result = findScrollableLayer(layerRoot, x, y,
        bounds);
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

LayerAndroid* compositeRoot() const
{
    ALOG_ASSERT(!m_baseLayer || m_baseLayer->countChildren() == 1,
            "base layer can't have more than one child %s", __FUNCTION__);
    if (m_baseLayer && m_baseLayer->countChildren() == 1)
        return static_cast<LayerAndroid*>(m_baseLayer->getChild(0));
    else
        return 0;
}

#if ENABLE(ANDROID_OVERFLOW_SCROLL)
static void copyScrollPositionRecursive(const LayerAndroid* from,
                                        LayerAndroid* root)
{
    if (!from || !root)
        return;
    for (int i = 0; i < from->countChildren(); i++) {
        const LayerAndroid* l = from->getChild(i);
        if (l->contentIsScrollable()) {
            const SkPoint& pos = l->getPosition();
            LayerAndroid* match = root->findById(l->uniqueId());
            if (match && match->contentIsScrollable())
                match->setPosition(pos.fX, pos.fY);
        }
        copyScrollPositionRecursive(l, root);
    }
}
#endif

bool setBaseLayer(BaseLayerAndroid* layer, SkRegion& inval, bool showVisualIndicator,
                  bool isPictureAfterFirstLayout)
{
    bool queueFull = false;
#if USE(ACCELERATED_COMPOSITING)
    if (m_glWebViewState) {
        if (layer)
            layer->markAsDirty(inval);
        queueFull = m_glWebViewState->setBaseLayer(layer, showVisualIndicator,
                                                   isPictureAfterFirstLayout);
    }
#endif

#if ENABLE(ANDROID_OVERFLOW_SCROLL)
    if (layer) {
        // TODO: the below tree copies are only necessary in software rendering
        LayerAndroid* newCompositeRoot = static_cast<LayerAndroid*>(layer->getChild(0));
        copyScrollPositionRecursive(compositeRoot(), newCompositeRoot);
    }
#endif
    SkSafeUnref(m_baseLayer);
    m_baseLayer = layer;

    return queueFull;
}

void replaceBaseContent(PictureSet* set)
{
    if (!m_baseLayer)
        return;
    m_baseLayer->setContent(*set);
    delete set;
}

void copyBaseContentToPicture(SkPicture* picture)
{
    if (!m_baseLayer)
        return;
    PictureSet* content = m_baseLayer->content();
    m_baseLayer->drawCanvas(picture->beginRecording(content->width(), content->height(),
            SkPicture::kUsePathBoundsForClip_RecordingFlag));
    picture->endRecording();
}

bool hasContent() {
    if (!m_baseLayer)
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

BaseLayerAndroid* getBaseLayer() {
    return m_baseLayer;
}

void setVisibleRect(SkRect& visibleRect) {
    m_visibleRect = visibleRect;
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

int getHandleLayerId(SelectText::HandleId handleId, SkIRect& cursorRect) {
    SelectText* selectText = static_cast<SelectText*>(getDrawExtra(DrawExtrasSelection));
    if (!selectText || !m_baseLayer)
        return -1;
    int layerId = selectText->caretLayerId(handleId);
    cursorRect = selectText->caretRect(handleId);
    mapLayerRect(layerId, cursorRect);
    return layerId;
}

void mapLayerRect(int layerId, SkIRect& rect) {
    if (layerId != -1) {
        // We need to make sure the drawTransform is up to date as this is
        // called before a draw() or drawGL()
        m_baseLayer->updateLayerPositions(m_visibleRect);
        LayerAndroid* root = compositeRoot();
        LayerAndroid* layer = root ? root->findById(layerId) : 0;
        if (layer && layer->drawTransform())
            rect = layer->drawTransform()->mapRect(rect);
    }
}

    bool m_isDrawingPaused;
private: // local state for WebView
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
    SkRect m_visibleRect;
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
            bool(WebView::*_funcPtr)(WebCore::IntRect&, WebCore::IntRect*,
                    WebCore::IntRect&, int, WebCore::IntRect&, jfloat, jint),
            WebCore::IntRect _viewRect, float _scale, int _extras) {
        wvInstance = _wvInstance;
        funcPtr = _funcPtr;
        viewRect = _viewRect;
        scale = _scale;
        extras = _extras;
    };
    status_t operator()(int messageId, void* data) {
        if (viewRect.isEmpty()) {
            // NOOP operation if viewport is empty
            return 0;
        }

        WebCore::IntRect inval;
        int titlebarHeight = webViewRect.height() - viewRect.height();

        uirenderer::DrawGlInfo* info = reinterpret_cast<uirenderer::DrawGlInfo*>(data);
        WebCore::IntRect localViewRect = viewRect;
        if (info->isLayer)
            localViewRect.move(-1 * localViewRect.x(), -1 * localViewRect.y());

        WebCore::IntRect clip(info->clipLeft, info->clipTop,
                              info->clipRight - info->clipLeft,
                              info->clipBottom - info->clipTop);
        TilesManager::instance()->shader()->setWebViewMatrix(info->transform, info->isLayer);

        bool retVal = (*wvInstance.*funcPtr)(localViewRect, &inval, webViewRect,
                titlebarHeight, clip, scale, extras);
        if (retVal) {
            IntRect finalInval;
            if (inval.isEmpty()) {
                finalInval = webViewRect;
                retVal = true;
            } else {
                finalInval.setX(webViewRect.x() + inval.x());
                finalInval.setY(webViewRect.y() + titlebarHeight + inval.y());
                finalInval.setWidth(inval.width());
                finalInval.setHeight(inval.height());
            }
            info->dirtyLeft = finalInval.x();
            info->dirtyTop = finalInval.y();
            info->dirtyRight = finalInval.maxX();
            info->dirtyBottom = finalInval.maxY();
        }
        // return 1 if invalidation needed, 0 otherwise
        return retVal ? 1 : 0;
    }
    void updateRect(WebCore::IntRect& _viewRect) {
        viewRect = _viewRect;
    }
    void updateViewRect(WebCore::IntRect& _viewRect) {
        webViewRect = _viewRect;
    }
    void updateScale(float _scale) {
        scale = _scale;
    }
    private:
    WebView* wvInstance;
    bool (WebView::*funcPtr)(WebCore::IntRect&, WebCore::IntRect*,
            WebCore::IntRect&, int, WebCore::IntRect&, float, int);
    WebCore::IntRect viewRect;
    WebCore::IntRect webViewRect;
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

static jint nativeDraw(JNIEnv *env, jobject obj, jobject canv,
        jobject visible, jint color,
        jint extras, jboolean split) {
    SkCanvas* canvas = GraphicsJNI::getNativeCanvas(env, canv);
    WebView* webView = GET_NATIVE_VIEW(env, obj);
    SkRect visibleRect = jrectf_to_rect(env, visible);
    webView->setVisibleRect(visibleRect);
    PictureSet* pictureSet = webView->draw(canvas, color,
            static_cast<WebView::DrawExtras>(extras), split);
    return reinterpret_cast<jint>(pictureSet);
}

static jint nativeGetDrawGLFunction(JNIEnv *env, jobject obj, jint nativeView,
                                    jobject jrect, jobject jviewrect,
                                    jobject jvisiblerect,
                                    jfloat scale, jint extras) {
    WebCore::IntRect viewRect = jrect_to_webrect(env, jrect);
    WebView *wvInstance = (WebView*) nativeView;
    SkRect visibleRect = jrectf_to_rect(env, jvisiblerect);
    wvInstance->setVisibleRect(visibleRect);

    GLDrawFunctor* functor = new GLDrawFunctor(wvInstance,
            &android::WebView::drawGL, viewRect, scale, extras);
    wvInstance->setFunctor((Functor*) functor);

    WebCore::IntRect webViewRect = jrect_to_webrect(env, jviewrect);
    functor->updateViewRect(webViewRect);

    return (jint)functor;
}

static void nativeUpdateDrawGLFunction(JNIEnv *env, jobject obj, jobject jrect,
        jobject jviewrect, jobject jvisiblerect, jfloat scale) {
    WebView *wvInstance = GET_NATIVE_VIEW(env, obj);
    if (wvInstance) {
        GLDrawFunctor* functor = (GLDrawFunctor*) wvInstance->getFunctor();
        if (functor) {
            WebCore::IntRect viewRect = jrect_to_webrect(env, jrect);
            functor->updateRect(viewRect);

            SkRect visibleRect = jrectf_to_rect(env, jvisiblerect);
            wvInstance->setVisibleRect(visibleRect);

            WebCore::IntRect webViewRect = jrect_to_webrect(env, jviewrect);
            functor->updateViewRect(webViewRect);

            functor->updateScale(scale);
        }
    }
}

static bool nativeEvaluateLayersAnimations(JNIEnv *env, jobject obj, jint nativeView)
{
    // only call in software rendering, initialize and evaluate animations
#if USE(ACCELERATED_COMPOSITING)
    LayerAndroid* root = ((WebView*)nativeView)->compositeRoot();
    if (root) {
        root->initAnimations();
        return root->evaluateAnimations();
    }
#endif
    return false;
}

static bool nativeSetBaseLayer(JNIEnv *env, jobject obj, jint nativeView, jint layer, jobject inval,
                               jboolean showVisualIndicator,
                               jboolean isPictureAfterFirstLayout)
{
    BaseLayerAndroid* layerImpl = reinterpret_cast<BaseLayerAndroid*>(layer);
    SkRegion invalRegion;
    if (inval)
        invalRegion = *GraphicsJNI::getNativeRegion(env, inval);
    return ((WebView*)nativeView)->setBaseLayer(layerImpl, invalRegion, showVisualIndicator,
                                                isPictureAfterFirstLayout);
}

static BaseLayerAndroid* nativeGetBaseLayer(JNIEnv *env, jobject obj)
{
    return GET_NATIVE_VIEW(env, obj)->getBaseLayer();
}

static void nativeReplaceBaseContent(JNIEnv *env, jobject obj, jint content)
{
    PictureSet* set = reinterpret_cast<PictureSet*>(content);
    GET_NATIVE_VIEW(env, obj)->replaceBaseContent(set);
}

static void nativeCopyBaseContentToPicture(JNIEnv *env, jobject obj, jobject pict)
{
    SkPicture* picture = GraphicsJNI::getNativePicture(env, pict);
    GET_NATIVE_VIEW(env, obj)->copyBaseContentToPicture(picture);
}

static bool nativeHasContent(JNIEnv *env, jobject obj)
{
    return GET_NATIVE_VIEW(env, obj)->hasContent();
}

static jobject nativeLayerBounds(JNIEnv* env, jobject obj, jint jlayer)
{
    SkRect r;
#if USE(ACCELERATED_COMPOSITING)
    LayerAndroid* layer = (LayerAndroid*) jlayer;
    r = layer->bounds();
#else
    r.setEmpty();
#endif
    SkIRect irect;
    r.round(&irect);
    jclass rectClass = env->FindClass("android/graphics/Rect");
    jmethodID init = env->GetMethodID(rectClass, "<init>", "(IIII)V");
    jobject rect = env->NewObject(rectClass, init, irect.fLeft, irect.fTop,
        irect.fRight, irect.fBottom);
    env->DeleteLocalRef(rectClass);
    return rect;
}

static jobject nativeSubtractLayers(JNIEnv* env, jobject obj, jobject jrect)
{
    SkIRect irect = jrect_to_webrect(env, jrect);
#if USE(ACCELERATED_COMPOSITING)
    LayerAndroid* root = GET_NATIVE_VIEW(env, obj)->compositeRoot();
    if (root) {
        SkRect rect;
        rect.set(irect);
        rect = root->subtractLayers(rect);
        rect.round(&irect);
    }
#endif
    jclass rectClass = env->FindClass("android/graphics/Rect");
    jmethodID init = env->GetMethodID(rectClass, "<init>", "(IIII)V");
    jobject rect = env->NewObject(rectClass, init, irect.fLeft, irect.fTop,
        irect.fRight, irect.fBottom);
    env->DeleteLocalRef(rectClass);
    return rect;
}

static void nativeSetHeightCanMeasure(JNIEnv *env, jobject obj, bool measure)
{
    WebView* view = GET_NATIVE_VIEW(env, obj);
    ALOG_ASSERT(view, "view not set in nativeSetHeightCanMeasure");
    view->setHeightCanMeasure(measure);
}

static void nativeDestroy(JNIEnv *env, jobject obj)
{
    WebView* view = GET_NATIVE_VIEW(env, obj);
    ALOGD("nativeDestroy view: %p", view);
    ALOG_ASSERT(view, "view not set in nativeDestroy");
    delete view;
}

static void nativeStopGL(JNIEnv *env, jobject obj)
{
    GET_NATIVE_VIEW(env, obj)->stopGL();
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
        return true;
    }
    else if (key == "use_minimal_memory") {
        TilesManager::instance()->setUseMinimalMemory(value == "true");
        return true;
    }
    else if (key == "use_double_buffering") {
        TilesManager::instance()->setUseDoubleBuffering(value == "true");
        return true;
    }
    else if (key == "tree_updates") {
        TilesManager::instance()->clearContentUpdates();
        return true;
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
        if (level >= TRIM_MEMORY_MODERATE
            && !tilesManager->highEndGfx()) {
            ALOGD("OnTrimMemory with EGL Context %p", eglGetCurrentContext());
            tilesManager->transferQueue()->emptyQueue();
            tilesManager->shader()->cleanupGLResources();
            tilesManager->videoLayerManager()->cleanupGLResources();
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
            view->draw(&canvas, 0, WebView::DrawExtrasNone, false);
            // we're done with the file now
            fwrite("\n", 1, 1, file);
            fclose(file);
        }
#if USE(ACCELERATED_COMPOSITING)
        const LayerAndroid* rootLayer = view->compositeRoot();
        if (rootLayer) {
          FILE* file = fopen(LAYERS_TREE_LOG_FILE,"w");
          if (file) {
              rootLayer->dumpLayers(file, 0);
              fclose(file);
          }
        }
#endif
    }
#endif
}

static int nativeScrollableLayer(JNIEnv* env, jobject jwebview, jint x, jint y,
    jobject rect, jobject bounds)
{
    WebView* view = GET_NATIVE_VIEW(env, jwebview);
    ALOG_ASSERT(view, "view not set in %s", __FUNCTION__);
    SkIRect nativeRect, nativeBounds;
    int id = view->scrollableLayer(x, y, &nativeRect, &nativeBounds);
    if (rect)
        GraphicsJNI::irect_to_jrect(nativeRect, env, rect);
    if (bounds)
        GraphicsJNI::irect_to_jrect(nativeBounds, env, bounds);
    return id;
}

static bool nativeScrollLayer(JNIEnv* env, jobject obj, jint layerId, jint x,
        jint y)
{
#if ENABLE(ANDROID_OVERFLOW_SCROLL)
    WebView* view = GET_NATIVE_VIEW(env, obj);
    view->scrollLayer(layerId, x, y);

    //TODO: the below only needed for the SW rendering path
    LayerAndroid* root = view->compositeRoot();
    if (!root)
        return false;
    LayerAndroid* layer = root->findById(layerId);
    if (!layer || !layer->contentIsScrollable())
        return false;
    return static_cast<ScrollableLayerAndroid*>(layer)->scrollTo(x, y);
#endif
    return false;
}

static void nativeSetIsScrolling(JNIEnv* env, jobject jwebview, jboolean isScrolling)
{
    WebView* view = GET_NATIVE_VIEW(env, jwebview);
    ALOG_ASSERT(view, "view not set in %s", __FUNCTION__);
    view->setIsScrolling(isScrolling);
}

static void nativeUseHardwareAccelSkia(JNIEnv*, jobject, jboolean enabled)
{
    BaseRenderer::setCurrentRendererType(enabled ? BaseRenderer::Ganesh : BaseRenderer::Raster);
}

static int nativeGetBackgroundColor(JNIEnv* env, jobject obj)
{
    WebView* view = GET_NATIVE_VIEW(env, obj);
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
    ((WebView*)nativeView)->m_isDrawingPaused = pause;
}

static void nativeSetTextSelection(JNIEnv *env, jobject obj, jint nativeView,
                                   jint selectionPtr)
{
    SelectText* selection = reinterpret_cast<SelectText*>(selectionPtr);
    reinterpret_cast<WebView*>(nativeView)->setTextSelection(selection);
}

static jint nativeGetHandleLayerId(JNIEnv *env, jobject obj, jint nativeView,
                                     jint handleIndex, jobject cursorRect)
{
    WebView* webview = reinterpret_cast<WebView*>(nativeView);
    SkIRect nativeRect;
    int layerId = webview->getHandleLayerId((SelectText::HandleId) handleIndex, nativeRect);
    if (cursorRect)
        GraphicsJNI::irect_to_jrect(nativeRect, env, cursorRect);
    return layerId;
}

static jboolean nativeIsBaseFirst(JNIEnv *env, jobject obj, jint nativeView)
{
    WebView* webview = reinterpret_cast<WebView*>(nativeView);
    SelectText* select = static_cast<SelectText*>(
            webview->getDrawExtra(WebView::DrawExtrasSelection));
    return select ? select->isBaseFirst() : false;
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

/*
 * JNI registration
 */
static JNINativeMethod gJavaWebViewMethods[] = {
    { "nativeCreate", "(ILjava/lang/String;Z)V",
        (void*) nativeCreate },
    { "nativeDestroy", "()V",
        (void*) nativeDestroy },
    { "nativeDraw", "(Landroid/graphics/Canvas;Landroid/graphics/RectF;IIZ)I",
        (void*) nativeDraw },
    { "nativeGetDrawGLFunction", "(ILandroid/graphics/Rect;Landroid/graphics/Rect;Landroid/graphics/RectF;FI)I",
        (void*) nativeGetDrawGLFunction },
    { "nativeUpdateDrawGLFunction", "(Landroid/graphics/Rect;Landroid/graphics/Rect;Landroid/graphics/RectF;F)V",
        (void*) nativeUpdateDrawGLFunction },
    { "nativeDumpDisplayTree", "(Ljava/lang/String;)V",
        (void*) nativeDumpDisplayTree },
    { "nativeEvaluateLayersAnimations", "(I)Z",
        (void*) nativeEvaluateLayersAnimations },
    { "nativeGetSelection", "()Ljava/lang/String;",
        (void*) nativeGetSelection },
    { "nativeLayerBounds", "(I)Landroid/graphics/Rect;",
        (void*) nativeLayerBounds },
    { "nativeSetHeightCanMeasure", "(Z)V",
        (void*) nativeSetHeightCanMeasure },
    { "nativeSetBaseLayer", "(IILandroid/graphics/Region;ZZ)Z",
        (void*) nativeSetBaseLayer },
    { "nativeGetBaseLayer", "()I",
        (void*) nativeGetBaseLayer },
    { "nativeReplaceBaseContent", "(I)V",
        (void*) nativeReplaceBaseContent },
    { "nativeCopyBaseContentToPicture", "(Landroid/graphics/Picture;)V",
        (void*) nativeCopyBaseContentToPicture },
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
    { "nativeStopGL", "()V",
        (void*) nativeStopGL },
    { "nativeSubtractLayers", "(Landroid/graphics/Rect;)Landroid/graphics/Rect;",
        (void*) nativeSubtractLayers },
    { "nativeScrollableLayer", "(IILandroid/graphics/Rect;Landroid/graphics/Rect;)I",
        (void*) nativeScrollableLayer },
    { "nativeScrollLayer", "(III)Z",
        (void*) nativeScrollLayer },
    { "nativeSetIsScrolling", "(Z)V",
        (void*) nativeSetIsScrolling },
    { "nativeUseHardwareAccelSkia", "(Z)V",
        (void*) nativeUseHardwareAccelSkia },
    { "nativeGetBackgroundColor", "()I",
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
    { "nativeGetHandleLayerId", "(IILandroid/graphics/Rect;)I",
        (void*) nativeGetHandleLayerId },
    { "nativeIsBaseFirst", "(I)Z",
        (void*) nativeIsBaseFirst },
    { "nativeMapLayerRect", "(IILandroid/graphics/Rect;)V",
        (void*) nativeMapLayerRect },
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
