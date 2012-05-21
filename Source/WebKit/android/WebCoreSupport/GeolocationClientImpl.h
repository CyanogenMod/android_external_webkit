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

#ifndef GeolocationClientImpl_h
#define GeolocationClientImpl_h

#include "GeolocationServiceBridge.h"
#include "GeolocationClient.h"
#include "GeolocationPermissions.h"

#include <Timer.h>
#include <wtf/OwnPtr.h>
#include <wtf/RefPtr.h>

namespace WebCore {
class Geolocation;
class GeolocationController;
}

namespace android {

class WebViewCore;

// The real implementation of GeolocationClient.
class GeolocationClientImpl : public WebCore::GeolocationClient, public GeolocationServiceBridge::Listener {
public:
    GeolocationClientImpl(WebViewCore*);
    virtual ~GeolocationClientImpl();

    // WebCore::GeolocationClient
    virtual void geolocationDestroyed();
    virtual void startUpdating();
    virtual void stopUpdating();
    virtual void setEnableHighAccuracy(bool);
    virtual WebCore::GeolocationPosition* lastPosition();
    virtual void requestPermission(WebCore::Geolocation*);
    virtual void cancelPermissionRequest(WebCore::Geolocation*);

    // GeolocationServiceBridge::Listener
    virtual void newPositionAvailable(PassRefPtr<WebCore::GeolocationPosition>);
    virtual void newErrorAvailable(PassRefPtr<WebCore::GeolocationError>);

    void suspend();
    void resume();
    void resetTemporaryPermissionStates();
    void providePermissionState(String origin, bool allow, bool remember);

private:
    GeolocationPermissions* permissions() const;
    void timerFired(WebCore::Timer<GeolocationClientImpl>*);

    WebViewCore* m_webViewCore;
    RefPtr<WebCore::GeolocationPosition> m_lastPosition;
    RefPtr<WebCore::GeolocationError> m_lastError;
    OwnPtr<GeolocationServiceBridge> m_javaBridge;
    mutable OwnPtr<GeolocationPermissions> m_permissions;
    WebCore::Timer<GeolocationClientImpl> m_timer;
    bool m_isSuspended;
    bool m_useGps;
};

} // namespace android

#endif // GeolocationClientImpl_h
