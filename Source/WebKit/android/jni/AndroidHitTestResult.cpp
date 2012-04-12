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

#define LOG_TAG "AndroidHitTestResult"

#include "config.h"
#include "AndroidHitTestResult.h"

#include "content/address_detector.h"
#include "content/PhoneEmailDetector.h"
#include "android/WebHitTestInfo.h"
#include "Document.h"
#include "Element.h"
#include "Frame.h"
#include "HitTestResult.h"
#include "KURL.h"
#include "LayerAndroid.h"
#include "PlatformString.h"
#include "Range.h"
#include "RenderLayer.h"
#include "RenderLayerBacking.h"
#include "RenderObject.h"
#include "WebCoreJni.h"
#include "WebViewCore.h"

#include <cutils/log.h>
#include <JNIHelp.h>
#include <JNIUtility.h>

namespace android {

using namespace WebCore;

static bool gJniInitialized = false;
static struct {
    jmethodID m_Init;
    jfieldID m_LinkUrl;
    jfieldID m_AnchorText;
    jfieldID m_ImageUrl;
    jfieldID m_AltDisplayString;
    jfieldID m_Title;
    jfieldID m_Editable;
    jfieldID m_TouchRects;
    jfieldID m_TapHighlightColor;
    jfieldID m_EnclosingParentRects;
    jfieldID m_HasFocus;
    jfieldID m_IntentUrl;
} gHitTestGlue;

struct field {
    jclass m_class;
    const char *m_fieldName;
    const char *m_fieldType;
    jfieldID *m_jfield;
};

static void InitJni(JNIEnv* env)
{
    if (gJniInitialized)
        return;

    jclass rectClass = env->FindClass("android/graphics/Rect");
    ALOG_ASSERT(rectClass, "Could not find android/graphics/Rect");
    jclass hitTestClass = env->FindClass("android/webkit/WebViewCore$WebKitHitTest");
    ALOG_ASSERT(hitTestClass, "Could not find android/webkit/WebViewCore$WebKitHitTest");

    gHitTestGlue.m_Init = env->GetMethodID(hitTestClass, "<init>",  "()V");
    ALOG_ASSERT(gHitTestGlue.m_Init, "Could not find init method on android/webkit/WebViewCore$WebKitHitTest");

    field fields[] = {
        { hitTestClass, "mTouchRects", "[Landroid/graphics/Rect;", &gHitTestGlue.m_TouchRects },
        { hitTestClass, "mEditable", "Z", &gHitTestGlue.m_Editable },
        { hitTestClass, "mLinkUrl", "Ljava/lang/String;", &gHitTestGlue.m_LinkUrl },
        { hitTestClass, "mIntentUrl", "Ljava/lang/String;", &gHitTestGlue.m_IntentUrl },
        { hitTestClass, "mAnchorText", "Ljava/lang/String;", &gHitTestGlue.m_AnchorText },
        { hitTestClass, "mImageUrl", "Ljava/lang/String;", &gHitTestGlue.m_ImageUrl },
        { hitTestClass, "mAltDisplayString", "Ljava/lang/String;", &gHitTestGlue.m_AltDisplayString },
        { hitTestClass, "mTitle", "Ljava/lang/String;", &gHitTestGlue.m_Title },
        { hitTestClass, "mTapHighlightColor", "I", &gHitTestGlue.m_TapHighlightColor },
        { hitTestClass, "mEnclosingParentRects", "[Landroid/graphics/Rect;", &gHitTestGlue.m_EnclosingParentRects },
        { hitTestClass, "mHasFocus", "Z", &gHitTestGlue.m_HasFocus },
        {0, 0, 0, 0},
    };

    for (int i = 0; fields[i].m_jfield; i++) {
        field *f = &fields[i];
        jfieldID field = env->GetFieldID(f->m_class, f->m_fieldName, f->m_fieldType);
        ALOG_ASSERT(field, "Can't find %s", f->m_fieldName);
        *(f->m_jfield) = field;
    }

    gJniInitialized = true;
}

AndroidHitTestResult::AndroidHitTestResult(WebViewCore* webViewCore, WebCore::HitTestResult& hitTestResult)
    : m_webViewCore(webViewCore)
    , m_hitTestResult(hitTestResult)
{
    buildHighlightRects();
}

void AndroidHitTestResult::setURLElement(Element* element)
{
    m_hitTestResult.setURLElement(element);
    buildHighlightRects();
}

void AndroidHitTestResult::buildHighlightRects()
{
    m_highlightRects.clear();
    Node* node = m_hitTestResult.URLElement();
    if (!node || !node->renderer())
        node = m_hitTestResult.innerNode();
    if (!node || !node->renderer())
        return;
    if (!WebViewCore::nodeIsClickableOrFocusable(node))
        return;
    Frame* frame = node->document()->frame();
    IntPoint frameOffset = m_webViewCore->convertGlobalContentToFrameContent(IntPoint(), frame);
    RenderObject* renderer = node->renderer();
    Vector<FloatQuad> quads;
    if (renderer->isInline())
        renderer->absoluteFocusRingQuads(quads);
    if (!quads.size())
        renderer->absoluteQuads(quads); // No fancy rings, grab a bounding box
    for (size_t i = 0; i < quads.size(); i++) {
        IntRect boundingBox = quads[i].enclosingBoundingBox();
        boundingBox.move(-frameOffset.x(), -frameOffset.y());
        m_highlightRects.append(boundingBox);
    }
}

void AndroidHitTestResult::searchContentDetectors()
{
    AddressDetector address;
    PhoneEmailDetector phoneEmail;
    Node* node = m_hitTestResult.innerNode();
    if (!node || !node->isTextNode())
        return;
    if (!m_hitTestResult.absoluteLinkURL().isEmpty())
        return;
    WebKit::WebHitTestInfo webHitTest(m_hitTestResult);
    m_searchResult = address.FindTappedContent(webHitTest);
    if (!m_searchResult.valid) {
        m_searchResult = phoneEmail.FindTappedContent(webHitTest);
    }
    if (m_searchResult.valid) {
        m_highlightRects.clear();
        RefPtr<Range> range = (PassRefPtr<Range>) m_searchResult.range;
        range->textRects(m_highlightRects, true);
    }
}

void setStringField(JNIEnv* env, jobject obj, jfieldID field, const String& str)
{
    jstring jstr = wtfStringToJstring(env, str, false);
    env->SetObjectField(obj, field, jstr);
    env->DeleteLocalRef(jstr);
}

void setStringField(JNIEnv* env, jobject obj, jfieldID field, const GURL& url)
{
    jstring jstr = stdStringToJstring(env, url.spec(), false);
    env->SetObjectField(obj, field, jstr);
    env->DeleteLocalRef(jstr);
}

void setRectArray(JNIEnv* env, jobject obj, jfieldID field, Vector<IntRect> &rects)
{
    jobjectArray array = intRectVectorToRectArray(env, rects);
    env->SetObjectField(obj, field, array);
    env->DeleteLocalRef(array);
}

// Some helper macros specific to setting hitTest fields
#define _SET(jtype, jfield, value) env->Set ## jtype ## Field(hitTest, gHitTestGlue.m_ ## jfield, value)
#define SET_BOOL(jfield, value) _SET(Boolean, jfield, value)
#define SET_STRING(jfield, value) setStringField(env, hitTest, gHitTestGlue.m_ ## jfield, value)
#define SET_INT(jfield, value) _SET(Int, jfield, value)

jobject AndroidHitTestResult::createJavaObject(JNIEnv* env)
{
    InitJni(env);
    jclass hitTestClass = env->FindClass("android/webkit/WebViewCore$WebKitHitTest");
    ALOG_ASSERT(hitTestClass, "Could not find android/webkit/WebViewCore$WebKitHitTest");

    jobject hitTest = env->NewObject(hitTestClass, gHitTestGlue.m_Init);
    setRectArray(env, hitTest, gHitTestGlue.m_TouchRects, m_highlightRects);

    Vector<IntRect> rects = enclosingParentRects(m_hitTestResult.innerNode());
    setRectArray(env, hitTest, gHitTestGlue.m_EnclosingParentRects, rects);

    SET_BOOL(Editable, m_hitTestResult.isContentEditable());
    SET_STRING(LinkUrl, m_hitTestResult.absoluteLinkURL().string());
    if (m_searchResult.valid)
        SET_STRING(IntentUrl, m_searchResult.intent_url);
    SET_STRING(ImageUrl, m_hitTestResult.absoluteImageURL().string());
    SET_STRING(AltDisplayString, m_hitTestResult.altDisplayString());
    TextDirection titleTextDirection;
    SET_STRING(Title, m_hitTestResult.title(titleTextDirection));
    if (m_hitTestResult.URLElement()) {
        Element* urlElement = m_hitTestResult.URLElement();
        SET_STRING(AnchorText, urlElement->innerText());
        if (urlElement->renderer()) {
            SET_INT(TapHighlightColor,
                    urlElement->renderer()->style()->tapHighlightColor().rgb());
        }
    }
    Node* focusedNode = m_webViewCore->focusedFrame()->document()->focusedNode();
    SET_BOOL(HasFocus,
             focusedNode == m_hitTestResult.URLElement()
             || focusedNode == m_hitTestResult.innerNode()
             || focusedNode == m_hitTestResult.innerNonSharedNode());

    env->DeleteLocalRef(hitTestClass);

    return hitTest;
}

Vector<IntRect> AndroidHitTestResult::enclosingParentRects(Node* node)
{
    int count = 0;
    int lastX = 0;
    Vector<IntRect> rects;

    while (node && count < 5) {
        RenderObject* render = node->renderer();
        if (!render || render->isBody())
            break;

        IntPoint frameOffset = m_webViewCore->convertGlobalContentToFrameContent(IntPoint(),
                node->document()->frame());
        IntRect rect = render->absoluteBoundingBoxRect();
        rect.move(-frameOffset.x(), -frameOffset.y());
        if (count == 0 || rect.x() != lastX) {
            rects.append(rect);
            lastX = rect.x();
            count++;
        }

        node = node->parentNode();
    }

    return rects;
}

} /* namespace android */
