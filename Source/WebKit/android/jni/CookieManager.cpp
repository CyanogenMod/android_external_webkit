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

#include "config.h"

#include "ChromiumIncludes.h"
#include "WebCookieJar.h"
#include "WebCoreJni.h"
#include <JNIHelp.h>

using namespace base;
using namespace net;

namespace android {

// JNI for android.webkit.CookieManagerClassic
static const char* javaCookieManagerClass = "android/webkit/CookieManagerClassic";

static bool acceptCookie(JNIEnv*, jobject)
{
    // This is a static method which gets the cookie policy for all WebViews. We
    // always apply the same configuration to the contexts for both regular and
    // private browsing, so expect the same result here.
    bool regularAcceptCookies = WebCookieJar::get(false)->allowCookies();
    ASSERT(regularAcceptCookies == WebCookieJar::get(true)->allowCookies());
    return regularAcceptCookies;
}

static jstring getCookie(JNIEnv* env, jobject, jstring url, jboolean privateBrowsing)
{
    GURL gurl(jstringToStdString(env, url));
    CookieOptions options;
    options.set_include_httponly();
    std::string cookies = WebCookieJar::get(privateBrowsing)->cookieStore()->GetCookieMonster()->GetCookiesWithOptions(gurl, options);
    return stdStringToJstring(env, cookies);
}

static bool hasCookies(JNIEnv*, jobject, jboolean privateBrowsing)
{
    return WebCookieJar::get(privateBrowsing)->getNumCookiesInDatabase() > 0;
}

static void removeAllCookie(JNIEnv*, jobject)
{
    WebCookieJar::get(false)->cookieStore()->GetCookieMonster()->DeleteAll(true);
    // This will lazily create a new private browsing context. However, if the
    // context doesn't already exist, there's no need to create it, as cookies
    // for such contexts are cleared up when we're done with them.
    // TODO: Consider adding an optimisation to not create the context if it
    // doesn't already exist.
    WebCookieJar::get(true)->cookieStore()->GetCookieMonster()->DeleteAll(true);

    // The Java code removes cookies directly from the backing database, so we do the same,
    // but with a NULL callback so it's asynchronous.
    WebCookieJar::get(true)->cookieStore()->GetCookieMonster()->FlushStore(NULL);
}

static void removeExpiredCookie(JNIEnv*, jobject)
{
    // This simply forces a GC. The getters delete expired cookies so won't return expired cookies anyway.
    WebCookieJar::get(false)->cookieStore()->GetCookieMonster()->GetAllCookies();
    WebCookieJar::get(true)->cookieStore()->GetCookieMonster()->GetAllCookies();
}

static void removeSessionCookies(WebCookieJar* cookieJar)
{
  CookieMonster* cookieMonster = cookieJar->cookieStore()->GetCookieMonster();
  CookieList cookies = cookieMonster->GetAllCookies();
  for (CookieList::const_iterator iter = cookies.begin(); iter != cookies.end(); ++iter) {
    if (iter->IsSessionCookie())
      cookieMonster->DeleteCanonicalCookie(*iter);
  }
}

static void removeSessionCookie(JNIEnv*, jobject)
{
  removeSessionCookies(WebCookieJar::get(false));
  removeSessionCookies(WebCookieJar::get(true));
}

static void setAcceptCookie(JNIEnv*, jobject, jboolean accept)
{
    // This is a static method which configures the cookie policy for all
    // WebViews, so we configure the contexts for both regular and private
    // browsing.
    WebCookieJar::get(false)->setAllowCookies(accept);
    WebCookieJar::get(true)->setAllowCookies(accept);
}

static void setCookie(JNIEnv* env, jobject, jstring url, jstring value, jboolean privateBrowsing)
{
    GURL gurl(jstringToStdString(env, url));
    std::string line(jstringToStdString(env, value));
    CookieOptions options;
    options.set_include_httponly();
    WebCookieJar::get(privateBrowsing)->cookieStore()->GetCookieMonster()->SetCookieWithOptions(gurl, line, options);
}

static void flushCookieStore(JNIEnv*, jobject)
{
    WebCookieJar::flush();
}

static bool acceptFileSchemeCookies(JNIEnv*, jobject)
{
    return WebCookieJar::acceptFileSchemeCookies();
}

static void setAcceptFileSchemeCookies(JNIEnv*, jobject, jboolean accept)
{
    WebCookieJar::setAcceptFileSchemeCookies(accept);
}

static JNINativeMethod gCookieManagerMethods[] = {
    { "nativeAcceptCookie", "()Z", (void*) acceptCookie },
    { "nativeGetCookie", "(Ljava/lang/String;Z)Ljava/lang/String;", (void*) getCookie },
    { "nativeHasCookies", "(Z)Z", (void*) hasCookies },
    { "nativeRemoveAllCookie", "()V", (void*) removeAllCookie },
    { "nativeRemoveExpiredCookie", "()V", (void*) removeExpiredCookie },
    { "nativeRemoveSessionCookie", "()V", (void*) removeSessionCookie },
    { "nativeSetAcceptCookie", "(Z)V", (void*) setAcceptCookie },
    { "nativeSetCookie", "(Ljava/lang/String;Ljava/lang/String;Z)V", (void*) setCookie },
    { "nativeFlushCookieStore", "()V", (void*) flushCookieStore },
    { "nativeAcceptFileSchemeCookies", "()Z", (void*) acceptFileSchemeCookies },
    { "nativeSetAcceptFileSchemeCookies", "(Z)V", (void*) setAcceptFileSchemeCookies },
};

int registerCookieManager(JNIEnv* env)
{
#ifndef NDEBUG
    jclass cookieManager = env->FindClass(javaCookieManagerClass);
    ALOG_ASSERT(cookieManager, "Unable to find class");
    env->DeleteLocalRef(cookieManager);
#endif
    return jniRegisterNativeMethods(env, javaCookieManagerClass, gCookieManagerMethods, NELEM(gCookieManagerMethods));
}

} // namespace android
