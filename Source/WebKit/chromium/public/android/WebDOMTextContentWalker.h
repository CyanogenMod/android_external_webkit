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

#ifndef WebDOMTextContentWalker_h
#define WebDOMTextContentWalker_h

#include "../WebPrivateOwnPtr.h"
#include "../WebRange.h"
#include "../WebString.h"

namespace WebCore {
class DOMTextContentWalker;
class Node;
}

namespace WebKit {

class WebHitTestInfo;

class WebDOMTextContentWalker {
public:
    WebDOMTextContentWalker();
    ~WebDOMTextContentWalker();

    // Creates a new text content walker centered in the position described by the hit test.
    // The maximum length of the contents retrieved by the walker is defined by maxLength.
    WEBKIT_API WebDOMTextContentWalker(const WebHitTestInfo&, size_t maxLength);

    // Creates a new text content walker centered in the selected offset of the given text node.
    // The maximum length of the contents retrieved by the walker is defined by maxLength.
    WEBKIT_API WebDOMTextContentWalker(WebCore::Node* textNode, size_t offset, size_t maxLength);

    // Text content retrieved by the walker.
    WEBKIT_API WebString content() const;

    // Position of the initial text node offset in the content string.
    WEBKIT_API size_t hitOffsetInContent() const;

    // Convert start/end positions in the content text string into a WebKit text range.
    WEBKIT_API WebRange contentOffsetsToRange(size_t startInContent, size_t endInContent);

protected:
    WebPrivateOwnPtr<WebCore::DOMTextContentWalker> m_private;
};

} // namespace WebKit

#endif
