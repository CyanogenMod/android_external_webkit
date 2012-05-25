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
#include "GeolocationClientImpl.h"

#include <Frame.h>
#include <Page.h>
#include <GeolocationController.h>
#include <GeolocationError.h>
#include <GeolocationPosition.h>
#include <WebViewCore.h>
#if PLATFORM(ANDROID)
// Required for sim-eng build
#include <math.h>
#endif
#include <wtf/CurrentTime.h>

using WebCore::Geolocation;
using WebCore::GeolocationError;
using WebCore::GeolocationPosition;
using WebCore::Timer;

using namespace std;

namespace {

bool isPositionMovement(GeolocationPosition* position1, GeolocationPosition* position2)
{
    // For the small distances in which we are likely concerned, it's reasonable
    // to approximate the distance between the two positions as the sum of the
    // differences in latitude and longitude.
    double delta = fabs(position1->latitude() - position2->latitude()) + fabs(position1->longitude() - position2->longitude());
    // Approximate conversion from degrees of arc to metres.
    delta *= 60 * 1852;
    // The threshold is when the distance between the two positions exceeds the
    // worse (larger) of the two accuracies.
    int maxAccuracy = max(position1->accuracy(), position2->accuracy());
    return delta > maxAccuracy;
}

bool isPositionMoreAccurate(GeolocationPosition* position1, GeolocationPosition* position2)
{
    return position2->accuracy() < position1->accuracy();
}

bool isPositionMoreTimely(GeolocationPosition* position1)
{
    double currentTime = WTF::currentTime();
    double maximumAge = 10 * 60; // 10 minutes
    return currentTime - position1->timestamp() > maximumAge;
}

} // anonymous namespace

namespace android {

GeolocationClientImpl::GeolocationClientImpl(WebViewCore* webViewCore)
    : m_webViewCore(webViewCore)
    , m_timer(this, &GeolocationClientImpl::timerFired)
    , m_isSuspended(false)
    , m_useGps(false)
{
}

GeolocationClientImpl::~GeolocationClientImpl()
{
}

void GeolocationClientImpl::geolocationDestroyed()
{
    // Lifetime is managed by GeolocationManager.
}

void GeolocationClientImpl::startUpdating()
{
    // This method is called every time a new watch or one-shot position request
    // is started. If we already have a position or an error, call back
    // immediately.
    if (m_lastPosition || m_lastError) {
        m_timer.startOneShot(0);
    }

    // Lazilly create the Java object.
    bool haveJavaBridge = m_javaBridge;
    if (!haveJavaBridge)
        m_javaBridge.set(new GeolocationServiceBridge(this, m_webViewCore));
    ASSERT(m_javaBridge);

    // Set whether to use GPS before we start the implementation.
    m_javaBridge->setEnableGps(m_useGps);

    // If we're suspended, don't start the service. It will be started when we
    // get the call to resume().
    if (!haveJavaBridge && !m_isSuspended)
        m_javaBridge->start();
}

void GeolocationClientImpl::stopUpdating()
{
    // TODO: It would be good to re-use the Java bridge object.
    m_javaBridge.clear();
    m_useGps = false;
    // Reset last position and error to make sure that we always try to get a
    // new position from the client when a request is first made.
    m_lastPosition = 0;
    m_lastError = 0;

    if (m_timer.isActive())
        m_timer.stop();
}

void GeolocationClientImpl::setEnableHighAccuracy(bool enableHighAccuracy)
{
    // On Android, high power == GPS.
    m_useGps = enableHighAccuracy;
    if (m_javaBridge)
        m_javaBridge->setEnableGps(m_useGps);
}

GeolocationPosition* GeolocationClientImpl::lastPosition()
{
    return m_lastPosition.get();
}

void GeolocationClientImpl::requestPermission(Geolocation* geolocation)
{
    permissions()->queryPermissionState(geolocation->frame());
}

void GeolocationClientImpl::cancelPermissionRequest(Geolocation* geolocation)
{
    permissions()->cancelPermissionStateQuery(geolocation->frame());
}

// Note that there is no guarantee that subsequent calls to this method offer a
// more accurate or updated position.
void GeolocationClientImpl::newPositionAvailable(PassRefPtr<GeolocationPosition> position)
{
    ASSERT(position);
    if (!m_lastPosition
        || isPositionMovement(m_lastPosition.get(), position.get())
        || isPositionMoreAccurate(m_lastPosition.get(), position.get())
        || isPositionMoreTimely(m_lastPosition.get())) {
        m_lastPosition = position;
        // Remove the last error.
        m_lastError = 0;
        m_webViewCore->mainFrame()->page()->geolocationController()->positionChanged(m_lastPosition.get());
    }
}

void GeolocationClientImpl::newErrorAvailable(PassRefPtr<WebCore::GeolocationError> error)
{
    ASSERT(error);
    // We leave the last position
    m_lastError = error;
    m_webViewCore->mainFrame()->page()->geolocationController()->errorOccurred(m_lastError.get());
}

void GeolocationClientImpl::suspend()
{
    m_isSuspended = true;
    if (m_javaBridge)
        m_javaBridge->stop();
}

void GeolocationClientImpl::resume()
{
    m_isSuspended = false;
    if (m_javaBridge)
        m_javaBridge->start();
}

void GeolocationClientImpl::resetTemporaryPermissionStates()
{
    permissions()->resetTemporaryPermissionStates();
}

void GeolocationClientImpl::providePermissionState(String origin, bool allow, bool remember)
{
    permissions()->providePermissionState(origin, allow, remember);
}

GeolocationPermissions* GeolocationClientImpl::permissions() const
{
    if (!m_permissions)
        m_permissions = new GeolocationPermissions(m_webViewCore);
    return m_permissions.get();
}

void GeolocationClientImpl::timerFired(Timer<GeolocationClientImpl>* timer)
{
    ASSERT(&m_timer == timer);
    ASSERT(m_lastPosition || m_lastError);
    if (m_lastPosition)
        m_webViewCore->mainFrame()->page()->geolocationController()->positionChanged(m_lastPosition.get());
    else
        m_webViewCore->mainFrame()->page()->geolocationController()->errorOccurred(m_lastError.get());
}

} // namespace android
