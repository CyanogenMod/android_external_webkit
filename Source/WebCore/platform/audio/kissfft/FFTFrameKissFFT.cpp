/*
* Copyright (C) 2011 Google Inc. All rights reserved.
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

#if ENABLE(WEB_AUDIO) && USE(WEBAUDIO_KISSFFT)

#include "FFTFrame.h"
#include "VectorMath.h"

#include <wtf/MathExtras.h>

#include <kiss_fft.h>

namespace WebCore {

const int kMaxFFTPow2Size = 24;

// Normal constructor: allocates for a given fftSize.
FFTFrame::FFTFrame(unsigned fftSize)
    : m_FFTSize(fftSize)
    , m_log2FFTSize(static_cast<unsigned>(log2((double)fftSize)))
    , m_forwardContext(0)
    , m_inverseContext(0)
    , m_realData(fftSize / 2)
    , m_imagData(fftSize / 2)
{
    // We only allow power of two.
    ASSERT(1UL << m_log2FFTSize == m_FFTSize);

    m_forwardContext = contextForSize(fftSize, 0);
    m_inverseContext = contextForSize(fftSize, 1);

    m_cpxInputData = new kiss_fft_cpx[m_FFTSize/2];
    m_cpxOutputData = new kiss_fft_cpx[m_FFTSize/2];
}

// Creates a blank/empty frame (interpolate() must later be called).
FFTFrame::FFTFrame()
    : m_FFTSize(0)
    , m_log2FFTSize(0)
    , m_forwardContext(0)
    , m_inverseContext(0)
{
}

// Copy constructor.
FFTFrame::FFTFrame(const FFTFrame& frame)
    : m_FFTSize(frame.m_FFTSize)
    , m_log2FFTSize(frame.m_log2FFTSize)
    , m_forwardContext(0)
    , m_inverseContext(0)
    , m_realData(frame.m_FFTSize / 2)
    , m_imagData(frame.m_FFTSize / 2)
{
    m_forwardContext = contextForSize(m_FFTSize, 0);
    m_inverseContext = contextForSize(m_FFTSize, 1);

    // Copy/setup frame data.
    unsigned nbytes = sizeof(float) * (m_FFTSize / 2);
    memcpy(realData(), frame.realData(), nbytes);
    memcpy(imagData(), frame.imagData(), nbytes);

    m_cpxInputData = new kiss_fft_cpx[m_FFTSize/2];
    m_cpxOutputData = new kiss_fft_cpx[m_FFTSize/2];
}

void FFTFrame::initialize()
{
}

void FFTFrame::cleanup()
{
}

FFTFrame::~FFTFrame()
{
    KISS_FFT_FREE(m_forwardContext);
    KISS_FFT_FREE(m_inverseContext);
    delete m_cpxInputData;
    delete m_cpxOutputData;
}

void FFTFrame::multiply(const FFTFrame& frame)
{
    FFTFrame& frame1 = *this;
    FFTFrame& frame2 = const_cast<FFTFrame&>(frame);

    float* realP1 = frame1.realData();
    float* imagP1 = frame1.imagData();
    const float* realP2 = frame2.realData();
    const float* imagP2 = frame2.imagData();

    // Scale accounts the peculiar scaling of vecLib on the Mac.
    // This ensures the right scaling all the way back to inverse FFT.
    // FIXME: if we change the scaling on the Mac then this scale
    // factor will need to change too.
    float scale = 0.5f;

    unsigned halfSize = fftSize() / 2;
    float real0 = realP1[0];
    float imag0 = imagP1[0];
    VectorMath::zvmul(realP1, imagP1, realP2, imagP2, realP1, imagP1, halfSize);

    // Multiply the packed DC/nyquist component
    realP1[0] = real0 * realP2[0];
    imagP1[0] = imag0 * imagP2[0];

    VectorMath::vsmul(realP1, 1, &scale, realP1, 1, halfSize);
    VectorMath::vsmul(imagP1, 1, &scale, imagP1, 1, halfSize);
}

void FFTFrame::doFFT(const float* data)
{
    float* srcData = const_cast<float*>(data);
    // Compute Forward transform.
    kiss_fft(m_forwardContext, reinterpret_cast<kiss_fft_cpx*>(srcData), m_cpxOutputData);

    // FIXME: see above comment in multiply() about scaling.
    const float scale = 2.0f;
    float* outputData = reinterpret_cast<float*>(m_cpxOutputData);

    VectorMath::vsmul(outputData, 1, &scale, outputData, 1, m_FFTSize);

    // De-interleave to separate real and complex arrays.
    VectorMath::vdeintlve(outputData, m_realData.data(), m_imagData.data(), m_FFTSize);
}

void FFTFrame::doInverseFFT(float* data)
{
    // Get interleaved real and complex samples
    VectorMath::vintlve(m_realData.data(), m_imagData.data(), reinterpret_cast<float*>(m_cpxInputData), m_FFTSize);

    // Compute inverse transform.
    kiss_fft(m_inverseContext, m_cpxInputData, m_cpxOutputData);

    // Prepare interleaved data.
    float* interleavedData = reinterpret_cast<float*>(m_cpxOutputData);

    // Scale so that a forward then inverse FFT yields exactly the original data.
    const float scale = 1.0 / m_FFTSize;
    VectorMath::vsmul(interleavedData, 1, &scale, data, 1, m_FFTSize);
}

float* FFTFrame::realData() const
{
    return const_cast<float*>(m_realData.data());
}

float* FFTFrame::imagData() const
{
    return const_cast<float*>(m_imagData.data());
}

kiss_fft_cfg FFTFrame::contextForSize(unsigned fftSize, int trans)
{
    // FIXME: This is non-optimal. Ideally, we'd like to share the contexts for FFTFrames of the same size.
    ASSERT(fftSize);
    int pow2size = static_cast<int>(log2((double)fftSize));
    ASSERT(pow2size < kMaxFFTPow2Size);

    kiss_fft_cfg context = kiss_fft_alloc(fftSize/2, trans, 0, 0);
    return context;
}

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO) && USE(WEBAUDIO_KISSFFT)

