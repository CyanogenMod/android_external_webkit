/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "android/WebDOMTextContentWalker.h"

#include "DOMTextContentWalker.h"
#include "Element.h"
#include "Node.h"
#include "Range.h"
#include "RenderObject.h"
#include "Text.h"
#include "VisiblePosition.h"
#include "android/WebHitTestInfo.h"

using namespace WebCore;

namespace WebKit {

WebDOMTextContentWalker::WebDOMTextContentWalker()
{
}

WebDOMTextContentWalker::~WebDOMTextContentWalker()
{
    m_private.reset(0);
}

WebDOMTextContentWalker::WebDOMTextContentWalker(const WebHitTestInfo& hitTestInfo, size_t maxLength)
{
    Node* node = hitTestInfo.node();
    if (!node)
        return;

    Element* element = node->parentElement();
    if (!node->inDocument() && element && element->inDocument())
        node = element;
    m_private.reset(new DOMTextContentWalker(node->renderer()->positionForPoint(hitTestInfo.point()), maxLength));
}

WebDOMTextContentWalker::WebDOMTextContentWalker(Node* node, size_t offset, size_t maxLength)
{
    if (!node || !node->isTextNode() || offset >= node->nodeValue().length())
        return;

    m_private.reset(new DOMTextContentWalker(VisiblePosition(Position(static_cast<Text*>(node), offset).parentAnchoredEquivalent(), DOWNSTREAM), maxLength));
}

WebString WebDOMTextContentWalker::content() const
{
    return m_private->content();
}

size_t WebDOMTextContentWalker::hitOffsetInContent() const
{
    return m_private->hitOffsetInContent();
}

WebRange WebDOMTextContentWalker::contentOffsetsToRange(size_t startInContent, size_t endInContent)
{
    return m_private->contentOffsetsToRange(startInContent, endInContent);
}

} // namespace WebKit
