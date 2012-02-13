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

#ifndef DOMTextContentWalker_h
#define DOMTextContentWalker_h

#if OS(ANDROID)

#include "PlatformString.h"

namespace WebCore {

class Range;
class VisiblePosition;

// Explore the DOM tree to find the text contents up to a limit
// around a position in a given text node.
class DOMTextContentWalker {
  WTF_MAKE_NONCOPYABLE(DOMTextContentWalker);
public:
  DOMTextContentWalker(const VisiblePosition& position, unsigned maxLength);

  String content() const;
  unsigned hitOffsetInContent() const;

  // Convert start/end positions in the content text string into a text range.
  PassRefPtr<Range> contentOffsetsToRange(unsigned startInContent, unsigned endInContent);

private:
  RefPtr<Range> m_contentRange;
  size_t m_hitOffsetInContent;
};

} // namespace WebCore

#endif // OS(ANDROID)

#endif // DOMTextContentWalker_h

