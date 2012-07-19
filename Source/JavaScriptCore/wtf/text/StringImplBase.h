/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef StringImplBase_h
#define StringImplBase_h

#include <wtf/unicode/Unicode.h>

namespace WTF {

class StringImplBase {
    WTF_MAKE_NONCOPYABLE(StringImplBase); WTF_MAKE_FAST_ALLOCATED;
public:
    bool isStringImpl() { return !(m_static && m_shouldReportCost); }
    unsigned length() const { return m_length; }
    void ref() { ++m_refCount; }

protected:
    enum BufferOwnership {
        BufferInternal,
        BufferOwned,
        BufferSubstring,
        BufferShared,
    };

    // For SmallStringStorage, which allocates an array and uses an in-place new.
    StringImplBase() { }

    StringImplBase(unsigned length, BufferOwnership ownership)
        : m_lower(false)
        , m_hasTerminatingNullCharacter(false)
        , m_atomic(false)
        , m_static(false)
        , m_shouldReportCost(true)
        , m_identifier(false)
        , m_bufferOwnership(ownership)
        , m_refCount(1)
        , m_length(length)
    {
        ASSERT(isStringImpl());
    }

    enum StaticStringConstructType { ConstructStaticString };
    StringImplBase(unsigned length, StaticStringConstructType)
        : m_lower(false)
        , m_hasTerminatingNullCharacter(false)
        , m_atomic(false)
        , m_static(true)
        , m_shouldReportCost(false)
        , m_identifier(true)
        , m_bufferOwnership(BufferOwned)
        , m_refCount(0)
        , m_length(length)
    {
        ASSERT(isStringImpl());
    }

    // This constructor is not used when creating StringImpl objects,
    // and sets the flags into a state marking the object as such.
    enum NonStringImplConstructType { ConstructNonStringImpl };
    StringImplBase(NonStringImplConstructType)
        : m_lower(false)
        , m_hasTerminatingNullCharacter(false)
        , m_atomic(false)
        , m_static(true)
        , m_shouldReportCost(true)
        , m_identifier(false)
        , m_bufferOwnership(0)
        , m_refCount(1)
        , m_length(0)
    {
        ASSERT(!isStringImpl());
    }

    bool m_lower : 1;
    bool m_hasTerminatingNullCharacter : 1;
    bool m_atomic : 1;
    bool m_static : 1;
    bool m_shouldReportCost  : 1;
    bool m_identifier : 1;
    unsigned m_bufferOwnership : 2;
    unsigned m_refCount : 24;

    unsigned m_length;
};

} // namespace WTF

using WTF::StringImplBase;

#endif
