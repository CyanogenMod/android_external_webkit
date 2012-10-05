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

#define LOG_TAG "webcoreglue"

#include "config.h"

#include "BackForwardList.h"
#include "ChromeClientAndroid.h"
#include "ContextMenuClientAndroid.h"
#include "CookieClient.h"
#include "DeviceMotionClientAndroid.h"
#include "DeviceOrientationClientAndroid.h"
#include "DragClientAndroid.h"
#include "EditorClientAndroid.h"
#include "FocusController.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameLoaderClientAndroid.h"
#include "FrameView.h"
#include "GraphicsContext.h"
#include "HistoryItem.h"
#include "InspectorClientAndroid.h"
#include "IntRect.h"
#include "JavaSharedClient.h"
#include "Page.h"
#include "PlatformGraphicsContext.h"
#include "ResourceRequest.h"
#include "ScriptController.h"
#include "SecurityOrigin.h"
#include "SelectionController.h"
#include "Settings.h"
#include "SharedBuffer.h"
#include "SkBitmap.h"
#include "SkCanvas.h"
#include "SkImageEncoder.h"
#include "SubstituteData.h"
#include "TimerClient.h"
#include "TextEncoding.h"
#include "WebCoreViewBridge.h"
#include "WebFrameView.h"
#include "WebViewCore.h"

#include <JNIUtility.h>
#include <jni.h>
#include <utils/Log.h>

#define EXPORT __attribute__((visibility("default")))

namespace android {

extern int registerWebFrame(JNIEnv*);
extern int registerJavaBridge(JNIEnv*);
extern int registerResourceLoader(JNIEnv*);
extern int registerWebViewCore(JNIEnv*);
extern int registerWebHistory(JNIEnv*);
extern int registerWebIconDatabase(JNIEnv*);
extern int registerWebSettings(JNIEnv*);
extern int registerWebView(JNIEnv*);
extern int registerViewStateSerializer(JNIEnv*);
#if ENABLE(DATABASE)
extern int registerWebStorage(JNIEnv*);
#endif
extern int registerGeolocationPermissions(JNIEnv*);
extern int registerMockGeolocation(JNIEnv*);
#if ENABLE(VIDEO)
extern int registerMediaPlayerAudio(JNIEnv*);
extern int registerMediaPlayerVideo(JNIEnv*);
#endif
extern int registerDeviceMotionAndOrientationManager(JNIEnv*);
extern int registerCookieManager(JNIEnv*);

}

struct RegistrationMethod {
    const char* name;
    int (*func)(JNIEnv*);
};

static RegistrationMethod gWebCoreRegMethods[] = {
    { "JavaBridge", android::registerJavaBridge },
    { "WebFrame", android::registerWebFrame },
    { "WebViewCore", android::registerWebViewCore },
    { "WebHistory", android::registerWebHistory },
    { "WebIconDatabase", android::registerWebIconDatabase },
    { "WebSettingsClassic", android::registerWebSettings },
#if ENABLE(DATABASE)
    { "WebStorage", android::registerWebStorage },
#endif
    { "WebView", android::registerWebView },
    { "ViewStateSerializer", android::registerViewStateSerializer },
    { "GeolocationPermissions", android::registerGeolocationPermissions },
    { "MockGeolocation", android::registerMockGeolocation },
#if ENABLE(VIDEO)
    { "HTML5Audio", android::registerMediaPlayerAudio },
    { "HTML5VideoViewProxy", android::registerMediaPlayerVideo },
#endif
    { "DeviceMotionAndOrientationManager", android::registerDeviceMotionAndOrientationManager },
    { "CookieManager", android::registerCookieManager },
};

EXPORT jint JNI_OnLoad(JavaVM* vm, void* reserved)
{
    // Save the JavaVM pointer for use globally.
    JSC::Bindings::setJavaVM(vm);

    JNIEnv* env = NULL;
    jint result = -1;

    if (vm->GetEnv((void**) &env, JNI_VERSION_1_4) != JNI_OK) {
        ALOGE("GetEnv failed!");
        return result;
    }
    ALOG_ASSERT(env, "Could not retrieve the env!");

    const RegistrationMethod* method = gWebCoreRegMethods;
    const RegistrationMethod* end = method + sizeof(gWebCoreRegMethods)/sizeof(RegistrationMethod);
    while (method != end) {
        if (method->func(env) < 0) {
            ALOGE("%s registration failed!", method->name);
            return result;
        }
        method++;
    }

    // Initialize rand() function. The rand() function is used in
    // FileSystemAndroid to create a random temporary filename.
    srand(time(NULL));

    return JNI_VERSION_1_4;
}
