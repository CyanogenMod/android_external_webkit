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

#ifndef GeolocationClientAndroid_h
#define GeolocationClientAndroid_h

#include <GeolocationClient.h>

namespace android {

class WebViewCore;

// The Android implementation of GeolocationClient. Acts as a proxy to
// the real or mock impl, which is owned by the GeolocationManager.
class GeolocationClientAndroid : public WebCore::GeolocationClient {
public:
    GeolocationClientAndroid();
    virtual ~GeolocationClientAndroid();

    // GeolocationClient
    virtual void geolocationDestroyed();
    virtual void startUpdating();
    virtual void stopUpdating();
    virtual void setEnableHighAccuracy(bool);
    virtual WebCore::GeolocationPosition* lastPosition();
    virtual void requestPermission(WebCore::Geolocation*);
    virtual void cancelPermissionRequest(WebCore::Geolocation*);

    void setWebViewCore(WebViewCore*);

private:
    WebCore::GeolocationClient* client() const;

    WebViewCore* m_webViewCore;
};

} // namespace android

#endif // GeolocationClientAndroid_h
