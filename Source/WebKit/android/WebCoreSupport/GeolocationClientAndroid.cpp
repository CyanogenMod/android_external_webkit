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
#include "GeolocationClientAndroid.h"

#include "WebViewCore.h"

#include <Frame.h>
#include <Page.h>

using WebCore::Geolocation;
using WebCore::GeolocationClient;
using WebCore::GeolocationController;
using WebCore::GeolocationPosition;

namespace android {

GeolocationClientAndroid::GeolocationClientAndroid() : m_webViewCore(0)
{
}

GeolocationClientAndroid::~GeolocationClientAndroid()
{
}

void GeolocationClientAndroid::geolocationDestroyed()
{
    delete this;
}

void GeolocationClientAndroid::startUpdating()
{
    client()->startUpdating();
}

void GeolocationClientAndroid::stopUpdating()
{
    client()->stopUpdating();
}

void GeolocationClientAndroid::setEnableHighAccuracy(bool enableHighAccuracy)
{
    client()->setEnableHighAccuracy(enableHighAccuracy);
}

GeolocationPosition* GeolocationClientAndroid::lastPosition()
{
    return client()->lastPosition();
}

void GeolocationClientAndroid::requestPermission(Geolocation* geolocation)
{
    client()->requestPermission(geolocation);
}

void GeolocationClientAndroid::cancelPermissionRequest(Geolocation* geolocation)
{
    client()->cancelPermissionRequest(geolocation);
}

void GeolocationClientAndroid::setWebViewCore(WebViewCore* webViewCore)
{
    ASSERT(!m_webViewCore);
    m_webViewCore = webViewCore;
    ASSERT(m_webViewCore);
}

GeolocationClient* GeolocationClientAndroid::client() const
{
    return m_webViewCore->geolocationManager()->client();
}

} // namespace android
