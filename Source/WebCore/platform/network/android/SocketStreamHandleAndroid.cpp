/*
 * Copyright (C) 2013 Google Inc.  All rights reserved.
 * Copyright (C) 2013 Oleg Smirnov
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "Logging.h"
#include "NotImplemented.h"
#include "SocketStreamHandle.h"
#include "SocketStreamHandleClient.h"
#include "WebSocketBridge.h"

namespace WebCore {

SocketStreamHandle::SocketStreamHandle(const KURL& url, SocketStreamHandleClient* client)
    : SocketStreamHandleBase(url, client)
    , m_url(url)
{
    LOG(Network, "SocketStreamHandle::SocketStreamHandle %p", this);
    bool isSecure = m_url.protocolIs("wss");
    int port = m_url.hasPort() ? m_url.port() : (isSecure ? 443 : 80);

    String httpProtocol = isSecure ? "https://" : "http://";
    String uri = httpProtocol + m_url.host() + ":" + String::number(port);

    m_webSocketBridge = adoptPtr(new WebSocketBridge(this, uri));
}

SocketStreamHandle::~SocketStreamHandle()
{
    LOG(Network, "SocketStreamHandle::~SocketStreamHandle %p", this);
}

void SocketStreamHandle::socketConnectedCallback()
{
    LOG(Network, "SocketStreamHandle::socketConnected %p", this);
    // The client can close the handle, potentially removing the last reference.
    RefPtr<SocketStreamHandle> protect(this);
    if (client()) {
        m_state = SocketStreamHandleBase::Open;
        client()->didOpen(this);
    }
}

void SocketStreamHandle::socketClosedCallback()
{
    LOG(Network, "SocketStreamHandle::socketClosedCallback %p", this);
    if (client()) {
        m_state = SocketStreamHandleBase::Closed;
        client()->didClose(this);
    }
}

void SocketStreamHandle::socketReadyReadCallback(const char* data, int length)
{
    LOG(Network, "SocketStreamHandle::socketReadyRead %p", this);
    if (client())
        client()->didReceiveData(this, data, length);
}

void SocketStreamHandle::socketErrorCallback()
{
    LOG(Network, "SocketStreamHandle::socketErrorCallback %p", this);
    if (client())
        client()->didClose(this);
}

int SocketStreamHandle::platformSend(const char* data, int len)
{
    LOG(Network, "SocketStreamHandle::platformSend %p", this);
    return m_webSocketBridge->send(data, len);
}

void SocketStreamHandle::platformClose()
{
    LOG(Network, "SocketStreamHandle %p platformClose", this);
    m_webSocketBridge->close();
}

void SocketStreamHandle::didReceiveAuthenticationChallenge(const AuthenticationChallenge&)
{
    notImplemented();
}

void SocketStreamHandle::receivedCredential(const AuthenticationChallenge&, const Credential&)
{
    notImplemented();
}

void SocketStreamHandle::receivedRequestToContinueWithoutCredential(const AuthenticationChallenge&)
{
    notImplemented();
}

void SocketStreamHandle::receivedCancellation(const AuthenticationChallenge&)
{
    notImplemented();
}
}  // namespace WebCore
