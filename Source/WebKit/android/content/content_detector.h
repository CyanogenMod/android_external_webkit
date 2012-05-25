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

#ifndef CONTENT_RENDERER_ANDROID_CONTENT_DETECTOR_H_
#define CONTENT_RENDERER_ANDROID_CONTENT_DETECTOR_H_
#pragma once

#include "build/build_config.h"  // Needed for OS_ANDROID

#if defined(OS_ANDROID)

#include "base/string_util.h"
#include "googleurl/src/gurl.h"
#include "public/WebRange.h"

namespace WebKit {
class WebHitTestInfo;
}

namespace WebCore {
class Settings;
}

// Base class for text-based content detectors.
class ContentDetector {
 public:

  // Holds the content detection results.
  struct Result {
    bool valid; // Flag indicating if the result is valid.
    WebKit::WebRange range; // Range describing the content boundaries.
    std::string text; // Processed text of the content.
    GURL intent_url; // URL of the intent that should process this content.

    Result() : valid(false) {}

    Result(const WebKit::WebRange& range,
           const std::string& text,
           const GURL& intent_url)
        : valid(true),
          range(range),
          text(text),
          intent_url(intent_url) {}
  };

  virtual ~ContentDetector() {}

  // Returns a WebKit range delimiting the contents found around the tapped
  // position. If no content is found a null range will be returned.
  Result FindTappedContent(const WebKit::WebHitTestInfo& hit_test);

 protected:
  // Parses the input string defined by the begin/end iterators returning true
  // if the desired content is found. The start and end positions relative to
  // the input iterators are returned in start_pos and end_pos.
  // The end position is assumed to be non-inclusive.
  virtual bool FindContent(const string16::const_iterator& begin,
                           const string16::const_iterator& end,
                           size_t* start_pos,
                           size_t* end_pos) = 0;

  virtual bool IsEnabled(const WebKit::WebHitTestInfo& hit_test) = 0;
  WebCore::Settings* GetSettings(const WebKit::WebHitTestInfo& hit_test);

  // Extracts and processes the text of the detected content.
  virtual std::string GetContentText(const WebKit::WebRange& range) = 0;

  // Returns the intent URL that should process the content, if any.
  virtual GURL GetIntentURL(const std::string& content_text) = 0;

  // Returns the maximum length of text to be extracted around the tapped
  // position in order to search for content.
  virtual size_t GetMaximumContentLength() = 0;

  ContentDetector() {}
  WebKit::WebRange FindContentRange(const WebKit::WebHitTestInfo& hit_test);

  DISALLOW_COPY_AND_ASSIGN(ContentDetector);
};

#endif  // defined(OS_ANDROID)

#endif  // CONTENT_RENDERER_ANDROID_CONTENT_DETECTOR_H_
