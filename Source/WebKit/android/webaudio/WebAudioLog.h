/*
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


#ifndef android_webaudio_log_h
#define android_webaudio_log_h

#include <android/log.h>

#define WA_LOG_TAG "WebAudio"

/** These values match the definitions in system/core/include/cutils/log.h */
#define WebAudioLogLevel_Unknown 0
#define WebAudioLogLevel_Default 1
#define WebAudioLogLevel_Verbose 2
#define WebAudioLogLevel_Debug   3
#define WebAudioLogLevel_Info    4
#define WebAudioLogLevel_Warn    5
#define WebAudioLogLevel_Error   6
#define WebAudioLogLevel_Fatal   7
#define WebAudioLogLevel_Silent  8

#ifndef WEBAUDIO_USE_LOG
#define WEBAUDIO_USE_LOG WebAudioLogLevel_Warn
#endif


#if (WEBAUDIO_USE_LOG <= WebAudioLogLevel_Verbose)
#define WEBAUDIO_LOGV(...) __android_log_print(ANDROID_LOG_VERBOSE, WA_LOG_TAG, __VA_ARGS__);
#else
#define WEBAUDIO_LOGV(...)  do { } while (0)
#endif

#if (WEBAUDIO_USE_LOG <= WebAudioLogLevel_Debug)
#define WEBAUDIO_LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, WA_LOG_TAG, __VA_ARGS__);
#else
#define WEBAUDIO_LOGD(...)  do { } while (0)
#endif

#if (WEBAUDIO_USE_LOG <= WebAudioLogLevel_Warn)
#define WEBAUDIO_LOGW(...) __android_log_print(ANDROID_LOG_WARN, WA_LOG_TAG, __VA_ARGS__);
#else
#define WEBAUDIO_LOGW(...)  do { } while (0)
#endif

#if (WEBAUDIO_USE_LOG <= WebAudioLogLevel_Error)
#define WEBAUDIO_LOGE(...) __android_log_print(ANDROID_LOG_ERROR, WA_LOG_TAG, __VA_ARGS__);
#else
#define WEBAUDIO_LOGE(...)  do { } while (0)
#endif

#if (WEBAUDIO_USE_LOG <= WebAudioLogLevel_Fatal)
#define WEBAUDIO_LOGF(...) __android_log_print(ANDROID_LOG_FATAL, WA_LOG_TAG, __VA_ARGS__);
#else
#define WEBAUDIO_LOGF(...)  do { } while (0)
#endif


#endif

