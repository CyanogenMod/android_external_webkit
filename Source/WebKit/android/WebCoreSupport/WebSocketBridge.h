/*
 * Copyright 2013, The Android Open Source Project
 * Copyright 2013 Oleg Smirnov
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

#ifndef WebSocketBridge_h
#define WebSocketBridge_h

#if ENABLE(WEB_SOCKETS)

#include "SocketStreamHandle.h"
#include <wtf/OwnPtr.h>
#include <jni.h>

namespace WebCore {

class WebSocketBridge
{
public:
    WebSocketBridge(SocketStreamHandle*, const String&);
    ~WebSocketBridge();

    int send(const char*, int);
    void close();

    void didWebSocketConnected();
    void didWebSocketClosed();
    void didWebSocketMessage(const char*, int);
    void didWebSocketError();

private:
    struct JavaGlue;
    OwnPtr<JavaGlue> m_glue;

    SocketStreamHandle* m_streamHandle;
};
} // namespace WebCore

#endif // ENABLE(WEB_SOCKETS)
#endif // WebSocketBridge_h
