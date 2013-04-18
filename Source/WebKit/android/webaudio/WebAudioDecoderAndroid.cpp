/*
* Copyright (C) 2012, The Linux Foundation. All rights reserved.
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

#include "WebAudioDecoder.h"
#include "WebAudioLog.h"

#undef ERROR
#include "media/stagefright/OMXCodec.h"
#include "media/stagefright/OMXClient.h"
#include "media/AudioSystem.h"
#include "media/AudioTrack.h"
#include "media/stagefright/DataSource.h"
#include "media/stagefright/MediaExtractor.h"
#include "media/stagefright/MediaSource.h"
#include "media/stagefright/MetaData.h"
#include "media/stagefright/MediaDefs.h"

#include "InMemoryDataSource.h"

#include <wtf/Assertions.h>
#include <wtf/Vector.h>
#include <wtf/RefPtr.h>
#include <wtf/text/WTFString.h>
#include <wtf/text/CString.h>
#include <wtf/PassRefPtr.h>
#include <wtf/CurrentTime.h>
#include <wtf/Threading.h>

static bool s_stagefright_initialized = false;
static const double s_numberOfMicroSecondsPerSecond = 1000000;

namespace android {

static bool deinterleaveAudioChannel(void* source,
                              float* destination,
                              int channels,
                              int channel_index,
                              size_t number_of_frames) {
    int16_t* source16 = static_cast<int16_t*>(source) + channel_index;
    const float kScale = 1.0f / 32768.0f;
    for (unsigned i = 0; i < number_of_frames; ++i) {
        destination[i] = kScale * *source16;
        source16 += channels;
    }
    return true;
}

class DecodedBuffer
{
    public:
        DecodedBuffer(float* p, unsigned size)
            : m_buffer(p)
            , m_bufferSize(size)
        {
        }
        float* m_buffer;
        unsigned m_bufferSize;
};


// Decode in-memory audio file data.
bool OMXCodecDecodeAudioFileData(
    WebCore::AudioBus** destination_bus,
    const char* data, size_t data_size, double sample_rate) {

    if (!destination_bus) {
        WEBAUDIO_LOGE("OMXCodecDecodeAudioFileData : Destination bus is NULL!");
        return false;
    }

    if (!s_stagefright_initialized) {
        DataSource::RegisterDefaultSniffers();
        s_stagefright_initialized = true;
    }

    sp<DataSource> dataSource = new InMemoryDataSource(
                    reinterpret_cast<const uint8_t*>(data), data_size);

    sp<MediaExtractor> extractor = MediaExtractor::Create(dataSource);

    if (extractor == NULL) {
        WTF::String header(data, 32);
        WTF::CString cstr = header.utf8();
        WEBAUDIO_LOGE("OMXCodecDecodeAudioFileData : Could not instantiate extractor! header: %s", cstr.data());
        return false;
    }

    size_t trackCount = extractor->countTracks();
    bool isRawAudio = false;
    int audioTrackIndex = -1;
    const char *mime;
    bool _debug_ = false;
    for (size_t track = 0; track < trackCount; ++track) {
        sp<MetaData> meta = extractor->getTrackMetaData(track);

        meta->findCString(kKeyMIMEType, &mime);
        if (!strncasecmp(mime, "audio/", 6)) {
            audioTrackIndex = track;
            if (!strcasecmp(MEDIA_MIMETYPE_AUDIO_RAW, mime))
              isRawAudio = true;
            break;
        }
    }

    if (audioTrackIndex < 0) {
        WTF::String header(data, 32);
        WTF::CString cstr = header.utf8();
        WEBAUDIO_LOGE("OMXCodecDecodeAudioFileData : Could not find a supported audio track - index: %d, track count: %d, header: %s", audioTrackIndex, trackCount, cstr.data());
        return false;
    }
    WEBAUDIO_LOGD("OMXCodecDecodeAudioFileData : Detected audio format: %s", mime);

    sp<MediaSource> audioTrack = extractor->getTrack(audioTrackIndex);
    sp<MetaData> meta = audioTrack->getFormat();
    if (!meta.get()) {
        WTF::String header(data, 32);
        WTF::CString cstr = header.utf8();
        WEBAUDIO_LOGE("OMXCodecDecodeAudioFileData : null format! - index: %d, track count: %d, header: %s", audioTrackIndex, trackCount, cstr.data());
        return false;
    }

    sp<MediaSource> audioSource;
    OMXClient client;
    status_t status = client.connect();
    if (isRawAudio) {
        audioSource = audioTrack;
        WEBAUDIO_LOGD("OMXCodecDecodeAudioFileData : Decoding RAW audio.");
    } else {
        audioSource = OMXCodec::Create(client.interface(), meta, false, audioTrack);
        if (audioSource == NULL) {
            WEBAUDIO_LOGE("OMXCodecDecodeAudioFileData : Could not instantiate decoder.");
            return false;
        }
    }

    status_t decodeStatus = OK;
    MediaBuffer *mDecodeBuffer = NULL;
    int64_t mSeeKPositionUs = 0;

    if (audioSource->start() != OK) {
        WEBAUDIO_LOGE("OMXCodecDecodeAudioFileData : Failed to start source/decoder");
        return false;
    }

    meta = audioSource->getFormat();

    // negative values indicate invalid value
    int32_t channelCount;
    int32_t bitRate;
    int32_t sampleRateHz;
    int64_t durationUsec;
    int64_t previousKeyTimeUs;

    size_t bytesPerSample = 2;  //TODO: Need to find a way to get this from MetaData
    size_t totalFramesRead = 0;
    bool result = true;
    bool initializeOutputBuffer = true;

    size_t totalFrames = 44100;
    WTF::Vector<WTF::Vector<DecodedBuffer>* > audio_data;

    do {
        decodeStatus = audioSource->read(&mDecodeBuffer, 0);
        if (decodeStatus == INFO_FORMAT_CHANGED) {
            meta = audioSource->getFormat();
            decodeStatus = OK;
            WEBAUDIO_LOGD("OMXCodecDecodeAudioFileData : AudioSource signaled format change.");
        }

        if (initializeOutputBuffer) {

            const char *mimeType;
            status = meta->findCString(kKeyMIMEType, &mimeType);
            status = meta->findInt32(kKeyBitRate, &bitRate);
            status = meta->findInt32(kKeyChannelCount, &channelCount);
            status = meta->findInt32(kKeySampleRate, &sampleRateHz);
            status = meta->findInt64(kKeyDuration, &durationUsec);
            if (!status)
                durationUsec = 0;

            if (durationUsec)
                totalFrames = ceil(((double)durationUsec/s_numberOfMicroSecondsPerSecond) * (double)sampleRateHz);

            WEBAUDIO_LOGD("OMXCodecDecodeAudioFileData : mimeType: %s, totalFrames : %d, durationUsec: %lld, channelCount: %d, sampleRateHz: %d",
                            mimeType, totalFrames, durationUsec, channelCount, sampleRateHz);

            audio_data.reserveInitialCapacity(channelCount);

            for (int32_t i = 0; i < channelCount; ++i)
                audio_data.append(new WTF::Vector<DecodedBuffer>);

            initializeOutputBuffer = false;
        }

        if (decodeStatus == OK && mDecodeBuffer) {

            int64_t keyTimeUs = 0;
            mDecodeBuffer->meta_data()->findInt64(kKeyTime, &keyTimeUs);

            size_t framesDecoded = mDecodeBuffer->range_length() / (channelCount * bytesPerSample);

            if (framesDecoded) {
                size_t offset = totalFramesRead;
                totalFramesRead += framesDecoded;

                const uint8_t * srcBuffer = (const uint8_t *)mDecodeBuffer->data() + mDecodeBuffer->range_offset();
                for (int32_t channelIndex = 0; channelIndex < channelCount; channelIndex++) {
                    float* dstBuffer = new float[framesDecoded];
                    deinterleaveAudioChannel((void*)srcBuffer, dstBuffer, channelCount, channelIndex, framesDecoded);
                    audio_data[channelIndex]->append(DecodedBuffer(dstBuffer, framesDecoded));
                }
            }

            mDecodeBuffer->release();
            mDecodeBuffer = NULL;

            previousKeyTimeUs = keyTimeUs;
            mSeeKPositionUs = s_numberOfMicroSecondsPerSecond * ((double)totalFramesRead / (double)sampleRateHz);
        }
    }while (decodeStatus == OK);

    WEBAUDIO_LOGD("OMXCodecDecodeAudioFileData : last decoder status : %d, result:%d", decodeStatus, result);
    if (result && decodeStatus == ERROR_END_OF_STREAM) {
        WebCore::AudioBus* audioBus = new WebCore::AudioBus(channelCount, totalFramesRead, true/*false*/);
        audioBus->setSampleRate(sampleRateHz);

        for (int32_t i = 0; i < channelCount; i++) {
            float* src = audioBus->channel(i)->mutableData();
            WTF::Vector<DecodedBuffer>* dataChunks = audio_data[i];
            for (int j = 0; j < dataChunks->size(); j++) {
                DecodedBuffer& df = dataChunks->at(j);
                memcpy(src, df.m_buffer, (df.m_bufferSize * sizeof(float)));
                src += df.m_bufferSize;
                delete df.m_buffer;
            }
            delete dataChunks;
        }

        *destination_bus = audioBus;
        result = true;
    }

    audioSource->stop();

    return result;
}

}  // namespace android

WTF::PassOwnPtr<WebCore::AudioBus> webaudio::decodeAudioFileData(const char* data, size_t size, float sampleRate)
{
    WebCore::AudioBus* audioBus = 0;
    if (android::OMXCodecDecodeAudioFileData(&audioBus, data, size, sampleRate)) {
        OwnPtr<WebCore::AudioBus> audioBusPtr(adoptPtr(audioBus));
        return audioBusPtr.release();
    }
    return 0;
}

