/*
* Copyright (C) 2011 Google Inc. All rights reserved.
* Copyright (C) 2012, The Linux Foundation. All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are
* met:
*     * Redistributions of source code must retain the above copyright
*       notice, this list of conditions and the following disclaimer.
*     * Redistributions in binary form must reproduce the above
*       copyright notice, this list of conditions and the following
*       disclaimer in the documentation and/or other materials provided
*       with the distribution.
*     * Neither the name of The Linux Foundation nor the names of its
*       contributors may be used to endorse or promote products derived
*       from this software without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
* WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
* ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
* BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
* BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
* WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
* OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
* IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "config.h"

#include "WebAudioAssets.h"
#include "WebAudioLog.h"

#define IDR_AUDIO_SPATIALIZATION_T000_P000  0

static const char* gWebAudioResourceFiles[] =
{
    "webkit/webaudio/IRC_Composite_C_R0195_T000_P000.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T000_P015.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T000_P030.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T000_P045.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T000_P060.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T000_P075.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T000_P090.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T000_P315.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T000_P330.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T000_P345.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T015_P000.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T015_P015.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T015_P030.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T015_P045.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T015_P060.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T015_P075.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T015_P090.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T015_P315.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T015_P330.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T015_P345.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T030_P000.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T030_P015.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T030_P030.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T030_P045.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T030_P060.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T030_P075.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T030_P090.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T030_P315.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T030_P330.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T030_P345.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T045_P000.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T045_P015.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T045_P030.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T045_P045.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T045_P060.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T045_P075.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T045_P090.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T045_P315.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T045_P330.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T045_P345.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T060_P000.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T060_P015.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T060_P030.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T060_P045.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T060_P060.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T060_P075.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T060_P090.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T060_P315.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T060_P330.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T060_P345.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T075_P000.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T075_P015.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T075_P030.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T075_P045.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T075_P060.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T075_P075.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T075_P090.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T075_P315.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T075_P330.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T075_P345.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T090_P000.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T090_P015.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T090_P030.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T090_P045.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T090_P060.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T090_P075.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T090_P090.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T090_P315.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T090_P330.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T090_P345.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T105_P000.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T105_P015.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T105_P030.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T105_P045.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T105_P060.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T105_P075.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T105_P090.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T105_P315.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T105_P330.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T105_P345.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T120_P000.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T120_P015.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T120_P030.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T120_P045.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T120_P060.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T120_P075.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T120_P090.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T120_P315.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T120_P330.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T120_P345.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T135_P000.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T135_P015.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T135_P030.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T135_P045.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T135_P060.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T135_P075.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T135_P090.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T135_P315.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T135_P330.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T135_P345.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T150_P000.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T150_P015.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T150_P030.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T150_P045.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T150_P060.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T150_P075.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T150_P090.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T150_P315.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T150_P330.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T150_P345.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T165_P000.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T165_P015.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T165_P030.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T165_P045.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T165_P060.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T165_P075.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T165_P090.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T165_P315.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T165_P330.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T165_P345.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T180_P000.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T180_P015.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T180_P030.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T180_P045.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T180_P060.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T180_P075.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T180_P090.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T180_P315.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T180_P330.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T180_P345.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T195_P000.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T195_P015.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T195_P030.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T195_P045.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T195_P060.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T195_P075.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T195_P090.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T195_P315.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T195_P330.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T195_P345.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T210_P000.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T210_P015.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T210_P030.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T210_P045.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T210_P060.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T210_P075.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T210_P090.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T210_P315.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T210_P330.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T210_P345.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T225_P000.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T225_P015.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T225_P030.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T225_P045.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T225_P060.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T225_P075.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T225_P090.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T225_P315.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T225_P330.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T225_P345.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T240_P000.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T240_P015.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T240_P030.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T240_P045.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T240_P060.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T240_P075.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T240_P090.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T240_P315.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T240_P330.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T240_P345.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T255_P000.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T255_P015.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T255_P030.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T255_P045.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T255_P060.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T255_P075.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T255_P090.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T255_P315.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T255_P330.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T255_P345.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T270_P000.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T270_P015.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T270_P030.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T270_P045.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T270_P060.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T270_P075.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T270_P090.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T270_P315.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T270_P330.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T270_P345.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T285_P000.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T285_P015.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T285_P030.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T285_P045.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T285_P060.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T285_P075.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T285_P090.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T285_P315.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T285_P330.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T285_P345.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T300_P000.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T300_P015.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T300_P030.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T300_P045.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T300_P060.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T300_P075.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T300_P090.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T300_P315.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T300_P330.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T300_P345.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T315_P000.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T315_P015.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T315_P030.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T315_P045.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T315_P060.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T315_P075.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T315_P090.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T315_P315.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T315_P330.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T315_P345.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T330_P000.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T330_P015.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T330_P030.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T330_P045.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T330_P060.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T330_P075.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T330_P090.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T330_P315.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T330_P330.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T330_P345.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T345_P000.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T345_P015.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T345_P030.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T345_P045.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T345_P060.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T345_P075.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T345_P090.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T345_P315.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T345_P330.wav",
    "webkit/webaudio/IRC_Composite_C_R0195_T345_P345.wav"
};

namespace webaudio {

const char* webAudioAssetFileName(const char* name)
{
      WEBAUDIO_LOGD("webAudioAssetFileName : name: %s", name);
      const size_t kExpectedSpatializationNameLength = 31;
      if (strlen(name) != kExpectedSpatializationNameLength) {
        WEBAUDIO_LOGE("webAudioAssetFileName : invalid name length");
        return 0;
      }

      // Extract the azimuth and elevation from the resource name.
      int azimuth = 0;
      int elevation = 0;
      int values_parsed = sscanf(name, "IRC_Composite_C_R0195_T%3d_P%3d", &azimuth, &elevation);
      if (values_parsed != 2) {
        WEBAUDIO_LOGE("webAudioAssetFileName : parse error - values_parsed: %d", values_parsed);
        return 0;
      }

      // The resource index values go through the elevations first, then azimuths.
      const int kAngleSpacing = 15;

      // 0 <= elevation <= 90 (or 315 <= elevation <= 345)
      // in increments of 15 degrees.
      int elevation_index =
          elevation <= 90 ? elevation / kAngleSpacing :
          7 + (elevation - 315) / kAngleSpacing;
      bool is_elevation_index_good = 0 <= elevation_index && elevation_index < 10;

      // 0 <= azimuth < 360 in increments of 15 degrees.
      int azimuth_index = azimuth / kAngleSpacing;
      bool is_azimuth_index_good = 0 <= azimuth_index && azimuth_index < 24;

      const int kNumberOfElevations = 10;
      const int kNumberOfAudioResources = 240;
      int resource_index = kNumberOfElevations * azimuth_index + elevation_index;
      bool is_resource_index_good = 0 <= resource_index &&
          resource_index < kNumberOfAudioResources;

      if (is_azimuth_index_good && is_elevation_index_good && is_resource_index_good)
          return gWebAudioResourceFiles[IDR_AUDIO_SPATIALIZATION_T000_P000 + resource_index];

      WEBAUDIO_LOGE("webAudioAssetFileName : parse error - azimuth_index: %d, elevation_index:%d, resource_index: %d",
                    azimuth_index, elevation_index, resource_index);
      return 0;
}

}

