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
#include "DOMTextContentWalker.h"

#if OS(ANDROID)

#include "Range.h"
#include "TextIterator.h"
#include "VisiblePosition.h"
#include "VisibleSelection.h"
#include "visible_units.h"

namespace WebCore {

static PassRefPtr<Range> getRange(const Position& start, const Position& end)
{
    return VisibleSelection(start.parentAnchoredEquivalent(), end.parentAnchoredEquivalent(), DOWNSTREAM).firstRange();
}

DOMTextContentWalker::DOMTextContentWalker(const VisiblePosition& position, unsigned maxLength)
    : m_hitOffsetInContent(0)
{
    const unsigned halfMaxLength = maxLength / 2;
    RefPtr<Range> forwardRange = makeRange(position, endOfDocument(position));
    if (!forwardRange)
        return;
    CharacterIterator forwardChar(forwardRange.get(), TextIteratorStopsOnFormControls);
    forwardChar.advance(maxLength - halfMaxLength);

    // No forward contents, started inside form control.
    if (getRange(position.deepEquivalent(), forwardChar.range()->startPosition())->text().length() == 0)
        return;

    RefPtr<Range> backwardsRange = makeRange(startOfDocument(position), position);
    if (!backwardsRange)
        return;
    BackwardsCharacterIterator backwardsChar(backwardsRange.get(), TextIteratorStopsOnFormControls);
    backwardsChar.advance(halfMaxLength);

    m_hitOffsetInContent = getRange(backwardsChar.range()->endPosition(), position.deepEquivalent())->text().length();
    m_contentRange = getRange(backwardsChar.range()->endPosition(), forwardChar.range()->startPosition());
}

PassRefPtr<Range> DOMTextContentWalker::contentOffsetsToRange(unsigned startInContent, unsigned endInContent)
{
    if (startInContent >= endInContent || endInContent > content().length())
        return 0;

    CharacterIterator iterator(m_contentRange.get());
    iterator.advance(startInContent);

    Position start = iterator.range()->startPosition();
    iterator.advance(endInContent - startInContent);
    Position end = iterator.range()->startPosition();
    return getRange(start, end);
}

String DOMTextContentWalker::content() const
{
    if (m_contentRange)
        return m_contentRange->text();
    return String();
}

unsigned DOMTextContentWalker::hitOffsetInContent() const
{
    return m_hitOffsetInContent;
}

} // namespace WebCore

#endif // OS(ANDROID)
