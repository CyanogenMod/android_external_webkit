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

#ifndef GeolocationManager_h
#define GeolocationManager_h

#include "GeolocationClientImpl.h"

#include <GeolocationClientMock.h>
#include <OwnPtr.h>
#include <PassRefPtr.h>

namespace WebCore {
class GeolocationError;
class GeolocationPosition;
}

namespace android {

class GeolocationClientImpl;
class WebViewCore;

// This class takes care of the fact that the client used for Geolocation
// may be either the real implementation or a mock. It also handles setting the
// data on the mock client. This class is owned by WebViewCore and exists to
// keep cruft out of that class.
class GeolocationManager {
public:
    GeolocationManager(WebViewCore*);

    // For use by GeolocationClientAndroid. Gets the current client, either the
    // real or mock.
    WebCore::GeolocationClient* client() const;

    void suspendRealClient();
    void resumeRealClient();
    void resetRealClientTemporaryPermissionStates();
    void provideRealClientPermissionState(WTF::String origin, bool allow, bool remember);

    // Sets use of the Geolocation mock client. Also resets that client.
    void setUseMock();
    void setMockPosition(PassRefPtr<WebCore::GeolocationPosition>);
    void setMockError(PassRefPtr<WebCore::GeolocationError>);
    void setMockPermission(bool allowed);

private:
    GeolocationClientImpl* realClient() const;
    WebCore::GeolocationClientMock* mockClient() const;

    bool m_useMock;
    WebViewCore* m_webViewCore;
    mutable OwnPtr<GeolocationClientImpl> m_realClient;
    mutable OwnPtr<WebCore::GeolocationClientMock> m_mockClient;
};

} // namespace android

#endif // GeolocationManager_h
