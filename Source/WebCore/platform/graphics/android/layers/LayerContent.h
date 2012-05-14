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

#ifndef LayerContent_h
#define LayerContent_h

#include "IntRect.h"
#include "SkRefCnt.h"
#include <utils/threads.h>

class SkCanvas;
class SkPicture;
class SkWStream;

namespace WebCore {

class PrerenderedInval;

class LayerContent : public SkRefCnt {
public:
    virtual int width() = 0;
    virtual int height() = 0;
    virtual bool isEmpty() { return !width() || !height(); }
    virtual void setCheckForOptimisations(bool check) = 0;
    virtual void checkForOptimisations() = 0;
    virtual bool hasText() = 0;
    virtual void draw(SkCanvas* canvas) = 0;
    virtual PrerenderedInval* prerenderForRect(const IntRect& dirty) { return 0; }
    virtual void clearPrerenders() { };

    virtual void serialize(SkWStream* stream) = 0;

protected:
    // used to prevent parallel draws, as both SkPicture and PictureSet don't support them
    android::Mutex m_drawLock;
};

} // WebCore

#endif // LayerContent_h
