/*
 * Copyright 2012, The Android Open Source Project
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

#ifndef AndroidHitTestResult_h
#define AndroidHitTestResult_h

#include "content/content_detector.h"
#include "Element.h"
#include "HitTestResult.h"
#include "IntRect.h"
#include "wtf/Vector.h"

#include <jni.h>

namespace android {

class WebViewCore;

class AndroidHitTestResult
{
public:
    AndroidHitTestResult(WebViewCore*, WebCore::HitTestResult&);
    ~AndroidHitTestResult() {}

    WebCore::HitTestResult& hitTestResult() { return m_hitTestResult; }
    Vector<WebCore::IntRect>& highlightRects() { return m_highlightRects; }

    void setURLElement(WebCore::Element* element);
    void buildHighlightRects();
    void searchContentDetectors();

    jobject createJavaObject(JNIEnv*);

private:
    Vector<WebCore::IntRect> enclosingParentRects(WebCore::Node* node);

    WebViewCore* m_webViewCore;
    WebCore::HitTestResult m_hitTestResult;
    Vector<WebCore::IntRect> m_highlightRects;
    ContentDetector::Result m_searchResult;
};

}   // namespace android

#endif // AndroidHitTestResult_h
