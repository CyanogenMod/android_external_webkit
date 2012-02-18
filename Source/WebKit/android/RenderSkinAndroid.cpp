/*
 * Copyright 2006, The Android Open Source Project
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

#define LOG_TAG "WebCore"

#include "config.h"
#include "RenderSkinAndroid.h"
#include "RenderSkinMediaButton.h"
#include "SkImageDecoder.h"

#include <androidfw/AssetManager.h>
#include <androidfw/Asset.h>

namespace WebCore {

String RenderSkinAndroid::s_drawableDirectory = "";
RenderSkinAndroid::Resolution RenderSkinAndroid::s_drawableResolution = RenderSkinAndroid::MedRes;

RenderSkinAndroid::~RenderSkinAndroid()
{
}
RenderSkinAndroid::RenderSkinAndroid(String drawableDirectory)
{
    if (s_drawableDirectory.isEmpty() && !drawableDirectory.isEmpty()) {
        s_drawableResolution = MedRes;
        if (drawableDirectory.endsWith("-hdpi/"))
            s_drawableResolution = HighRes;
        else if (drawableDirectory.endsWith("-xhdpi/"))
            s_drawableResolution = ExtraHighRes;

        s_drawableDirectory = drawableDirectory;
    }
}

bool RenderSkinAndroid::DecodeBitmap(android::AssetManager* am, const char* fileName, SkBitmap* bitmap)
{
    android::Asset* asset = am->open(fileName, android::Asset::ACCESS_BUFFER);
    if (!asset) {
        asset = am->openNonAsset(fileName, android::Asset::ACCESS_BUFFER);
        if (!asset) {
            ALOGD("RenderSkinAndroid: File \"%s\" not found.\n", fileName);
            return false;
        }
    }
    
    bool success = SkImageDecoder::DecodeMemory(asset->getBuffer(false), asset->getLength(), bitmap);
    if (!success) {
        ALOGD("RenderSkinAndroid: Failed to decode %s\n", fileName);
    }

    delete asset;
    return success;
}

} // namespace WebCore
