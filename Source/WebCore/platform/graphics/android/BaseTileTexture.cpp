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

#include "config.h"
#include "BaseTileTexture.h"

#include "BaseTile.h"
#include "ClassTracker.h"
#include "DeleteTextureOperation.h"
#include "GLUtils.h"
#include "TilesManager.h"

#include <cutils/log.h>
#include <wtf/text/CString.h>

#undef XLOGC
#define XLOGC(...) android_printLog(ANDROID_LOG_DEBUG, "BaseTileTexture", __VA_ARGS__)

#ifdef DEBUG

#undef XLOG
#define XLOG(...) android_printLog(ANDROID_LOG_DEBUG, "BaseTileTexture", __VA_ARGS__)

#else

#undef XLOG
#define XLOG(...)

#endif // DEBUG

namespace WebCore {

BaseTileTexture::BaseTileTexture(uint32_t w, uint32_t h)
    : m_owner(0)
{
    m_size.set(w, h);
    m_ownTextureId = 0;

    // Make sure they are created on the UI thread.
    TilesManager::instance()->transferQueue()->initSharedSurfaceTextures(w, h);

#ifdef DEBUG_COUNT
    ClassTracker::instance()->increment("BaseTileTexture");
#endif
}

BaseTileTexture::~BaseTileTexture()
{
#ifdef DEBUG_COUNT
    ClassTracker::instance()->decrement("BaseTileTexture");
#endif
}

void BaseTileTexture::requireGLTexture()
{
    if (!m_ownTextureId)
        m_ownTextureId = GLUtils::createBaseTileGLTexture(m_size.width(), m_size.height());
}

void BaseTileTexture::discardGLTexture()
{
    if (m_ownTextureId)
        GLUtils::deleteTexture(&m_ownTextureId);

    if (m_owner) {
        // clear both Tile->Texture and Texture->Tile links
        m_owner->removeTexture(this);
        release(m_owner);
    }
}

bool BaseTileTexture::acquire(TextureOwner* owner, bool force)
{
    if (m_owner == owner)
        return true;

    return setOwner(owner, force);
}

bool BaseTileTexture::setOwner(TextureOwner* owner, bool force)
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

bool BaseTileTexture::release(TextureOwner* owner)
{
    XLOG("texture %p releasing tile %p, m_owner %p", this, owner, m_owner);
    if (m_owner != owner)
        return false;

    m_owner = 0;
    return true;
}

float BaseTileTexture::scale()
{
    TextureTileInfo* textureInfo = &m_ownTextureTileInfo;
    return textureInfo->m_scale;
}

// This function + TilesManager::addItemInTransferQueue() is replacing the
// setTile().
void BaseTileTexture::setOwnTextureTileInfoFromQueue(const TextureTileInfo* info)
{
    m_ownTextureTileInfo.m_x = info->m_x;
    m_ownTextureTileInfo.m_y = info->m_y;
    m_ownTextureTileInfo.m_scale = info->m_scale;
    m_ownTextureTileInfo.m_painter = info->m_painter;
    m_ownTextureTileInfo.m_picture = info->m_picture;
    m_ownTextureTileInfo.m_inverted = TilesManager::instance()->invertedScreen();
    if (m_owner) {
        BaseTile* owner = static_cast<BaseTile*>(m_owner);
        owner->backTextureTransfer();
    }

}

bool BaseTileTexture::readyFor(BaseTile* baseTile)
{
    const TextureTileInfo* info = &m_ownTextureTileInfo;
    if (info &&
        (info->m_x == baseTile->x()) &&
        (info->m_y == baseTile->y()) &&
        (info->m_scale == baseTile->scale()) &&
        (info->m_painter == baseTile->painter()) &&
        (info->m_inverted == TilesManager::instance()->invertedScreen()))
        return true;

    XLOG("texture %p readyFor return false for tile x, y (%d %d) texId %d ,"
         " BaseTileTexture %p, BaseTile is %p, SCALE %f, painter %p, inv %d",
         this, baseTile->x(), baseTile->y(), m_ownTextureId, this, baseTile,
         baseTile->scale(), baseTile->painter(), TilesManager::instance()->invertedScreen());
    return false;
}

} // namespace WebCore
