/*
 * Copyright 2009, The Android Open Source Project
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

#ifndef GeolocationServiceAndroid_h
#define GeolocationServiceAndroid_h

#include "GeolocationService.h"
#include "Timer.h"

#include <GeolocationServiceBridge.h>
#include <wtf/OwnPtr.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefPtr.h>

namespace WebCore {

class GeolocationServiceAndroid : public GeolocationService, public android::GeolocationServiceBridge::Listener {
public:
    static GeolocationService* create(GeolocationServiceClient*);

    virtual ~GeolocationServiceAndroid() { };

    // GeolocationService
    // ANDROID
    // TODO: Upstream to webkit.org. See https://bugs.webkit.org/show_bug.cgi?id=34082
    virtual bool startUpdating(PositionOptions*, bool suspend);
    virtual void stopUpdating();
    virtual Geoposition* lastPosition() const { return m_lastPosition.get(); }
    virtual PositionError* lastError() const { return m_lastError.get(); }
    virtual void suspend();
    virtual void resume();

    // android::GeolocationServiceBridge::Listener
    virtual void newPositionAvailable(PassRefPtr<Geoposition>);
    virtual void newErrorAvailable(PassRefPtr<PositionError>);

    void timerFired(Timer<GeolocationServiceAndroid>* timer);

private:
    GeolocationServiceAndroid(GeolocationServiceClient*);

    static bool isPositionMovement(Geoposition* position1, Geoposition* position2);
    static bool isPositionMoreAccurate(Geoposition* position1, Geoposition* position2);
    static bool isPositionMoreTimely(Geoposition* position1, Geoposition* position2);

    Timer<GeolocationServiceAndroid> m_timer;
    RefPtr<Geoposition> m_lastPosition;
    RefPtr<PositionError> m_lastError;
    OwnPtr<android::GeolocationServiceBridge> m_javaBridge;
};

} // namespace WebCore

#endif // GeolocationServiceAndroid_h
