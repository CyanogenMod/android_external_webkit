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

#undef WEBKIT_IMPLEMENTATION
#undef LOG

#include "base/utf_string_conversions.h"
#include "net/base/escape.h"
#include "PhoneEmailDetector.h"
#include "Settings.h"
#include "WebString.h"

#define LOG_TAG "PhoneNumberDetector"
#include <cutils/log.h>

#define CHINA_PHONE_PATTERN "130 - 0000 - 0000"
#define PHONE_PATTERN "(200) /-.\\ 100 -. 0000"

static const char kTelSchemaPrefix[] = "tel:";
static const char kEmailSchemaPrefix[] = "mailto:";

void FindStateCopy(FindState* to, const FindState* from);
void ChinaFindReset(FindState* state);
void ChinaFindResetNumber(FindState* state);
FoundState ChinaFindPhoneNum(const UChar* chars, unsigned length,
                             FindState* s);

void FindReset(FindState* state);
void FindResetNumber(FindState* state);
FoundState FindPartialNumber(const UChar* chars, unsigned length,
                             FindState* s);
struct FindState;

static FoundState FindPartialEMail(const UChar* , unsigned length, FindState* );
static bool IsDomainChar(UChar ch);
static bool IsMailboxChar(UChar ch);

PhoneEmailDetector::PhoneEmailDetector()
    : m_foundResult(FOUND_NONE)
{
}

bool PhoneEmailDetector::IsEnabled(const WebKit::WebHitTestInfo& hit_test)
{
    WebCore::Settings* settings = GetSettings(hit_test);
    if (!settings)
        return false;
    m_isPhoneDetectionEnabled = settings->formatDetectionTelephone();
    m_isEmailDetectionEnabled = settings->formatDetectionEmail();
    return m_isEmailDetectionEnabled || m_isPhoneDetectionEnabled;
}



bool PhoneEmailDetector::FindContent(const string16::const_iterator& begin,
                             const string16::const_iterator& end,
                             size_t* start_pos,
                             size_t* end_pos)
{
    #define HANDLE_FOUND_RESULTS() \
            if (foundResult == FOUND_COMPLETE && \
                (m_foundResult != FOUND_COMPLETE || \
                findState.mStartResult < m_findState.mStartResult)) { \
                FindStateCopy(&m_findState, &findState); \
                m_foundResult = foundResult; \
            }

    FindReset(&m_findState);
    m_foundResult = FOUND_NONE;
    if (m_isPhoneDetectionEnabled) {
        FoundState foundResult = FOUND_NONE;
        FindState findState;

        ChinaFindReset(&findState);
        foundResult = ChinaFindPhoneNum(begin, end - begin, &findState);
        HANDLE_FOUND_RESULTS();

        FindReset(&findState);
        foundResult = FindPartialNumber(begin, end - begin, &findState);
        HANDLE_FOUND_RESULTS();
    }

    if (m_foundResult == FOUND_COMPLETE)
        m_prefix = kTelSchemaPrefix;
    else {
        FindReset(&m_findState);
        if (m_isEmailDetectionEnabled)
            m_foundResult = FindPartialEMail(begin, end - begin, &m_findState);
        m_prefix = kEmailSchemaPrefix;
    }
    *start_pos = m_findState.mStartResult;
    *end_pos = m_findState.mEndResult;
    return m_foundResult == FOUND_COMPLETE;
}

std::string PhoneEmailDetector::GetContentText(const WebKit::WebRange& range)
{
    if (m_foundResult == FOUND_COMPLETE) {
        if (m_prefix == kTelSchemaPrefix)
            return UTF16ToUTF8(m_findState.mStore);
        else
            return UTF16ToUTF8(range.toPlainText());
    }
    return std::string();
}

GURL PhoneEmailDetector::GetIntentURL(const std::string& content_text)
{
    return GURL(m_prefix +
            EscapeQueryParamValue(content_text, true));
}

void FindStateCopy(FindState* to, const FindState* from)
{
    if (to != NULL && from != NULL) {
        memcpy(to, from, sizeof(FindState));
        to->mStorePtr = to->mStore + (from->mStorePtr - from->mStore);
    }
}

void ChinaFindReset(FindState* state)
{
    memset(state, 0, sizeof(FindState));
    state->mCurrent = ' ';
    ChinaFindResetNumber(state);
}

void ChinaFindResetNumber(FindState* state)
{
    state->mOpenParen = false;
    state->mPattern = (char*) CHINA_PHONE_PATTERN;
    state->mStorePtr = state->mStore;
}

FoundState ChinaFindPhoneNum(const UChar* chars, unsigned length,
    FindState* s)
{
    #define PREPARE_GOTO_NEXT() \
       *store++ = ch; \
       pattern++; \
       lastDigit = chars;

    char* pattern = s->mPattern;
    UChar* store = s->mStorePtr;
    const UChar* start = chars;
    const UChar* end = chars + length;
    const UChar* lastDigit = 0;
    string16 search16(chars, length);
    std::string searchSpace = UTF16ToUTF8(search16);
retry:
    do {
        bool initialized = s->mInitialized;
        while (chars < end) {
            if (initialized == false) {
                s->mBackThree = s->mBackTwo;
                s->mBackTwo = s->mBackOne;
                s->mBackOne = s->mCurrent;
            }
            UChar ch = s->mCurrent = *chars;
            do {
                char patternChar = *pattern;
                switch (patternChar) {
                    case '1':
                        if (initialized == false) {
                            s->mStartResult = chars - start;
                            initialized = true;
                        }
                        if (ch != patternChar) {
                            goto resetPattern;
                        }
                        PREPARE_GOTO_NEXT();
                        goto nextChar;
                    case '3':
                        if (ch != '3' && ch != '5' && ch != '8') {
                            goto resetPattern;
                        }
                        PREPARE_GOTO_NEXT();
                        goto nextChar;
                    case '0':
                        if (ch < patternChar || ch > '9')
                            goto resetPattern;
                        PREPARE_GOTO_NEXT();
                        goto nextChar;
                    case '\0':
                        if (WTF::isASCIIDigit(ch) == false) {
                            *store = '\0';
                            goto checkMatch;
                        }
                        goto resetPattern;
                    case ' ':
                        if (ch == patternChar)
                            goto nextChar;
                        break;
                    default:
                    commonPunctuation:
                        if (ch == patternChar) {
                            pattern++;
                            goto nextChar;
                        }
                }
            } while (++pattern); // never false
    nextChar:
            chars++;
        }
        break;
resetPattern:
        if (s->mContinuationNode)
            return FOUND_NONE;
        ChinaFindResetNumber(s);
        pattern = s->mPattern;
        store = s->mStorePtr;
    } while (++chars < end);
checkMatch:
    if (WTF::isASCIIDigit((s->mBackOne == '6' && s->mBackTwo == '8') ?
            s->mBackThree : s->mBackOne) || s->mBackOne == '+') {
        if(++chars < end) {
            if (s->mContinuationNode) {
                return FOUND_NONE;
            }
            ChinaFindResetNumber(s);
            pattern = s->mPattern;
            store = s->mStorePtr;
            goto retry;
        } else {
            return FOUND_NONE;
        }
    }
    *store = '\0';
    s->mStorePtr = store;
    s->mPattern = pattern;
    s->mEndResult = lastDigit - start + 1;
    char pState = pattern[0];
    return pState == '\0' ? FOUND_COMPLETE : FOUND_NONE;
}

void FindReset(FindState* state)
{
    memset(state, 0, sizeof(FindState));
    state->mCurrent = ' ';
    FindResetNumber(state);
}

void FindResetNumber(FindState* state)
{
    state->mOpenParen = false;
    state->mPattern = (char*) PHONE_PATTERN;
    state->mStorePtr = state->mStore;
}

FoundState FindPartialNumber(const UChar* chars, unsigned length,
    FindState* s)
{
    char* pattern = s->mPattern;
    UChar* store = s->mStorePtr;
    const UChar* start = chars;
    const UChar* end = chars + length;
    const UChar* lastDigit = 0;
    string16 search16(chars, length);
    std::string searchSpace = UTF16ToUTF8(search16);
retry:
    do {
        bool initialized = s->mInitialized;
        while (chars < end) {
            if (initialized == false) {
                s->mBackTwo = s->mBackOne;
                s->mBackOne = s->mCurrent;
            }
            UChar ch = s->mCurrent = *chars;
            do {
                char patternChar = *pattern;
                switch (patternChar) {
                    case '2':
                        if (initialized == false) {
                            s->mStartResult = chars - start;
                            initialized = true;
                        }
                    case '0':
                    case '1':
                        if (ch < patternChar || ch > '9')
                            goto resetPattern;
                        *store++ = ch;
                        pattern++;
                        lastDigit = chars;
                        goto nextChar;
                    case '\0':
                        if (WTF::isASCIIDigit(ch) == false) {
                            *store = '\0';
                            goto checkMatch;
                        }
                        goto resetPattern;
                    case ' ':
                        if (ch == patternChar)
                            goto nextChar;
                        break;
                    case '(':
                        if (ch == patternChar) {
                            s->mStartResult = chars - start;
                            initialized = true;
                            s->mOpenParen = true;
                        }
                        goto commonPunctuation;
                    case ')':
                        if ((ch == patternChar) ^ s->mOpenParen)
                            goto resetPattern;
                    default:
                    commonPunctuation:
                        if (ch == patternChar) {
                            pattern++;
                            goto nextChar;
                        }
                }
            } while (++pattern); // never false
    nextChar:
            chars++;
        }
        break;
resetPattern:
        if (s->mContinuationNode)
            return FOUND_NONE;
        FindResetNumber(s);
        pattern = s->mPattern;
        store = s->mStorePtr;
    } while (++chars < end);
checkMatch:
    /*
     * A few interesting cases:
     *  03122572251 3122572251     # two numbers, s->mBackOne = 0,                  return second
     *  013122572251 3122572251    # two numbers, s->mBackOne = 1, s->mBackTwo = 0, return second
     *  113122572251 3122572251    # two numbers, s->mBackOne = 1, s->mBackTwo = 1, return second
     *
     *  The prefix of above US phone number is "0" or "01" or "11".
     *  Such as three cases mentioned above, the first group phone number
     *  is invalid, but the detection blocks also have a telephone number,
     *  the second valid phone number should be detected.
     */
    if (WTF::isASCIIDigit(s->mBackOne != '1' ? s->mBackOne : s->mBackTwo)) {
        if(++chars < end) {
            if (s->mContinuationNode) {
                return FOUND_NONE;
            }
            FindResetNumber(s);
            pattern = s->mPattern;
            store = s->mStorePtr;
            goto retry;
        } else {
            return FOUND_NONE;
        }
    }
    *store = '\0';
    s->mStorePtr = store;
    s->mPattern = pattern;
    s->mEndResult = lastDigit - start + 1;
    char pState = pattern[0];
    return pState == '\0' ? FOUND_COMPLETE : pState == '(' || (WTF::isASCIIDigit(pState) && WTF::isASCIIDigit(pattern[-1])) ?
        FOUND_NONE : FOUND_PARTIAL;
}

FoundState FindPartialEMail(const UChar* chars, unsigned length,
    FindState* s)
{
    // the following tables were generated by tests/browser/focusNavigation/BrowserDebug.cpp
    // hand-edit at your own risk
    static const int domainTwoLetter[] = {
        0x02df797c,  // a followed by: [cdefgilmnoqrstuwxz]
        0x036e73fb,  // b followed by: [abdefghijmnorstvwyz]
        0x03b67ded,  // c followed by: [acdfghiklmnorsuvxyz]
        0x02005610,  // d followed by: [ejkmoz]
        0x001e00d4,  // e followed by: [ceghrstu]
        0x00025700,  // f followed by: [ijkmor]
        0x015fb9fb,  // g followed by: [abdefghilmnpqrstuwy]
        0x001a3400,  // h followed by: [kmnrtu]
        0x000f7818,  // i followed by: [delmnoqrst]
        0x0000d010,  // j followed by: [emop]
        0x0342b1d0,  // k followed by: [eghimnprwyz]
        0x013e0507,  // l followed by: [abcikrstuvy]
        0x03fffccd,  // m followed by: [acdghklmnopqrstuvwxyz]
        0x0212c975,  // n followed by: [acefgilopruz]
        0x00001000,  // o followed by: [m]
        0x014e3cf1,  // p followed by: [aefghklmnrstwy]
        0x00000001,  // q followed by: [a]
        0x00504010,  // r followed by: [eouw]
        0x032a7fdf,  // s followed by: [abcdeghijklmnortvyz]
        0x026afeec,  // t followed by: [cdfghjklmnoprtvwz]
        0x03041441,  // u followed by: [agkmsyz]
        0x00102155,  // v followed by: [aceginu]
        0x00040020,  // w followed by: [fs]
        0x00000000,  // x
        0x00180010,  // y followed by: [etu]
        0x00401001,  // z followed by: [amw]
    };

    static char const* const longDomainNames[] = {
        "\x03" "ero" "\x03" "rpa",  // aero, arpa
        "\x02" "iz",  // biz
        "\x02" "at" "\x02" "om" "\x03" "oop",  // cat, com, coop
        NULL,  // d
        "\x02" "du",  // edu
        NULL,  // f
        "\x02" "ov",  // gov
        NULL,  // h
        "\x03" "nfo" "\x02" "nt",  // info, int
        "\x03" "obs",  // jobs
        NULL,  // k
        NULL,  // l
        "\x02" "il" "\x03" "obi" "\x05" "useum",  // mil, mobi, museum
        "\x03" "ame" "\x02" "et",  // name, net
        "\x02" "rg",  // , org
        "\x02" "ro",  // pro
        NULL,  // q
        NULL,  // r
        NULL,  // s
        "\x05" "ravel",  // travel
        NULL,  // u
        NULL,  // v
        NULL,  // w
        NULL,  // x
        NULL,  // y
        NULL,  // z
    };

    const UChar* start = chars;
    const UChar* end = chars + length;
    while (chars < end) {
        UChar ch = *chars++;
        if (ch != '@')
            continue;
        const UChar* atLocation = chars - 1;
        // search for domain
        ch = *chars++ | 0x20; // convert uppercase to lower
        if (ch < 'a' || ch > 'z')
            continue;
        while (chars < end) {
            ch = *chars++;
            if (IsDomainChar(ch) == false)
                goto nextAt;
            if (ch != '.')
                continue;
            UChar firstLetter = *chars++ | 0x20; // first letter of the domain
            if (chars >= end)
                return FOUND_NONE; // only one letter; must be at least two
            firstLetter -= 'a';
            if (firstLetter > 'z' - 'a')
                continue; // non-letter followed '.'
            int secondLetterMask = domainTwoLetter[firstLetter];
            ch = *chars | 0x20; // second letter of the domain
            ch -= 'a';
            if (ch >= 'z' - 'a')
                continue;
            bool secondMatch = (secondLetterMask & 1 << ch) != 0;
            const char* wordMatch = longDomainNames[firstLetter];
            int wordIndex = 0;
            while (wordMatch != NULL) {
                int len = *wordMatch++;
                char match;
                do {
                    match = wordMatch[wordIndex];
                    if (match < 0x20)
                        goto foundDomainStart;
                    if (chars[wordIndex] != match)
                        break;
                    wordIndex++;
                } while (true);
                wordMatch += len;
                if (*wordMatch == '\0')
                    break;
                wordIndex = 0;
            }
            if (secondMatch) {
                wordIndex = 1;
        foundDomainStart:
                chars += wordIndex;
                if (chars < end) {
                    ch = *chars;
                    if (ch != '.') {
                        if (IsDomainChar(ch))
                            goto nextDot;
                    } else if (chars + 1 < end && IsDomainChar(chars[1]))
                        goto nextDot;
                }
                // found domain. Search backwards from '@' for beginning of email address
                s->mEndResult = chars - start;
                chars = atLocation;
                if (chars <= start)
                    goto nextAt;
                ch = *--chars;
                if (ch == '.')
                    goto nextAt; // mailbox can't end in period
                do {
                    if (IsMailboxChar(ch) == false) {
                        chars++;
                        break;
                    }
                    if (chars == start)
                        break;
                    ch = *--chars;
                } while (true);
                UChar firstChar = *chars;
                if (firstChar == '.' || firstChar == '@') // mailbox can't start with period or be empty
                    goto nextAt;
                s->mStartResult = chars - start;
                return FOUND_COMPLETE;
            }
    nextDot:
            ;
        }
nextAt:
        chars = atLocation + 1;
    }
    return FOUND_NONE;
}

bool IsDomainChar(UChar ch)
{
    static const unsigned body[] = {0x03ff6000, 0x07fffffe, 0x07fffffe}; // 0-9 . - A-Z a-z
    ch -= 0x20;
    if (ch > 'z' - 0x20)
        return false;
    return (body[ch >> 5] & 1 << (ch & 0x1f)) != 0;
}

bool IsMailboxChar(UChar ch)
{
    // According to http://en.wikipedia.org/wiki/Email_address
    // ! # $ % & ' * + - . / 0-9 = ?
    // A-Z ^ _
    // ` a-z { | } ~
    static const unsigned body[] = {0xa3ffecfa, 0xc7fffffe, 0x7fffffff};
    ch -= 0x20;
    if (ch > '~' - 0x20)
        return false;
    return (body[ch >> 5] & 1 << (ch & 0x1f)) != 0;
}
