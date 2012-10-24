/*
* Copyright (c) 2012, The Linux Foundation. All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are
* met:
*     * Redistributions of source code must retain the above copyright
*       notice, this list of conditions and the following disclaimer.
*     * Redistributions in binary form must reproduce the above
*       copyright notice, this list of conditions and the following
*       disclaimer in the documentation and/or other materials provided
*       with the distribution.
*     * Neither the name of The Linux Foundation nor the names of its
*       contributors may be used to endorse or promote products derived
*       from this software without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
* WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
* ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
* BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
* BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
* WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
* OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
* IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/


#include "config.h"

#if ENABLE(WEB_AUDIO)

#include "AudioBus.h"

#include "AudioFileReader.h"
#include <wtf/PassOwnPtr.h>
#include <wtf/text/WTFString.h>
#include <wtf/text/CString.h>

#include "WebAudioDecoder.h"
#include "WebAudioAssets.h"
#include "WebAudioLog.h"

#include "androidfw/AssetManager.h"
#include "androidfw/Asset.h"

static const char* kWebAudioAssets = "app/webaudiores.apk";

android::AssetManager* webAudioAssetManager() {
    static android::AssetManager* gWebAudioAssetMgr;
    if (!gWebAudioAssetMgr) {
        const char* root = getenv("ANDROID_ROOT");
        LOG_ALWAYS_FATAL_IF(root == NULL, "ANDROID_ROOT not set");

        android::String8 path(root);
        path.appendPath(kWebAudioAssets);

        gWebAudioAssetMgr = new android::AssetManager();
        gWebAudioAssetMgr->addAssetPath(path,NULL);
    }
    return gWebAudioAssetMgr;
}

namespace WebCore {

PassOwnPtr<AudioBus> AudioBus::loadPlatformResource(const char* name, float sampleRate)
{
    WEBAUDIO_LOGW("AudioBus::loadPlatformResource : name: %s, sampleRate: %f", name, sampleRate);
    const char * str = webaudio::webAudioAssetFileName(name);
    if (!str) {
        WEBAUDIO_LOGW("AudioBus::loadPlatformResource : error webaudio::webAudioAssetFileName() - name: %s", name);
        return 0;
    }

    android::AssetManager* am = webAudioAssetManager();
    android::Asset* asset = am->open(str, android::Asset::ACCESS_BUFFER);
    if (!asset) {
        asset = am->openNonAsset(str, android::Asset::ACCESS_BUFFER);
        if (!asset) {
            WEBAUDIO_LOGW("AudioBus::loadPlatformResource : asset not found - name: %s", str);
            return 0;
        }
    }

    OwnPtr<AudioBus> audioBus = webaudio::decodeAudioFileData((const char*)asset->getBuffer(false), asset->getLength(), sampleRate);
    delete asset;
    if (!audioBus.get()) {
        WEBAUDIO_LOGW("AudioBus::loadPlatformResource : audio file decode error - name: %s", str);
        return 0;
    }
    WEBAUDIO_LOGW("AudioBus::loadPlatformResource : decoded AudioBus - channels: %d, length: %d, sampleRate: %f",
        audioBus->numberOfChannels(), audioBus->length(), audioBus->sampleRate());

    // If the bus is already at the requested sample-rate then return as is.
    if (audioBus->sampleRate() == sampleRate)
        return audioBus.release();

    return AudioBus::createBySampleRateConverting(audioBus.get(), false, sampleRate);
}

PassOwnPtr<AudioBus> createBusFromInMemoryAudioFile(const void* data, size_t dataSize, bool mixToMono, float sampleRate)
{
    WEBAUDIO_LOGD("createBusFromInMemoryAudioFile : data: %d, dataSize: %d, mixToMono: %d, sampleRate: %f", data, dataSize, mixToMono, sampleRate);

    OwnPtr<AudioBus> audioBus = webaudio::decodeAudioFileData(static_cast<const char*>(data), dataSize, sampleRate);
    if (!audioBus.get()) {
        WEBAUDIO_LOGW("AudioBus::createBusFromInMemoryAudioFile : audio file decode error - dataSize", dataSize);
        return 0;
    }

    WEBAUDIO_LOGD("createBusFromInMemoryAudioFile : decoded AudioBus - channels: %d, length: %d, sampleRate: %f",
        audioBus->numberOfChannels(), audioBus->length(), audioBus->sampleRate());

    // If the bus needs no conversion then return as is.
    if ((!mixToMono || audioBus->numberOfChannels() == 1) && audioBus->sampleRate() == sampleRate)
        return audioBus.release();

    return AudioBus::createBySampleRateConverting(audioBus.get(), mixToMono, sampleRate);
}

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO)

