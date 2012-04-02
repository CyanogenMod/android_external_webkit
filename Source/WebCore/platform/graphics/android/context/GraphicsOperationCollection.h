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

#ifndef GraphicsOperationCollection_h
#define GraphicsOperationCollection_h

#if USE(ACCELERATED_COMPOSITING)

#include "Color.h"
#include "GraphicsOperation.h"
#include "IntRect.h"
#include "SkRefCnt.h"

namespace WebCore {

class PlatformGraphicsContext;

class GraphicsOperationCollection : public SkRefCnt {
public:
    GraphicsOperationCollection(const IntRect& drawArea);
    ~GraphicsOperationCollection();

    void apply(PlatformGraphicsContext* context);
    void append(GraphicsOperation::Operation* operation);

    bool isEmpty();

private:
    IntRect m_drawArea;
    Vector<GraphicsOperation::Operation*> m_operations;
};

class AutoGraphicsOperationCollection {
public:
   AutoGraphicsOperationCollection(const IntRect& area);
   ~AutoGraphicsOperationCollection();
   GraphicsContext* context() { return m_graphicsContext; }
   GraphicsOperationCollection* picture() { return m_graphicsOperationCollection; }

private:
   GraphicsOperationCollection* m_graphicsOperationCollection;
   PlatformGraphicsContext* m_platformGraphicsContext;
   GraphicsContext* m_graphicsContext;
};

}

#endif // USE(ACCELERATED_COMPOSITING)

#endif // GraphicsOperationCollection_h
