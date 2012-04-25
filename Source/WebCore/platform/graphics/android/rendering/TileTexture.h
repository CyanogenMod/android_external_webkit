/*
 * Copyright 2010, The Android Open Source Project
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

#ifndef TileTexture_h
#define TileTexture_h

#include "Color.h"
#include "FloatRect.h"
#include "SkBitmap.h"
#include "SkRect.h"
#include "SkSize.h"
#include "TextureInfo.h"

#include <GLES2/gl2.h>

class SkCanvas;

namespace WebCore {

class TextureOwner;
class Tile;
class TransformationMatrix;

class TileTexture {
public:
    // This object is to be constructed on the consumer's thread and must have
    // a width and height greater than 0.
    TileTexture(uint32_t w, uint32_t h);
    virtual ~TileTexture();

    // allows consumer thread to assign ownership of the texture to the tile. It
    // returns false if ownership cannot be transferred because the tile is busy
    bool acquire(TextureOwner* owner);
    bool release(TextureOwner* owner);

    // set the texture owner if not busy. Return false if busy, true otherwise.
    bool setOwner(TextureOwner* owner);

    // private member accessor functions
    TextureOwner* owner() { return m_owner; } // only used by the consumer thread

    const SkSize& getSize() const { return m_size; }

    // OpenGL ID of backing texture, 0 when not allocated
    GLuint m_ownTextureId;
    // these are used for dynamically (de)allocating backing graphics memory
    void requireGLTexture();
    void discardGLTexture();

    void transferComplete();

    TextureInfo* getTextureInfo() { return &m_ownTextureInfo; }

    // Make sure the following pureColor getter/setter are only read/written
    // in UI thread. Therefore no need for a lock.
    void setPure(bool pure) { m_isPureColor = pure; }
    bool isPureColor() {return m_isPureColor; }
    void setPureColor(const Color& color) { m_pureColor = color; setPure(true); }
    Color pureColor() { return m_pureColor; }

    void drawGL(bool isLayer, const SkRect& rect, float opacity,
                const TransformationMatrix* transform, bool forceBlending, bool usePointSampling,
                const FloatRect& fillPortion);
private:
    TextureInfo m_ownTextureInfo;
    SkSize m_size;

    // Tile owning the texture, only modified by UI thread
    TextureOwner* m_owner;

    // When the whole tile is single color, skip the transfer queue and draw
    // it directly through shader.
    bool m_isPureColor;
    Color m_pureColor;
};

} // namespace WebCore

#endif // TileTexture_h
