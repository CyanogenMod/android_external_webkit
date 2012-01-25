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

#include "Element.h"
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
static struct JavaGlue {
    jmethodID m_hitTestInit;
    jfieldID m_hitTestLinkUrl;
    jfieldID m_hitTestAnchorText;
    jfieldID m_hitTestImageUrl;
    jfieldID m_hitTestAltDisplayString;
    jfieldID m_hitTestTitle;
    jfieldID m_hitTestEditable;
    jfieldID m_hitTestTouchRects;
    jfieldID m_hitTestTapHighlightColor;
} gJavaGlue;

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

    gJavaGlue.m_hitTestInit = env->GetMethodID(hitTestClass, "<init>",  "()V");
    ALOG_ASSERT(gJavaGlue.m_hitTestInit, "Could not find init method on android/webkit/WebViewCore$WebKitHitTest");

    field fields[] = {
        { hitTestClass, "mTouchRects", "[Landroid/graphics/Rect;", &gJavaGlue.m_hitTestTouchRects },
        { hitTestClass, "mEditable", "Z", &gJavaGlue.m_hitTestEditable },
        { hitTestClass, "mLinkUrl", "Ljava/lang/String;", &gJavaGlue.m_hitTestLinkUrl },
        { hitTestClass, "mAnchorText", "Ljava/lang/String;", &gJavaGlue.m_hitTestAnchorText },
        { hitTestClass, "mImageUrl", "Ljava/lang/String;", &gJavaGlue.m_hitTestImageUrl },
        { hitTestClass, "mAltDisplayString", "Ljava/lang/String;", &gJavaGlue.m_hitTestAltDisplayString },
        { hitTestClass, "mTitle", "Ljava/lang/String;", &gJavaGlue.m_hitTestTitle },
        { hitTestClass, "mTapHighlightColor", "I", &gJavaGlue.m_hitTestTapHighlightColor },
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

AndroidHitTestResult::AndroidHitTestResult(WebCore::HitTestResult& hitTestResult)
    : m_hitTestResult(hitTestResult)
{
}

void setStringField(JNIEnv* env, jobject obj, jfieldID field, const String& str)
{
    jstring jstr = wtfStringToJstring(env, str, false);
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
#define _SET(jtype, jfield, value) env->Set ## jtype ## Field(hitTest, gJavaGlue.m_hitTest ## jfield, value)
#define SET_BOOL(jfield, value) _SET(Boolean, jfield, value)
#define SET_STRING(jfield, value) setStringField(env, hitTest, gJavaGlue.m_hitTest ## jfield, value)
#define SET_INT(jfield, value) _SET(Int, jfield, value)

jobject AndroidHitTestResult::createJavaObject(JNIEnv* env)
{
    InitJni(env);
    jclass hitTestClass = env->FindClass("android/webkit/WebViewCore$WebKitHitTest");
    ALOG_ASSERT(hitTestClass, "Could not find android/webkit/WebViewCore$WebKitHitTest");

    jobject hitTest = env->NewObject(hitTestClass, gJavaGlue.m_hitTestInit);
    setRectArray(env, hitTest, gJavaGlue.m_hitTestTouchRects, m_highlightRects);

    SET_BOOL(Editable, m_hitTestResult.isContentEditable());
    SET_STRING(LinkUrl, m_hitTestResult.absoluteLinkURL().string());
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

    env->DeleteLocalRef(hitTestClass);

    return hitTest;
}

} /* namespace android */
