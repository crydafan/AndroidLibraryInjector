LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := AndroidLibraryInjector

LOCAL_SRC_FILES := \
    injector.c \
    remote.c

LOCAL_LDLIBS := \
    -llog

LOCAL_CFLAGS   := \
    -Wall -Wextra -Werror \
    -Wno-unused-parameter \
    -O2 \
    -fvisibility=hidden

include $(BUILD_SHARED_LIBRARY)
