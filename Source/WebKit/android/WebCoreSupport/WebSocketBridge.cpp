/*
 * Copyright 2013, The Android Open Source Project
 * Copyright 2013 Oleg Smirnov
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

#include "config.h"

#if ENABLE(WEB_SOCKETS)

#include "Logging.h"
#include "WebSocketBridge.h"
#include "WebCoreJni.h"
#include <JNIHelp.h>
#include <JNIUtility.h>

using namespace android;

static const char* javaWebSocketClass = "android/webkit/HTML5WebSocket";

namespace WebCore {

struct WebSocketBridge::JavaGlue
{
    jobject   m_javaProxy;
    jmethodID m_getInstance;
    jmethodID m_send;
    jmethodID m_close;
};

WebSocketBridge::WebSocketBridge(SocketStreamHandle* client, const String& uri)
    : m_streamHandle(client)
{
    JNIEnv* env = JSC::Bindings::getJNIEnv();
    if (!env)
        return;

    jclass clazz = env->FindClass(javaWebSocketClass);
    if (!clazz)
        return;

    m_glue = new JavaGlue;
    m_glue->m_getInstance = env->GetStaticMethodID(clazz, "getInstance", "(ILjava/lang/String;)Landroid/webkit/HTML5WebSocket;");
    m_glue->m_send = env->GetMethodID(clazz, "send", "([B)V");
    m_glue->m_close = env->GetMethodID(clazz, "close", "()V");

    jstring jUrl = env->NewString((unsigned short *)uri.characters(), uri.length());
    jobject obj = env->CallStaticObjectMethod(clazz, m_glue->m_getInstance, this, jUrl);
    m_glue->m_javaProxy = env->NewGlobalRef(obj);

    // Clean up
    env->DeleteLocalRef(obj);
    env->DeleteLocalRef(clazz);
    env->DeleteLocalRef(jUrl);
    checkException(env);
}

WebSocketBridge::~WebSocketBridge()
{
    if (m_glue->m_javaProxy) {
        if (JNIEnv* env = JSC::Bindings::getJNIEnv())
            env->DeleteGlobalRef(m_glue->m_javaProxy);
    }
}

int WebSocketBridge::send(const char* data, int length)
{
    if (!data || length <= 0)
        return length;

    JNIEnv* env = JSC::Bindings::getJNIEnv();
    if (!env || !m_glue->m_javaProxy || !m_glue->m_send)
        return 0;

    jbyteArray jByteArray = env->NewByteArray(length);
    if (!jByteArray)
        return 0;

    env->SetByteArrayRegion(jByteArray, 0, length, reinterpret_cast<const jbyte*>(data));
    env->CallVoidMethod(m_glue->m_javaProxy, m_glue->m_send, jByteArray);
    env->DeleteLocalRef(jByteArray);
    checkException(env);
    return length;
}

void WebSocketBridge::close()
{
    JNIEnv* env = JSC::Bindings::getJNIEnv();
    if (!env || !m_glue->m_javaProxy || !m_glue->m_close)
        return;

    env->CallVoidMethod(m_glue->m_javaProxy, m_glue->m_close);
    checkException(env);
}

void WebSocketBridge::didWebSocketConnected()
{
    m_streamHandle->socketConnectedCallback();
}

void WebSocketBridge::didWebSocketClosed()
{
    m_streamHandle->socketClosedCallback();
}

void WebSocketBridge::didWebSocketMessage(const char* data, int length)
{
    m_streamHandle->socketReadyReadCallback(data, length);
}

void WebSocketBridge::didWebSocketError()
{
    m_streamHandle->socketErrorCallback();
}

} // namespace WebCore

namespace android {

static void OnWebSocketConnected(JNIEnv* env, jobject obj, int pointer) {
    if (pointer) {
        WebCore::WebSocketBridge* bridge = reinterpret_cast<WebCore::WebSocketBridge*>(pointer);
        bridge->didWebSocketConnected();
    }
}

static void OnWebSocketClosed(JNIEnv* env, jobject obj, int pointer) {
    if (pointer) {
        WebCore::WebSocketBridge* bridge = reinterpret_cast<WebCore::WebSocketBridge*>(pointer);
        bridge->didWebSocketClosed();
    }
}

static void OnWebSocketMessage(JNIEnv* env, jobject obj, int pointer, jbyteArray dataArray, int length) {
    if (pointer) {
        WebCore::WebSocketBridge* bridge = reinterpret_cast<WebCore::WebSocketBridge*>(pointer);
        if (jbyte* data = env->GetByteArrayElements(dataArray, NULL)) {
            bridge->didWebSocketMessage(reinterpret_cast<const char*>(data), length);
            env->ReleaseByteArrayElements(dataArray, data, JNI_ABORT);
        }
    }
}

static void OnWebSocketError(JNIEnv* env, jobject obj, int pointer) {
    if (pointer) {
        WebCore::WebSocketBridge* bridge = reinterpret_cast<WebCore::WebSocketBridge*>(pointer);
        bridge->didWebSocketError();
    }
}

/*
 * JNI registration
 */
static JNINativeMethod g_WebSocketMethods[] = {
    { "nativeOnWebSocketConnected", "(I)V", (void*) OnWebSocketConnected },
    { "nativeOnWebSocketClosed", "(I)V", (void*) OnWebSocketClosed },
    { "nativeOnWebSocketMessage", "(I[BI)V", (void*) OnWebSocketMessage },
    { "nativeOnWebSocketError", "(I)V", (void*) OnWebSocketError }
};

int registerWebSockets(JNIEnv* env)
{
#ifndef NDEBUG
    jclass webSocket = env->FindClass(javaWebSocketClass);
    ALOG_ASSERT(webSocket, "Unable to find class");
    env->DeleteLocalRef(webSocket);
#endif
    return jniRegisterNativeMethods(env, javaWebSocketClass, g_WebSocketMethods, NELEM(g_WebSocketMethods));
}

} // namespace android
#endif // ENABLE(WEB_SOCKETS)