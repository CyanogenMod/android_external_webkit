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

#define LOG_TAG "GraphicsOperationCollection"
#define LOG_NDEBUG 1

#include "config.h"
#include "GraphicsOperationCollection.h"

#include "AndroidLog.h"
#include "GraphicsContext.h"
#include "GraphicsOperation.h"
#include "PlatformGraphicsContext.h"
#include "PlatformGraphicsContextRecording.h"

namespace WebCore {

GraphicsOperationCollection::GraphicsOperationCollection()
{
}

GraphicsOperationCollection::~GraphicsOperationCollection()
{
    clear();
}

void GraphicsOperationCollection::apply(PlatformGraphicsContext* context) const
{
    size_t size = m_operations.size();
    for (size_t i = 0; i < size; i++)
        if (!m_operations[i]->apply(context))
            return;
}

void GraphicsOperationCollection::adoptAndAppend(GraphicsOperation::Operation* operation)
{
    m_operations.append(operation);
}

void GraphicsOperationCollection::transferFrom(GraphicsOperationCollection& moveFrom)
{
    size_t size = moveFrom.m_operations.size();
    m_operations.reserveCapacity(m_operations.size() + size);
    for (size_t i = 0; i < size; i++)
        m_operations.append(moveFrom.m_operations[i]);
    moveFrom.m_operations.clear();
}

bool GraphicsOperationCollection::isEmpty()
{
    return !m_operations.size();
}

void GraphicsOperationCollection::clear()
{
    size_t size = m_operations.size();
    for (size_t i = 0; i < size; i++)
        delete m_operations[i];
    m_operations.clear();
}

} // namespace WebCore
