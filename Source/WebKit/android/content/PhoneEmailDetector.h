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

#include "content/content_detector.h"
#include "PlatformString.h"

#define NAVIGATION_MAX_PHONE_LENGTH 14

struct FindState {
    int mStartResult;
    int mEndResult;
    char* mPattern;
    UChar mStore[NAVIGATION_MAX_PHONE_LENGTH + 1];
    UChar* mStorePtr;
    UChar mBackOne;
    UChar mBackTwo;
    UChar mCurrent;
    bool mOpenParen;
    bool mInitialized;
    bool mContinuationNode;
};

enum FoundState {
    FOUND_NONE,
    FOUND_PARTIAL,
    FOUND_COMPLETE
};

// Searches for phone numbers (US only) or email addresses based off of the navcache code
class PhoneEmailDetector : public ContentDetector {
public:
    PhoneEmailDetector();
    virtual ~PhoneEmailDetector() {}

private:
    // Implementation of ContentDetector.
    virtual bool FindContent(const string16::const_iterator& begin,
                             const string16::const_iterator& end,
                             size_t* start_pos,
                             size_t* end_pos);

    virtual std::string GetContentText(const WebKit::WebRange& range);
    virtual GURL GetIntentURL(const std::string& content_text);
    virtual size_t GetMaximumContentLength() {
        return NAVIGATION_MAX_PHONE_LENGTH * 4;
    }
    virtual bool IsEnabled(const WebKit::WebHitTestInfo& hit_test) OVERRIDE;

    DISALLOW_COPY_AND_ASSIGN(PhoneEmailDetector);

    FindState m_findState;
    FoundState m_foundResult;
    const char* m_prefix;
    // TODO: This shouldn't be done like this. PhoneEmailDetector should be
    // refactored into two pieces and follow the IsEnabled style. This will
    // only work because we always call IsEnabled before FindContent
    bool m_isPhoneDetectionEnabled;
    bool m_isEmailDetectionEnabled;
};
