##
## Copyright 2009, The Android Open Source Project
##
## Redistribution and use in source and binary forms, with or without
## modification, are permitted provided that the following conditions
## are met:
##  * Redistributions of source code must retain the above copyright
##    notice, this list of conditions and the following disclaimer.
##  * Redistributions in binary form must reproduce the above copyright
##    notice, this list of conditions and the following disclaimer in the
##    documentation and/or other materials provided with the distribution.
##
## THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
## EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
## IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
## PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
## CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
## EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
## PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
## PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
## OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
## (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
## OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
##

# Control SVG compiling in webkit.
# Default is true unless explictly disabled.
ifneq ($(ENABLE_SVG),false)
    ENABLE_SVG = true
endif

# Control complex scripts support compiling in webkit.
# Default is true unless explictly disabled.
ifneq ($(SUPPORT_COMPLEX_SCRIPTS),false)
    SUPPORT_COMPLEX_SCRIPTS = true
endif

# Read the environment variable to determine if Autofill is compiled.
# The default is on.
# is turned on.
ifneq ($(ENABLE_AUTOFILL),false)
  ENABLE_AUTOFILL = true
endif

BASE_PATH := $(call my-dir)
include $(CLEAR_VARS)

# Define our module and find the intermediates directory
LOCAL_MODULE := libwebcore
LOCAL_MODULE_CLASS := STATIC_LIBRARIES
base_intermediates := $(call local-intermediates-dir)

# Using := here prevents recursive expansion
WEBKIT_SRC_FILES :=

# We have to use bison 2.3
include $(BASE_PATH)/bison_check.mk

SOURCE_PATH := $(BASE_PATH)/Source
WEBCORE_PATH := $(SOURCE_PATH)/WebCore
JAVASCRIPTCORE_PATH := $(SOURCE_PATH)/JavaScriptCore
WEBKIT_PATH := $(SOURCE_PATH)/WebKit
WEBCORE_INTERMEDIATES_PATH := $(base_intermediates)/Source/WebCore

# Build our list of include paths. We include Source/WebKit/android/icu first so that
# any files that include <unicode/ucnv.h> will include our ucnv.h first. We
# also add external/ as an include directory so that we can specify the real
# icu header directory as a more exact reference to avoid including our ucnv.h.
#
# Note that JavasCriptCore/ must be included after WebCore/, so that we pick up
# the right config.h.
LOCAL_C_INCLUDES := \
	$(JNI_H_INCLUDE) \
	$(WEBKIT_PATH)/android/icu \
	external/ \
	external/icu4c/common \
	external/icu4c/i18n \
	external/jpeg \
	external/libxml2/include \
	external/libxslt \
	external/hyphenation \
	external/skia/emoji \
	external/skia/include/core \
	external/skia/include/effects \
	external/skia/include/gpu \
	external/skia/include/images \
	external/skia/include/ports \
	external/skia/include/utils \
	external/skia/src/gpu \
	external/skia/src/ports \
	external/sqlite/dist \
	frameworks/base/core/jni/android/graphics \
	frameworks/base/include

# Add Source/ for the include of <JavaScriptCore/config.h> from WebCore/config.h
LOCAL_C_INCLUDES := $(LOCAL_C_INCLUDES) \
	$(SOURCE_PATH)

LOCAL_C_INCLUDES := $(LOCAL_C_INCLUDES) \
	$(WEBCORE_PATH) \
	$(WEBCORE_PATH)/accessibility \
	$(WEBCORE_PATH)/bindings/ \
	$(WEBCORE_PATH)/bindings/generic \
	$(WEBCORE_PATH)/css \
	$(WEBCORE_PATH)/dom \
	$(WEBCORE_PATH)/editing \
	$(WEBCORE_PATH)/fileapi \
	$(WEBCORE_PATH)/history \
	$(WEBCORE_PATH)/history/android \
	$(WEBCORE_PATH)/html \
	$(WEBCORE_PATH)/html/canvas \
	$(WEBCORE_PATH)/html/parser \
	$(WEBCORE_PATH)/html/shadow \
	$(WEBCORE_PATH)/inspector \
	$(WEBCORE_PATH)/loader \
	$(WEBCORE_PATH)/loader/appcache \
	$(WEBCORE_PATH)/loader/archive \
	$(WEBCORE_PATH)/loader/archive/android \
	$(WEBCORE_PATH)/loader/cache \
	$(WEBCORE_PATH)/loader/icon \
	$(WEBCORE_PATH)/notifications \
	$(WEBCORE_PATH)/page \
	$(WEBCORE_PATH)/page/android \
	$(WEBCORE_PATH)/page/animation \
	$(WEBCORE_PATH)/platform \
	$(WEBCORE_PATH)/platform/android \
	$(WEBCORE_PATH)/platform/animation \
	$(WEBCORE_PATH)/platform/graphics \
	$(WEBCORE_PATH)/platform/graphics/android \
	$(WEBCORE_PATH)/platform/graphics/android/context \
	$(WEBCORE_PATH)/platform/graphics/android/fonts \
	$(WEBCORE_PATH)/platform/graphics/android/layers \
	$(WEBCORE_PATH)/platform/graphics/android/rendering \
	$(WEBCORE_PATH)/platform/graphics/android/utils \
	$(WEBCORE_PATH)/platform/graphics/filters \
	$(WEBCORE_PATH)/platform/graphics/gpu \
	$(WEBCORE_PATH)/platform/graphics/network \
	$(WEBCORE_PATH)/platform/graphics/skia \
	$(WEBCORE_PATH)/platform/graphics/transforms \
	$(WEBCORE_PATH)/platform/image-decoders \
	$(WEBCORE_PATH)/platform/image-decoders/bmp \
	$(WEBCORE_PATH)/platform/image-decoders/gif \
	$(WEBCORE_PATH)/platform/image-decoders/ico \
	$(WEBCORE_PATH)/platform/image-decoders/jpeg \
	$(WEBCORE_PATH)/platform/image-decoders/png \
	$(WEBCORE_PATH)/platform/image-decoders/webp \
	$(WEBCORE_PATH)/platform/mock \
	$(WEBCORE_PATH)/platform/network \
	$(WEBCORE_PATH)/platform/network/android \
	$(WEBCORE_PATH)/platform/sql \
	$(WEBCORE_PATH)/platform/text \
	$(WEBCORE_PATH)/platform/text/transcoder \
	$(WEBCORE_PATH)/plugins \
	$(WEBCORE_PATH)/plugins/android \
	$(WEBCORE_PATH)/rendering \
	$(WEBCORE_PATH)/rendering/style \
	$(WEBCORE_PATH)/rendering/svg \
	$(WEBCORE_PATH)/storage \
	$(WEBCORE_PATH)/svg \
	$(WEBCORE_PATH)/svg/animation \
	$(WEBCORE_PATH)/svg/graphics \
	$(WEBCORE_PATH)/svg/graphics/filters \
	$(WEBCORE_PATH)/svg/properties \
	$(WEBCORE_PATH)/websockets \
	$(WEBCORE_PATH)/workers \
	$(WEBCORE_PATH)/xml

LOCAL_C_INCLUDES := $(LOCAL_C_INCLUDES) \
	$(WEBKIT_PATH)/android \
	$(WEBKIT_PATH)/android/WebCoreSupport \
	$(WEBKIT_PATH)/android/jni \
	$(WEBKIT_PATH)/android/nav \
	$(WEBKIT_PATH)/android/plugins

LOCAL_C_INCLUDES := $(LOCAL_C_INCLUDES) \
	$(JAVASCRIPTCORE_PATH) \
	$(JAVASCRIPTCORE_PATH)/collector/handles \
	$(JAVASCRIPTCORE_PATH)/heap \
	$(JAVASCRIPTCORE_PATH)/wtf \
	$(JAVASCRIPTCORE_PATH)/wtf/unicode \
	$(JAVASCRIPTCORE_PATH)/wtf/unicode/icu

LOCAL_C_INCLUDES := $(LOCAL_C_INCLUDES) \
	$(WEBCORE_INTERMEDIATES_PATH) \
	$(WEBCORE_INTERMEDIATES_PATH)/css \
	$(WEBCORE_INTERMEDIATES_PATH)/html \
	$(WEBCORE_INTERMEDIATES_PATH)/platform \
	$(WEBCORE_INTERMEDIATES_PATH)/xml

# The following includes are needed by the AutoFill feature, or the chrome http
# stack
LOCAL_C_INCLUDES := $(LOCAL_C_INCLUDES) \
	$(WEBKIT_PATH)/chromium \
	$(WEBKIT_PATH)/chromium/public \
	external/chromium/chrome/browser \
	external/chromium/chrome/renderer \
	external/chromium \
	external/chromium/chrome \
	external/skia

LOCAL_CFLAGS += -DWEBKIT_IMPLEMENTATION=1

# Include WTF source file.
d := Source/JavaScriptCore
LOCAL_PATH := $(BASE_PATH)/$d
intermediates := $(base_intermediates)/$d
include $(LOCAL_PATH)/Android.v8.wtf.mk
WEBKIT_SRC_FILES += $(addprefix $d/,$(LOCAL_SRC_FILES))

# Include source files for WebCore
d := Source/WebCore
LOCAL_PATH := $(BASE_PATH)/$d
intermediates := $(base_intermediates)/$d
include $(LOCAL_PATH)/Android.mk
include $(LOCAL_PATH)/Android.v8bindings.mk
WEBKIT_SRC_FILES += $(addprefix $d/,$(LOCAL_SRC_FILES))
LOCAL_C_INCLUDES += $(BINDING_C_INCLUDES)

# Include the derived source files for WebCore. Uses the same path as
# WebCore
include $(LOCAL_PATH)/Android.derived.mk
include $(LOCAL_PATH)/Android.derived.v8bindings.mk

# Include source files for android WebKit port
d := Source/WebKit
LOCAL_PATH := $(BASE_PATH)/$d
intermediates := $(base_intermediates)/$d
include $(LOCAL_PATH)/Android.mk
WEBKIT_SRC_FILES += $(addprefix $d/,$(LOCAL_SRC_FILES))

# Redefine LOCAL_PATH here so the build system is not confused
LOCAL_PATH := $(BASE_PATH)

# Define our compiler flags
LOCAL_CFLAGS += -Wno-endif-labels -Wno-import -Wno-format
LOCAL_CFLAGS += -fno-strict-aliasing
LOCAL_CFLAGS += -include "WebCorePrefix.h"
LOCAL_CFLAGS += -fvisibility=hidden
LOCAL_CFLAGS += -DALWAYS_INLINE=inline
# Make sure assert.h is included before assert is defined
LOCAL_CFLAGS += -include "assert.h"
LOCAL_CFLAGS += -DGOOGLEURL
LOCAL_CPPFLAGS := -Wno-sign-promo
LOCAL_CPPFLAGS := -Wno-c++0x-compat

# Adds GL and EGL extensions for the GL backend
LOCAL_CFLAGS += -DGL_GLEXT_PROTOTYPES -DEGL_EGLEXT_PROTOTYPES

ifeq ($(TARGET_ARCH),arm)
LOCAL_CFLAGS += -Darm
# remove this warning: "note: the mangling of 'va_list' has changed in GCC 4.4"
LOCAL_CFLAGS += -Wno-psabi
endif

ifeq ($(TARGET_ARCH_VARIANT),x86-atom)
LOCAL_CFLAGS += -fno-pic
endif

# need a flag to tell the C side when we're on devices with large memory
# budgets (i.e. larger than the low-end devices that initially shipped)
ifeq ($(ARCH_ARM_HAVE_VFP),true)
LOCAL_CFLAGS += -DANDROID_LARGE_MEMORY_DEVICE
endif

ifeq ($(TARGET_ARCH),x86)
LOCAL_CFLAGS += -DANDROID_LARGE_MEMORY_DEVICE
endif

ifeq ($(ENABLE_SVG),true)
LOCAL_CFLAGS += -DENABLE_SVG=1 -DENABLE_SVG_ANIMATION=1
endif

ifeq ($(ENABLE_WTF_USE_ACCELERATED_COMPOSITING),false)
LOCAL_CFLAGS += -DWTF_USE_ACCELERATED_COMPOSITING=0
endif

ifeq ($(ENABLE_WTF_USE_ACCELERATED_COMPOSITING),true)
LOCAL_CFLAGS += -DWTF_USE_ACCELERATED_COMPOSITING=1
endif

# LOCAL_LDLIBS is used in simulator builds only and simulator builds are only
# valid on Linux
LOCAL_LDLIBS += -lpthread -ldl

# Build the list of shared libraries
# We have to use the android version of libdl
LOCAL_SHARED_LIBRARIES := \
	libEGL \
	libGLESv2 \
	libandroid \
	libandroidfw \
	libandroid_runtime \
	libchromium_net \
	libcrypto \
	libcutils \
	libdl \
	libgui \
	libicuuc \
	libicui18n \
	libmedia \
	libmedia_native \
	libnativehelper \
	libskia \
	libsqlite \
	libssl \
	libstlport \
	libutils \
	libui \
	libz

# We have to fake out some headers when using stlport.
LOCAL_C_INCLUDES += \
	external/chromium/android
include external/stlport/libstlport.mk

# We need Harfbuzz library to support complex scripts(Arabic, Thai, Hindi...).
ifeq ($(SUPPORT_COMPLEX_SCRIPTS),true)
LOCAL_C_INCLUDES := $(LOCAL_C_INCLUDES) \
	external/harfbuzz/src \
	external/harfbuzz/contrib
LOCAL_SHARED_LIBRARIES += libharfbuzz
LOCAL_CFLAGS += -DSUPPORT_COMPLEX_SCRIPTS=1
endif

# Build the list of static libraries
LOCAL_STATIC_LIBRARIES := libxml2 libxslt libhyphenation libskiagpu libv8

ifeq ($(ENABLE_AUTOFILL),true)
LOCAL_SHARED_LIBRARIES += libexpat
endif

# Redefine LOCAL_SRC_FILES to be all the WebKit source files
LOCAL_SRC_FILES := $(WEBKIT_SRC_FILES)

# Define this for use in other makefiles.
WEBKIT_C_INCLUDES := $(LOCAL_C_INCLUDES)
WEBKIT_CFLAGS := $(LOCAL_CFLAGS)
WEBKIT_CPPFLAGS := $(LOCAL_CPPFLAGS)
WEBKIT_GENERATED_SOURCES := $(LOCAL_GENERATED_SOURCES)
WEBKIT_LDLIBS := $(LOCAL_LDLIBS)
WEBKIT_SHARED_LIBRARIES := $(LOCAL_SHARED_LIBRARIES)
WEBKIT_STATIC_LIBRARIES := $(LOCAL_STATIC_LIBRARIES)

ifneq ($(strip $(WITH_ADDRESS_SANITIZER)),)
    LOCAL_MODULE_PATH := $(TARGET_OUT_STATIC_LIBRARIES)/asan
    LOCAL_ADDRESS_SANITIZER := true
endif

# Build the library all at once
include $(BUILD_STATIC_LIBRARY)

# Now build the shared library using only the exported jni entry point. This
# will strip out any unused code from the entry point.
include $(CLEAR_VARS)
# Do not attempt prelink this library. Needed to keep master-gpl happy, no
# effect in master.
# TODO: remove this when master-gpl is updated.
LOCAL_PRELINK_MODULE := false
LOCAL_MODULE := libwebcore
LOCAL_LDLIBS := $(WEBKIT_LDLIBS)
LOCAL_SHARED_LIBRARIES := $(WEBKIT_SHARED_LIBRARIES)
LOCAL_STATIC_LIBRARIES := libwebcore $(WEBKIT_STATIC_LIBRARIES)
LOCAL_LDFLAGS := -fvisibility=hidden
LOCAL_CFLAGS := $(WEBKIT_CFLAGS)
LOCAL_CPPFLAGS := $(WEBKIT_CPPFLAGS)
LOCAL_C_INCLUDES := $(WEBKIT_C_INCLUDES)
LOCAL_PATH := $(BASE_PATH)
LOCAL_SRC_FILES := \
	Source/WebKit/android/jni/WebCoreJniOnLoad.cpp \
	Source/WebKit/chromium/src/android/WebDOMTextContentWalker.cpp \
	Source/WebKit/chromium/src/android/WebHitTestInfo.cpp \
	Source/WebKit/chromium/src/WebRange.cpp \
	Source/WebKit/chromium/src/WebString.cpp

ifeq ($(ENABLE_AUTOFILL),true)
# AutoFill requires some cpp files from Chromium to link with
# libchromium_net. We cannot compile them into libchromium_net
# because they have cpp file extensions, not .cc.
LOCAL_SRC_FILES += \
	Source/WebKit/android/WebCoreSupport/autofill/MainThreadProxy.cpp \
	Source/WebKit/chromium/src/WebCString.cpp \
	Source/WebKit/chromium/src/WebRegularExpression.cpp
endif

# Do this dependency by hand. The reason we have to do this is because the
# headers that this file pulls in are generated during the build of webcore.
# We make all of our object files depend on those files so that they are built
# before we try to compile the file.
LOCAL_ADDITIONAL_DEPENDENCIES := $(filter %.h, $(WEBKIT_GENERATED_SOURCES))

ifneq ($(strip $(WITH_ADDRESS_SANITIZER)),)
    LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/asan
    LOCAL_ADDRESS_SANITIZER := true
endif

include $(BUILD_SHARED_LIBRARY)

# Build the wds client
include $(WEBKIT_PATH)/android/wds/client/Android.mk

# Build the webkit merge tool.
include $(BASE_PATH)/Tools/android/webkitmerge/Android.mk
