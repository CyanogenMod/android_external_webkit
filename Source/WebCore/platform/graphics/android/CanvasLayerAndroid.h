/*
Copyright (c) 2013, The Linux Foundation. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above
      copyright notice, this list of conditions and the following
      disclaimer in the documentation and/or other materials provided
      with the distribution.
    * Neither the name of The Linux Foundation nor the names of its
      contributors may be used to endorse or promote products derived
      from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef CanvasLayerAndroid_h
#define CanvasLayerAndroid_h

#if USE(ACCELERATED_COMPOSITING)

#include "LayerAndroid.h"
#include "SkPicture.h"
#include "CanvasLayerShader.h"
#include <map>
#include <list>

class SkAltCanvas;

namespace WebCore {

class CanvasLayerAndroid : public LayerAndroid {

public:
    CanvasLayerAndroid();
    explicit CanvasLayerAndroid(const CanvasLayerAndroid& layer);

    virtual LayerAndroid* copy() const { return new CanvasLayerAndroid(*this); }
    virtual bool isVideo() const { return false; }
    virtual bool isCanvas() const { return true; }
    virtual bool needsTexture() const { return false; }
    virtual void paintBitmapGL() {}

    virtual bool drawGL(bool layerTilesDisabled, TransformationMatrix& drawTransform);

    void setPicture(SkPicture& picture, IntSize& size);

    void setCanvasID(int& id)   {   m_canvas_id = id;   }
    int getCanvasID()   {   return m_canvas_id; }
    void setGpuCanvasStatus(bool val)  {    m_gpuCanvasEnabled = val;   }
    bool isGpuCanvasEnabled()          {    bool val = m_gpuCanvasEnabled;  return val;}

    static void markGLAssetsForRemoval(int id);
    static void cleanupAssets();
    static void cleanupUnusedAssets(std::vector<uint32_t>& deleteIds);
    static bool isCanvasOOM(int id);

protected:
    SkBitmap ScaleBitmap(SkBitmap src, float sx, float sy);

private:
    int m_width, m_height;
    int m_canvas_id;
    bool m_gpuCanvasEnabled;
    bool m_oomStatus;
    WTF::Mutex m_mutex;

    static WTF::Mutex s_mutex;

    static bool s_shader_initialized;
    static CanvasLayerShader s_shader;

    static std::map<int, uint32_t> s_bitmap_map2;
    static std::map<uint32_t, GLuint> s_texture_map2;
    static std::map<uint32_t, int> s_width_map;
    static std::map<uint32_t, int> s_height_map;

    static std::map<int, SkPicture> s_picture_map;
    static std::map<int, SkBitmap> s_bitmap_map;

    static int s_maxTextureSize;

    //Texture Management
    static int s_maxTextureThreshold;
    static std::map<int, std::vector<uint32_t> > s_canvas_textures;
    static std::vector<int> s_deleted_canvases;
    static std::map<uint32_t, std::vector<int> > s_texture_refs;
    static std::map<uint32_t, int> s_texture_usage;

    //OOM Management
    static std::list<int> s_canvas_oom;

    static std::map<int, IntSize> s_canvas_dimensions;
};  // class CanvasLayerAndroid

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)

#endif // CanvasLayerAndroid_h
