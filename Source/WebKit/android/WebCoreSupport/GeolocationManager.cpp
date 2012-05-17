/*
 * Copyright 2012, The Android Open Source Project
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
#include "GeolocationManager.h"

#include "GeolocationClientImpl.h"
#include "WebViewCore.h"

#include <GeolocationError.h>
#include <GeolocationPosition.h>
#include <JNIHelp.h>

using WebCore::GeolocationClient;

namespace android {

GeolocationManager::GeolocationManager(WebViewCore* webViewCore)
    : m_webViewCore(webViewCore)
{
}

GeolocationClient* GeolocationManager::client() const
{
    // TODO: Return the mock client when appropriate. See http://b/6511338.
    return realClient();
}

void GeolocationManager::suspendRealClient()
{
    // Don't create the real client if it's not present.
    if (m_realClient)
        m_realClient->suspend();
}

void GeolocationManager::resumeRealClient()
{
    // Don't create the real client if it's not present.
    if (m_realClient)
        m_realClient->resume();
}

void GeolocationManager::resetRealClientTemporaryPermissionStates()
{
    // Don't create the real client if it's not present.
    if (m_realClient)
        m_realClient->resetTemporaryPermissionStates();
}

void GeolocationManager::provideRealClientPermissionState(WTF::String origin, bool allow, bool remember)
{
    // Don't create the real client if it's not present.
    if (m_realClient)
        m_realClient->providePermissionState(origin, allow, remember);
}

GeolocationClientImpl* GeolocationManager::realClient() const
{
    if (!m_realClient)
        m_realClient.set(new GeolocationClientImpl(m_webViewCore));
    return m_realClient.get();
}

} // namespace android
