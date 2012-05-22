/*
 * Copyright 2006, The Android Open Source Project
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

#define LOG_TAG "favicons"

#include "config.h"
#include "WebIconDatabase.h"

#include "FileSystem.h"
#include "GraphicsJNI.h"
#include "IconDatabase.h"
#include "Image.h"
#include "IntRect.h"
#include "JavaSharedClient.h"
#include "KURL.h"
#include "WebCoreJni.h"

#include <JNIHelp.h>
#include <JNIUtility.h>
#include <SharedBuffer.h>
#include <SkBitmap.h>
#include <SkImageDecoder.h>
#include <SkTemplates.h>
#include <pthread.h>
#include <utils/misc.h>
#include <wtf/Platform.h>
#include <wtf/text/CString.h>

namespace android {

SkBitmap* webcoreImageToSkBitmap(WebCore::Image* icon)
{
    if (!icon)
        return 0;
    WebCore::SharedBuffer* buffer = icon->data();
    if (!buffer)
        return 0;
    SkBitmap* bm = new SkBitmap;
    if (!SkImageDecoder::DecodeMemory(buffer->data(), buffer->size(), bm,
                                      SkBitmap::kNo_Config,
                                      SkImageDecoder::kDecodePixels_Mode)
            || bm->isNull() || !bm->width() || !bm->height()
            || bm->config() == SkBitmap::kNo_Config) {
        delete bm;
        return 0;
    }
    return bm;
}

jobject webcoreImageToJavaBitmap(JNIEnv* env, WebCore::Image* icon)
{
    SkBitmap* bm = webcoreImageToSkBitmap(icon);
    if (!bm)
        return 0;
    return GraphicsJNI::createBitmap(env, bm, false, NULL);
}

static WebIconDatabase* gIconDatabaseClient = new WebIconDatabase();

bool WebIconDatabase::performImport()
{
    // We don't do do any old-style database importing.
    return true;
}

// Called on the WebCore thread
void WebIconDatabase::didImportIconURLForPageURL(const WTF::String& pageURL)
{
    // FIXME: After http://trac.webkit.org/changeset/81719 this method is called
    // on the WebCore thread, so switching threads via this queue is superfluous
    // and should be removed. http://b/4565022
    mNotificationsMutex.lock();
    mNotifications.append(pageURL);
    if (!mDeliveryRequested) {
        mDeliveryRequested = true;
        JavaSharedClient::EnqueueFunctionPtr(DeliverNotifications, this);
    }
    mNotificationsMutex.unlock();
}

void WebIconDatabase::didImportIconDataForPageURL(const WTF::String& pageURL)
{
    // WebKit1 only has a single "icon did change" notification.
    didImportIconURLForPageURL(pageURL);
}

void WebIconDatabase::didChangeIconForPageURL(const WTF::String& pageURL)
{
    // WebKit1 only has a single "icon did change" notification.
    didImportIconURLForPageURL(pageURL);
}

void WebIconDatabase::didRemoveAllIcons()
{
}

void WebIconDatabase::didFinishURLImport()
{
}

// Called in the WebCore thread
void WebIconDatabase::RegisterForIconNotification(WebIconDatabaseClient* client)
{
    WebIconDatabase* db = gIconDatabaseClient;
    for (unsigned i = 0; i < db->mClients.size(); ++i) {
        // Do not add the same client twice.
        if (db->mClients[i] == client)
            return;
    }
    gIconDatabaseClient->mClients.append(client);
}

// Called in the WebCore thread
void WebIconDatabase::UnregisterForIconNotification(WebIconDatabaseClient* client)
{
    WebIconDatabase* db = gIconDatabaseClient;
    for (unsigned i = 0; i < db->mClients.size(); ++i) {
        if (db->mClients[i] == client) {
            db->mClients.remove(i);
            break;
        }
    }
}

// Called in the WebCore thread
void WebIconDatabase::DeliverNotifications(void* v)
{
    ASSERT(v);
    ((WebIconDatabase*)v)->deliverNotifications();
}

// Called in the WebCore thread
void WebIconDatabase::deliverNotifications()
{
    ASSERT(mDeliveryRequested);

    // Swap the notifications queue
    Vector<WTF::String> queue;
    mNotificationsMutex.lock();
    queue.swap(mNotifications);
    mDeliveryRequested = false;
    mNotificationsMutex.unlock();

    // Swap the clients queue
    Vector<WebIconDatabaseClient*> clients;
    clients.swap(mClients);

    for (unsigned i = 0; i < queue.size(); ++i) {
        for (unsigned j = 0; j < clients.size(); ++j) {
            clients[j]->didAddIconForPageUrl(queue[i]);
        }
    }
}

static void Open(JNIEnv* env, jobject obj, jstring path)
{
    WebCore::IconDatabaseBase& iconDb = WebCore::iconDatabase();
    if (iconDb.isOpen())
        return;
    iconDb.setEnabled(true);
    iconDb.setClient(gIconDatabaseClient);
    ALOG_ASSERT(path, "No path given to nativeOpen");
    WTF::String pathStr = jstringToWtfString(env, path);
    WTF::CString fullPath = WebCore::pathByAppendingComponent(pathStr,
            WebCore::IconDatabase::defaultDatabaseFilename()).utf8();
    mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP;
    bool didSetPermissions = false;
    if (access(fullPath.data(), F_OK) == 0) {
        if (chmod(fullPath.data(), mode) == 0)
            didSetPermissions = true;
    } else {
        int fd = open(fullPath.data(), O_CREAT, mode);
        if (fd >= 0) {
            close(fd);
            didSetPermissions = true;
        }
    }
    if (didSetPermissions) {
        ALOGV("Opening WebIconDatabase file '%s'", pathStr.latin1().data());
        bool res = iconDb.open(pathStr, WebCore::IconDatabase::defaultDatabaseFilename());
        if (!res)
            ALOGE("Open failed!");
    } else
        ALOGE("Failed to set permissions on '%s'", fullPath.data());
}

static void Close(JNIEnv* env, jobject obj)
{
    WebCore::iconDatabase().close();
}

static void RemoveAllIcons(JNIEnv* env, jobject obj)
{
    ALOGV("Removing all icons");
    WebCore::iconDatabase().removeAllIcons();
}

static jobject IconForPageUrl(JNIEnv* env, jobject obj, jstring url)
{
    ALOG_ASSERT(url, "No url given to iconForPageUrl");
    WTF::String urlStr = jstringToWtfString(env, url);

    // FIXME: This method should not be used from outside WebCore and will be removed.
    // http://trac.webkit.org/changeset/81484
    WebCore::Image* icon = WebCore::iconDatabase().synchronousIconForPageURL(urlStr, WebCore::IntSize(16, 16));
    ALOGV("Retrieving icon for '%s' %p", urlStr.latin1().data(), icon);
    return webcoreImageToJavaBitmap(env, icon);
}

static void RetainIconForPageUrl(JNIEnv* env, jobject obj, jstring url)
{
    ALOG_ASSERT(url, "No url given to retainIconForPageUrl");
    WTF::String urlStr = jstringToWtfString(env, url);

    ALOGV("Retaining icon for '%s'", urlStr.latin1().data());
    WebCore::iconDatabase().retainIconForPageURL(urlStr);
}

static void ReleaseIconForPageUrl(JNIEnv* env, jobject obj, jstring url)
{
    ALOG_ASSERT(url, "No url given to releaseIconForPageUrl");
    WTF::String urlStr = jstringToWtfString(env, url);

    ALOGV("Releasing icon for '%s'", urlStr.latin1().data());
    WebCore::iconDatabase().releaseIconForPageURL(urlStr);
}

/*
 * JNI registration
 */
static JNINativeMethod gWebIconDatabaseMethods[] = {
    { "nativeOpen", "(Ljava/lang/String;)V",
        (void*) Open },
    { "nativeClose", "()V",
        (void*) Close },
    { "nativeRemoveAllIcons", "()V",
        (void*) RemoveAllIcons },
    { "nativeIconForPageUrl", "(Ljava/lang/String;)Landroid/graphics/Bitmap;",
        (void*) IconForPageUrl },
    { "nativeRetainIconForPageUrl", "(Ljava/lang/String;)V",
        (void*) RetainIconForPageUrl },
    { "nativeReleaseIconForPageUrl", "(Ljava/lang/String;)V",
        (void*) ReleaseIconForPageUrl }
};

int registerWebIconDatabase(JNIEnv* env)
{
#ifndef NDEBUG
    jclass webIconDatabase = env->FindClass("android/webkit/WebIconDatabaseClassic");
    ALOG_ASSERT(webIconDatabase, "Unable to find class android.webkit.WebIconDatabaseClassic");
    env->DeleteLocalRef(webIconDatabase);
#endif

    return jniRegisterNativeMethods(env, "android/webkit/WebIconDatabaseClassic",
            gWebIconDatabaseMethods, NELEM(gWebIconDatabaseMethods));
}

}
