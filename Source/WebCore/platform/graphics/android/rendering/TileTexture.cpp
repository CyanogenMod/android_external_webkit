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

#define LOG_TAG "TileTexture"
#define LOG_NDEBUG 1

#include "config.h"
#include "TileTexture.h"

#include "AndroidLog.h"
#include "Tile.h"
#include "ClassTracker.h"
#include "DrawQuadData.h"
#include "GLUtils.h"
#include "GLWebViewState.h"
#include "TextureOwner.h"
#include "TilesManager.h"

namespace WebCore {

TileTexture::TileTexture(uint32_t w, uint32_t h)
    : m_owner(0)
    , m_isPureColor(false)
{
    m_size.set(w, h);
    m_ownTextureId = 0;

#ifdef DEBUG_COUNT
    ClassTracker::instance()->increment("TileTexture");
#endif
}

TileTexture::~TileTexture()
{
#ifdef DEBUG_COUNT
    ClassTracker::instance()->decrement("TileTexture");
#endif
}

void TileTexture::requireGLTexture()
{
    if (!m_ownTextureId)
        m_ownTextureId = GLUtils::createTileGLTexture(m_size.width(), m_size.height());
}

void TileTexture::discardGLTexture()
{
    if (m_ownTextureId)
        GLUtils::deleteTexture(&m_ownTextureId);

    if (m_owner) {
        // clear both Tile->Texture and Texture->Tile links
        m_owner->removeTexture(this);
        release(m_owner);
    }
}

bool TileTexture::acquire(TextureOwner* owner)
{
    if (m_owner == owner)
        return true;

    return setOwner(owner);
}

bool TileTexture::setOwner(TextureOwner* owner)
{
    bool proceed = true;
    if (m_owner && m_owner != owner)
        proceed = m_owner->removeTexture(this);

    if (proceed) {
        m_owner = owner;
        return true;
    }

    return false;
}

bool TileTexture::release(TextureOwner* owner)
{
    ALOGV("texture %p releasing tile %p, m_owner %p", this, owner, m_owner);
    if (m_owner != owner)
        return false;

    m_owner = 0;
    return true;
}

void TileTexture::transferComplete()
{
    if (m_owner) {
        Tile* owner = static_cast<Tile*>(m_owner);
        owner->backTextureTransfer();
    } else
        ALOGE("ERROR: owner missing after transfer of texture %p", this);
}

void TileTexture::drawGL(bool isLayer, const SkRect& rect, float opacity,
                         const TransformationMatrix* transform,
                         bool forceBlending, bool usePointSampling,
                         const FloatRect& fillPortion)
{
    ShaderProgram* shader = TilesManager::instance()->shader();

    if (isLayer && !transform) {
        ALOGE("ERROR: Missing tranform for layers!");
        return;
    }

    // For base layer, we just follow the forceBlending, otherwise, blending is
    // always turned on.
    // TODO: Don't blend tiles if they are fully opaque.
    bool useBlending = forceBlending || isLayer;
    DrawQuadData commonData(isLayer ? LayerQuad : BaseQuad, transform, &rect,
                            opacity, useBlending, fillPortion);
    if (isPureColor()) {
        PureColorQuadData data(commonData, pureColor());
        shader->drawQuad(&data);
    } else {
        GLint filter = usePointSampling ? GL_NEAREST : GL_LINEAR;
        TextureQuadData data(commonData, m_ownTextureId, GL_TEXTURE_2D, filter);
        shader->drawQuad(&data);
    }
}

} // namespace WebCore
