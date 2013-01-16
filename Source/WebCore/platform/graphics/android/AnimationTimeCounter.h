/*
 * Copyright (c) 2013, The Linux Foundation. All rights reserved.
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

#ifndef AnimationTimeCounter_h
#define AnimationTimeCounter_h

#include <wtf/CurrentTime.h>

/**
 * Detects animation based on a FPS threshold.
 */
class AnimationTimeCounter
{
    public:
        AnimationTimeCounter()
            : m_numFrames(0)
            , m_startTime(0)
            , m_elapsedTime(0)
        {
        }

        ~AnimationTimeCounter()
        {
        }

        double elapsedTime()
        {
            if (!m_startTime)
                m_startTime = currentTime();

            return currentTime() - m_startTime;
        }

        void tick()
        {
            m_elapsedTime = elapsedTime();
            ++m_numFrames;
        }

        bool isAnimating() const
        {
            return (m_elapsedTime >= 1) && (static_cast<int>(m_numFrames / m_elapsedTime) > ANIMATION_FPS_THRESHOLD);
        }

        void setAnimationDetectionThreshold(int val)
        {
            ANIMATION_FPS_THRESHOLD = val;
        }

    private:
        int m_numFrames;
        double m_startTime; // seconds
        double m_elapsedTime; // seconds
        int ANIMATION_FPS_THRESHOLD;
};

#endif // AnimationTimeCounter_h
