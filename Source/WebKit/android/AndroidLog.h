/*
 * Copyright 2008, The Android Open Source Project
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

#ifndef AndroidLog_h
#define AndroidLog_h

#ifndef LOG_TAG
#define LOG_TAG __FILE__
#endif

#define ATRACE_TAG ATRACE_TAG_WEBVIEW

#include <cutils/log.h>
#include <utils/Trace.h>
#include <wtf/CurrentTime.h>

#ifdef ANDROID_DOM_LOGGING
#include <stdio.h>
extern FILE* gDomTreeFile;
#define DOM_TREE_LOG_FILE "/sdcard/domTree.txt"
#define DUMP_DOM_LOGD(...) { if (gDomTreeFile) \
    fprintf(gDomTreeFile, __VA_ARGS__); else ALOGD(__VA_ARGS__); }

extern FILE* gRenderTreeFile;
#define RENDER_TREE_LOG_FILE "/sdcard/renderTree.txt"
#define DUMP_RENDER_LOGD(...) { if (gRenderTreeFile) \
    fprintf(gRenderTreeFile, __VA_ARGS__); else ALOGD(__VA_ARGS__); }
#else
#define DUMP_DOM_LOGD(...) ((void)0)
#define DUMP_RENDER_LOGD(...) ((void)0)
#endif /* ANDROID_DOM_LOGGING */

#define DISPLAY_TREE_LOG_FILE "/sdcard/displayTree.txt"
#define LAYERS_TREE_LOG_FILE "/sdcard/layersTree.plist"

#define FLOAT_RECT_FORMAT "[x=%.2f,y=%.2f,w=%.2f,h=%.2f]"
#define FLOAT_RECT_ARGS(fr) fr.x(), fr.y(), fr.width(), fr.height()
#define INT_RECT_FORMAT "[x=%d,y=%d,w=%d,h=%d]"
#define INT_RECT_ARGS(ir) ir.x(), ir.y(), ir.width(), ir.height()

#define TRACE_METHOD() android::ScopedTrace __st(ATRACE_TAG, __func__);

#define TIME_METHOD() MethodTimer __method_timer(__func__)
class MethodTimer {
public:
    MethodTimer(const char* name)
        : m_methodName(name)
    {
        m_startTime = currentTimeMS();
    }
    virtual ~MethodTimer() {
        double duration = currentTimeMS() - m_startTime;
        ALOGD("%s took %.2fms", m_methodName, duration);
    }
private:
    const char* m_methodName;
    double m_startTime;
};

#endif // AndroidLog_h
