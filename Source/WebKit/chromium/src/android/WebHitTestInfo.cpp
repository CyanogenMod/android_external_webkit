/*
* Copyright (C) 2011 Google Inc. All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions
* are met:
*
* 1.  Redistributions of source code must retain the above copyright
*     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "android/WebHitTestInfo.h"

#include "Element.h"
#include "HitTestResult.h"
#include "KURL.h"
#include "Node.h"
#include "RenderObject.h"
#include "VisiblePosition.h"

using namespace WebCore;

namespace WebKit {

WebHitTestInfo::WebHitTestInfo()
{
}

WebHitTestInfo::WebHitTestInfo(const WebHitTestInfo& testInfo)
    : m_private(new HitTestResult(testInfo))
{
}

WebURL WebHitTestInfo::linkURL() const
{
    return m_private->absoluteLinkURL();
}

WebHitTestInfo::~WebHitTestInfo()
{
    m_private.reset(0);
}

WebURL WebHitTestInfo::imageURL() const
{
    return m_private->absoluteImageURL();
}

Node* WebHitTestInfo::node() const
{
    return m_private->innerNode();
}

WebPoint WebHitTestInfo::point() const
{
    return WebPoint(m_private->localPoint());
}

bool WebHitTestInfo::isContentEditable() const
{
    return m_private->isContentEditable();
}

WebHitTestInfo::WebHitTestInfo(const HitTestResult& result)
{
    m_private.reset(new HitTestResult(result));
}

WebHitTestInfo& WebHitTestInfo::operator=(const HitTestResult& result)
{
    m_private.reset(new HitTestResult(result));
    return *this;
}

WebHitTestInfo::operator HitTestResult() const
{
    return *m_private.get();
}

} // namespace WebKit
