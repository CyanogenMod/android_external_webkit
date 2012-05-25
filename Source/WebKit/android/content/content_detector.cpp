/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *    * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *    * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *    * Neither the name of Google Inc. nor the names of its
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

// Magic pretend-to-be-a-chromium-build flags
#undef WEBKIT_IMPLEMENTATION
#undef LOG

#include "content/content_detector.h"

#include "public/android/WebDOMTextContentWalker.h"
#include "public/android/WebHitTestInfo.h"

#include "Document.h"
#include "Node.h"
#include "Page.h"
#include "Settings.h"

using WebKit::WebDOMTextContentWalker;
using WebKit::WebRange;

ContentDetector::Result ContentDetector::FindTappedContent(
    const WebKit::WebHitTestInfo& hit_test) {
  if (!IsEnabled(hit_test))
    return Result();
  WebKit::WebRange range = FindContentRange(hit_test);
  if (range.isNull())
    return Result();

  std::string text = GetContentText(range);
  GURL intent_url = GetIntentURL(text);
  return Result(range, text, intent_url);
}

WebRange ContentDetector::FindContentRange(
    const WebKit::WebHitTestInfo& hit_test) {
  WebDOMTextContentWalker content_walker(hit_test, GetMaximumContentLength());
  string16 content = content_walker.content();
  if (content.empty())
    return WebRange();

  size_t selected_offset = content_walker.hitOffsetInContent();
  for (size_t start_offset = 0; start_offset < content.length();) {
    size_t relative_start, relative_end;
    if (!FindContent(content.begin() + start_offset,
        content.end(), &relative_start, &relative_end)) {
      break;
    } else {
      size_t content_start = start_offset + relative_start;
      size_t content_end = start_offset + relative_end;
      DCHECK(content_end <= content.length());

      if (selected_offset >= content_start && selected_offset < content_end) {
        WebRange range = content_walker.contentOffsetsToRange(
            content_start, content_end);
        DCHECK(!range.isNull());
        return range;
      } else {
        start_offset += relative_end;
      }
    }
  }

  return WebRange();
}

WebCore::Settings* ContentDetector::GetSettings(const WebKit::WebHitTestInfo& hit_test) {
  if (!hit_test.node() || !hit_test.node()->document())
    return 0;
  return hit_test.node()->document()->page()->settings();
}
