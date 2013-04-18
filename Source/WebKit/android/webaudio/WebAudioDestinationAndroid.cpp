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

#if ENABLE(WEB_AUDIO)

#include "AudioContext.h"
#include "Document.h"
#include "Frame.h"
#include "FrameView.h"

#include "WebAudioDestinationAndroid.h"
#include "WebViewCore.h"

#include <assert.h>
#include <wtf/Threading.h>
#include <wtf/Vector.h>

#include "WebAudioLog.h"

#include "media/AudioSystem.h"
#include "media/AudioTrack.h"

#include <system/audio.h>

using namespace android;

namespace WebCore {

// Frame count at which the web audio engine will render.
const unsigned g_audioBusFrameCount = 128;//Probably we can make this equal to audio system frame count

// FIXME: Are we always going to use 2 channels?
const unsigned g_channelCount = 2;

// Factory method: Android-implementation
PassOwnPtr<AudioDestination> AudioDestination::create(AudioSourceProvider& provider, float sampleRate)
{
    return adoptPtr(new AudioDestinationAndroid(provider, sampleRate));
}

AudioDestinationAndroid::AudioDestinationAndroid(AudioSourceProvider& provider, float sampleRate)
    : m_provider(provider)
    , m_renderBus(g_channelCount, g_audioBusFrameCount, false)
    , m_audioTrack(NULL)
    , m_sampleRate(sampleRate)
    , m_isPlaying(false)
    , m_latency(0)
    , m_frameSize(0)
    , m_started(false)
    , m_channels(g_channelCount)
{
    WEBAUDIO_LOGD( "AudioDestinationAndroid: ctor - this: %d, sampleRate: %f", this, sampleRate);

    uint32 afFrameCount = 0;
    android::AudioSystem::getOutputFrameCount(&afFrameCount, AUDIO_STREAM_MUSIC);

    m_channel1Buffer = adoptPtr(new AudioFloatArray(afFrameCount));
    m_channel2Buffer = adoptPtr(new AudioFloatArray(afFrameCount));

    uint32_t afLatency = 0;
    android::AudioSystem::getOutputLatency(&afLatency, AUDIO_STREAM_MUSIC);

    uint32 afSampleRate = 0;
    android::AudioSystem::getOutputSamplingRate(&afSampleRate, AUDIO_STREAM_MUSIC);

    WEBAUDIO_LOGD("AudioDestinationAndroid: frameCount: %d, latency: %d, sampleRate: %d", afFrameCount, afLatency, afSampleRate);

    // Figure out how many render calls per call back, rounding up if needed.
    m_renderCountPerCallback = afFrameCount / g_audioBusFrameCount;
    m_callbackFrameCount = m_renderCountPerCallback * g_audioBusFrameCount; //This could be less than frame count

    WEBAUDIO_LOGD("AudioDestinationAndroid: m_renderCountPerCallback: %d, m_callbackFrameCount: %d", m_renderCountPerCallback, m_callbackFrameCount);
}

AudioDestinationAndroid::~AudioDestinationAndroid()
{
    WEBAUDIO_LOGD("AudioDestinationAndroid:~AudioDestinationAndroid() - this: %d", this);
    stop();
}

void AudioDestinationAndroid::setAudioContext(AudioContext* ctx)
{
    m_context = ctx;
    m_core = android::WebViewCore::getWebViewCore(ctx->document()->frame()->view());
}

void AudioDestinationAndroid::start()
{
    WEBAUDIO_LOGD("AudioDestinationAndroid::start() m_started: %d, m_isPlaying: %d", m_started, m_isPlaying);
    status_t result;
    if (!m_audioTrack) {

        int channelOut = (m_channels > 1) ? AUDIO_CHANNEL_OUT_STEREO : AUDIO_CHANNEL_OUT_MONO;

        m_audioTrack = new AudioTrack();
        m_audioTrack->set(AUDIO_STREAM_MUSIC,
                                        m_sampleRate,
                                        AUDIO_FORMAT_PCM_16_BIT,
                                        channelOut,
                                        0,
                                        AUDIO_OUTPUT_FLAG_NONE,
                                        &AudioDestinationAndroid::audioTrackCallback,
                                        this,
                                        m_callbackFrameCount,
                                        0 /*sharedBuffer*/,
                                        true /*threadCanCallJava*/,
                                        0/*sessionId*/);

        if ((result = m_audioTrack->initCheck()) != OK) {
            WEBAUDIO_LOGD("AudioDestinationAndroid::start() invalid audio track status - result: %d", result);
            delete m_audioTrack;
            m_audioTrack = NULL;
            return;
        }
        m_latency = (int64_t)m_audioTrack->latency() * 1000;
        m_frameSize = m_audioTrack->frameSize();
        if (m_core)
            m_core->addAudioDestination(this);
        WEBAUDIO_LOGD("AudioDestinationAndroid::start() m_latency: %d, m_frameSize: %d", m_latency, m_frameSize);

    }

    m_audioTrack->start();
    m_isPlaying = true;
    m_started = true;
}

void AudioDestinationAndroid::stop()
{
    WEBAUDIO_LOGD("AudioDestinationAndroid::stop() m_started: %d, m_isPlaying: %d", m_started, m_isPlaying);
    if (m_audioTrack) {
        android::AutoMutex lock(m_lock);

        if (m_core)
            m_core->removeAudioDestination(this);

        m_audioTrack->stop();
        delete m_audioTrack;
        m_audioTrack = NULL;

        m_started = false;
        m_isPlaying = false;
    }
    WEBAUDIO_LOGD("AudioDestinationAndroid::stop() END!");
}

void AudioDestinationAndroid::pause()
{
    if (m_audioTrack && m_isPlaying) {
        m_audioTrack->pause();
        m_isPlaying = false;
    }
}

void AudioDestinationAndroid::resume()
{
    if (m_audioTrack && !m_isPlaying) {
        m_audioTrack->start();
        m_isPlaying = true;
    }
}

float AudioDestination::hardwareSampleRate()
{
    uint32 sampleRate = 44100; //Default sample rate supported by device
    android::AudioSystem::getOutputSamplingRate(&sampleRate, AUDIO_STREAM_MUSIC);
    return sampleRate;
}

static void interleaveFloatToInt16(const WTF::Vector<float*>& source,
                            int16_t* destination,
                            size_t number_of_frames) {
  const float kScale = 32768.0f;
  int channels = source.size();
  for (int i = 0; i < channels; ++i) {
    float* channel_data = source[i];
    for (size_t j = 0; j < number_of_frames; ++j) {
      float sample = kScale * channel_data[j];
      if (sample < -32768.0)
        sample = -32768.0;
      else if (sample > 32767.0)
        sample = 32767.0;

      destination[j * channels + i] = static_cast<int16_t>(sample);
    }
  }
}

//This functions assumes the playback format AudioSystem::PCM_16_BIT
void AudioDestinationAndroid::audioTrackCallback(int event, void* user, void *info)
{
    //WEBAUDIO_LOGD("AudioDestinationAndroid::callback() - user: %d, event: %d", user, event);
    if (event != AudioTrack::EVENT_MORE_DATA)
        return;

    AudioDestinationAndroid* ada = static_cast<AudioDestinationAndroid*>(user);
    ASSERT(NULL == ada);

    android::AutoMutex lock(ada->m_lock);

    if (!ada->m_started)
        return;

    AudioTrack::Buffer *buffer = (AudioTrack::Buffer *)info;
    ASSERT(NULL == buffer);

    WTF::Vector<float*> audio_data;
    ASSERT(buffer->frameCount > ada->m_channel1Buffer->size());
    ada->m_channel1Buffer->zero();
    ada->m_channel2Buffer->zero();
    audio_data.append(ada->m_channel1Buffer->data());
    audio_data.append(ada->m_channel2Buffer->data());

    ada->render(audio_data, buffer->frameCount);

    interleaveFloatToInt16(audio_data, buffer->i16, buffer->frameCount);
}

// Pulls on our provider to get the rendered audio stream.
void AudioDestinationAndroid::render(const WTF::Vector<float*>& audioData, size_t numberOfFrames)
{
    unsigned renderCountPerCallback = numberOfFrames / g_audioBusFrameCount ;

    for (unsigned i = 0; i < renderCountPerCallback; ++i) {
        m_renderBus.setChannelMemory(0, audioData[0] + i * g_audioBusFrameCount, g_audioBusFrameCount);
        m_renderBus.setChannelMemory(1, audioData[1] + i * g_audioBusFrameCount, g_audioBusFrameCount);
        m_provider.provideInput(&m_renderBus, g_audioBusFrameCount);
    }

    // Render the remaining frames
    unsigned renderedFrames = renderCountPerCallback * g_audioBusFrameCount;
    unsigned remainingFrames = numberOfFrames - renderedFrames;
    if (remainingFrames > 0) {
        m_renderBus.setChannelMemory(0, audioData[0] + renderedFrames, remainingFrames);
        m_renderBus.setChannelMemory(1, audioData[1] + renderedFrames, remainingFrames);
        m_provider.provideInput(&m_renderBus, remainingFrames);
    }
}

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO)

