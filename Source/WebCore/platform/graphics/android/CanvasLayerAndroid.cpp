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

#include "config.h"
#include "CanvasLayerAndroid.h"

#if USE(ACCELERATED_COMPOSITING)
#include "GLUtils.h"
#include "TilesManager.h"
#include "SkAltCanvas.h"
#include "CanvasLayerShader.h"
#include <wtf/CurrentTime.h>
#include <cutils/log.h>
#include <map>

namespace WebCore {

std::map<int, uint32_t> CanvasLayerAndroid::s_bitmap_map2;
std::map<uint32_t, GLuint> CanvasLayerAndroid::s_texture_map2;
std::map<uint32_t, int> CanvasLayerAndroid::s_width_map;
std::map<uint32_t, int> CanvasLayerAndroid::s_height_map;
bool CanvasLayerAndroid::s_shader_initialized = false;
int CanvasLayerAndroid::s_maxTextureSize = -1;
CanvasLayerShader CanvasLayerAndroid::s_shader;
std::map<int, std::vector<uint32_t> > CanvasLayerAndroid::s_canvas_textures;
std::vector<int> CanvasLayerAndroid::s_deleted_canvases;
std::map<uint32_t, std::vector<int> > CanvasLayerAndroid::s_texture_refs;
WTF::Mutex CanvasLayerAndroid::s_mutex;
std::map<uint32_t, int> CanvasLayerAndroid::s_texture_usage;
std::list<int> CanvasLayerAndroid::s_canvas_oom;
int CanvasLayerAndroid::s_maxTextureThreshold = 5;     //Aggressive collection of resources
std::map<int, IntSize> CanvasLayerAndroid::s_canvas_dimensions;

std::map<int, SkPicture> CanvasLayerAndroid::s_picture_map;
std::map<int, SkBitmap> CanvasLayerAndroid::s_bitmap_map;

CanvasLayerAndroid::CanvasLayerAndroid()
    : LayerAndroid((RenderLayer*)0)
    , m_canvas_id(-1)
    , m_gpuCanvasEnabled(false)
{
}

CanvasLayerAndroid::CanvasLayerAndroid(const CanvasLayerAndroid& layer)
    : LayerAndroid(layer)
    , m_canvas_id(layer.m_canvas_id)
    , m_gpuCanvasEnabled(layer.m_gpuCanvasEnabled)
{
}

void CanvasLayerAndroid::markGLAssetsForRemoval(int id)
{
    MutexLocker locker(s_mutex);
    s_deleted_canvases.push_back(id);
}

bool CanvasLayerAndroid::isCanvasOOM(int id)
{
    //Following code is commented out. It is legacy implementation not used currently.
    //MutexLocker locker(s_mutex);
    //return (std::find(s_canvas_oom.begin(), s_canvas_oom.end(), id) != s_canvas_oom.end());
    //if(std::find(s_canvas_oom.begin(), s_canvas_oom.end(), id) == s_canvas_oom.end())
    //    return false;
    //else
    return false;
}

void CanvasLayerAndroid::cleanupAssets()
{
    MutexLocker locker(s_mutex);

    for(int ii=0; ii<s_deleted_canvases.size(); ++ii)
    {
        int& canvas_id = s_deleted_canvases[ii];

        //Got canvas id -- delete skpicture and bitmap associated with this canvas
        std::map<int, SkPicture>::iterator pic_it = s_picture_map.find(canvas_id);
        std::map<int, SkBitmap>::iterator bmp_it = s_bitmap_map.find(canvas_id);
        std::map<int, IntSize>::iterator dim_it = s_canvas_dimensions.find(canvas_id);
        if(pic_it != s_picture_map.end())
        {
            s_picture_map.erase(canvas_id);
        }
        if(bmp_it != s_bitmap_map.end())
        {
            s_bitmap_map.erase(canvas_id);
        }
        if(dim_it != s_canvas_dimensions.end())
        {
            s_canvas_dimensions.erase(canvas_id);
        }

        //Get generation ids
        std::map<int, std::vector<uint32_t> >::iterator gen_it = s_canvas_textures.find(canvas_id);
        if(gen_it != s_canvas_textures.end())
        {
            std::vector<uint32_t>& genIDs = gen_it->second;

            for(int jj=0; jj<genIDs.size(); ++jj)
            {
                uint32_t& gid = genIDs[jj];

                std::map<uint32_t, std::vector<int> >::iterator canvas_list_it = s_texture_refs.find(gid);
                if(canvas_list_it != s_texture_refs.end())
                {
                    //Get the list of canvases referencing this tex
                    std::vector<int>& canvas_list = canvas_list_it->second;

                    std::vector<int>::iterator canvas_it = std::find(canvas_list.begin(), canvas_list.end(), canvas_id);
                    if(canvas_it != canvas_list.end())
                    {
                        if(canvas_list.size() == 1)
                        {
                            //Get gl texture
                            std::map<uint32_t, GLuint>::iterator tex_it = s_texture_map2.find(gid);
                            if(tex_it != s_texture_map2.end())
                            {
                                GLuint tex_id = tex_it->second;
                                glDeleteTextures(1, &tex_id);
                                s_texture_map2.erase(tex_it);

                                s_width_map.erase(gid);
                                s_height_map.erase(gid);

                                s_shader.cleanupData(tex_id);
                            }

                            canvas_list.clear();
                            s_texture_refs.erase(canvas_list_it);
                        }
                        else
                        {
                            //Erase the canvas from the list of referrers
                            canvas_list.erase(canvas_it);
                        }
                    }
                }

            }
            s_canvas_textures.erase(gen_it);
        }
    }

    s_deleted_canvases.clear();

    std::vector<uint32_t> deleteIDs;
    for(std::map<uint32_t, int>::iterator usage_it = s_texture_usage.begin();
            usage_it != s_texture_usage.end();
            ++usage_it)
    {
        uint32_t generationID = usage_it->first;
        int& value = usage_it->second;

        ++value;

        if(value > s_maxTextureThreshold)
        {
            deleteIDs.push_back(generationID);
        }
    }
    cleanupUnusedAssets(deleteIDs);
}

void CanvasLayerAndroid::cleanupUnusedAssets(std::vector<uint32_t>& deleteIds)
{
    for(int ii=0; ii<deleteIds.size(); ++ii)
    {
        uint32_t& id = deleteIds[ii];

        //Check if this canvas is the only one using this
        std::map<uint32_t, std::vector<int> >::iterator ref_it = s_texture_refs.find(id);
        if(ref_it == s_texture_refs.end())
            return;

        //Passes checks ... Delete assets
        //Get gl texture
        std::map<uint32_t, GLuint>::iterator tex_it = s_texture_map2.find(id);
        if(tex_it != s_texture_map2.end())
        {
            //Remove from s_texture_map2
            GLuint tex_id = tex_it->second;
            glDeleteTextures(1, &tex_id);
            s_texture_map2.erase(tex_it);

            //Remove from widht and height map
            s_width_map.erase(id);
            s_height_map.erase(id);

            //Cleanup associated data in shader
            s_shader.cleanupData(tex_id);

            //Remove the tex from the canvas_tex map
            for(std::map<int, std::vector<uint32_t> >::iterator canvas_tex_list = s_canvas_textures.begin();
            canvas_tex_list != s_canvas_textures.end(); ++canvas_tex_list)
            {
                std::vector<uint32_t>& tex_list = canvas_tex_list->second;
                std::vector<uint32_t>::iterator it2 = std::find(tex_list.begin(), tex_list.end(), id);

                if(it2 != tex_list.end())
                    tex_list.erase(it2);
            }

            //Remove from texture_refs
            s_texture_refs.erase(ref_it);

            //Remove from texture usage
            s_texture_usage.erase(id);
        }
    }
}

void CanvasLayerAndroid::setPicture(SkPicture& picture, IntSize& size)
{
    std::map<int, SkPicture>::iterator pic_it = s_picture_map.find(m_canvas_id);
    std::map<int, SkBitmap>::iterator bmp_it = s_bitmap_map.find(m_canvas_id);
    std::map<int, IntSize>::iterator dim_it = s_canvas_dimensions.find(m_canvas_id);
    if(pic_it == s_picture_map.end() || bmp_it == s_bitmap_map.end() || dim_it == s_canvas_dimensions.end())
    {
        //Create a picture
        SkPicture currentPicture;
        currentPicture.swap(picture);
        s_picture_map.insert(std::make_pair(m_canvas_id, currentPicture));

        SkBitmap bitmap;
        if(bitmap.width() != picture.width() || bitmap.height() != picture.height())
        {
            if(!(bitmap.isNull() || bitmap.empty()))
                bitmap.reset();
            bitmap.setConfig(SkBitmap::kARGB_8888_Config, picture.width(), picture.height());
            bitmap.allocPixels();
        }
        s_bitmap_map.insert(std::make_pair(m_canvas_id, bitmap));

        s_canvas_dimensions.insert(std::make_pair(m_canvas_id, size));

    }
    else
    {
        SkPicture& currentPicture = pic_it->second;
        currentPicture.swap(picture);

        SkBitmap& bitmap = bmp_it->second;
        if(bitmap.width() != currentPicture.width() || bitmap.height() != currentPicture.height())
        {
            if(!(bitmap.isNull() || bitmap.empty()))
                bitmap.reset();
            bitmap.setConfig(SkBitmap::kARGB_8888_Config, currentPicture.width(), currentPicture.height());
            bitmap.allocPixels();
        }

        IntSize& currentSize = dim_it->second;
        currentSize.setWidth(size.width());
        currentSize.setHeight(size.height());
    }
}

SkBitmap CanvasLayerAndroid::ScaleBitmap(SkBitmap src, float sx, float sy)
{
    int width = (int)round(src.width() * sx);
    int height = (int)round(src.height() * sy);

    SkBitmap dst;
    dst.setConfig(src.config(), width, height);
    dst.allocPixels();
    dst.eraseColor(0);

    SkCanvas canvas(dst);
    canvas.scale(sx, sy);
    SkPaint paint;
    paint.setFilterBitmap(true);
    SkScalar tmp = SkIntToScalar(0);
    canvas.drawBitmap(src, tmp , tmp, &paint);

    return dst;
}

bool CanvasLayerAndroid::drawGL(bool layerTilesDisabled, TransformationMatrix& drawTransform)
{
    m_drawTransform = drawTransform;
    std::vector<uint32_t> generationIDs;
    std::vector<uint32_t> generationIDsUsed;

    //Will need to lock only if we use isCanvasOOM which we don't currently
    std::map<int, SkPicture>::iterator pic_it = s_picture_map.find(m_canvas_id);
    std::map<int, SkBitmap>::iterator bmp_it = s_bitmap_map.find(m_canvas_id);
    std::map<int, IntSize>::iterator dim_it = s_canvas_dimensions.find(m_canvas_id);
    if(pic_it == s_picture_map.end() || bmp_it == s_bitmap_map.end() || dim_it == s_canvas_dimensions.end())
        return true;

    SkBitmap& currentBitmap = bmp_it->second;
    SkPicture& currentPicture = pic_it->second;
    IntSize& currentSize = dim_it->second;

    SkAltCanvas canvas(currentBitmap);
    if(currentBitmap.isNull() || currentBitmap.empty())
        return true;

    //if(m_canvas_id >= 0 && m_canvas != NULL)
    {
        currentPicture.drawAltCanvas(&canvas);
        int numBitmaps = canvas.getNumBitmaps();
        int numPrimitives = canvas.getNumPrimitives();
        int bitmap_height, bitmap_width;

        //Clear content before new run
        s_bitmap_map2.clear();

        //Time to transfer setup from the TilesManager to the custom shader
        //CanvasLayerShader shader;
        if(!s_shader_initialized)
        {
            glGetIntegerv(GL_MAX_TEXTURE_SIZE, &s_maxTextureSize);
            s_shader.initialize();
            s_shader_initialized = true;
        }

        for(int jj=0; jj<numBitmaps; ++jj)
        {
            SkBitmap* bmp = canvas.getBitmap(jj);
            bitmap_height = bmp->height();
            bitmap_width = bmp->width();

            uint32_t generationID = bmp->getGenerationID();

            generationIDsUsed.push_back(generationID);

            std::map<uint32_t, int>::iterator tmp_it = s_texture_usage.find(generationID);
            if(tmp_it == s_texture_usage.end())
                s_texture_usage.insert(std::make_pair(generationID, 0));

            GLuint texture;
            std::map<uint32_t, GLuint>::iterator it = s_texture_map2.find(generationID);
            if(it != s_texture_map2.end())
            {
                texture = it->second;

                //Find the bitmap and add the canvas to the generationID
                std::map<uint32_t, std::vector<int> >::iterator ref_it = s_texture_refs.find(generationID);
                if(ref_it != s_texture_refs.end())
                {
                    std::vector<int>& canvas_list = ref_it->second;
                    if(std::find(canvas_list.begin(), canvas_list.end(), m_canvas_id) == canvas_list.end())
                    {
                        canvas_list.push_back(m_canvas_id);
                    }
                }
            }else
            {
                float scale = 1.0f;
                if(bitmap_height > s_maxTextureSize)
                {
                    float tmp_scale = (float)s_maxTextureSize/(float)bitmap_height;
                    if(tmp_scale < scale)
                    {
                        scale = tmp_scale;
                    }
                }

                if(bitmap_width > s_maxTextureSize)
                {
                    float tmp_scale = (float)s_maxTextureSize/(float)bitmap_width;
                    if(tmp_scale < scale)
                        scale = tmp_scale;
                }

                SkBitmap dst = ScaleBitmap(*bmp, scale, scale);

                glGenTextures(1, &texture);
                bool val = GLUtils::createTextureWithBitmapFailSafe(texture, dst);
                //Do not draw if encounter GL error
                if(!val)
                {
                    s_canvas_oom.push_back(m_canvas_id);
                    return true;
                }

                //Store for future runs
                s_texture_map2.insert(std::make_pair(generationID, texture));
                s_width_map.insert(std::make_pair(generationID, bitmap_width));
                s_height_map.insert(std::make_pair(generationID, bitmap_height));

                //Store for asset management
                generationIDs.push_back(generationID);
                std::vector<int> canvas_list;
                canvas_list.push_back(m_canvas_id);
                s_texture_refs.insert(std::make_pair(generationID, canvas_list));

            }

            s_bitmap_map2.insert(std::make_pair(jj, generationID));
        }

        glUseProgram(s_shader.getProgram());
        glUniform1i(s_shader.getSampler(), 0);
        s_shader.setTitleBarHeight(TilesManager::instance()->shader()->getTitleBarHeight());

        s_shader.setContentViewport(TilesManager::instance()->shader()->getContentViewport());
        s_shader.setSurfaceProjectionMatrix(TilesManager::instance()->shader()->getSurfaceProjectionMatrix());
        s_shader.setClipProjectionMatrix(TilesManager::instance()->shader()->getClipProjectionMatrix());
        s_shader.setVisibleContentRectProjectionMatrix(TilesManager::instance()->shader()->getVisibleContentRectProjectionMatrix());

        FloatRect clippingRect = TilesManager::instance()->shader()->rectInInvViewCoord(m_drawTransform, currentSize);
        TilesManager::instance()->shader()->clip(clippingRect);

        std::map<int, std::vector<SkRect> > primitives_map;
        std::map<int, std::vector<FloatRect> > primTexCoord_map;
        std::map<int, std::vector<int> > primScaleX_map;
        std::map<int, std::vector<int> > primScaleY_map;
        std::map<int, std::vector<SkMatrix> > primMatrix_map;

        int texture_id;
        for(int ii=0; ii<numPrimitives; ++ii)
        {
            SkRect& rect = canvas.getPrimitive(ii);
            SkIRect& tex_rect = canvas.getPrimitiveTexCoord(ii);
            int& _scaleX = canvas.getScaleX(ii);
            int& _scaleY = canvas.getScaleY(ii);
            SkMatrix& matrix = canvas.getMatrix(ii);

            int& bm = canvas.getPrimitiveBmMap(ii);

            std::map<int, uint32_t>::iterator it_bm = s_bitmap_map2.find(bm);
            if(it_bm != s_bitmap_map2.end())
            {
                uint32_t generationID = it_bm->second;  //Get the generation id from the map

                std::map<uint32_t, GLuint>::iterator it_tm = s_texture_map2.find(generationID);
                std::map<uint32_t, int>::iterator it_wm = s_width_map.find(generationID);
                std::map<uint32_t, int>::iterator it_hm = s_height_map.find(generationID);

                if(it_tm != s_texture_map2.end())
                {
                    texture_id = it_tm->second;

                    std::map<int, std::vector<SkRect> >::iterator pr_it = primitives_map.find(texture_id);
                    std::map<int, std::vector<FloatRect> >::iterator pr_tx_it = primTexCoord_map.find(texture_id);
                    std::map<int, std::vector<int> >::iterator prScX_it = primScaleX_map.find(texture_id);
                    std::map<int, std::vector<int> >::iterator prScY_it = primScaleY_map.find(texture_id);
                    std::map<int, std::vector<SkMatrix> >::iterator prMat_it = primMatrix_map.find(texture_id);

                    if(pr_it != primitives_map.end() &&
                        pr_tx_it != primTexCoord_map.end() &&
                        prScX_it != primScaleX_map.end() &&
                        prScY_it != primScaleY_map.end() &&
                        prMat_it != primMatrix_map.end())
                    {
                        std::vector<SkRect>& primitives = pr_it->second;
                        std::vector<FloatRect>& primTexCoord = pr_tx_it->second;
                        std::vector<int>& primScaleX = prScX_it->second;
                        std::vector<int>& primScaleY = prScY_it->second;
                        std::vector<SkMatrix>& primMatrix = prMat_it->second;

                        SkRect temp;
                        temp.set(rect.fLeft, rect.fTop, rect.fRight, rect.fBottom);
                        primitives.push_back(temp);

                        int width = tex_rect.fRight - tex_rect.fLeft;
                        int height = tex_rect.fBottom - tex_rect.fTop;

                        //TODO::Defensive programming needed here
                        //DO not assume this will succeed
                        int bitmap_width_m = it_wm->second;
                        int bitmap_height_m = it_hm->second;

                        float scaling_width = (float) width/ (float) bitmap_width_m;
                        float scaling_height = (float) height/ (float) bitmap_height_m;

                        FloatRect textemp((float)tex_rect.fLeft/(float)bitmap_width_m,
                                    (float)tex_rect.fTop/(float)bitmap_height_m,
                                    scaling_width,
                                    scaling_height);
                        primTexCoord.push_back(textemp);

                        primScaleX.push_back(_scaleX);
                        primScaleY.push_back(_scaleY);
                        primMatrix.push_back(matrix);
                    }
                    else
                    {
                        std::vector<SkRect> primitives;
                        std::vector<FloatRect> primTexCoord;
                        std::vector<int> primScaleX;
                        std::vector<int> primScaleY;
                        std::vector<SkMatrix> primMatrix;

                        SkRect temp;
                        temp.set(rect.fLeft, rect.fTop, rect.fRight, rect.fBottom);
                        primitives.push_back(temp);

                        int width = tex_rect.fRight - tex_rect.fLeft;
                        int height = tex_rect.fBottom - tex_rect.fTop;

                        //TODO::Defensive programming needed here
                        //DO not assume this will succeed
                        int bitmap_width_m = it_wm->second;
                        int bitmap_height_m = it_hm->second;

                        float scaling_width = (float) width/ (float) bitmap_width_m;
                        float scaling_height = (float) height/ (float) bitmap_height_m;

                        FloatRect textemp((float)tex_rect.fLeft/(float)bitmap_width_m,
                                    (float)tex_rect.fTop/(float)bitmap_height_m,
                                    scaling_width,
                                    scaling_height);
                        primTexCoord.push_back(textemp);

                        primScaleX.push_back(_scaleX);
                        primScaleY.push_back(_scaleY);
                        primMatrix.push_back(matrix);

                        //Add it to map
                        primitives_map.insert(std::make_pair(texture_id, primitives));
                        primTexCoord_map.insert(std::make_pair(texture_id, primTexCoord));
                        primScaleX_map.insert(std::make_pair(texture_id, primScaleX));
                        primScaleY_map.insert(std::make_pair(texture_id, primScaleY));
                        primMatrix_map.insert(std::make_pair(texture_id, primMatrix));
                    }
                }
            }
        }

        for(int jj=0; jj<numBitmaps; ++jj)
        {
            SkBitmap* bmp = canvas.getBitmap(jj);
            uint32_t generationID = bmp->getGenerationID();

            GLuint texture;
            std::map<uint32_t, GLuint>::iterator it = s_texture_map2.find(generationID);
            if(it != s_texture_map2.end())
            {
                texture = it->second;

                std::map<int, std::vector<SkRect> >::iterator pr_it = primitives_map.find(texture);
                std::map<int, std::vector<FloatRect> >::iterator pr_tx_it = primTexCoord_map.find(texture);
                std::map<int, std::vector<int> >::iterator prScX_it = primScaleX_map.find(texture);
                std::map<int, std::vector<int> >::iterator prScY_it = primScaleY_map.find(texture);
                std::map<int, std::vector<SkMatrix> >::iterator prMat_it = primMatrix_map.find(texture);

                if(pr_it != primitives_map.end() &&
                    pr_tx_it != primTexCoord_map.end() &&
                    prScX_it != primScaleX_map.end() &&
                    prScY_it != primScaleY_map.end() &&
                    prMat_it != primMatrix_map.end())
                {
                    std::vector<SkRect>& primitives = pr_it->second;
                    std::vector<FloatRect>& primTexCoord = pr_tx_it->second;
                    std::vector<int>& primScaleX = prScX_it->second;
                    std::vector<int>& primScaleY = prScY_it->second;
                    std::vector<SkMatrix>& primMatrix = prMat_it->second;

                    if(primitives.size() > 0)
                    {
                        bool drawVal = s_shader.drawPrimitives(primitives, primTexCoord, primScaleX, primScaleY, primMatrix, texture, m_drawTransform, 1.0f);

                        if(!drawVal)
                        {
                            s_canvas_oom.push_back(m_canvas_id);
                            return true;
                        }
                    }
                }
            }
        }
    }

    //Need to reset it back to the original shader
    //Reset cached states so that next draw call indexes into the array of programs
    TilesManager::instance()->shader()->resetCachedStates();

    //Check generationIDs against list
    if(!generationIDs.empty())
    {
        std::map<int, std::vector<uint32_t> >::iterator gen_it = s_canvas_textures.find(m_canvas_id);
        if(gen_it != s_canvas_textures.end())
        {
            std::vector<uint32_t>& genIDs = gen_it->second;
            for(int ii=0; ii<generationIDs.size(); ++ii)
            {
                if(std::find(genIDs.begin(), genIDs.end(), generationIDs[ii]) == genIDs.end())
                {
                    genIDs.push_back(generationIDs[ii]);
                }
            }
        }
        else
        {
            std::vector<uint32_t> genIDs;
            for(int ii=0; ii<generationIDs.size(); ++ii)
            {
                if(std::find(genIDs.begin(), genIDs.end(), generationIDs[ii]) == genIDs.end())
                {
                    genIDs.push_back(generationIDs[ii]);
                }
            }
            s_canvas_textures.insert(std::make_pair(m_canvas_id, genIDs));
        }
    }

    //Keep track of usage and track bitmaps that need to be deleted
    for(std::map<uint32_t, int>::iterator usage_it = s_texture_usage.begin();
            usage_it != s_texture_usage.end();
            ++usage_it)
    {
        uint32_t generationID = usage_it->first;
        int& value = usage_it->second;

        //Check if this tex belongs to this canvas
        std::map<uint32_t, std::vector<int> >::iterator canvas_it = s_texture_refs.find(generationID);
        if(canvas_it != s_texture_refs.end())
        {
            std::vector<int>& canvas_list = canvas_it->second;
            if(std::find(canvas_list.begin(), canvas_list.end(), m_canvas_id) == canvas_list.end())
                continue;

            if(std::find(generationIDsUsed.begin(), generationIDsUsed.end(), generationID) != generationIDsUsed.end())
            {
                value = 0;
                --value;
            }
        }
    }

    return true;
}

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
