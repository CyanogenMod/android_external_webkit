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

#define LOG_TAG "webcoreglue"

#include "config.h"
#include "IntRect.h"
#include "WebCoreJni.h"
#include "wtf/Vector.h"

#include "NotImplemented.h"
#include <JNIUtility.h>
#include <jni.h>
#include <utils/Log.h>

namespace android {

AutoJObject getRealObject(JNIEnv* env, jobject obj)
{
    jobject real = env->NewLocalRef(obj);
    ALOG_ASSERT(real, "The real object has been deleted!");
    return AutoJObject(env, real);
}

/**
 * Helper method for checking java exceptions
 * @return true if an exception occurred.
 */
bool checkException(JNIEnv* env)
{
    if (env->ExceptionCheck() != 0)
    {
        ALOGE("*** Uncaught exception returned from Java call!\n");
        env->ExceptionDescribe();
        return true;
    }
    return false;
}

// This method is safe to call from the ui thread and the WebCore thread.
WTF::String jstringToWtfString(JNIEnv* env, jstring str)
{
    if (!str || !env)
        return WTF::String();
    const jchar* s = env->GetStringChars(str, NULL);
    if (!s)
        return WTF::String();
    WTF::String ret(s, env->GetStringLength(str));
    env->ReleaseStringChars(str, s);
    checkException(env);
    return ret;
}

jstring wtfStringToJstring(JNIEnv* env, const WTF::String& str, bool validOnZeroLength)
{
    int length = str.length();
    return length || validOnZeroLength ? env->NewString(str.characters(), length) : 0;
}

string16 jstringToString16(JNIEnv* env, jstring jstr)
{
    if (!jstr || !env)
        return string16();

    const char* s = env->GetStringUTFChars(jstr, 0);
    if (!s)
        return string16();
    string16 str = UTF8ToUTF16(s);
    env->ReleaseStringUTFChars(jstr, s);
    checkException(env);
    return str;
}

std::string jstringToStdString(JNIEnv* env, jstring jstr)
{
    if (!jstr || !env)
        return std::string();

    const char* s = env->GetStringUTFChars(jstr, 0);
    if (!s)
        return std::string();
    std::string str(s);
    env->ReleaseStringUTFChars(jstr, s);
    checkException(env);
    return str;
}

jstring stdStringToJstring(JNIEnv* env, const std::string& str, bool validOnZeroLength)
{
    return !str.empty() || validOnZeroLength ? env->NewStringUTF(str.c_str()) : 0;
}

jobject intRectToRect(JNIEnv* env, const WebCore::IntRect& rect)
{
    jclass rectClass = env->FindClass("android/graphics/Rect");
    ALOG_ASSERT(rectClass, "Could not find android/graphics/Rect");
    jmethodID rectInit = env->GetMethodID(rectClass, "<init>", "(IIII)V");
    ALOG_ASSERT(rectInit, "Could not find init method on Rect");
    jobject jrect = env->NewObject(rectClass, rectInit, rect.x(), rect.y(),
            rect.maxX(), rect.maxY());
    env->DeleteLocalRef(rectClass);
    return jrect;
}

jobjectArray intRectVectorToRectArray(JNIEnv* env, Vector<WebCore::IntRect>& rects)
{
    jclass rectClass = env->FindClass("android/graphics/Rect");
    ALOG_ASSERT(rectClass, "Could not find android/graphics/Rect");
    jmethodID rectInit = env->GetMethodID(rectClass, "<init>", "(IIII)V");
    ALOG_ASSERT(rectInit, "Could not find init method on Rect");
    jobjectArray array = env->NewObjectArray(rects.size(), rectClass, 0);
    ALOG_ASSERT(array, "Could not create a Rect array");
    for (size_t i = 0; i < rects.size(); i++) {
        jobject rect = env->NewObject(rectClass, rectInit,
                rects[i].x(), rects[i].y(),
                rects[i].maxX(), rects[i].maxY());
        if (rect) {
            env->SetObjectArrayElement(array, i, rect);
            env->DeleteLocalRef(rect);
        }
    }
    env->DeleteLocalRef(rectClass);
    return array;
}

}
