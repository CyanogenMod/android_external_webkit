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

#ifndef TilesManager_h
#define TilesManager_h

#if USE(ACCELERATED_COMPOSITING)

#include "LayerAndroid.h"
#include "ShaderProgram.h"
#include "TexturesGenerator.h"
#include "TilesProfiler.h"
#include "VideoLayerManager.h"
#include <utils/threads.h>
#include <wtf/HashMap.h>

namespace WebCore {

class OperationFilter;
class Tile;
class TileTexture;
class TransferQueue;

class TilesManager {
public:
    // May only be called from the UI thread
    static TilesManager* instance();

    static bool hardwareAccelerationEnabled()
    {
        return gInstance != 0;
    }

    void removeOperationsForFilter(OperationFilter* filter)
    {
        m_pixmapsGenerationThread->removeOperationsForFilter(filter);
    }

    bool tryUpdateOperationWithPainter(Tile* tile, TilePainter* painter)
    {
        return m_pixmapsGenerationThread->tryUpdateOperationWithPainter(tile, painter);
    }

    void scheduleOperation(QueuedOperation* operation)
    {
        m_pixmapsGenerationThread->scheduleOperation(operation);
    }

    ShaderProgram* shader() { return &m_shader; }
    TransferQueue* transferQueue();
    VideoLayerManager* videoLayerManager() { return &m_videoLayerManager; }

    void updateTilesIfContextVerified();
    void cleanupGLResources();

    void gatherTextures();
    bool layerTexturesRemain() { return m_layerTexturesRemain; }
    void gatherTexturesNumbers(int* nbTextures, int* nbAllocatedTextures,
                               int* nbLayerTextures, int* nbAllocatedLayerTextures);

    TileTexture* getAvailableTexture(Tile* owner);

    void dirtyAllTiles();

    void printTextures();

    // m_highEndGfx is written/read only on UI thread, no need for a lock.
    void setHighEndGfx(bool highEnd);
    bool highEndGfx();

    int currentTextureCount();
    int currentLayerTextureCount();
    void setCurrentTextureCount(int newTextureCount);
    void setCurrentLayerTextureCount(int newTextureCount);
    static int tileWidth();
    static int tileHeight();

    void allocateTextures();

    // remove all tiles from textures (and optionally deallocate gl memory)
    void discardTextures(bool allTextures, bool glTextures);

    bool getShowVisualIndicator()
    {
        return m_showVisualIndicator;
    }

    void setShowVisualIndicator(bool showVisualIndicator)
    {
        m_showVisualIndicator = showVisualIndicator;
    }

    TilesProfiler* getProfiler()
    {
        return &m_profiler;
    }

    bool invertedScreen()
    {
        return m_invertedScreen;
    }

    void setInvertedScreen(bool invert)
    {
        m_invertedScreen = invert;
    }

    void setInvertedScreenContrast(float contrast)
    {
        m_shader.setContrast(contrast);
    }

    void setUseMinimalMemory(bool useMinimalMemory)
    {
        m_useMinimalMemory = useMinimalMemory;
    }

    bool useMinimalMemory()
    {
        return m_useMinimalMemory;
    }

    void setUseDoubleBuffering(bool useDoubleBuffering)
    {
        m_useDoubleBuffering = useDoubleBuffering;
    }
    bool useDoubleBuffering() { return m_useDoubleBuffering; }


    unsigned int incWebkitContentUpdates() { return m_webkitContentUpdates++; }

    void incContentUpdates() { m_contentUpdates++; }
    unsigned int getContentUpdates() { return m_contentUpdates; }
    void clearContentUpdates() { m_contentUpdates = 0; }

    void incDrawGLCount()
    {
        m_drawGLCount++;
    }

    unsigned long long getDrawGLCount()
    {
        return m_drawGLCount;
    }

private:
    TilesManager();

    void discardTexturesVector(unsigned long long sparedDrawCount,
                               WTF::Vector<TileTexture*>& textures,
                               bool deallocateGLTextures);
    void dirtyTexturesVector(WTF::Vector<TileTexture*>& textures);
    void markAllGLTexturesZero();
    int getMaxTextureAllocation();

    WTF::Vector<TileTexture*> m_textures;
    WTF::Vector<TileTexture*> m_availableTextures;

    WTF::Vector<TileTexture*> m_tilesTextures;
    WTF::Vector<TileTexture*> m_availableTilesTextures;
    bool m_layerTexturesRemain;

    bool m_highEndGfx;
    int m_currentTextureCount;
    int m_currentLayerTextureCount;
    int m_maxTextureAllocation;

    bool m_generatorReady;

    bool m_showVisualIndicator;
    bool m_invertedScreen;

    bool m_useMinimalMemory;

    bool m_useDoubleBuffering;
    unsigned int m_contentUpdates; // nr of successful tiled paints
    unsigned int m_webkitContentUpdates; // nr of paints from webkit

    sp<TexturesGenerator> m_pixmapsGenerationThread;

    android::Mutex m_texturesLock;

    static TilesManager* gInstance;

    ShaderProgram m_shader;
    TransferQueue* m_queue;

    VideoLayerManager m_videoLayerManager;

    TilesProfiler m_profiler;
    unsigned long long m_drawGLCount;
    double m_lastTimeLayersUsed;
    bool m_hasLayerTextures;

    EGLContext m_eglContext;
};

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
#endif // TilesManager_h
