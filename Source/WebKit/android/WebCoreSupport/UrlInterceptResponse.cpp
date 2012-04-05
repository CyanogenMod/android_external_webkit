/*
 * Copyright 2010, The Android Open Source Project
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

#define LOG_TAG "UrlInterceptResponse"
#include "config.h"

#include "JNIUtility.h"
#include "UrlInterceptResponse.h"
#include "WebCoreJni.h"

#include <ScopedLocalRef.h>
#include <utils/Log.h>

namespace android {

class JavaInputStreamWrapper {
public:
    JavaInputStreamWrapper(JNIEnv* env, jobject inputStream)
            : m_inputStream(env->NewGlobalRef(inputStream))
            , m_buffer(NULL) {
        LOG_ALWAYS_FATAL_IF(!m_inputStream);
        ScopedLocalRef<jclass> inputStreamClass(env, env->FindClass("java/io/InputStream"));
        LOG_ALWAYS_FATAL_IF(!inputStreamClass.get());
        m_read = env->GetMethodID(inputStreamClass.get(), "read", "([B)I");
        LOG_ALWAYS_FATAL_IF(!m_read);
        m_close = env->GetMethodID(inputStreamClass.get(), "close", "()V");
        LOG_ALWAYS_FATAL_IF(!m_close);
    }

    ~JavaInputStreamWrapper() {
        JNIEnv* env = JSC::Bindings::getJNIEnv();
        env->CallVoidMethod(m_inputStream, m_close);
        checkException(env);
        env->DeleteGlobalRef(m_inputStream);
        // In case we never call read().
        if (m_buffer)
            env->DeleteGlobalRef(m_buffer);
    }

    void read(std::vector<char>* out) {
        JNIEnv* env = JSC::Bindings::getJNIEnv();
        // Initialize our read buffer to the capacity of out.
        if (!m_buffer) {
            ScopedLocalRef<jbyteArray> buffer_local(env, env->NewByteArray(out->capacity()));
            m_buffer = static_cast<jbyteArray>(env->NewGlobalRef(buffer_local.get()));
        }
        int size = env->CallIntMethod(m_inputStream, m_read, m_buffer);
        if (checkException(env) || size < 0)
            return;
        // Copy from m_buffer to out.
        out->resize(size);
        env->GetByteArrayRegion(m_buffer, 0, size, (jbyte*)&out->front());
    }

private:
    jobject    m_inputStream;
    jbyteArray m_buffer;
    jmethodID  m_read;
    jmethodID  m_close;
};

UrlInterceptResponse::UrlInterceptResponse(JNIEnv* env, jobject response) {
    ScopedLocalRef<jclass> javaResponse(env, env->FindClass("android/webkit/WebResourceResponse"));
    LOG_ALWAYS_FATAL_IF(!javaResponse.get());
    jfieldID mimeType = env->GetFieldID(javaResponse.get(), "mMimeType", "Ljava/lang/String;");
    LOG_ALWAYS_FATAL_IF(!mimeType);
    jfieldID encoding = env->GetFieldID(javaResponse.get(), "mEncoding", "Ljava/lang/String;");
    LOG_ALWAYS_FATAL_IF(!encoding);
    jfieldID inputStream = env->GetFieldID(javaResponse.get(), "mInputStream", "Ljava/io/InputStream;");
    LOG_ALWAYS_FATAL_IF(!inputStream);

    ScopedLocalRef<jobject> stream(env, env->GetObjectField(response, inputStream));
    if (stream.get())
        m_inputStream.set(new JavaInputStreamWrapper(env, stream.get()));

    ScopedLocalRef<jstring> mimeStr(env, static_cast<jstring>(env->GetObjectField(response, mimeType)));
    ScopedLocalRef<jstring> encodingStr(env, static_cast<jstring>(env->GetObjectField(response, encoding)));

    if (mimeStr.get()) {
        const char* s = env->GetStringUTFChars(mimeStr.get(), NULL);
        m_mimeType.assign(s, env->GetStringUTFLength(mimeStr.get()));
        env->ReleaseStringUTFChars(mimeStr.get(), s);
    }
    if (encodingStr.get()) {
        const char* s = env->GetStringUTFChars(encodingStr.get(), NULL);
        m_encoding.assign(s, env->GetStringUTFLength(encodingStr.get()));
        env->ReleaseStringUTFChars(encodingStr.get(), s);
    }
}

UrlInterceptResponse::~UrlInterceptResponse() {
    // Cannot be inlined because of JavaInputStreamWrapper visibility.
}

bool UrlInterceptResponse::readStream(std::vector<char>* out) const {
    if (!m_inputStream)
        return false;
    m_inputStream->read(out);
    return true;
}

}  // namespace android
