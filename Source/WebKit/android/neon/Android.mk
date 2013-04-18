LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := vadd.S vmul.S vsmul.S zvmul.S vinterleave.S vdeinterleave.S

LOCAL_CFLAGS := -O3 -fvisibility=hidden

LOCAL_MODULE := libvmathneon

include $(BUILD_STATIC_LIBRARY)

