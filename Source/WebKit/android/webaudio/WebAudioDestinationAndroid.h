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


#ifndef WebAudioDestinationAndroid_h
#define WebAudioDestinationAndroid_h

#include "AudioArray.h"
#include "AudioBus.h"
#include "AudioDestination.h"
#include "AudioSourceProvider.h"
#include <utils/threads.h>

#include <wtf/Vector.h>
#include <wtf/PassOwnPtr.h>

namespace android {
class AudioTrack;
class WebViewCore;
}

namespace WebCore {

// AudioDestination using Android's audio system
class AudioDestinationAndroid : public AudioDestination {
public:
    AudioDestinationAndroid(AudioSourceProvider&, float sampleRate);
    virtual ~AudioDestinationAndroid();

    virtual void start();
    virtual void stop();
    virtual bool isPlaying() { return m_isPlaying; }

    virtual float sampleRate() const { return m_sampleRate; }

    virtual void setAudioContext(AudioContext* ctx);
    virtual void pause();
    virtual void resume();

    static void audioTrackCallback(int event, void* user, void *info);

private:
    void render(const WTF::Vector<float*>& audioData, size_t numberOfFrames);

    AudioSourceProvider& m_provider;
    AudioBus m_renderBus;

    android::AudioTrack* m_audioTrack;
    android::Mutex       m_lock;

    float m_sampleRate;
    bool m_isPlaying;
    int64_t m_latency;
    size_t m_frameSize;
    bool m_started;
    size_t m_channels;

    size_t m_callbackFrameCount;
    unsigned m_renderCountPerCallback;

    AudioContext* m_context;
    android::WebViewCore* m_core;
    OwnPtr<AudioFloatArray> m_channel1Buffer;
    OwnPtr<AudioFloatArray> m_channel2Buffer;
};

} // namespace WebCore

#endif // AudioDestinationAndroid_libmedia_h
