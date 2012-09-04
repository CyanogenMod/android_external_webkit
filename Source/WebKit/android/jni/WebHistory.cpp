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

#define LOG_TAG "webhistory"

#include "config.h"
#include "WebHistory.h"

#include "AndroidLog.h"
#include "BackForwardList.h"
#include "BackForwardListImpl.h"
#include "DocumentLoader.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameLoaderClientAndroid.h"
#include "FrameTree.h"
#include "GraphicsJNI.h"
#include "HistoryItem.h"
#include "IconDatabase.h"
#include "Page.h"
#include "TextEncoding.h"
#include "WebCoreFrameBridge.h"
#include "WebCoreJni.h"
#include "WebIconDatabase.h"

#include <JNIHelp.h>
#include "JNIUtility.h"
#include <SkUtils.h>
#include <utils/misc.h>
#include <wtf/OwnPtr.h>
#include <wtf/Platform.h>
#include <wtf/text/CString.h>

namespace android {

// Forward declarations
static void writeItem(WTF::Vector<char>& vector, WebCore::HistoryItem* item);
static void writeChildrenRecursive(WTF::Vector<char>& vector, WebCore::HistoryItem* parent);
static bool readItemRecursive(WebCore::HistoryItem* child, const char** pData, int length);

// Field ids for WebHistoryClassicItems
struct WebHistoryItemClassicFields {
    jmethodID   mInit;
} gWebHistoryItemClassic;

struct WebBackForwardListClassicFields {
    jmethodID   mAddHistoryItem;
    jmethodID   mRemoveHistoryItem;
    jmethodID   mSetCurrentIndex;
} gWebBackForwardListClassic;

//--------------------------------------------------------------------------
// WebBackForwardListClassic native methods.
//--------------------------------------------------------------------------

static void WebHistoryClose(JNIEnv* env, jobject obj, jint frame)
{
    ALOG_ASSERT(frame, "Close needs a valid Frame pointer!");
    WebCore::Frame* pFrame = (WebCore::Frame*)frame;

    WebCore::BackForwardListImpl* list = static_cast<WebCore::BackForwardListImpl*>(pFrame->page()->backForwardList());
    RefPtr<WebCore::HistoryItem> current = list->currentItem();
    // Remove each item instead of using close(). close() is intended to be used
    // right before the list is deleted.
    WebCore::HistoryItemVector& entries = list->entries();
    int size = entries.size();
    for (int i = size - 1; i >= 0; --i)
        list->removeItem(entries[i].get());
    // Add the current item back to the list.
    if (current) {
        current->setBridge(0);
        // addItem will update the children to match the newly created bridge
        list->addItem(current);

        /*
         * The Grand Prix site uses anchor navigations to change the display.
         * WebKit tries to be smart and not load child frames that have the
         * same history urls during an anchor navigation. This means that the
         * current history item stored in the child frame's loader does not
         * match the item found in the history tree. If we remove all the
         * entries in the back/foward list, we have to restore the entire tree
         * or else a HistoryItem might have a deleted parent.
         *
         * In order to restore the history tree correctly, we have to look up
         * all the frames first and then look up the history item. We do this
         * because the history item in the tree may be null at this point.
         * Unfortunately, a HistoryItem can only search its immediately
         * children so we do a breadth-first rebuild of the tree.
         */

        // Keep a small list of child frames to traverse.
        WTF::Vector<WebCore::Frame*> frameQueue;
        // Fix the top-level item.
        pFrame->loader()->history()->setCurrentItem(current.get());
        WebCore::Frame* child = pFrame->tree()->firstChild();
        // Remember the parent history item so we can search for a child item.
        RefPtr<WebCore::HistoryItem> parent = current;
        while (child) {
            // Use the old history item since the current one may have a
            // deleted parent.
            WebCore::HistoryItem* item = parent->childItemWithTarget(child->tree()->name());
            child->loader()->history()->setCurrentItem(item);
            // Append the first child to the queue if it exists. If there is no
            // item, then we do not need to traverse the children since there
            // will be no parent history item.
            WebCore::Frame* firstChild;
            if (item && (firstChild = child->tree()->firstChild()))
                frameQueue.append(firstChild);
            child = child->tree()->nextSibling();
            // If we don't have a sibling for this frame and the queue isn't
            // empty, use the next entry in the queue.
            if (!child && !frameQueue.isEmpty()) {
                child = frameQueue.at(0);
                frameQueue.remove(0);
                // Figure out the parent history item used when searching for
                // the history item to use.
                parent = child->tree()->parent()->loader()->history()->currentItem();
            }
        }
    }
}

static void WebHistoryRestoreIndex(JNIEnv* env, jobject obj, jint frame, jint index)
{
    ALOG_ASSERT(frame, "RestoreState needs a valid Frame pointer!");
    WebCore::Frame* pFrame = (WebCore::Frame*)frame;
    WebCore::Page* page = pFrame->page();
    WebCore::HistoryItem* currentItem =
            static_cast<WebCore::BackForwardListImpl*>(page->backForwardList())->entries()[index].get();

    // load the current page with FrameLoadTypeIndexedBackForward so that it
    // will use cache when it is possible
    page->goToItem(currentItem, FrameLoadTypeIndexedBackForward);
}

static jint WebHistoryInflate(JNIEnv* env, jobject obj, jint frame, jbyteArray data)
{
    ALOG_ASSERT(frame, "Inflate needs a valid frame pointer!");
    ALOG_ASSERT(data, "Inflate needs a valid data pointer!");

    // Get the actual bytes and the length from the java array.
    const jbyte* bytes = env->GetByteArrayElements(data, NULL);
    jsize size = env->GetArrayLength(data);

    // Inflate the history tree into one HistoryItem or null if the inflation
    // failed.
    RefPtr<WebCore::HistoryItem> newItem = WebCore::HistoryItem::create();
    WebHistoryItem* bridge = new WebHistoryItem(newItem.get());
    newItem->setBridge(bridge);

    // Inflate the item recursively. If it fails, that is ok. We'll have an
    // incomplete HistoryItem but that is better than crashing due to a null
    // item.
    // We have a 2nd local variable since read_item_recursive may change the
    // ptr's value. We can't pass &bytes since we have to send bytes to
    // ReleaseByteArrayElements unchanged.
    const char* ptr = reinterpret_cast<const char*>(bytes);
    readItemRecursive(newItem.get(), &ptr, (int)size);
    env->ReleaseByteArrayElements(data, const_cast<jbyte*>(bytes), JNI_ABORT);
    bridge->setActive();

    // Add the new item to the back/forward list.
    WebCore::Frame* pFrame = (WebCore::Frame*)frame;
    pFrame->page()->backForwardList()->addItem(newItem);

    // Update the item.
    bridge->updateHistoryItem(newItem.get());
    // Ref here because Java expects to adopt the reference, and as such will not
    // call ref on it. However, setBridge has also adopted the reference
    // TODO: This is confusing as hell, clean up ownership and have setBridge
    // take a RefPtr instead of a raw ptr and calling adoptRef on it
    bridge->ref();
    return reinterpret_cast<jint>(bridge);
}

static void WebHistoryRef(JNIEnv* env, jobject obj, jint ptr)
{
    if (ptr)
        reinterpret_cast<WebHistoryItem*>(ptr)->ref();
}

static void WebHistoryUnref(JNIEnv* env, jobject obj, jint ptr)
{
    if (ptr)
        reinterpret_cast<WebHistoryItem*>(ptr)->deref();
}

static jobject WebHistoryGetTitle(JNIEnv* env, jobject obj, jint ptr)
{
    if (!ptr)
        return 0;
    WebHistoryItem* item = reinterpret_cast<WebHistoryItem*>(ptr);
    MutexLocker locker(item->m_lock);
    return wtfStringToJstring(env, item->m_title, false);
}

static jobject WebHistoryGetUrl(JNIEnv* env, jobject obj, jint ptr)
{
    if (!ptr)
        return 0;
    WebHistoryItem* item = reinterpret_cast<WebHistoryItem*>(ptr);
    MutexLocker locker(item->m_lock);
    return wtfStringToJstring(env, item->m_url, false);
}

static jobject WebHistoryGetOriginalUrl(JNIEnv* env, jobject obj, jint ptr)
{
    if (!ptr)
        return 0;
    WebHistoryItem* item = reinterpret_cast<WebHistoryItem*>(ptr);
    MutexLocker locker(item->m_lock);
    return wtfStringToJstring(env, item->m_originalUrl, false);
}

static jobject WebHistoryGetFlattenedData(JNIEnv* env, jobject obj, jint ptr)
{
    if (!ptr)
        return 0;

    WebHistoryItem* item = reinterpret_cast<WebHistoryItem*>(ptr);
    MutexLocker locker(item->m_lock);

    if (!item->m_dataCached) {
        // Try to create a new java byte array.
        jbyteArray b = env->NewByteArray(item->m_data.size());
        if (!b)
            return NULL;

        // Write our flattened data to the java array.
        env->SetByteArrayRegion(b, 0, item->m_data.size(),
                                (const jbyte*)item->m_data.data());
        item->m_dataCached = env->NewGlobalRef(b);
        env->DeleteLocalRef(b);
    }
    return item->m_dataCached;
}

static jobject WebHistoryGetFavicon(JNIEnv* env, jobject obj, jint ptr)
{
    if (!ptr)
        return 0;
    WebHistoryItem* item = reinterpret_cast<WebHistoryItem*>(ptr);
    MutexLocker locker(item->m_lock);
    if (!item->m_faviconCached && item->m_favicon) {
        jobject favicon = GraphicsJNI::createBitmap(env,
                                                    item->m_favicon,
                                                    false, NULL);
        item->m_favicon = 0; // Framework now owns the pointer
        item->m_faviconCached = env->NewGlobalRef(favicon);
        env->DeleteLocalRef(favicon);
    }
    return item->m_faviconCached;
}

// 6 empty strings + no document state + children count + 2 scales = 10 unsigned values
// 1 char for isTargetItem.
#define HISTORY_MIN_SIZE ((int)(sizeof(unsigned) * 10 + sizeof(char)))

void WebHistory::Flatten(JNIEnv* env, WTF::Vector<char>& vector, WebCore::HistoryItem* item)
{
    if (!item)
        return;

    // Reserve a vector of chars with an initial size of HISTORY_MIN_SIZE.
    vector.reserveCapacity(HISTORY_MIN_SIZE);

    // Write the top-level history item and then write all the children
    // recursively.
    ALOG_ASSERT(item->bridge(), "Why don't we have a bridge object here?");
    writeItem(vector, item);
    writeChildrenRecursive(vector, item);
}

void WebHistoryItem::updateHistoryItem(WebCore::HistoryItem* item) {
    // Do not want to update during inflation.
    if (!m_active)
        return;
    WebHistoryItem* webItem = this;
    // Now we need to update the top-most WebHistoryItem based on the top-most
    // HistoryItem.
    if (m_parent) {
        webItem = m_parent.get();
        if (webItem->hasOneRef()) {
            // if the parent only has one ref, it is from this WebHistoryItem.
            // This means that the matching WebCore::HistoryItem has been freed.
            // This can happen during clear().
            ALOGW("Can't updateHistoryItem as the top HistoryItem is gone");
            return;
        }
        while (webItem->parent())
            webItem = webItem->parent();
        item = webItem->historyItem();
        if (!item) {
            // If a HistoryItem only exists for page cache, it is possible that
            // the parent HistoryItem destroyed before the child HistoryItem. If
            // it happens, skip updating.
            ALOGW("Can't updateHistoryItem as the top HistoryItem is gone");
            return;
        }
    }
    JNIEnv* env = JSC::Bindings::getJNIEnv();
    if (!env)
        return;

    MutexLocker locker(webItem->m_lock);

    // TODO: Figure out if we can't just use item->urlString() instead...
    const WTF::String urlString = WebFrame::convertIDNToUnicode(item->url());
    webItem->m_url = urlString.threadsafeCopy();
    const WTF::String originalUrlString = WebFrame::convertIDNToUnicode(item->originalURL());
    webItem->m_originalUrl = originalUrlString.threadsafeCopy();
    const WTF::String& titleString = item->title();
    webItem->m_title = titleString.threadsafeCopy();

    // Try to get the favicon from the history item. For some pages like Grand
    // Prix, there are history items with anchors. If the icon fails for the
    // item, try to get the icon using the url without the ref.
    jobject favicon = NULL;
    WTF::String url = item->urlString();
    if (item->url().hasFragmentIdentifier()) {
        int refIndex = url.reverseFind('#');
        url = url.substring(0, refIndex);
    }
    // FIXME: This method should not be used from outside WebCore and will be removed.
    // http://trac.webkit.org/changeset/81484
    WebCore::Image* icon = WebCore::iconDatabase().synchronousIconForPageURL(url, WebCore::IntSize(16, 16));
    delete webItem->m_favicon;
    webItem->m_favicon = webcoreImageToSkBitmap(icon);
    if (webItem->m_faviconCached) {
        env->DeleteGlobalRef(webItem->m_faviconCached);
        webItem->m_faviconCached = 0;
    }

    webItem->m_data.clear();
    WebHistory::Flatten(env, webItem->m_data, item);
    if (webItem->m_dataCached) {
        env->DeleteGlobalRef(webItem->m_dataCached);
        webItem->m_dataCached = 0;
    }
}

WebHistoryItem::~WebHistoryItem()
{
    delete m_favicon;
    JNIEnv* env = JSC::Bindings::getJNIEnv();
    if (!env) {
        ALOGW("Failed to get JNIEnv*! Potential memory leak!");
        return;
    }
    if (m_faviconCached) {
        env->DeleteGlobalRef(m_faviconCached);
        m_faviconCached = 0;
    }
    if (m_dataCached) {
        env->DeleteGlobalRef(m_dataCached);
        m_dataCached = 0;
    }
}

static void historyItemChanged(WebCore::HistoryItem* item) {
    ALOG_ASSERT(item, "historyItemChanged called with a null item");

    if (item->bridge())
        item->bridge()->updateHistoryItem(item);
}

void WebHistory::AddItem(const AutoJObject& list, WebCore::HistoryItem* item)
{
    ALOG_ASSERT(item, "newItem must take a valid HistoryItem!");
    // Item already added. Should only happen when we are inflating the list.
    if (item->bridge() || !list.get())
        return;

    JNIEnv* env = list.env();
    // Create the bridge, make it active, and attach it to the item.
    WebHistoryItem* bridge = new WebHistoryItem(item);
    bridge->setActive();
    item->setBridge(bridge);
    // Allocate a blank WebHistoryItemClassic
    jclass clazz = env->FindClass("android/webkit/WebHistoryItemClassic");
    jobject newItem = env->NewObject(clazz, gWebHistoryItemClassic.mInit,
            reinterpret_cast<int>(bridge));
    env->DeleteLocalRef(clazz);

    // Update the history item which will flatten the data and call update on
    // the java item.
    bridge->updateHistoryItem(item);

    // Add it to the list.
    env->CallVoidMethod(list.get(), gWebBackForwardListClassic.mAddHistoryItem, newItem);

    // Delete our local reference.
    env->DeleteLocalRef(newItem);
}

void WebHistory::RemoveItem(const AutoJObject& list, int index)
{
    if (list.get())
        list.env()->CallVoidMethod(list.get(), gWebBackForwardListClassic.mRemoveHistoryItem, index);
}

void WebHistory::UpdateHistoryIndex(const AutoJObject& list, int newIndex)
{
    if (list.get())
        list.env()->CallVoidMethod(list.get(), gWebBackForwardListClassic.mSetCurrentIndex, newIndex);
}

static void writeString(WTF::Vector<char>& vector, const WTF::String& str)
{
    unsigned strLen = str.length();
    // Only do work if the string has data.
    if (strLen) {
        // Determine how much to grow the vector. Use the worst case for utf8 to
        // avoid reading the string twice. Add sizeof(unsigned) to hold the
        // string length in utf8.
        unsigned vectorLen = vector.size() + sizeof(unsigned);
        unsigned length = (strLen << 2) + vectorLen;
        // Grow the vector. This will change the value of v.size() but we
        // remember the original size above.
        vector.grow(length);
        // Grab the position to write to.
        char* data = vector.begin() + vectorLen;
        // Write the actual string
        int l = SkUTF16_ToUTF8(str.characters(), strLen, data);
        ALOGV("Writing string          %d %.*s", l, l, data);
        // Go back and write the utf8 length. Subtract sizeof(unsigned) from
        // data to get the position to write the length.
        memcpy(data - sizeof(unsigned), (char*)&l, sizeof(unsigned));
        // Shrink the internal state of the vector so we match what was
        // actually written.
        vector.shrink(vectorLen + l);
    } else
        vector.append((char*)&strLen, sizeof(unsigned));
}

static void writeItem(WTF::Vector<char>& vector, WebCore::HistoryItem* item)
{
    // Original url
    writeString(vector, item->originalURLString());

    // Url
    writeString(vector, item->urlString());

    // Title
    writeString(vector, item->title());

    // Form content type
    writeString(vector, item->formContentType());

    // Form data
    const WebCore::FormData* formData = item->formData();
    if (formData) {
        WTF::String flattenedFormData = formData->flattenToString();
        writeString(vector, flattenedFormData);
        if (!flattenedFormData.isEmpty()) {
            // save the identifier as it is not included in the flatten data
            int64_t id = formData->identifier();
            vector.append((char*)&id, sizeof(int64_t));
        }
    } else
        writeString(vector, WTF::String()); // Empty constructor does not allocate a buffer.

    // Target
    writeString(vector, item->target());

    AndroidWebHistoryBridge* bridge = item->bridge();
    ALOG_ASSERT(bridge, "We should have a bridge here!");
    // Screen scale
    const float scale = bridge->scale();
    ALOGV("Writing scale           %f", scale);
    vector.append((char*)&scale, sizeof(float));
    const float textWrapScale = bridge->textWrapScale();
    ALOGV("Writing text wrap scale %f", textWrapScale);
    vector.append((char*)&textWrapScale, sizeof(float));

    // Scroll position.
    const int scrollX = item->scrollPoint().x();
    vector.append((char*)&scrollX, sizeof(int));
    const int scrollY = item->scrollPoint().y();
    vector.append((char*)&scrollY, sizeof(int));

    // Document state
    const WTF::Vector<WTF::String>& docState = item->documentState();
    WTF::Vector<WTF::String>::const_iterator end = docState.end();
    unsigned stateSize = docState.size();
    ALOGV("Writing docState        %d", stateSize);
    vector.append((char*)&stateSize, sizeof(unsigned));
    for (WTF::Vector<WTF::String>::const_iterator i = docState.begin(); i != end; ++i) {
        writeString(vector, *i);
    }

    // Is target item
    ALOGV("Writing isTargetItem    %d", item->isTargetItem());
    vector.append((char)item->isTargetItem());

    // Children count
    unsigned childCount = item->children().size();
    ALOGV("Writing childCount      %d", childCount);
    vector.append((char*)&childCount, sizeof(unsigned));
}

static void writeChildrenRecursive(WTF::Vector<char>& vector, WebCore::HistoryItem* parent)
{
    const WebCore::HistoryItemVector& children = parent->children();
    WebCore::HistoryItemVector::const_iterator end = children.end();
    for (WebCore::HistoryItemVector::const_iterator i = children.begin(); i != end; ++i) {
        WebCore::HistoryItem* item = (*i).get();
        ALOG_ASSERT(parent->bridge(),
                "The parent item should have a bridge object!");
        if (!item->bridge()) {
            WebHistoryItem* bridge = new WebHistoryItem(static_cast<WebHistoryItem*>(parent->bridge()));
            item->setBridge(bridge);
            bridge->setActive();
        } else {
            // The only time this item's parent may not be the same as the
            // parent's bridge is during history close. In that case, the
            // parent must not have a parent bridge.
            WebHistoryItem* bridge = static_cast<WebHistoryItem*>(item->bridge());
            WebHistoryItem* parentBridge = static_cast<WebHistoryItem*>(parent->bridge());
            ALOG_ASSERT(parentBridge->parent() == 0 ||
                    bridge->parent() == parentBridge,
                    "Somehow this item has an incorrect parent");
            bridge->setParent(parentBridge);
        }
        writeItem(vector, item);
        writeChildrenRecursive(vector, item);
    }
}

bool readUnsigned(const char*& data, const char* end, unsigned& result, const char* dbgLabel = 0);
bool readInt(const char*& data, const char* end, int& result, const char* dbgLabel = 0);
bool readInt64(const char*& data, const char* end, int64_t& result, const char* dbgLabel = 0);
bool readFloat(const char*& data, const char* end, float& result, const char* dbgLabel = 0);
bool readBool(const char*& data, const char* end, bool& result, const char* dbgLabel = 0);
bool readString(const char*& data, const char* end, String& result, const char* dbgLabel = 0);

bool readUnsigned(const char*& data, const char* end, unsigned& result, const char* dbgLabel)
{
    // Check if we have enough data left to continue.
    if ((end < data) || (static_cast<size_t>(end - data) < sizeof(unsigned))) {
        ALOGW("\tNot enough data to read unsigned; tag=\"%s\" end=%p data=%p",
              dbgLabel ? dbgLabel : "<no tag>", end, data);
        return false;
    }

    memcpy(&result, data, sizeof(unsigned));
    data += sizeof(unsigned);
    if (dbgLabel)
        ALOGV("Reading %-16s %u", dbgLabel, result);
    return true;
}

bool readInt(const char*& data, const char* end, int& result, const char* dbgLabel)
{
    // Check if we have enough data left to continue.
    if ((end < data) || (static_cast<size_t>(end - data) < sizeof(int))) {
        ALOGW("Not enough data to read int; tag=\"%s\" end=%p data=%p",
              dbgLabel ? dbgLabel : "<no tag>", end, data);
        return false;
    }

    memcpy(&result, data, sizeof(int));
    data += sizeof(int);
    if (dbgLabel)
        ALOGV("Reading %-16s %d", dbgLabel, result);
    return true;
}

bool readInt64(const char*& data, const char* end, int64_t& result, const char* dbgLabel)
{
    // Check if we have enough data left to continue.
    if ((end < data) || (static_cast<size_t>(end - data) < sizeof(int64_t))) {
        ALOGW("Not enough data to read int64_t; tag=\"%s\" end=%p data=%p",
              dbgLabel ? dbgLabel : "<no tag>", end, data);
        return false;
    }

    memcpy(&result, data, sizeof(int64_t));
    data += sizeof(int64_t);
    if (dbgLabel)
        ALOGV("Reading %-16s %ll", dbgLabel, result);
    return true;
}

bool readFloat(const char*& data, const char* end, float& result, const char* dbgLabel)
{
    // Check if we have enough data left to continue.
    if ((end < data) || (static_cast<size_t>(end - data) < sizeof(float))) {
        ALOGW("Not enough data to read float; tag=\"%s\" end=%p data=%p",
              dbgLabel ? dbgLabel : "<no tag>", end, data);
        return false;
    }

    memcpy(&result, data, sizeof(float));
    data += sizeof(float);
    if (dbgLabel)
        ALOGV("Reading %-16s %f", dbgLabel, result);
    return true;
}

// Note that the return value indicates success or failure, while the result
// parameter indicates the read value of the bool
bool readBool(const char*& data, const char* end, bool& result, const char* dbgLabel)
{
    // Check if we have enough data left to continue.
    if ((end < data) || (static_cast<size_t>(end - data) < sizeof(char))) {
        ALOGW("Not enough data to read bool; tag=\"%s\" end=%p data=%p",
              dbgLabel ? dbgLabel : "<no tag>", end, data);
        return false;
    }

    char c;
    memcpy(&c, data, sizeof(char));
    data += sizeof(char);
    if (dbgLabel)
        ALOGV("Reading %-16s %d", dbgLabel, c);
    result = c;

    // Valid bool results are 0 or 1
    if ((c != 0) && (c != 1)) {
        ALOGW("Invalid value for bool; tag=\"%s\" end=%p data=%p c=%u",
              dbgLabel ? dbgLabel : "<no tag>", end, data, c);
        return false;
    }

    return true;
}

bool readString(const char*& data, const char* end, String& result, const char* dbgLabel)
{
    unsigned stringLength;
    if (!readUnsigned(data, end, stringLength)) {
        ALOGW("Not enough data to read string length; tag=\"%s\" end=%p data=%p",
              dbgLabel ? dbgLabel : "<no tag>", end, data);
        return false;
    }

    if (dbgLabel)
        ALOGV("Reading %-16s %d %.*s", dbgLabel, stringLength, stringLength, data);

    // If length was 0, there will be no string content, but still return true
    if (!stringLength) {
        result = String();
        return true;
    }

    if ((end < data) || ((unsigned)(end - data) < stringLength)) {
        ALOGW("Not enough data to read content; tag=\"%s\" end=%p data=%p stringLength=%u",
              dbgLabel ? dbgLabel : "<no tag>", end, data, stringLength);
        return false;
    }

    const unsigned MAX_REASONABLE_STRING_LENGTH = 10000;
    if (stringLength > MAX_REASONABLE_STRING_LENGTH) {
        ALOGW("String length is suspiciously large (>%d); tag=\"%s\" end=%p data=%p stringLength=%u",
              MAX_REASONABLE_STRING_LENGTH, dbgLabel ? dbgLabel : "<no tag>",
              end, data, stringLength);
    }

    bool decodeFailed = false;
    static const WebCore::TextEncoding& encoding = WebCore::UTF8Encoding();
    result = encoding.decode(data, stringLength, true, decodeFailed);
    if (decodeFailed) {
        ALOGW("Decode failed, tag=\"%s\" end=%p data=%p stringLength=%u content=\"%s\"",
              dbgLabel ? dbgLabel : "<no tag>", end, data, stringLength,
              result.utf8().data());
        return false;
    }

    if (stringLength > MAX_REASONABLE_STRING_LENGTH) {
        ALOGW("\tdecodeFailed=%d (flag is ignored) content=\"%s\"",
              decodeFailed, result.utf8().data());
    }

    data += stringLength;
    return true;
}

static bool readItemRecursive(WebCore::HistoryItem* newItem,
        const char** pData, int length)
{
    if (!pData || length < HISTORY_MIN_SIZE) {
        ALOGW("readItemRecursive() bad params; pData=%p length=%d", pData, length);
        return false;
    }

    const char* data = *pData;
    const char* end = data + length;
    String content;

    // Read the original url
    if (readString(data, end, content, "Original url"))
        newItem->setOriginalURLString(content);
    else
        return false;

    // Read the url
    if (readString(data, end, content, "Url"))
        newItem->setURLString(content);
    else
        return false;

    // Read the title
    if (readString(data, end, content, "Title"))
        newItem->setTitle(content);
    else
        return false;

    // Generate a new ResourceRequest object for populating form information.
    // Read the form content type
    WTF::String formContentType;
    if (!readString(data, end, formContentType, "Content type"))
        return false;

    // Read the form data size
    unsigned formDataSize;
    if (!readUnsigned(data, end, formDataSize, "Form data size"))
        return false;

    // Read the form data
    WTF::RefPtr<WebCore::FormData> formData;
    if (formDataSize) {
        ALOGV("Reading Form data       %d %.*s", formDataSize, formDataSize, data);
        if ((end < data) || ((size_t)(end - data) < formDataSize)) {
            ALOGW("\tNot enough data to read form data; returning");
            return false;
        }
        formData = WebCore::FormData::create(data, formDataSize);
        data += formDataSize;
        // Read the identifier
        int64_t id;
        if (!readInt64(data, end, id, "Form id"))
            return false;
        if (id)
            formData->setIdentifier(id);
    }

    // Set up the form info
    if (formData != NULL) {
        WebCore::ResourceRequest r;
        r.setHTTPMethod("POST");
        r.setHTTPContentType(formContentType);
        r.setHTTPBody(formData);
        newItem->setFormInfoFromRequest(r);
    }

    // Read the target
    if (readString(data, end, content, "Target"))
        newItem->setTarget(content);
    else
        return false;

    AndroidWebHistoryBridge* bridge = newItem->bridge();
    ALOG_ASSERT(bridge, "There should be a bridge object during inflate");

    // Read the screen scale
    float fValue;
    if (readFloat(data, end, fValue, "Screen scale"))
        bridge->setScale(fValue);
    else
        return false;

    // Read the text wrap scale
    if (readFloat(data, end, fValue, "Text wrap scale"))
        bridge->setTextWrapScale(fValue);
    else
        return false;

    // Read scroll position.
    int scrollX;
    if (!readInt(data, end, scrollX, "Scroll pos x"))
        return false;
    int scrollY;
    if (!readInt(data, end, scrollY, "Scroll pos y"))
        return false;
    newItem->setScrollPoint(IntPoint(scrollX, scrollY));

    // Read the document state
    unsigned docStateCount;
    if (!readUnsigned(data, end, docStateCount, "Doc state count"))
        return false;
    if (docStateCount) {
        // Create a new vector and reserve enough space for the document state.
        WTF::Vector<WTF::String> docState;
        docState.reserveCapacity(docStateCount);
        while (docStateCount--) {
            // Read a document state string
            if (readString(data, end, content, "Document state"))
                docState.append(content);
            else
                return false;
        }
        newItem->setDocumentState(docState);
    }

    // Read is target item
    bool c;
    if (readBool(data, end, c, "Target item"))
        newItem->setIsTargetItem(c);
    else
        return false;

    // Read the child count
    unsigned count;
    if (!readUnsigned(data, end, count, "Child count"))
        return false;
    *pData = data;
    if (count) {
        while (count--) {
            // No need to check the length each time because read_item_recursive
            // will return null if there isn't enough data left to parse.
            WTF::RefPtr<WebCore::HistoryItem> child = WebCore::HistoryItem::create();
            // Set a bridge that will not call into java.
            child->setBridge(new WebHistoryItem(static_cast<WebHistoryItem*>(bridge)));
            // Read the child item.
            if (!readItemRecursive(child.get(), pData, end - data))
                return false;
            child->bridge()->setActive();
            newItem->addChildItem(child);
        }
    }
    return true;
}

// On arm, this test will cause memory corruption since converting char* will
// byte align the result and this test does not use memset (it probably
// should).
// On the simulator, using HistoryItem will invoke the IconDatabase which will
// initialize the main thread. Since this is invoked by the Zygote process, the
// main thread will be incorrect and an assert will fire later.
// In conclusion, define UNIT_TEST only if you know what you are doing.
#ifdef UNIT_TEST
static void unitTest()
{
    ALOGD("Entering history unit test!");
    const char* test1 = new char[0];
    WTF::RefPtr<WebCore::HistoryItem> item = WebCore::HistoryItem::create();
    WebCore::HistoryItem* testItem = item.get();
    testItem->setBridge(new WebHistoryItem(0));
    ALOG_ASSERT(!readItemRecursive(testItem, &test1, 0), "0 length array should fail!");
    delete[] test1;
    const char* test2 = new char[2];
    ALOG_ASSERT(!readItemRecursive(testItem, &test2, 2), "Small array should fail!");
    delete[] test2;
    ALOG_ASSERT(!readItemRecursive(testItem, NULL, HISTORY_MIN_SIZE), "Null data should fail!");
    // Original Url
    char* test3 = new char[HISTORY_MIN_SIZE];
    const char* ptr = (const char*)test3;
    memset(test3, 0, HISTORY_MIN_SIZE);
    *(int*)test3 = 4000;
    ALOG_ASSERT(!readItemRecursive(testItem, &ptr, HISTORY_MIN_SIZE), "4000 length originalUrl should fail!");
    // Url
    int offset = 4;
    memset(test3, 0, HISTORY_MIN_SIZE);
    ptr = (const char*)test3;
    *(int*)(test3 + offset) = 4000;
    ALOG_ASSERT(!readItemRecursive(testItem, &ptr, HISTORY_MIN_SIZE), "4000 length url should fail!");
    // Title
    offset += 4;
    memset(test3, 0, HISTORY_MIN_SIZE);
    ptr = (const char*)test3;
    *(int*)(test3 + offset) = 4000;
    ALOG_ASSERT(!readItemRecursive(testItem, &ptr, HISTORY_MIN_SIZE), "4000 length title should fail!");
    // Form content type
    offset += 4;
    memset(test3, 0, HISTORY_MIN_SIZE);
    ptr = (const char*)test3;
    *(int*)(test3 + offset) = 4000;
    ALOG_ASSERT(!readItemRecursive(testItem, &ptr, HISTORY_MIN_SIZE), "4000 length contentType should fail!");
    // Form data
    offset += 4;
    memset(test3, 0, HISTORY_MIN_SIZE);
    ptr = (const char*)test3;
    *(int*)(test3 + offset) = 4000;
    ALOG_ASSERT(!readItemRecursive(testItem, &ptr, HISTORY_MIN_SIZE), "4000 length form data should fail!");
    // Target
    offset += 4;
    memset(test3, 0, HISTORY_MIN_SIZE);
    ptr = (const char*)test3;
    *(int*)(test3 + offset) = 4000;
    ALOG_ASSERT(!readItemRecursive(testItem, &ptr, HISTORY_MIN_SIZE), "4000 length target should fail!");
    offset += 4; // Screen scale
    offset += 4; // Text wrap scale
    offset += 4; // Scroll pos x
    offset += 4; // Scroll pos y
    // Document state 
    offset += 4;
    memset(test3, 0, HISTORY_MIN_SIZE);
    ptr = (const char*)test3;
    *(int*)(test3 + offset) = 4000;
    ALOG_ASSERT(!readItemRecursive(testItem, &ptr, HISTORY_MIN_SIZE), "4000 length document state should fail!");
    // Is target item
    offset += 1;
    memset(test3, 0, HISTORY_MIN_SIZE);
    ptr = (const char*)test3;
    *(char*)(test3 + offset) = '!';
    ALOG_ASSERT(!readItemRecursive(testItem, &ptr, HISTORY_MIN_SIZE), "IsTargetItem should fail with ! as the value!");
    // Child count
    offset += 4;
    memset(test3, 0, HISTORY_MIN_SIZE);
    ptr = (const char*)test3;
    *(int*)(test3 + offset) = 4000;
    ALOG_ASSERT(!readItemRecursive(testItem, &ptr, HISTORY_MIN_SIZE), "4000 kids should fail!");
    // Test document state
    offset = 40;
    delete[] test3;
    test3 = new char[HISTORY_MIN_SIZE + sizeof(unsigned)];
    memset(test3, 0, HISTORY_MIN_SIZE + sizeof(unsigned));
    ptr = (const char*)test3;
    *(int*)(test3 + offset) = 1;
    *(int*)(test3 + offset + 4) = 20;
    ALOG_ASSERT(!readItemRecursive(testItem, &ptr, HISTORY_MIN_SIZE + sizeof(unsigned)), "1 20 length document state string should fail!");
    delete[] test3;
    test3 = new char[HISTORY_MIN_SIZE + 2 * sizeof(unsigned)];
    memset(test3, 0, HISTORY_MIN_SIZE + 2 * sizeof(unsigned));
    ptr = (const char*)test3;
    *(int*)(test3 + offset) = 2;
    *(int*)(test3 + offset + 4) = 0;
    *(int*)(test3 + offset + 8) = 20;
    ALOG_ASSERT(!readItemRecursive(testItem, &ptr, HISTORY_MIN_SIZE + 2 * sizeof(unsigned) ), "2 20 length document state string should fail!");
    delete[] test3;
    ALOGD("Leaving history unit test!");
}
#endif

//---------------------------------------------------------
// JNI registration
//---------------------------------------------------------
static JNINativeMethod gWebBackForwardListClassicMethods[] = {
    { "nativeClose", "(I)V",
        (void*) WebHistoryClose },
    { "restoreIndex", "(II)V",
        (void*) WebHistoryRestoreIndex }
};

static JNINativeMethod gWebHistoryItemClassicMethods[] = {
    { "inflate", "(I[B)I",
        (void*) WebHistoryInflate },
    { "nativeRef", "(I)V",
        (void*) WebHistoryRef },
    { "nativeUnref", "(I)V",
        (void*) WebHistoryUnref },
    { "nativeGetTitle", "(I)Ljava/lang/String;",
        (void*) WebHistoryGetTitle },
    { "nativeGetUrl", "(I)Ljava/lang/String;",
        (void*) WebHistoryGetUrl },
    { "nativeGetOriginalUrl", "(I)Ljava/lang/String;",
        (void*) WebHistoryGetOriginalUrl },
    { "nativeGetFlattenedData", "(I)[B",
        (void*) WebHistoryGetFlattenedData },
    { "nativeGetFavicon", "(I)Landroid/graphics/Bitmap;",
        (void*) WebHistoryGetFavicon },
};

int registerWebHistory(JNIEnv* env)
{
    // Get notified of all changes to history items.
    WebCore::notifyHistoryItemChanged = historyItemChanged;
#ifdef UNIT_TEST
    unitTest();
#endif
    // Find WebHistoryItemClassic, its constructor, and the update method.
    jclass clazz = env->FindClass("android/webkit/WebHistoryItemClassic");
    ALOG_ASSERT(clazz, "Unable to find class android/webkit/WebHistoryItemClassic");
    gWebHistoryItemClassic.mInit = env->GetMethodID(clazz, "<init>", "(I)V");
    ALOG_ASSERT(gWebHistoryItemClassic.mInit, "Could not find WebHistoryItemClassic constructor");
    env->DeleteLocalRef(clazz);

    // Find the WebBackForwardListClassic object and method.
    clazz = env->FindClass("android/webkit/WebBackForwardListClassic");
    ALOG_ASSERT(clazz, "Unable to find class android/webkit/WebBackForwardListClassic");
    gWebBackForwardListClassic.mAddHistoryItem = env->GetMethodID(clazz, "addHistoryItem",
            "(Landroid/webkit/WebHistoryItem;)V");
    ALOG_ASSERT(gWebBackForwardListClassic.mAddHistoryItem, "Could not find method addHistoryItem");
    gWebBackForwardListClassic.mRemoveHistoryItem = env->GetMethodID(clazz, "removeHistoryItem",
            "(I)V");
    ALOG_ASSERT(gWebBackForwardListClassic.mRemoveHistoryItem, "Could not find method removeHistoryItem");
    gWebBackForwardListClassic.mSetCurrentIndex = env->GetMethodID(clazz, "setCurrentIndex", "(I)V");
    ALOG_ASSERT(gWebBackForwardListClassic.mSetCurrentIndex, "Could not find method setCurrentIndex");
    env->DeleteLocalRef(clazz);

    int result = jniRegisterNativeMethods(env, "android/webkit/WebBackForwardListClassic",
            gWebBackForwardListClassicMethods, NELEM(gWebBackForwardListClassicMethods));
    return (result < 0) ? result : jniRegisterNativeMethods(env, "android/webkit/WebHistoryItemClassic",
            gWebHistoryItemClassicMethods, NELEM(gWebHistoryItemClassicMethods));
}

} /* namespace android */
