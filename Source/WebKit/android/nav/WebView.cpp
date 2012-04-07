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
#include "CachedFrame.h"
#include "CachedNode.h"
#include "CachedRoot.h"
#include "DrawExtra.h"
#include "FindCanvas.h"
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
#ifdef ANDROID_INSTRUMENT
#include "TimeCounter.h"
#endif
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
#include <ui/KeycodeLabels.h>
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
    LOG_ASSERT(m, "Could not find method %s", name);
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

enum DrawExtras { // keep this in sync with WebView.java
    DrawExtrasNone = 0,
    DrawExtrasFind = 1,
    DrawExtrasSelection = 2,
    DrawExtrasCursorRing = 3
};

struct JavaGlue {
    jweak       m_obj;
    jmethodID   m_overrideLoading;
    jmethodID   m_scrollBy;
    jmethodID   m_sendMoveFocus;
    jmethodID   m_sendMoveMouse;
    jmethodID   m_sendMoveMouseIfLatest;
    jmethodID   m_sendMotionUp;
    jmethodID   m_domChangedFocus;
    jmethodID   m_getScaledMaxXScroll;
    jmethodID   m_getScaledMaxYScroll;
    jmethodID   m_getVisibleRect;
    jmethodID   m_rebuildWebTextView;
    jmethodID   m_viewInvalidate;
    jmethodID   m_viewInvalidateRect;
    jmethodID   m_postInvalidateDelayed;
    jmethodID   m_pageSwapCallback;
    jmethodID   m_inFullScreenMode;
    jfieldID    m_rectLeft;
    jfieldID    m_rectTop;
    jmethodID   m_rectWidth;
    jmethodID   m_rectHeight;
    jfieldID    m_rectFLeft;
    jfieldID    m_rectFTop;
    jmethodID   m_rectFWidth;
    jmethodID   m_rectFHeight;
    jmethodID   m_getTextHandleScale;
    AutoJObject object(JNIEnv* env) {
        return getRealObject(env, m_obj);
    }
} m_javaGlue;

WebView(JNIEnv* env, jobject javaWebView, int viewImpl, WTF::String drawableDir,
        bool isHighEndGfx) :
    m_ring((WebViewCore*) viewImpl)
    , m_isHighEndGfx(isHighEndGfx)
{
    jclass clazz = env->FindClass("android/webkit/WebView");
 //   m_javaGlue = new JavaGlue;
    m_javaGlue.m_obj = env->NewWeakGlobalRef(javaWebView);
    m_javaGlue.m_scrollBy = GetJMethod(env, clazz, "setContentScrollBy", "(IIZ)Z");
    m_javaGlue.m_overrideLoading = GetJMethod(env, clazz, "overrideLoading", "(Ljava/lang/String;)V");
    m_javaGlue.m_sendMoveFocus = GetJMethod(env, clazz, "sendMoveFocus", "(II)V");
    m_javaGlue.m_sendMoveMouse = GetJMethod(env, clazz, "sendMoveMouse", "(IIII)V");
    m_javaGlue.m_sendMoveMouseIfLatest = GetJMethod(env, clazz, "sendMoveMouseIfLatest", "(ZZ)V");
    m_javaGlue.m_sendMotionUp = GetJMethod(env, clazz, "sendMotionUp", "(IIIII)V");
    m_javaGlue.m_domChangedFocus = GetJMethod(env, clazz, "domChangedFocus", "()V");
    m_javaGlue.m_getScaledMaxXScroll = GetJMethod(env, clazz, "getScaledMaxXScroll", "()I");
    m_javaGlue.m_getScaledMaxYScroll = GetJMethod(env, clazz, "getScaledMaxYScroll", "()I");
    m_javaGlue.m_getVisibleRect = GetJMethod(env, clazz, "sendOurVisibleRect", "()Landroid/graphics/Rect;");
    m_javaGlue.m_rebuildWebTextView = GetJMethod(env, clazz, "rebuildWebTextView", "()V");
    m_javaGlue.m_viewInvalidate = GetJMethod(env, clazz, "viewInvalidate", "()V");
    m_javaGlue.m_viewInvalidateRect = GetJMethod(env, clazz, "viewInvalidate", "(IIII)V");
    m_javaGlue.m_postInvalidateDelayed = GetJMethod(env, clazz,
        "viewInvalidateDelayed", "(JIIII)V");
    m_javaGlue.m_pageSwapCallback = GetJMethod(env, clazz, "pageSwapCallback", "(Z)V");
    m_javaGlue.m_inFullScreenMode = GetJMethod(env, clazz, "inFullScreenMode", "()Z");
    m_javaGlue.m_getTextHandleScale = GetJMethod(env, clazz, "getTextHandleScale", "()F");
    env->DeleteLocalRef(clazz);

    jclass rectClass = env->FindClass("android/graphics/Rect");
    LOG_ASSERT(rectClass, "Could not find Rect class");
    m_javaGlue.m_rectLeft = env->GetFieldID(rectClass, "left", "I");
    m_javaGlue.m_rectTop = env->GetFieldID(rectClass, "top", "I");
    m_javaGlue.m_rectWidth = GetJMethod(env, rectClass, "width", "()I");
    m_javaGlue.m_rectHeight = GetJMethod(env, rectClass, "height", "()I");
    env->DeleteLocalRef(rectClass);

    jclass rectClassF = env->FindClass("android/graphics/RectF");
    LOG_ASSERT(rectClassF, "Could not find RectF class");
    m_javaGlue.m_rectFLeft = env->GetFieldID(rectClassF, "left", "F");
    m_javaGlue.m_rectFTop = env->GetFieldID(rectClassF, "top", "F");
    m_javaGlue.m_rectFWidth = GetJMethod(env, rectClassF, "width", "()F");
    m_javaGlue.m_rectFHeight = GetJMethod(env, rectClassF, "height", "()F");
    env->DeleteLocalRef(rectClassF);

    env->SetIntField(javaWebView, gWebViewField, (jint)this);
    m_viewImpl = (WebViewCore*) viewImpl;
    m_frameCacheUI = 0;
    m_navPictureUI = 0;
    m_generation = 0;
    m_heightCanMeasure = false;
    m_lastDx = 0;
    m_lastDxTime = 0;
    m_ringAnimationEnd = 0;
    m_baseLayer = 0;
    m_glDrawFunctor = 0;
    m_isDrawingPaused = false;
    m_buttonSkin = drawableDir.isEmpty() ? 0 : new RenderSkinButton(drawableDir);
#if USE(ACCELERATED_COMPOSITING)
    m_glWebViewState = 0;
    m_pageSwapCallbackRegistered = false;
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
    delete m_frameCacheUI;
    delete m_navPictureUI;
    SkSafeUnref(m_baseLayer);
    delete m_glDrawFunctor;
    delete m_buttonSkin;
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

float getTextHandleScale()
{
    LOG_ASSERT(m_javaGlue.m_obj, "A java object was not associated with this native WebView!");
    JNIEnv* env = JSC::Bindings::getJNIEnv();
    AutoJObject javaObject = m_javaGlue.object(env);
    if (!javaObject.get())
        return 0;
    float result = env->CallFloatMethod(javaObject.get(), m_javaGlue.m_getTextHandleScale);
    checkException(env);
    return result;
}

void updateSelectionHandles()
{
    if (!m_baseLayer)
        return;
    // Adjust for device density & scale
    m_selectText.updateHandleScale(getTextHandleScale());
}

// removes the cursor altogether (e.g., when going to a new page)
void clearCursor()
{
    CachedRoot* root = getFrameCache(AllowNewer);
    if (!root)
        return;
    DBG_NAV_LOG("");
    m_viewImpl->m_hasCursorBounds = false;
    root->clearCursor();
    viewInvalidate();
}

// leaves the cursor where it is, but suppresses drawing it
void hideCursor()
{
    CachedRoot* root = getFrameCache(AllowNewer);
    if (!root)
        return;
    DBG_NAV_LOG("");
    hideCursor(root);
    viewInvalidate();
}

void hideCursor(CachedRoot* root)
{
    DBG_NAV_LOG("inner");
    m_viewImpl->m_hasCursorBounds = false;
    root->hideCursor();
}

#if DUMP_NAV_CACHE
void debugDump()
{
    CachedRoot* root = getFrameCache(DontAllowNewer);
    if (root)
        root->mDebug.print();
}
#endif

void scrollToCurrentMatch()
{
    if (!m_findOnPage.currentMatchIsInLayer()) {
        scrollRectOnScreen(m_findOnPage.currentMatchBounds());
        return;
    }

    SkRect matchBounds = m_findOnPage.currentMatchBounds();
    LayerAndroid* rootLayer = getFrameCache(DontAllowNewer)->rootLayer();
    Layer* layerContainingMatch = rootLayer->findById(m_findOnPage.currentMatchLayerId());
    ASSERT(layerContainingMatch);

    // If the match is in a fixed position layer, there's nothing to do.
    if (layerContainingMatch->shouldInheritFromRootTransform())
        return;

    // If the match is in a scrollable layer or a descendant of such a layer,
    // there may be a range of of scroll configurations that will make the
    // current match visible. Our approach is the simplest possible. Starting at
    // the layer in which the match is found, we move up the layer tree,
    // scrolling any scrollable layers as little as possible to make sure that
    // the current match is in view. This approach has the disadvantage that we
    // may end up scrolling a larger number of elements than is necessary, which
    // may be visually jarring. However, minimising the number of layers
    // scrolled would complicate the code significantly.

    bool didScrollLayer = false;
    for (Layer* layer = layerContainingMatch; layer; layer = layer->getParent()) {
        ASSERT(layer->getParent() || layer == rootLayer);

        if (layer->contentIsScrollable()) {
            // Convert the match location to layer's local space and scroll it.
            // Repeatedly calling Layer::localToAncestor() is inefficient as
            // each call repeats part of the calculation. It would be more
            // efficient to maintain the transform here and update it on each
            // iteration, but that would mean duplicating logic from
            // Layer::localToAncestor() and would complicate things.
            SkMatrix transform;
            layerContainingMatch->localToAncestor(layer, &transform);
            SkRect transformedMatchBounds;
            transform.mapRect(&transformedMatchBounds, matchBounds);
            SkIRect roundedTransformedMatchBounds;
            transformedMatchBounds.roundOut(&roundedTransformedMatchBounds);
            // Only ScrollableLayerAndroid returns true for contentIsScrollable().
            didScrollLayer |= static_cast<ScrollableLayerAndroid*>(layer)->scrollRectIntoView(roundedTransformedMatchBounds);
        }
    }
    // Invalidate, as the call below to scroll the main page may be a no-op.
    if (didScrollLayer)
        viewInvalidate();

    // Convert matchBounds to the global space so we can scroll the main page.
    SkMatrix transform;
    layerContainingMatch->localToGlobal(&transform);
    SkRect transformedMatchBounds;
    transform.mapRect(&transformedMatchBounds, matchBounds);
    SkIRect roundedTransformedMatchBounds;
    transformedMatchBounds.roundOut(&roundedTransformedMatchBounds);
    scrollRectOnScreen(roundedTransformedMatchBounds);
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

void resetCursorRing()
{
    m_ringAnimationEnd = 0;
    m_viewImpl->m_hasCursorBounds = false;
}

bool drawCursorPreamble(CachedRoot* root)
{
    if (!root) return false;
    const CachedFrame* frame;
    const CachedNode* node = root->currentCursor(&frame);
    if (!node) {
        DBG_NAV_LOGV("%s", "!node");
        resetCursorRing();
        return false;
    }
    m_ring.setIsButton(node);
    if (node->isHidden()) {
        DBG_NAV_LOG("node->isHidden()");
        m_viewImpl->m_hasCursorBounds = false;
        return false;
    }
#if USE(ACCELERATED_COMPOSITING)
    if (node->isInLayer() && root->rootLayer()) {
        LayerAndroid* layer = root->rootLayer();
        layer->updateFixedLayersPositions(m_visibleRect);
        layer->updatePositions();
    }
#endif
    setVisibleRect(root);
    m_ring.m_root = root;
    m_ring.m_frame = frame;
    m_ring.m_node = node;
    SkMSec time = SkTime::GetMSecs();
    m_ring.m_isPressed = time < m_ringAnimationEnd
        && m_ringAnimationEnd != UINT_MAX;
    return true;
}

void drawCursorPostamble()
{
    if (m_ringAnimationEnd == UINT_MAX)
        return;
    SkMSec time = SkTime::GetMSecs();
    if (time < m_ringAnimationEnd) {
        // views assume that inval bounds coordinates are non-negative
        WebCore::IntRect invalBounds(0, 0, INT_MAX, INT_MAX);
        invalBounds.intersect(m_ring.m_absBounds);
        postInvalidateDelayed(m_ringAnimationEnd - time, invalBounds);
    } else {
        hideCursor(const_cast<CachedRoot*>(m_ring.m_root));
    }
}

bool drawGL(WebCore::IntRect& viewRect, WebCore::IntRect* invalRect,
        WebCore::IntRect& webViewRect, int titleBarHeight,
        WebCore::IntRect& clip, float scale, int extras)
{
#if USE(ACCELERATED_COMPOSITING)
    if (!m_baseLayer || inFullScreenMode())
        return false;

    if (!m_glWebViewState) {
        m_glWebViewState = new GLWebViewState();
        m_glWebViewState->setHighEndGfx(m_isHighEndGfx);
        m_glWebViewState->glExtras()->setCursorRingExtra(&m_ring);
        m_glWebViewState->glExtras()->setFindOnPageExtra(&m_findOnPage);
        if (m_baseLayer->content()) {
            SkRegion region;
            SkIRect rect;
            rect.set(0, 0, m_baseLayer->content()->width(), m_baseLayer->content()->height());
            region.setRect(rect);
            m_glWebViewState->setBaseLayer(m_baseLayer, region, false, true);
        }
    }

    CachedRoot* root = getFrameCache(AllowNewer);
    if (!root) {
        DBG_NAV_LOG("!root");
        if (extras == DrawExtrasCursorRing)
            resetCursorRing();
    }
    DrawExtra* extra = 0;
    switch (extras) {
        case DrawExtrasFind:
            extra = &m_findOnPage;
            break;
        case DrawExtrasSelection:
            // This will involve a JNI call, but under normal circumstances we will
            // not hit this anyway. Only if USE_JAVA_TEXT_SELECTION is disabled
            // in WebView.java will we hit this (so really debug only)
            updateSelectionHandles();
            extra = &m_selectText;
            break;
        case DrawExtrasCursorRing:
            if (drawCursorPreamble(root) && m_ring.setup()) {
                if (m_ring.m_isPressed || m_ringAnimationEnd == UINT_MAX)
                    extra = &m_ring;
                drawCursorPostamble();
            }
            break;
        default:
            ;
    }

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
    if (treesSwapped && (m_pageSwapCallbackRegistered || newTreeHasAnim)) {
        m_pageSwapCallbackRegistered = false;
        LOG_ASSERT(m_javaGlue.m_obj, "A java object was not associated with this native WebView!");
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

PictureSet* draw(SkCanvas* canvas, SkColor bgColor, int extras, bool split)
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

    CachedRoot* root = getFrameCache(AllowNewer);
    if (!root) {
        DBG_NAV_LOG("!root");
        if (extras == DrawExtrasCursorRing)
            resetCursorRing();
    }
    LayerAndroid mainPicture(m_navPictureUI);
    DrawExtra* extra = 0;
    switch (extras) {
        case DrawExtrasFind:
            extra = &m_findOnPage;
            break;
        case DrawExtrasSelection:
            // This will involve a JNI call, but under normal circumstances we will
            // not hit this anyway. Only if USE_JAVA_TEXT_SELECTION is disabled
            // in WebView.java will we hit this (so really debug only)
            updateSelectionHandles();
            extra = &m_selectText;
            break;
        case DrawExtrasCursorRing:
            if (drawCursorPreamble(root) && m_ring.setup()) {
                extra = &m_ring;
                drawCursorPostamble();
            }
            break;
        default:
            ;
    }
#if USE(ACCELERATED_COMPOSITING)
    LayerAndroid* compositeLayer = compositeRoot();
    if (compositeLayer) {
        // call this to be sure we've adjusted for any scrolling or animations
        // before we actually draw
        compositeLayer->updateFixedLayersPositions(m_visibleRect);
        compositeLayer->updatePositions();
        // We have to set the canvas' matrix on the base layer
        // (to have fixed layers work as intended)
        SkAutoCanvasRestore restore(canvas, true);
        m_baseLayer->setMatrix(canvas->getTotalMatrix());
        canvas->resetMatrix();
        m_baseLayer->draw(canvas);
    }
#endif
    if (extra) {
        IntRect dummy; // inval area, unused for now
        extra->draw(canvas, &mainPicture, &dummy);
    }
    return ret;
}


bool cursorIsTextInput(FrameCachePermission allowNewer)
{
    CachedRoot* root = getFrameCache(allowNewer);
    if (!root) {
        DBG_NAV_LOG("!root");
        return false;
    }
    const CachedNode* cursor = root->currentCursor();
    if (!cursor) {
        DBG_NAV_LOG("!cursor");
        return false;
    }
    DBG_NAV_LOGD("%s", cursor->isTextInput() ? "true" : "false");
    return cursor->isTextInput();
}

void cursorRingBounds(WebCore::IntRect* bounds)
{
    DBG_NAV_LOGD("%s", "");
    CachedRoot* root = getFrameCache(DontAllowNewer);
    if (root) {
        const CachedFrame* cachedFrame;
        const CachedNode* cachedNode = root->currentCursor(&cachedFrame);
        if (cachedNode) {
            *bounds = cachedNode->cursorRingBounds(cachedFrame);
            DBG_NAV_LOGD("bounds={%d,%d,%d,%d}", bounds->x(), bounds->y(),
                bounds->width(), bounds->height());
            return;
        }
    }
    *bounds = WebCore::IntRect(0, 0, 0, 0);
}

void fixCursor()
{
    m_viewImpl->gCursorBoundsMutex.lock();
    bool hasCursorBounds = m_viewImpl->m_hasCursorBounds;
    IntRect bounds = m_viewImpl->m_cursorBounds;
    m_viewImpl->gCursorBoundsMutex.unlock();
    if (!hasCursorBounds)
        return;
    int x, y;
    const CachedFrame* frame;
    const CachedNode* node = m_frameCacheUI->findAt(bounds, &frame, &x, &y, true);
    if (!node)
        return;
    // require that node have approximately the same bounds (+/- 4) and the same
    // center (+/- 2)
    IntPoint oldCenter = IntPoint(bounds.x() + (bounds.width() >> 1),
        bounds.y() + (bounds.height() >> 1));
    IntRect newBounds = node->bounds(frame);
    IntPoint newCenter = IntPoint(newBounds.x() + (newBounds.width() >> 1),
        newBounds.y() + (newBounds.height() >> 1));
    DBG_NAV_LOGD("oldCenter=(%d,%d) newCenter=(%d,%d)"
        " bounds=(%d,%d,w=%d,h=%d) newBounds=(%d,%d,w=%d,h=%d)",
        oldCenter.x(), oldCenter.y(), newCenter.x(), newCenter.y(),
        bounds.x(), bounds.y(), bounds.width(), bounds.height(),
        newBounds.x(), newBounds.y(), newBounds.width(), newBounds.height());
    if (abs(oldCenter.x() - newCenter.x()) > 2)
        return;
    if (abs(oldCenter.y() - newCenter.y()) > 2)
        return;
    if (abs(bounds.x() - newBounds.x()) > 4)
        return;
    if (abs(bounds.y() - newBounds.y()) > 4)
        return;
    if (abs(bounds.maxX() - newBounds.maxX()) > 4)
        return;
    if (abs(bounds.maxY() - newBounds.maxY()) > 4)
        return;
    DBG_NAV_LOGD("node=%p frame=%p x=%d y=%d bounds=(%d,%d,w=%d,h=%d)",
        node, frame, x, y, bounds.x(), bounds.y(), bounds.width(),
        bounds.height());
    m_frameCacheUI->setCursor(const_cast<CachedFrame*>(frame),
        const_cast<CachedNode*>(node));
}

CachedRoot* getFrameCache(FrameCachePermission allowNewer)
{
    if (!m_viewImpl->m_updatedFrameCache) {
        DBG_NAV_LOGV("%s", "!m_viewImpl->m_updatedFrameCache");
        return m_frameCacheUI;
    }
    if (allowNewer == DontAllowNewer && m_viewImpl->m_lastGeneration < m_generation) {
        DBG_NAV_LOGD("allowNewer==DontAllowNewer m_viewImpl->m_lastGeneration=%d"
            " < m_generation=%d", m_viewImpl->m_lastGeneration, m_generation);
        return m_frameCacheUI;
    }
    DBG_NAV_LOGD("%s", "m_viewImpl->m_updatedFrameCache == true");
    const CachedFrame* oldCursorFrame;
    const CachedNode* oldCursorNode = m_frameCacheUI ?
        m_frameCacheUI->currentCursor(&oldCursorFrame) : 0;
#if USE(ACCELERATED_COMPOSITING)
    int layerId = -1;
    if (oldCursorNode && oldCursorNode->isInLayer()) {
        const LayerAndroid* cursorLayer = oldCursorFrame->layer(oldCursorNode)
            ->layer(m_frameCacheUI->rootLayer());
        if (cursorLayer)
            layerId = cursorLayer->uniqueId();
    }
#endif
    // get id from old layer and use to find new layer
    bool oldFocusIsTextInput = false;
    void* oldFocusNodePointer = 0;
    if (m_frameCacheUI) {
        const CachedNode* oldFocus = m_frameCacheUI->currentFocus();
        if (oldFocus) {
            oldFocusIsTextInput = oldFocus->isTextInput();
            oldFocusNodePointer = oldFocus->nodePointer();
        }
    }
    m_viewImpl->gFrameCacheMutex.lock();
    delete m_frameCacheUI;
    SkSafeUnref(m_navPictureUI);
    m_viewImpl->m_updatedFrameCache = false;
    m_frameCacheUI = m_viewImpl->m_frameCacheKit;
    m_navPictureUI = m_viewImpl->m_navPictureKit;
    m_viewImpl->m_frameCacheKit = 0;
    m_viewImpl->m_navPictureKit = 0;
    m_viewImpl->gFrameCacheMutex.unlock();
    if (m_frameCacheUI)
        m_frameCacheUI->setRootLayer(compositeRoot());
#if USE(ACCELERATED_COMPOSITING)
    if (layerId >= 0) {
        LayerAndroid* layer = const_cast<LayerAndroid*>(
                                                m_frameCacheUI->rootLayer());
        if (layer) {
            layer->updateFixedLayersPositions(m_visibleRect);
            layer->updatePositions();
        }
    }
#endif
    fixCursor();
    if (oldFocusIsTextInput) {
        const CachedNode* newFocus = m_frameCacheUI->currentFocus();
        if (newFocus && oldFocusNodePointer != newFocus->nodePointer()
                && newFocus->isTextInput()
                && newFocus != m_frameCacheUI->currentCursor()) {
            // The focus has changed.  We may need to update things.
            LOG_ASSERT(m_javaGlue.m_obj, "A java object was not associated with this native WebView!");
            JNIEnv* env = JSC::Bindings::getJNIEnv();
            AutoJObject javaObject = m_javaGlue.object(env);
            if (javaObject.get()) {
                env->CallVoidMethod(javaObject.get(), m_javaGlue.m_domChangedFocus);
                checkException(env);
            }
        }
    }
    if (oldCursorNode && (!m_frameCacheUI || !m_frameCacheUI->currentCursor()))
        viewInvalidate(); // redraw in case cursor ring is still visible
    return m_frameCacheUI;
}

int getScaledMaxXScroll()
{
    LOG_ASSERT(m_javaGlue.m_obj, "A java object was not associated with this native WebView!");
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
    LOG_ASSERT(m_javaGlue.m_obj, "A java object was not associated with this native WebView!");
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
    LOG_ASSERT(m_javaGlue.m_obj, "A java object was not associated with this native WebView!");
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

static CachedFrame::Direction KeyToDirection(int32_t keyCode)
{
    switch (keyCode) {
        case AKEYCODE_DPAD_RIGHT:
            DBG_NAV_LOGD("keyCode=%s", "right");
            return CachedFrame::RIGHT;
        case AKEYCODE_DPAD_LEFT:
            DBG_NAV_LOGD("keyCode=%s", "left");
            return CachedFrame::LEFT;
        case AKEYCODE_DPAD_DOWN:
            DBG_NAV_LOGD("keyCode=%s", "down");
            return CachedFrame::DOWN;
        case AKEYCODE_DPAD_UP:
            DBG_NAV_LOGD("keyCode=%s", "up");
            return CachedFrame::UP;
        default:
            DBG_NAV_LOGD("bad key %d sent", keyCode);
            return CachedFrame::UNINITIALIZED;
    }
}

WTF::String imageURI(int x, int y)
{
    const CachedRoot* root = getFrameCache(DontAllowNewer);
    return root ? root->imageURI(x, y) : WTF::String();
}

bool cursorWantsKeyEvents()
{
    const CachedRoot* root = getFrameCache(DontAllowNewer);
    if (root) {
        const CachedNode* focus = root->currentCursor();
        if (focus)
            return focus->wantsKeyEvents();
    }
    return false;
}


/* returns true if the key had no effect (neither scrolled nor changed cursor) */
bool moveCursor(int keyCode, int count, bool ignoreScroll)
{
    CachedRoot* root = getFrameCache(AllowNewer);
    if (!root) {
        DBG_NAV_LOG("!root");
        return true;
    }

    m_viewImpl->m_moveGeneration++;
    CachedFrame::Direction direction = KeyToDirection(keyCode);
    const CachedFrame* cachedFrame, * oldFrame = 0;
    const CachedNode* cursor = root->currentCursor(&oldFrame);
    WebCore::IntPoint cursorLocation = root->cursorLocation();
    DBG_NAV_LOGD("old cursor %d (nativeNode=%p) cursorLocation={%d, %d}",
        cursor ? cursor->index() : 0,
        cursor ? cursor->nodePointer() : 0, cursorLocation.x(), cursorLocation.y());
    WebCore::IntRect visibleRect = setVisibleRect(root);
    int xMax = getScaledMaxXScroll();
    int yMax = getScaledMaxYScroll();
    root->setMaxScroll(xMax, yMax);
    const CachedNode* cachedNode = 0;
    int dx = 0;
    int dy = 0;
    int counter = count;
    while (--counter >= 0) {
        WebCore::IntPoint scroll = WebCore::IntPoint(0, 0);
        cachedNode = root->moveCursor(direction, &cachedFrame, &scroll);
        dx += scroll.x();
        dy += scroll.y();
    }
    DBG_NAV_LOGD("new cursor %d (nativeNode=%p) cursorLocation={%d, %d}"
        "bounds={%d,%d,w=%d,h=%d}", cachedNode ? cachedNode->index() : 0,
        cachedNode ? cachedNode->nodePointer() : 0,
            root->cursorLocation().x(), root->cursorLocation().y(),
            cachedNode ? cachedNode->bounds(cachedFrame).x() : 0,
            cachedNode ? cachedNode->bounds(cachedFrame).y() : 0,
            cachedNode ? cachedNode->bounds(cachedFrame).width() : 0,
            cachedNode ? cachedNode->bounds(cachedFrame).height() : 0);
    // If !m_heightCanMeasure (such as in the browser), we want to scroll no
    // matter what
    if (!ignoreScroll && (!m_heightCanMeasure ||
            !cachedNode ||
            (cursor && cursor->nodePointer() == cachedNode->nodePointer())))
    {
        if (count == 1 && dx != 0 && dy == 0 && -m_lastDx == dx &&
                SkTime::GetMSecs() - m_lastDxTime < 1000)
            root->checkForJiggle(&dx);
        DBG_NAV_LOGD("scrollBy %d,%d", dx, dy);
        if ((dx | dy))
            this->scrollBy(dx, dy);
        m_lastDx = dx;
        m_lastDxTime = SkTime::GetMSecs();
    }
    bool result = false;
    if (cachedNode) {
        showCursorUntimed();
        m_viewImpl->updateCursorBounds(root, cachedFrame, cachedNode);
        root->setCursor(const_cast<CachedFrame*>(cachedFrame),
                const_cast<CachedNode*>(cachedNode));
        const CachedNode* focus = root->currentFocus();
        bool clearTextEntry = cachedNode != focus && focus
                && cachedNode->nodePointer() != focus->nodePointer() && focus->isTextInput();
        // Stop painting the caret if the old focus was a text input and so is the new cursor.
        bool stopPaintingCaret = clearTextEntry && cachedNode->wantsKeyEvents();
        sendMoveMouseIfLatest(clearTextEntry, stopPaintingCaret);
    } else {
        int docHeight = root->documentHeight();
        int docWidth = root->documentWidth();
        if (visibleRect.maxY() + dy > docHeight)
            dy = docHeight - visibleRect.maxY();
        else if (visibleRect.y() + dy < 0)
            dy = -visibleRect.y();
        if (visibleRect.maxX() + dx > docWidth)
            dx = docWidth - visibleRect.maxX();
        else if (visibleRect.x() < 0)
            dx = -visibleRect.x();
        result = direction == CachedFrame::LEFT ? dx >= 0 :
            direction == CachedFrame::RIGHT ? dx <= 0 :
            direction == CachedFrame::UP ? dy >= 0 : dy <= 0;
    }
    return result;
}

void notifyProgressFinished()
{
    DBG_NAV_LOGD("cursorIsTextInput=%d", cursorIsTextInput(DontAllowNewer));
    rebuildWebTextView();
#if DEBUG_NAV_UI
    if (m_frameCacheUI) {
        const CachedNode* focus = m_frameCacheUI->currentFocus();
        DBG_NAV_LOGD("focus %d (nativeNode=%p)",
            focus ? focus->index() : 0,
            focus ? focus->nodePointer() : 0);
    }
#endif
}

const CachedNode* findAt(CachedRoot* root, const WebCore::IntRect& rect,
    const CachedFrame** framePtr, int* rxPtr, int* ryPtr)
{
    *rxPtr = 0;
    *ryPtr = 0;
    *framePtr = 0;
    if (!root)
        return 0;
    setVisibleRect(root);
    return root->findAt(rect, framePtr, rxPtr, ryPtr, true);
}

IntRect setVisibleRect(CachedRoot* root)
{
    IntRect visibleRect = getVisibleRect();
    DBG_NAV_LOGD("getVisibleRect %d,%d,%d,%d",
        visibleRect.x(), visibleRect.y(), visibleRect.width(), visibleRect.height());
    root->setVisibleRect(visibleRect);
    return visibleRect;
}

void selectBestAt(const WebCore::IntRect& rect)
{
    const CachedFrame* frame;
    int rx, ry;
    CachedRoot* root = getFrameCache(AllowNewer);
    if (!root)
        return;
    const CachedNode* node = findAt(root, rect, &frame, &rx, &ry);
    if (!node) {
        DBG_NAV_LOGD("no nodes found root=%p", root);
        root->rootHistory()->setMouseBounds(rect);
        m_viewImpl->m_hasCursorBounds = false;
        root->setCursor(0, 0);
        viewInvalidate();
    } else {
        DBG_NAV_LOGD("CachedNode:%p (%d)", node, node->index());
        WebCore::IntRect bounds = node->bounds(frame);
        root->rootHistory()->setMouseBounds(bounds);
        m_viewImpl->updateCursorBounds(root, frame, node);
        showCursorTimed();
        root->setCursor(const_cast<CachedFrame*>(frame),
                const_cast<CachedNode*>(node));
    }
    sendMoveMouseIfLatest(false, false);
}

const CachedNode* m_cacheHitNode;
const CachedFrame* m_cacheHitFrame;

bool pointInNavCache(int x, int y, int slop)
{
    CachedRoot* root = getFrameCache(AllowNewer);
    if (!root)
        return false;
    IntRect rect = IntRect(x - slop, y - slop, slop * 2, slop * 2);
    int rx, ry;
    return (m_cacheHitNode = findAt(root, rect, &m_cacheHitFrame, &rx, &ry));
}

bool motionUp(int x, int y, int slop)
{
    bool pageScrolled = false;
    IntRect rect = IntRect(x - slop, y - slop, slop * 2, slop * 2);
    int rx, ry;
    CachedRoot* root = getFrameCache(AllowNewer);
    if (!root)
        return 0;
    const CachedFrame* frame = 0;
    const CachedNode* result = findAt(root, rect, &frame, &rx, &ry);
    CachedHistory* history = root->rootHistory();
    if (!result) {
        DBG_NAV_LOGD("no nodes found root=%p", root);
        history->setNavBounds(rect);
        m_viewImpl->m_hasCursorBounds = false;
        root->hideCursor();
        int dx = root->checkForCenter(x, y);
        if (dx) {
            scrollBy(dx, 0);
            pageScrolled = true;
        }
        sendMotionUp(frame ? (WebCore::Frame*) frame->framePointer() : 0,
            0, x, y);
        viewInvalidate();
        return pageScrolled;
    }
    DBG_NAV_LOGD("CachedNode:%p (%d) x=%d y=%d rx=%d ry=%d", result,
        result->index(), x, y, rx, ry);
    WebCore::IntRect navBounds = WebCore::IntRect(rx, ry, 1, 1);
    history->setNavBounds(navBounds);
    history->setMouseBounds(navBounds);
    m_viewImpl->updateCursorBounds(root, frame, result);
    root->setCursor(const_cast<CachedFrame*>(frame),
        const_cast<CachedNode*>(result));
    if (result->isSyntheticLink())
        overrideUrlLoading(result->getExport());
    else {
        sendMotionUp(
            (WebCore::Frame*) frame->framePointer(),
            (WebCore::Node*) result->nodePointer(), rx, ry);
    }
    if (result->isTextInput() || result->isSelect()
            || result->isContentEditable()) {
        showCursorUntimed();
    } else
        showCursorTimed();
    return pageScrolled;
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

int getBlockLeftEdge(int x, int y, float scale)
{
    CachedRoot* root = getFrameCache(AllowNewer);
    if (root)
        return root->getBlockLeftEdge(x, y, scale);
    return -1;
}

void overrideUrlLoading(const WTF::String& url)
{
    JNIEnv* env = JSC::Bindings::getJNIEnv();
    AutoJObject javaObject = m_javaGlue.object(env);
    if (!javaObject.get())
        return;
    jstring jName = wtfStringToJstring(env, url);
    env->CallVoidMethod(javaObject.get(), m_javaGlue.m_overrideLoading, jName);
    env->DeleteLocalRef(jName);
}

void setFindIsUp(bool up)
{
    DBG_NAV_LOGD("up=%d", up);
    m_viewImpl->m_findIsUp = up;
}

void setFindIsEmpty()
{
    DBG_NAV_LOG("");
    m_findOnPage.clearCurrentLocation();
}

void showCursorTimed()
{
    DBG_NAV_LOG("");
    m_ringAnimationEnd = SkTime::GetMSecs() + PRESSED_STATE_DURATION;
    viewInvalidate();
}

void showCursorUntimed()
{
    DBG_NAV_LOG("");
    m_ring.m_isPressed = false;
    m_ringAnimationEnd = UINT_MAX;
    viewInvalidate();
}

void setHeightCanMeasure(bool measure)
{
    m_heightCanMeasure = measure;
}

String getSelection()
{
    return m_selectText.getSelection();
}

void moveSelection(int x, int y)
{
    m_selectText.moveSelection(getVisibleRect(), x, y);
}

IntPoint selectableText()
{
    const CachedRoot* root = getFrameCache(DontAllowNewer);
    if (!root)
        return IntPoint(0, 0);
    return m_selectText.selectableText(root);
}

void selectAll()
{
    m_selectText.selectAll();
}

int selectionX()
{
    return m_selectText.selectionX();
}

int selectionY()
{
    return m_selectText.selectionY();
}

void resetSelection()
{
    m_selectText.reset();
}

bool startSelection(int x, int y)
{
    const CachedRoot* root = getFrameCache(DontAllowNewer);
    if (!root)
        return false;
    updateSelectionHandles();
    return m_selectText.startSelection(root, getVisibleRect(), x, y);
}

bool wordSelection(int x, int y)
{
    const CachedRoot* root = getFrameCache(DontAllowNewer);
    if (!root)
        return false;
    updateSelectionHandles();
    return m_selectText.wordSelection(root, getVisibleRect(), x, y);
}

bool extendSelection(int x, int y)
{
    m_selectText.extendSelection(getVisibleRect(), x, y);
    return true;
}

bool hitSelection(int x, int y)
{
    updateSelectionHandles();
    return m_selectText.hitSelection(x, y);
}

void setExtendSelection()
{
    m_selectText.setExtendSelection(true);
}

void setSelectionPointer(bool set, float scale, int x, int y)
{
    m_selectText.setDrawPointer(set);
    if (!set)
        return;
    m_selectText.m_inverseScale = scale;
    m_selectText.m_selectX = x;
    m_selectText.m_selectY = y;
}

void sendMoveFocus(WebCore::Frame* framePtr, WebCore::Node* nodePtr)
{
    DBG_NAV_LOGD("framePtr=%p nodePtr=%p", framePtr, nodePtr);
    JNIEnv* env = JSC::Bindings::getJNIEnv();
    AutoJObject javaObject = m_javaGlue.object(env);
    if (!javaObject.get())
        return;
    env->CallVoidMethod(javaObject.get(), m_javaGlue.m_sendMoveFocus, (jint) framePtr, (jint) nodePtr);
    checkException(env);
}

void sendMoveMouse(WebCore::Frame* framePtr, WebCore::Node* nodePtr, int x, int y)
{
    DBG_NAV_LOGD("framePtr=%p nodePtr=%p x=%d y=%d", framePtr, nodePtr, x, y);
    JNIEnv* env = JSC::Bindings::getJNIEnv();
    AutoJObject javaObject = m_javaGlue.object(env);
    if (!javaObject.get())
        return;
    env->CallVoidMethod(javaObject.get(), m_javaGlue.m_sendMoveMouse, reinterpret_cast<jint>(framePtr), reinterpret_cast<jint>(nodePtr), x, y);
    checkException(env);
}

void sendMoveMouseIfLatest(bool clearTextEntry, bool stopPaintingCaret)
{
    LOG_ASSERT(m_javaGlue.m_obj, "A java object was not associated with this native WebView!");
    JNIEnv* env = JSC::Bindings::getJNIEnv();
    AutoJObject javaObject = m_javaGlue.object(env);
    if (!javaObject.get())
        return;
    env->CallVoidMethod(javaObject.get(), m_javaGlue.m_sendMoveMouseIfLatest, clearTextEntry, stopPaintingCaret);
    checkException(env);
}

void sendMotionUp(WebCore::Frame* framePtr, WebCore::Node* nodePtr, int x, int y)
{
    DBG_NAV_LOGD("m_generation=%d framePtr=%p nodePtr=%p x=%d y=%d", m_generation, framePtr, nodePtr, x, y);
    LOG_ASSERT(m_javaGlue.m_obj, "A WebView was not associated with this WebViewNative!");

    JNIEnv* env = JSC::Bindings::getJNIEnv();
    AutoJObject javaObject = m_javaGlue.object(env);
    if (!javaObject.get())
        return;
    m_viewImpl->m_touchGeneration = ++m_generation;
    env->CallVoidMethod(javaObject.get(), m_javaGlue.m_sendMotionUp, m_generation, (jint) framePtr, (jint) nodePtr, x, y);
    checkException(env);
}

void findNext(bool forward)
{
    m_findOnPage.findNext(forward);
    scrollToCurrentMatch();
    viewInvalidate();
}

// With this call, WebView takes ownership of matches, and is responsible for
// deleting it.
void setMatches(WTF::Vector<MatchInfo>* matches, jboolean sameAsLastSearch)
{
    // If this search is the same as the last one, check against the old
    // location to determine whether to scroll.  If the same word is found
    // in the same place, then do not scroll.
    IntRect oldLocation;
    bool checkAgainstOldLocation = false;
    if (sameAsLastSearch && m_findOnPage.isCurrentLocationValid()) {
        oldLocation = m_findOnPage.currentMatchBounds();
        checkAgainstOldLocation = true;
    }

    m_findOnPage.setMatches(matches);

    if (!checkAgainstOldLocation || oldLocation != m_findOnPage.currentMatchBounds())
        scrollToCurrentMatch();
    viewInvalidate();
}

int currentMatchIndex()
{
    return m_findOnPage.currentMatchIndex();
}

bool scrollBy(int dx, int dy)
{
    LOG_ASSERT(m_javaGlue.m_obj, "A java object was not associated with this native WebView!");

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

bool hasCursorNode()
{
    CachedRoot* root = getFrameCache(DontAllowNewer);
    if (!root) {
        DBG_NAV_LOG("!root");
        return false;
    }
    const CachedNode* cursorNode = root->currentCursor();
    DBG_NAV_LOGD("cursorNode=%d (nodePointer=%p)",
        cursorNode ? cursorNode->index() : -1,
        cursorNode ? cursorNode->nodePointer() : 0);
    return cursorNode;
}

bool hasFocusNode()
{
    CachedRoot* root = getFrameCache(DontAllowNewer);
    if (!root) {
        DBG_NAV_LOG("!root");
        return false;
    }
    const CachedNode* focusNode = root->currentFocus();
    DBG_NAV_LOGD("focusNode=%d (nodePointer=%p)",
        focusNode ? focusNode->index() : -1,
        focusNode ? focusNode->nodePointer() : 0);
    return focusNode;
}

void rebuildWebTextView()
{
    JNIEnv* env = JSC::Bindings::getJNIEnv();
    AutoJObject javaObject = m_javaGlue.object(env);
    if (!javaObject.get())
        return;
    env->CallVoidMethod(javaObject.get(), m_javaGlue.m_rebuildWebTextView);
    checkException(env);
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

bool inFullScreenMode()
{
    JNIEnv* env = JSC::Bindings::getJNIEnv();
    AutoJObject javaObject = m_javaGlue.object(env);
    if (!javaObject.get())
        return false;
    jboolean result = env->CallBooleanMethod(javaObject.get(), m_javaGlue.m_inFullScreenMode);
    checkException(env);
    return result;
}

int moveGeneration()
{
    return m_viewImpl->m_moveGeneration;
}

LayerAndroid* compositeRoot() const
{
    LOG_ASSERT(!m_baseLayer || m_baseLayer->countChildren() == 1,
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

void registerPageSwapCallback()
{
    m_pageSwapCallbackRegistered = true;
}

void setBaseLayer(BaseLayerAndroid* layer, SkRegion& inval, bool showVisualIndicator,
                  bool isPictureAfterFirstLayout, bool registerPageSwapCallback)
{
#if USE(ACCELERATED_COMPOSITING)
    if (m_glWebViewState)
        m_glWebViewState->setBaseLayer(layer, inval, showVisualIndicator,
                                       isPictureAfterFirstLayout);
    m_pageSwapCallbackRegistered |= registerPageSwapCallback;
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
    CachedRoot* root = getFrameCache(DontAllowNewer);
    if (!root)
        return;
    root->resetLayers();
    root->setRootLayer(compositeRoot());
}

void getTextSelectionRegion(SkRegion *region)
{
    m_selectText.getSelectionRegion(getVisibleRect(), region, compositeRoot());
}

void getTextSelectionHandles(int* handles)
{
    m_selectText.getSelectionHandles(handles, compositeRoot());
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

    bool m_isDrawingPaused;
private: // local state for WebView
    // private to getFrameCache(); other functions operate in a different thread
    CachedRoot* m_frameCacheUI; // navigation data ready for use
    WebViewCore* m_viewImpl;
    int m_generation; // associate unique ID with sent kit focus to match with ui
    SkPicture* m_navPictureUI;
    SkMSec m_ringAnimationEnd;
    // Corresponds to the same-named boolean on the java side.
    bool m_heightCanMeasure;
    int m_lastDx;
    SkMSec m_lastDxTime;
    SelectText m_selectText;
    FindOnPage m_findOnPage;
    CursorRing m_ring;
    BaseLayerAndroid* m_baseLayer;
    Functor* m_glDrawFunctor;
#if USE(ACCELERATED_COMPOSITING)
    GLWebViewState* m_glWebViewState;
    bool m_pageSwapCallbackRegistered;
#endif
    RenderSkinButton* m_buttonSkin;
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
                    WebCore::IntRect&, int, WebCore::IntRect&,
                    jfloat, jint),
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
    private:
    WebView* wvInstance;
    bool (WebView::*funcPtr)(WebCore::IntRect&, WebCore::IntRect*,
            WebCore::IntRect&, int, WebCore::IntRect&, float, int);
    WebCore::IntRect viewRect;
    WebCore::IntRect webViewRect;
    jfloat scale;
    jint extras;
};

static jobject createJavaRect(JNIEnv* env, int x, int y, int right, int bottom)
{
    jclass rectClass = env->FindClass("android/graphics/Rect");
    jmethodID init = env->GetMethodID(rectClass, "<init>", "(IIII)V");
    jobject rect = env->NewObject(rectClass, init, x, y, right, bottom);
    env->DeleteLocalRef(rectClass);
    return rect;
}

/*
 * Native JNI methods
 */
static int nativeCacheHitFramePointer(JNIEnv *env, jobject obj)
{
    return reinterpret_cast<int>(GET_NATIVE_VIEW(env, obj)
            ->m_cacheHitFrame->framePointer());
}

static jobject nativeCacheHitNodeBounds(JNIEnv *env, jobject obj)
{
    WebCore::IntRect bounds = GET_NATIVE_VIEW(env, obj)
        ->m_cacheHitNode->originalAbsoluteBounds();
    return createJavaRect(env, bounds.x(), bounds.y(),
                          bounds.maxX(), bounds.maxY());
}

static int nativeCacheHitNodePointer(JNIEnv *env, jobject obj)
{
    return reinterpret_cast<int>(GET_NATIVE_VIEW(env, obj)
        ->m_cacheHitNode->nodePointer());
}

static bool nativeCacheHitIsPlugin(JNIEnv *env, jobject obj)
{
    return GET_NATIVE_VIEW(env, obj)->m_cacheHitNode->isPlugin();
}

static void nativeClearCursor(JNIEnv *env, jobject obj)
{
    WebView* view = GET_NATIVE_VIEW(env, obj);
    LOG_ASSERT(view, "view not set in %s", __FUNCTION__);
    view->clearCursor();
}

static void nativeCreate(JNIEnv *env, jobject obj, int viewImpl,
                         jstring drawableDir, jboolean isHighEndGfx)
{
    WTF::String dir = jstringToWtfString(env, drawableDir);
    WebView* webview = new WebView(env, obj, viewImpl, dir, isHighEndGfx);
    // NEED THIS OR SOMETHING LIKE IT!
    //Release(obj);
}

static jint nativeCursorFramePointer(JNIEnv *env, jobject obj)
{
    WebView* view = GET_NATIVE_VIEW(env, obj);
    CachedRoot* root = view->getFrameCache(WebView::DontAllowNewer);
    if (!root)
        return 0;
    const CachedFrame* frame = 0;
    (void) root->currentCursor(&frame);
    return reinterpret_cast<int>(frame ? frame->framePointer() : 0);
}

static const CachedNode* getCursorNode(JNIEnv *env, jobject obj)
{
    WebView* view = GET_NATIVE_VIEW(env, obj);
    CachedRoot* root = view->getFrameCache(WebView::DontAllowNewer);
    return root ? root->currentCursor() : 0;
}

static const CachedNode* getCursorNode(JNIEnv *env, jobject obj,
    const CachedFrame** frame)
{
    WebView* view = GET_NATIVE_VIEW(env, obj);
    CachedRoot* root = view->getFrameCache(WebView::DontAllowNewer);
    return root ? root->currentCursor(frame) : 0;
}

static const CachedNode* getFocusCandidate(JNIEnv *env, jobject obj,
    const CachedFrame** frame)
{
    WebView* view = GET_NATIVE_VIEW(env, obj);
    CachedRoot* root = view->getFrameCache(WebView::DontAllowNewer);
    if (!root)
        return 0;
    const CachedNode* cursor = root->currentCursor(frame);
    if (cursor && cursor->wantsKeyEvents())
        return cursor;
    return root->currentFocus(frame);
}

static bool focusCandidateHasNextTextfield(JNIEnv *env, jobject obj)
{
    WebView* view = GET_NATIVE_VIEW(env, obj);
    CachedRoot* root = view->getFrameCache(WebView::DontAllowNewer);
    if (!root)
        return false;
    const CachedNode* cursor = root->currentCursor();
    if (!cursor || !cursor->isTextInput())
        cursor = root->currentFocus();
    if (!cursor || !cursor->isTextInput()) return false;
    return root->nextTextField(cursor, 0);
}

static const CachedNode* getFocusNode(JNIEnv *env, jobject obj)
{
    WebView* view = GET_NATIVE_VIEW(env, obj);
    CachedRoot* root = view->getFrameCache(WebView::DontAllowNewer);
    return root ? root->currentFocus() : 0;
}

static const CachedNode* getFocusNode(JNIEnv *env, jobject obj,
    const CachedFrame** frame)
{
    WebView* view = GET_NATIVE_VIEW(env, obj);
    CachedRoot* root = view->getFrameCache(WebView::DontAllowNewer);
    return root ? root->currentFocus(frame) : 0;
}

static const CachedInput* getInputCandidate(JNIEnv *env, jobject obj)
{
    WebView* view = GET_NATIVE_VIEW(env, obj);
    CachedRoot* root = view->getFrameCache(WebView::DontAllowNewer);
    if (!root)
        return 0;
    const CachedFrame* frame;
    const CachedNode* cursor = root->currentCursor(&frame);
    if (!cursor || !cursor->wantsKeyEvents())
        cursor = root->currentFocus(&frame);
    return cursor ? frame->textInput(cursor) : 0;
}

static jboolean nativePageShouldHandleShiftAndArrows(JNIEnv *env, jobject obj)
{
    const CachedNode* focus = getFocusNode(env, obj);
    if (!focus) return false;
    // Plugins handle shift and arrows whether or not they have focus.
    if (focus->isPlugin()) return true;
    const CachedNode* cursor = getCursorNode(env, obj);
    // ContentEditable nodes should only receive shift and arrows if they have
    // both the cursor and the focus.
    return cursor && cursor->nodePointer() == focus->nodePointer()
            && cursor->isContentEditable();
}

static jobject nativeCursorNodeBounds(JNIEnv *env, jobject obj)
{
    const CachedFrame* frame;
    const CachedNode* node = getCursorNode(env, obj, &frame);
    WebCore::IntRect bounds = node ? node->bounds(frame)
        : WebCore::IntRect(0, 0, 0, 0);
    return createJavaRect(env, bounds.x(), bounds.y(),
                          bounds.maxX(), bounds.maxY());
}

static jint nativeCursorNodePointer(JNIEnv *env, jobject obj)
{
    const CachedNode* node = getCursorNode(env, obj);
    return reinterpret_cast<int>(node ? node->nodePointer() : 0);
}

static jobject nativeCursorPosition(JNIEnv *env, jobject obj)
{
    WebView* view = GET_NATIVE_VIEW(env, obj);
    const CachedRoot* root = view->getFrameCache(WebView::DontAllowNewer);
    WebCore::IntPoint pos = WebCore::IntPoint(0, 0);
    if (root)
        root->getSimulatedMousePosition(&pos);
    jclass pointClass = env->FindClass("android/graphics/Point");
    jmethodID init = env->GetMethodID(pointClass, "<init>", "(II)V");
    jobject point = env->NewObject(pointClass, init, pos.x(), pos.y());
    env->DeleteLocalRef(pointClass);
    return point;
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

static bool nativeCursorIntersects(JNIEnv *env, jobject obj, jobject visRect)
{
    const CachedFrame* frame;
    const CachedNode* node = getCursorNode(env, obj, &frame);
    return node ? node->bounds(frame).intersects(
        jrect_to_webrect(env, visRect)) : false;
}

static bool nativeCursorIsAnchor(JNIEnv *env, jobject obj)
{
    const CachedNode* node = getCursorNode(env, obj);
    return node ? node->isAnchor() : false;
}

static bool nativeCursorIsTextInput(JNIEnv *env, jobject obj)
{
    const CachedNode* node = getCursorNode(env, obj);
    return node ? node->isTextInput() : false;
}

static jobject nativeCursorText(JNIEnv *env, jobject obj)
{
    const CachedNode* node = getCursorNode(env, obj);
    if (!node)
        return 0;
    WTF::String value = node->getExport();
    return wtfStringToJstring(env, value);
}

static void nativeDebugDump(JNIEnv *env, jobject obj)
{
#if DUMP_NAV_CACHE
    WebView* view = GET_NATIVE_VIEW(env, obj);
    LOG_ASSERT(view, "view not set in %s", __FUNCTION__);
    view->debugDump();
#endif
}

static jint nativeDraw(JNIEnv *env, jobject obj, jobject canv,
        jobject visible, jint color,
        jint extras, jboolean split) {
    SkCanvas* canvas = GraphicsJNI::getNativeCanvas(env, canv);
    WebView* webView = GET_NATIVE_VIEW(env, obj);
    SkRect visibleRect = jrectf_to_rect(env, visible);
    webView->setVisibleRect(visibleRect);
    PictureSet* pictureSet = webView->draw(canvas, color, extras, split);
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
        jobject jviewrect, jobject jvisiblerect) {
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

static void nativeSetBaseLayer(JNIEnv *env, jobject obj, jint layer, jobject inval,
                                jboolean showVisualIndicator,
                                jboolean isPictureAfterFirstLayout,
                                jboolean registerPageSwapCallback)
{
    BaseLayerAndroid* layerImpl = reinterpret_cast<BaseLayerAndroid*>(layer);
    SkRegion invalRegion;
    if (inval)
        invalRegion = *GraphicsJNI::getNativeRegion(env, inval);
    GET_NATIVE_VIEW(env, obj)->setBaseLayer(layerImpl, invalRegion, showVisualIndicator,
                                            isPictureAfterFirstLayout,
                                            registerPageSwapCallback);
}

static void nativeGetTextSelectionRegion(JNIEnv *env, jobject obj, jint view,
                                         jobject region)
{
    if (!region)
        return;
    SkRegion* nregion = GraphicsJNI::getNativeRegion(env, region);
    ((WebView*)view)->getTextSelectionRegion(nregion);
}

static void nativeGetSelectionHandles(JNIEnv *env, jobject obj, jint view,
                                      jintArray arr)
{
    int handles[4];
    ((WebView*)view)->getTextSelectionHandles(handles);
    env->SetIntArrayRegion(arr, 0, 4, handles);
    checkException(env);
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

static jobject nativeImageURI(JNIEnv *env, jobject obj, jint x, jint y)
{
    WebView* view = GET_NATIVE_VIEW(env, obj);
    LOG_ASSERT(view, "view not set in %s", __FUNCTION__);
    WTF::String uri = view->imageURI(x, y);
    return wtfStringToJstring(env, uri);
}

static jint nativeFocusCandidateFramePointer(JNIEnv *env, jobject obj)
{
    WebView* view = GET_NATIVE_VIEW(env, obj);
    CachedRoot* root = view->getFrameCache(WebView::DontAllowNewer);
    if (!root)
        return 0;
    const CachedFrame* frame = 0;
    const CachedNode* cursor = root->currentCursor(&frame);
    if (!cursor || !cursor->wantsKeyEvents())
        (void) root->currentFocus(&frame);
    return reinterpret_cast<int>(frame ? frame->framePointer() : 0);
}

static bool nativeFocusCandidateIsPassword(JNIEnv *env, jobject obj)
{
    const CachedInput* input = getInputCandidate(env, obj);
    return input && input->getType() == CachedInput::PASSWORD;
}

static bool nativeFocusCandidateIsRtlText(JNIEnv *env, jobject obj)
{
    const CachedInput* input = getInputCandidate(env, obj);
    return input ? input->isRtlText() : false;
}

static bool nativeFocusCandidateIsTextInput(JNIEnv *env, jobject obj)
{
    const CachedNode* node = getFocusCandidate(env, obj, 0);
    return node ? node->isTextInput() : false;
}

static jint nativeFocusCandidateMaxLength(JNIEnv *env, jobject obj)
{
    const CachedInput* input = getInputCandidate(env, obj);
    return input ? input->maxLength() : false;
}

static jint nativeFocusCandidateIsAutoComplete(JNIEnv *env, jobject obj)
{
    const CachedInput* input = getInputCandidate(env, obj);
    return input ? input->autoComplete() : false;
}

static jobject nativeFocusCandidateName(JNIEnv *env, jobject obj)
{
    const CachedInput* input = getInputCandidate(env, obj);
    if (!input)
        return 0;
    const WTF::String& name = input->name();
    return wtfStringToJstring(env, name);
}

static jobject nativeFocusCandidateNodeBounds(JNIEnv *env, jobject obj)
{
    const CachedFrame* frame;
    const CachedNode* node = getFocusCandidate(env, obj, &frame);
    WebCore::IntRect bounds = node ? node->originalAbsoluteBounds()
        : WebCore::IntRect(0, 0, 0, 0);
    // Inset the rect by 1 unit, so that the focus candidate's border can still
    // be seen behind it.
    return createJavaRect(env, bounds.x(), bounds.y(),
                          bounds.maxX(), bounds.maxY());
}

static jobject nativeFocusCandidatePaddingRect(JNIEnv *env, jobject obj)
{
    const CachedInput* input = getInputCandidate(env, obj);
    if (!input)
        return 0;
    // Note that the Java Rect is being used to pass four integers, rather than
    // being used as an actual rectangle.
    return createJavaRect(env, input->paddingLeft(), input->paddingTop(),
            input->paddingRight(), input->paddingBottom());
}

static jint nativeFocusCandidatePointer(JNIEnv *env, jobject obj)
{
    const CachedNode* node = getFocusCandidate(env, obj, 0);
    return reinterpret_cast<int>(node ? node->nodePointer() : 0);
}

static jint nativeFocusCandidateIsSpellcheck(JNIEnv *env, jobject obj)
{
    const CachedInput* input = getInputCandidate(env, obj);
    return input ? input->spellcheck() : false;
}

static jobject nativeFocusCandidateText(JNIEnv *env, jobject obj)
{
    const CachedNode* node = getFocusCandidate(env, obj, 0);
    if (!node)
        return 0;
    WTF::String value = node->getExport();
    return wtfStringToJstring(env, value);
}

static int nativeFocusCandidateLineHeight(JNIEnv *env, jobject obj)
{
    const CachedInput* input = getInputCandidate(env, obj);
    return input ? input->lineHeight() : 0;
}

static jfloat nativeFocusCandidateTextSize(JNIEnv *env, jobject obj)
{
    const CachedInput* input = getInputCandidate(env, obj);
    return input ? input->textSize() : 0.f;
}

static int nativeFocusCandidateType(JNIEnv *env, jobject obj)
{
    const CachedInput* input = getInputCandidate(env, obj);
    if (!input)
        return CachedInput::NONE;

    if (input->isTextArea())
        return CachedInput::TEXT_AREA;

    return input->getType();
}

static int nativeFocusCandidateLayerId(JNIEnv *env, jobject obj)
{
    const CachedFrame* frame = 0;
    const CachedNode* node = getFocusNode(env, obj, &frame);
    if (!node || !frame)
        return -1;
    const CachedLayer* layer = frame->layer(node);
    if (!layer)
        return -1;
    return layer->uniqueId();
}

static bool nativeFocusIsPlugin(JNIEnv *env, jobject obj)
{
    const CachedNode* node = getFocusNode(env, obj);
    return node ? node->isPlugin() : false;
}

static jobject nativeFocusNodeBounds(JNIEnv *env, jobject obj)
{
    const CachedFrame* frame;
    const CachedNode* node = getFocusNode(env, obj, &frame);
    WebCore::IntRect bounds = node ? node->bounds(frame)
        : WebCore::IntRect(0, 0, 0, 0);
    return createJavaRect(env, bounds.x(), bounds.y(),
                          bounds.maxX(), bounds.maxY());
}

static jint nativeFocusNodePointer(JNIEnv *env, jobject obj)
{
    const CachedNode* node = getFocusNode(env, obj);
    return node ? reinterpret_cast<int>(node->nodePointer()) : 0;
}

static bool nativeCursorWantsKeyEvents(JNIEnv* env, jobject jwebview) {
    WebView* view = GET_NATIVE_VIEW(env, jwebview);
    LOG_ASSERT(view, "view not set in %s", __FUNCTION__);
    return view->cursorWantsKeyEvents();
}

static void nativeHideCursor(JNIEnv *env, jobject obj)
{
    WebView* view = GET_NATIVE_VIEW(env, obj);
    LOG_ASSERT(view, "view not set in %s", __FUNCTION__);
    view->hideCursor();
}

static void nativeInstrumentReport(JNIEnv *env, jobject obj)
{
#ifdef ANDROID_INSTRUMENT
    TimeCounter::reportNow();
#endif
}

static void nativeSelectBestAt(JNIEnv *env, jobject obj, jobject jrect)
{
    WebView* view = GET_NATIVE_VIEW(env, obj);
    LOG_ASSERT(view, "view not set in %s", __FUNCTION__);
    WebCore::IntRect rect = jrect_to_webrect(env, jrect);
    view->selectBestAt(rect);
}

static void nativeSelectAt(JNIEnv *env, jobject obj, jint x, jint y)
{
    WebView* view = GET_NATIVE_VIEW(env, obj);
    LOG_ASSERT(view, "view not set in %s", __FUNCTION__);
    WebCore::IntRect rect = IntRect(x, y , 1, 1);
    view->selectBestAt(rect);
    if (view->hasCursorNode())
        view->showCursorUntimed();
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

static jint nativeTextGeneration(JNIEnv *env, jobject obj)
{
    WebView* view = GET_NATIVE_VIEW(env, obj);
    CachedRoot* root = view->getFrameCache(WebView::DontAllowNewer);
    return root ? root->textGeneration() : 0;
}

static bool nativePointInNavCache(JNIEnv *env, jobject obj,
    int x, int y, int slop)
{
    return GET_NATIVE_VIEW(env, obj)->pointInNavCache(x, y, slop);
}

static bool nativeMotionUp(JNIEnv *env, jobject obj,
    int x, int y, int slop)
{
    WebView* view = GET_NATIVE_VIEW(env, obj);
    LOG_ASSERT(view, "view not set in %s", __FUNCTION__);
    return view->motionUp(x, y, slop);
}

static bool nativeHasCursorNode(JNIEnv *env, jobject obj)
{
    return GET_NATIVE_VIEW(env, obj)->hasCursorNode();
}

static bool nativeHasFocusNode(JNIEnv *env, jobject obj)
{
    return GET_NATIVE_VIEW(env, obj)->hasFocusNode();
}

static bool nativeMoveCursor(JNIEnv *env, jobject obj,
    int key, int count, bool ignoreScroll)
{
    WebView* view = GET_NATIVE_VIEW(env, obj);
    DBG_NAV_LOGD("env=%p obj=%p view=%p", env, obj, view);
    LOG_ASSERT(view, "view not set in %s", __FUNCTION__);
    return view->moveCursor(key, count, ignoreScroll);
}

static void nativeSetFindIsUp(JNIEnv *env, jobject obj, jboolean isUp)
{
    WebView* view = GET_NATIVE_VIEW(env, obj);
    LOG_ASSERT(view, "view not set in %s", __FUNCTION__);
    view->setFindIsUp(isUp);
}

static void nativeSetFindIsEmpty(JNIEnv *env, jobject obj)
{
    GET_NATIVE_VIEW(env, obj)->setFindIsEmpty();
}

static void nativeShowCursorTimed(JNIEnv *env, jobject obj)
{
    GET_NATIVE_VIEW(env, obj)->showCursorTimed();
}

static void nativeSetHeightCanMeasure(JNIEnv *env, jobject obj, bool measure)
{
    WebView* view = GET_NATIVE_VIEW(env, obj);
    LOG_ASSERT(view, "view not set in nativeSetHeightCanMeasure");
    view->setHeightCanMeasure(measure);
}

static jobject nativeGetCursorRingBounds(JNIEnv *env, jobject obj)
{
    WebView* view = GET_NATIVE_VIEW(env, obj);
    LOG_ASSERT(view, "view not set in %s", __FUNCTION__);
    jclass rectClass = env->FindClass("android/graphics/Rect");
    LOG_ASSERT(rectClass, "Could not find Rect class!");
    jmethodID init = env->GetMethodID(rectClass, "<init>", "(IIII)V");
    LOG_ASSERT(init, "Could not find constructor for Rect");
    WebCore::IntRect webRect;
    view->cursorRingBounds(&webRect);
    jobject rect = env->NewObject(rectClass, init, webRect.x(),
        webRect.y(), webRect.maxX(), webRect.maxY());
    env->DeleteLocalRef(rectClass);
    return rect;
}

static int nativeFindAll(JNIEnv *env, jobject obj, jstring findLower,
        jstring findUpper, jboolean sameAsLastSearch)
{
    // If one or the other is null, do not search.
    if (!(findLower && findUpper))
        return 0;
    // Obtain the characters for both the lower case string and the upper case
    // string representing the same word.
    const jchar* findLowerChars = env->GetStringChars(findLower, 0);
    const jchar* findUpperChars = env->GetStringChars(findUpper, 0);
    // If one or the other is null, do not search.
    if (!(findLowerChars && findUpperChars)) {
        if (findLowerChars)
            env->ReleaseStringChars(findLower, findLowerChars);
        if (findUpperChars)
            env->ReleaseStringChars(findUpper, findUpperChars);
        checkException(env);
        return 0;
    }
    WebView* view = GET_NATIVE_VIEW(env, obj);
    LOG_ASSERT(view, "view not set in nativeFindAll");
    CachedRoot* root = view->getFrameCache(WebView::AllowNewer);
    if (!root) {
        env->ReleaseStringChars(findLower, findLowerChars);
        env->ReleaseStringChars(findUpper, findUpperChars);
        checkException(env);
        return 0;
    }
    int length = env->GetStringLength(findLower);
    // If the lengths of the strings do not match, then they are not the same
    // word, so do not search.
    if (!length || env->GetStringLength(findUpper) != length) {
        env->ReleaseStringChars(findLower, findLowerChars);
        env->ReleaseStringChars(findUpper, findUpperChars);
        checkException(env);
        return 0;
    }
    int width = root->documentWidth();
    int height = root->documentHeight();
    // Create a FindCanvas, which allows us to fake draw into it so we can
    // figure out where our search string is rendered (and how many times).
    FindCanvas canvas(width, height, (const UChar*) findLowerChars,
            (const UChar*) findUpperChars, length << 1);
    SkBitmap bitmap;
    bitmap.setConfig(SkBitmap::kARGB_8888_Config, width, height);
    canvas.setBitmapDevice(bitmap);
    root->draw(canvas);
    WTF::Vector<MatchInfo>* matches = canvas.detachMatches();
    // With setMatches, the WebView takes ownership of matches
    view->setMatches(matches, sameAsLastSearch);

    env->ReleaseStringChars(findLower, findLowerChars);
    env->ReleaseStringChars(findUpper, findUpperChars);
    checkException(env);
    return canvas.found();
}

static void nativeFindNext(JNIEnv *env, jobject obj, bool forward)
{
    WebView* view = GET_NATIVE_VIEW(env, obj);
    LOG_ASSERT(view, "view not set in nativeFindNext");
    view->findNext(forward);
}

static int nativeFindIndex(JNIEnv *env, jobject obj)
{
    WebView* view = GET_NATIVE_VIEW(env, obj);
    LOG_ASSERT(view, "view not set in nativeFindIndex");
    return view->currentMatchIndex();
}

static void nativeUpdateCachedTextfield(JNIEnv *env, jobject obj, jstring updatedText, jint generation)
{
    WebView* view = GET_NATIVE_VIEW(env, obj);
    LOG_ASSERT(view, "view not set in nativeUpdateCachedTextfield");
    CachedRoot* root = view->getFrameCache(WebView::DontAllowNewer);
    if (!root)
        return;
    const CachedNode* cachedFocusNode = root->currentFocus();
    if (!cachedFocusNode || !cachedFocusNode->isTextInput())
        return;
    WTF::String webcoreString = jstringToWtfString(env, updatedText);
    (const_cast<CachedNode*>(cachedFocusNode))->setExport(webcoreString);
    root->setTextGeneration(generation);
    checkException(env);
}

static jint nativeGetBlockLeftEdge(JNIEnv *env, jobject obj, jint x, jint y,
        jfloat scale)
{
    WebView* view = GET_NATIVE_VIEW(env, obj);
    LOG_ASSERT(view, "view not set in %s", __FUNCTION__);
    if (!view)
        return -1;
    return view->getBlockLeftEdge(x, y, scale);
}

static void nativeDestroy(JNIEnv *env, jobject obj)
{
    WebView* view = GET_NATIVE_VIEW(env, obj);
    LOGD("nativeDestroy view: %p", view);
    LOG_ASSERT(view, "view not set in nativeDestroy");
    delete view;
}

static void nativeStopGL(JNIEnv *env, jobject obj)
{
    GET_NATIVE_VIEW(env, obj)->stopGL();
}

static bool nativeMoveCursorToNextTextInput(JNIEnv *env, jobject obj)
{
    WebView* view = GET_NATIVE_VIEW(env, obj);
    CachedRoot* root = view->getFrameCache(WebView::DontAllowNewer);
    if (!root)
        return false;
    const CachedNode* current = root->currentCursor();
    if (!current || !current->isTextInput())
        current = root->currentFocus();
    if (!current || !current->isTextInput())
        return false;
    const CachedFrame* frame;
    const CachedNode* next = root->nextTextField(current, &frame);
    if (!next)
        return false;
    const WebCore::IntRect& bounds = next->bounds(frame);
    root->rootHistory()->setMouseBounds(bounds);
    view->getWebViewCore()->updateCursorBounds(root, frame, next);
    view->showCursorUntimed();
    root->setCursor(const_cast<CachedFrame*>(frame),
            const_cast<CachedNode*>(next));
    view->sendMoveFocus(static_cast<WebCore::Frame*>(frame->framePointer()),
            static_cast<WebCore::Node*>(next->nodePointer()));
    if (!next->isInLayer())
        view->scrollRectOnScreen(bounds);
    view->getWebViewCore()->m_moveGeneration++;
    return true;
}

static int nativeMoveGeneration(JNIEnv *env, jobject obj)
{
    WebView* view = GET_NATIVE_VIEW(env, obj);
    if (!view)
        return 0;
    return view->moveGeneration();
}

static void nativeMoveSelection(JNIEnv *env, jobject obj, int x, int y)
{
    GET_NATIVE_VIEW(env, obj)->moveSelection(x, y);
}

static void nativeResetSelection(JNIEnv *env, jobject obj)
{
    return GET_NATIVE_VIEW(env, obj)->resetSelection();
}

static jobject nativeSelectableText(JNIEnv* env, jobject obj)
{
    IntPoint pos = GET_NATIVE_VIEW(env, obj)->selectableText();
    jclass pointClass = env->FindClass("android/graphics/Point");
    jmethodID init = env->GetMethodID(pointClass, "<init>", "(II)V");
    jobject point = env->NewObject(pointClass, init, pos.x(), pos.y());
    env->DeleteLocalRef(pointClass);
    return point;
}

static void nativeSelectAll(JNIEnv* env, jobject obj)
{
    GET_NATIVE_VIEW(env, obj)->selectAll();
}

static void nativeSetExtendSelection(JNIEnv *env, jobject obj)
{
    GET_NATIVE_VIEW(env, obj)->setExtendSelection();
}

static jboolean nativeStartSelection(JNIEnv *env, jobject obj, int x, int y)
{
    return GET_NATIVE_VIEW(env, obj)->startSelection(x, y);
}

static jboolean nativeWordSelection(JNIEnv *env, jobject obj, int x, int y)
{
    return GET_NATIVE_VIEW(env, obj)->wordSelection(x, y);
}

static void nativeExtendSelection(JNIEnv *env, jobject obj, int x, int y)
{
    GET_NATIVE_VIEW(env, obj)->extendSelection(x, y);
}

static jobject nativeGetSelection(JNIEnv *env, jobject obj)
{
    WebView* view = GET_NATIVE_VIEW(env, obj);
    LOG_ASSERT(view, "view not set in %s", __FUNCTION__);
    String selection = view->getSelection();
    return wtfStringToJstring(env, selection);
}

static jboolean nativeHitSelection(JNIEnv *env, jobject obj, int x, int y)
{
    return GET_NATIVE_VIEW(env, obj)->hitSelection(x, y);
}

static jint nativeSelectionX(JNIEnv *env, jobject obj)
{
    return GET_NATIVE_VIEW(env, obj)->selectionX();
}

static jint nativeSelectionY(JNIEnv *env, jobject obj)
{
    return GET_NATIVE_VIEW(env, obj)->selectionY();
}

static void nativeSetSelectionPointer(JNIEnv *env, jobject obj, jint nativeView,
                                      jboolean set, jfloat scale, jint x, jint y)
{
    ((WebView*)nativeView)->setSelectionPointer(set, scale, x, y);
}

static void nativeRegisterPageSwapCallback(JNIEnv *env, jobject obj)
{
    GET_NATIVE_VIEW(env, obj)->registerPageSwapCallback();
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
        if (value == "true")
            TilesManager::instance()->setInvertedScreen(true);
        else
            TilesManager::instance()->setInvertedScreen(false);
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
    return false;
}

static jstring nativeGetProperty(JNIEnv *env, jobject obj, jstring key)
{
    return 0;
}

static void nativeOnTrimMemory(JNIEnv *env, jobject obj, jint level)
{
    if (TilesManager::hardwareAccelerationEnabled()) {
        bool freeAllTextures = (level > TRIM_MEMORY_UI_HIDDEN);
        TilesManager::instance()->deallocateTextures(freeAllTextures);
    }
}

static void nativeDumpDisplayTree(JNIEnv* env, jobject jwebview, jstring jurl)
{
#ifdef ANDROID_DUMP_DISPLAY_TREE
    WebView* view = GET_NATIVE_VIEW(env, jwebview);
    LOG_ASSERT(view, "view not set in %s", __FUNCTION__);

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
            view->draw(&canvas, 0, 0, false);
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
    LOG_ASSERT(view, "view not set in %s", __FUNCTION__);
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
    LOG_ASSERT(view, "view not set in %s", __FUNCTION__);
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

/*
 * JNI registration
 */
static JNINativeMethod gJavaWebViewMethods[] = {
    { "nativeCacheHitFramePointer", "()I",
        (void*) nativeCacheHitFramePointer },
    { "nativeCacheHitIsPlugin", "()Z",
        (void*) nativeCacheHitIsPlugin },
    { "nativeCacheHitNodeBounds", "()Landroid/graphics/Rect;",
        (void*) nativeCacheHitNodeBounds },
    { "nativeCacheHitNodePointer", "()I",
        (void*) nativeCacheHitNodePointer },
    { "nativeClearCursor", "()V",
        (void*) nativeClearCursor },
    { "nativeCreate", "(ILjava/lang/String;Z)V",
        (void*) nativeCreate },
    { "nativeCursorFramePointer", "()I",
        (void*) nativeCursorFramePointer },
    { "nativePageShouldHandleShiftAndArrows", "()Z",
        (void*) nativePageShouldHandleShiftAndArrows },
    { "nativeCursorNodeBounds", "()Landroid/graphics/Rect;",
        (void*) nativeCursorNodeBounds },
    { "nativeCursorNodePointer", "()I",
        (void*) nativeCursorNodePointer },
    { "nativeCursorIntersects", "(Landroid/graphics/Rect;)Z",
        (void*) nativeCursorIntersects },
    { "nativeCursorIsAnchor", "()Z",
        (void*) nativeCursorIsAnchor },
    { "nativeCursorIsTextInput", "()Z",
        (void*) nativeCursorIsTextInput },
    { "nativeCursorPosition", "()Landroid/graphics/Point;",
        (void*) nativeCursorPosition },
    { "nativeCursorText", "()Ljava/lang/String;",
        (void*) nativeCursorText },
    { "nativeCursorWantsKeyEvents", "()Z",
        (void*)nativeCursorWantsKeyEvents },
    { "nativeDebugDump", "()V",
        (void*) nativeDebugDump },
    { "nativeDestroy", "()V",
        (void*) nativeDestroy },
    { "nativeDraw", "(Landroid/graphics/Canvas;Landroid/graphics/RectF;IIZ)I",
        (void*) nativeDraw },
    { "nativeGetDrawGLFunction", "(ILandroid/graphics/Rect;Landroid/graphics/Rect;Landroid/graphics/RectF;FI)I",
        (void*) nativeGetDrawGLFunction },
    { "nativeUpdateDrawGLFunction", "(Landroid/graphics/Rect;Landroid/graphics/Rect;Landroid/graphics/RectF;)V",
        (void*) nativeUpdateDrawGLFunction },
    { "nativeDumpDisplayTree", "(Ljava/lang/String;)V",
        (void*) nativeDumpDisplayTree },
    { "nativeEvaluateLayersAnimations", "(I)Z",
        (void*) nativeEvaluateLayersAnimations },
    { "nativeExtendSelection", "(II)V",
        (void*) nativeExtendSelection },
    { "nativeFindAll", "(Ljava/lang/String;Ljava/lang/String;Z)I",
        (void*) nativeFindAll },
    { "nativeFindNext", "(Z)V",
        (void*) nativeFindNext },
    { "nativeFindIndex", "()I",
        (void*) nativeFindIndex},
    { "nativeFocusCandidateFramePointer", "()I",
        (void*) nativeFocusCandidateFramePointer },
    { "nativeFocusCandidateHasNextTextfield", "()Z",
        (void*) focusCandidateHasNextTextfield },
    { "nativeFocusCandidateIsPassword", "()Z",
        (void*) nativeFocusCandidateIsPassword },
    { "nativeFocusCandidateIsRtlText", "()Z",
        (void*) nativeFocusCandidateIsRtlText },
    { "nativeFocusCandidateIsTextInput", "()Z",
        (void*) nativeFocusCandidateIsTextInput },
    { "nativeFocusCandidateLineHeight", "()I",
        (void*) nativeFocusCandidateLineHeight },
    { "nativeFocusCandidateMaxLength", "()I",
        (void*) nativeFocusCandidateMaxLength },
    { "nativeFocusCandidateIsAutoComplete", "()Z",
        (void*) nativeFocusCandidateIsAutoComplete },
    { "nativeFocusCandidateIsSpellcheck", "()Z",
        (void*) nativeFocusCandidateIsSpellcheck },
    { "nativeFocusCandidateName", "()Ljava/lang/String;",
        (void*) nativeFocusCandidateName },
    { "nativeFocusCandidateNodeBounds", "()Landroid/graphics/Rect;",
        (void*) nativeFocusCandidateNodeBounds },
    { "nativeFocusCandidatePaddingRect", "()Landroid/graphics/Rect;",
        (void*) nativeFocusCandidatePaddingRect },
    { "nativeFocusCandidatePointer", "()I",
        (void*) nativeFocusCandidatePointer },
    { "nativeFocusCandidateText", "()Ljava/lang/String;",
        (void*) nativeFocusCandidateText },
    { "nativeFocusCandidateTextSize", "()F",
        (void*) nativeFocusCandidateTextSize },
    { "nativeFocusCandidateType", "()I",
        (void*) nativeFocusCandidateType },
    { "nativeFocusCandidateLayerId", "()I",
        (void*) nativeFocusCandidateLayerId },
    { "nativeFocusIsPlugin", "()Z",
        (void*) nativeFocusIsPlugin },
    { "nativeFocusNodeBounds", "()Landroid/graphics/Rect;",
        (void*) nativeFocusNodeBounds },
    { "nativeFocusNodePointer", "()I",
        (void*) nativeFocusNodePointer },
    { "nativeGetCursorRingBounds", "()Landroid/graphics/Rect;",
        (void*) nativeGetCursorRingBounds },
    { "nativeGetSelection", "()Ljava/lang/String;",
        (void*) nativeGetSelection },
    { "nativeHasCursorNode", "()Z",
        (void*) nativeHasCursorNode },
    { "nativeHasFocusNode", "()Z",
        (void*) nativeHasFocusNode },
    { "nativeHideCursor", "()V",
        (void*) nativeHideCursor },
    { "nativeHitSelection", "(II)Z",
        (void*) nativeHitSelection },
    { "nativeImageURI", "(II)Ljava/lang/String;",
        (void*) nativeImageURI },
    { "nativeInstrumentReport", "()V",
        (void*) nativeInstrumentReport },
    { "nativeLayerBounds", "(I)Landroid/graphics/Rect;",
        (void*) nativeLayerBounds },
    { "nativeMotionUp", "(III)Z",
        (void*) nativeMotionUp },
    { "nativeMoveCursor", "(IIZ)Z",
        (void*) nativeMoveCursor },
    { "nativeMoveCursorToNextTextInput", "()Z",
        (void*) nativeMoveCursorToNextTextInput },
    { "nativeMoveGeneration", "()I",
        (void*) nativeMoveGeneration },
    { "nativeMoveSelection", "(II)V",
        (void*) nativeMoveSelection },
    { "nativePointInNavCache", "(III)Z",
        (void*) nativePointInNavCache },
    { "nativeResetSelection", "()V",
        (void*) nativeResetSelection },
    { "nativeSelectableText", "()Landroid/graphics/Point;",
        (void*) nativeSelectableText },
    { "nativeSelectAll", "()V",
        (void*) nativeSelectAll },
    { "nativeSelectBestAt", "(Landroid/graphics/Rect;)V",
        (void*) nativeSelectBestAt },
    { "nativeSelectAt", "(II)V",
        (void*) nativeSelectAt },
    { "nativeSelectionX", "()I",
        (void*) nativeSelectionX },
    { "nativeSelectionY", "()I",
        (void*) nativeSelectionY },
    { "nativeSetExtendSelection", "()V",
        (void*) nativeSetExtendSelection },
    { "nativeSetFindIsEmpty", "()V",
        (void*) nativeSetFindIsEmpty },
    { "nativeSetFindIsUp", "(Z)V",
        (void*) nativeSetFindIsUp },
    { "nativeSetHeightCanMeasure", "(Z)V",
        (void*) nativeSetHeightCanMeasure },
    { "nativeSetBaseLayer", "(ILandroid/graphics/Region;ZZZ)V",
        (void*) nativeSetBaseLayer },
    { "nativeGetTextSelectionRegion", "(ILandroid/graphics/Region;)V",
        (void*) nativeGetTextSelectionRegion },
    { "nativeGetSelectionHandles", "(I[I)V",
        (void*) nativeGetSelectionHandles },
    { "nativeGetBaseLayer", "()I",
        (void*) nativeGetBaseLayer },
    { "nativeReplaceBaseContent", "(I)V",
        (void*) nativeReplaceBaseContent },
    { "nativeCopyBaseContentToPicture", "(Landroid/graphics/Picture;)V",
        (void*) nativeCopyBaseContentToPicture },
    { "nativeHasContent", "()Z",
        (void*) nativeHasContent },
    { "nativeSetSelectionPointer", "(IZFII)V",
        (void*) nativeSetSelectionPointer },
    { "nativeShowCursorTimed", "()V",
        (void*) nativeShowCursorTimed },
    { "nativeRegisterPageSwapCallback", "()V",
        (void*) nativeRegisterPageSwapCallback },
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
    { "nativeStartSelection", "(II)Z",
        (void*) nativeStartSelection },
    { "nativeStopGL", "()V",
        (void*) nativeStopGL },
    { "nativeSubtractLayers", "(Landroid/graphics/Rect;)Landroid/graphics/Rect;",
        (void*) nativeSubtractLayers },
    { "nativeTextGeneration", "()I",
        (void*) nativeTextGeneration },
    { "nativeUpdateCachedTextfield", "(Ljava/lang/String;I)V",
        (void*) nativeUpdateCachedTextfield },
    {  "nativeWordSelection", "(II)Z",
        (void*) nativeWordSelection },
    { "nativeGetBlockLeftEdge", "(IIF)I",
        (void*) nativeGetBlockLeftEdge },
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
};

int registerWebView(JNIEnv* env)
{
    jclass clazz = env->FindClass("android/webkit/WebView");
    LOG_ASSERT(clazz, "Unable to find class android/webkit/WebView");
    gWebViewField = env->GetFieldID(clazz, "mNativeClass", "I");
    LOG_ASSERT(gWebViewField, "Unable to find android/webkit/WebView.mNativeClass");
    env->DeleteLocalRef(clazz);

    return jniRegisterNativeMethods(env, "android/webkit/WebView", gJavaWebViewMethods, NELEM(gJavaWebViewMethods));
}

} // namespace android
