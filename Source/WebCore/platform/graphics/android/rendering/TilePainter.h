/*
 * Copyright 2011, The Android Open Source Project
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

#ifndef TilePainter_h
#define TilePainter_h

#include "TransformationMatrix.h"
#include "SkRefCnt.h"

class SkCanvas;

namespace WebCore {

class Color;
class Tile;

class TilePainter : public SkRefCnt {
// TODO: investigate webkit threadsafe ref counting
public:
    virtual ~TilePainter() { }
    virtual bool paint(SkCanvas* canvas) = 0;
    virtual float opacity() { return 1.0; }
    enum SurfaceType { Painted, Image };
    virtual SurfaceType type() { return Painted; }
    virtual Color* background() { return 0; }
    virtual bool blitFromContents(Tile* tile) { return false; }

    unsigned int getUpdateCount() { return m_updateCount; }
    void setUpdateCount(unsigned int updateCount) { m_updateCount = updateCount; }

private:
    unsigned int m_updateCount;
};

}

#endif // TilePainter_h
