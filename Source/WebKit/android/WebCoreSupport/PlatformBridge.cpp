/*
 * Copyright 2009, The Android Open Source Project
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
#include <PlatformBridge.h>

#include "CookieClient.h"
#include "Document.h"
#include "FileSystemClient.h"
#include "FrameView.h"
#include "JNIUtility.h"
#include "JavaSharedClient.h"
#include "KeyGeneratorClient.h"
#include "MemoryUsage.h"
#include "PluginView.h"
#include "RenderLayer.h"
#include "RenderView.h"
#include "Settings.h"
#include "WebCookieJar.h"
#include "WebRequestContext.h"
#include "WebViewCore.h"
#include "npruntime.h"

#include <gui/SurfaceComposerClient.h>
#include <ui/DisplayInfo.h>
#include <ui/PixelFormat.h>
#include <wtf/android/AndroidThreading.h>
#include <wtf/MainThread.h>

#include <algorithm>

using namespace android;

namespace WebCore {

WTF::Vector<String> PlatformBridge::getSupportedKeyStrengthList()
{
    KeyGeneratorClient* client = JavaSharedClient::GetKeyGeneratorClient();
    if (!client)
        return WTF::Vector<String>();

    return client->getSupportedKeyStrengthList();
}

String PlatformBridge::getSignedPublicKeyAndChallengeString(unsigned index, const String& challenge, const KURL& url)
{
    KeyGeneratorClient* client = JavaSharedClient::GetKeyGeneratorClient();
    if (!client)
        return String();

    return client->getSignedPublicKeyAndChallengeString(index, challenge, url);
}

void PlatformBridge::setCookies(const Document* document, const KURL& url, const String& value)
{
    std::string cookieValue(value.utf8().data());
    GURL cookieGurl(url.string().utf8().data());
    bool isPrivateBrowsing = document->settings() && document->settings()->privateBrowsingEnabled();
    WebCookieJar* cookieJar = WebCookieJar::get(isPrivateBrowsing);
    if (cookieJar->allowCookies())
        cookieJar->cookieStore()->SetCookie(cookieGurl, cookieValue);
}

String PlatformBridge::cookies(const Document* document, const KURL& url)
{
    GURL cookieGurl(url.string().utf8().data());
    bool isPrivateBrowsing = document->settings() && document->settings()->privateBrowsingEnabled();
    WebCookieJar* cookieJar = WebCookieJar::get(isPrivateBrowsing);
    String cookieString;
    if (cookieJar->allowCookies()) {
        std::string cookies = cookieJar->cookieStore()->GetCookies(cookieGurl);
        cookieString = cookies.c_str();
    }
    return cookieString;
}

bool PlatformBridge::cookiesEnabled(const Document* document)
{
    bool isPrivateBrowsing = document->settings() && document->settings()->privateBrowsingEnabled();
    return WebCookieJar::get(isPrivateBrowsing)->allowCookies();
}

NPObject* PlatformBridge::pluginScriptableObject(Widget* widget)
{
    if (!widget->isPluginView())
        return 0;

    PluginView* pluginView = static_cast<PluginView*>(widget);
    return pluginView->getNPObject();
}

bool PlatformBridge::isWebViewPaused(const WebCore::FrameView* frameView)
{
    android::WebViewCore* webViewCore = android::WebViewCore::getWebViewCore(frameView);
    return webViewCore->isPaused();
}

bool PlatformBridge::popupsAllowed(NPP)
{
    return false;
}

String PlatformBridge::resolveFilePathForContentUri(const String& contentUri)
{
    FileSystemClient* client = JavaSharedClient::GetFileSystemClient();
    return client->resolveFilePathForContentUri(contentUri);
}

int PlatformBridge::PlatformBridge::screenDepth()
{
    android::DisplayInfo info;
    android::SurfaceComposerClient::getDisplayInfo(android::DisplayID(0), &info);
    return info.pixelFormatInfo.bitsPerPixel;
}

FloatRect PlatformBridge::screenRect()
{
    android::DisplayInfo info;
    android::SurfaceComposerClient::getDisplayInfo(android::DisplayID(0), &info);
    return FloatRect(0.0, 0.0, info.w, info.h);
}

// The visible size on screen in document coordinate
int PlatformBridge::screenWidthInDocCoord(const WebCore::FrameView* frameView)
{
    android::WebViewCore* webViewCore = android::WebViewCore::getWebViewCore(frameView);
    return webViewCore->screenWidth();
}

int PlatformBridge::screenHeightInDocCoord(const WebCore::FrameView* frameView)
{
    android::WebViewCore* webViewCore = android::WebViewCore::getWebViewCore(frameView);
    return webViewCore->screenHeight();
}

String PlatformBridge::computeDefaultLanguage()
{
    String acceptLanguages = WebRequestContext::acceptLanguage();
    size_t length = acceptLanguages.find(',');
    if (length == std::string::npos)
        length = acceptLanguages.length();
    return acceptLanguages.substring(0, length);
}

void PlatformBridge::updateViewport(FrameView* frameView)
{
    android::WebViewCore* webViewCore = android::WebViewCore::getWebViewCore(frameView);
    webViewCore->updateViewport();
}

void PlatformBridge::updateTextfield(FrameView* frameView, Node* nodePtr, bool changeToPassword, const WTF::String& text)
{
    android::WebViewCore* webViewCore = android::WebViewCore::getWebViewCore(frameView);
    webViewCore->updateTextfield(nodePtr, changeToPassword, text);
}

void PlatformBridge::setScrollPosition(ScrollView* scrollView, int x, int y) {
    FrameView* frameView = scrollView->frameView();
    if (!frameView) return;
    // Check to make sure the view is the main FrameView.
    android::WebViewCore *webViewCore = android::WebViewCore::getWebViewCore(scrollView);
    if (webViewCore->mainFrame()->view() == scrollView) {
        x = std::max(0, std::min(frameView->contentsWidth(), x));
        y = std::max(0, std::min(frameView->contentsHeight(), y));
        webViewCore->scrollTo(x, y);
    }
}

int PlatformBridge::lowMemoryUsageMB()
{
    return MemoryUsage::lowMemoryUsageMb();
}

int PlatformBridge::highMemoryUsageMB()
{
    return MemoryUsage::highMemoryUsageMb();
}

int PlatformBridge::highUsageDeltaMB()
{
    return MemoryUsage::highUsageDeltaMb();
}

int PlatformBridge::memoryUsageMB()
{
    return MemoryUsage::memoryUsageMb(false);
}

int PlatformBridge::actualMemoryUsageMB()
{
    return MemoryUsage::memoryUsageMb(true);
}

bool PlatformBridge::canSatisfyMemoryAllocation(long bytes)
{
    JNIEnv* env = JSC::Bindings::getJNIEnv();
    jclass bridgeClass = env->FindClass("android/webkit/JniUtil");
    jmethodID method = env->GetStaticMethodID(bridgeClass, "canSatisfyMemoryAllocation", "(J)Z");
    jboolean canAllocate = env->CallStaticBooleanMethod(bridgeClass, method, static_cast<jlong>(bytes));
    env->DeleteLocalRef(bridgeClass);

    return canAllocate == JNI_TRUE;
}

}  // namespace WebCore


// This is the implementation of AndroidThreading, which is declared in
// JavaScriptCore/wtf/android/AndroidThreading.h. It is provided here, rather
// than in its own source file, to avoid linker problems.
//
// By default, when building a shared library, the linker strips from static
// libraries any compilation units which do not contain any code referenced from
// that static library. Since
// AndroidThreading::scheduleDispatchFunctionsOnMainThread is not referenced
// from libwebcore.a, implementing it in its own compilation unit results in it
// being stripped. This stripping can be avoided by using the linker option
// --whole-archive for libwebcore.a, but this adds considerably to the size of
// libwebcore.so.

namespace WTF {

// Callback in the main thread.
static void timeoutFired(void*)
{
    dispatchFunctionsFromMainThread();
}

void AndroidThreading::scheduleDispatchFunctionsOnMainThread()
{
    JavaSharedClient::EnqueueFunctionPtr(timeoutFired, 0);
}

}  // namespace WTF
