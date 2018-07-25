LOCAL_PATH := external/gptfdisk

sgdisk_src_files := \
    sgdisk.cc \
    gptcl.cc \
    crc32.cc \
    support.cc \
    guid.cc \
    gptpart.cc \
    mbrpart.cc \
    basicmbr.cc \
    mbr.cc \
    gpt.cc \
    bsd.cc \
    parttypes.cc \
    attributes.cc \
    diskio.cc \
    diskio-unix.cc \
    android_popt.cc \

sgdisk_c_includes := external/e2fsprogs/lib

sgdisk_shared_libraries := \
        libext2_uuid

sgdisk_system_shared_libraries := libc

sgdisk_cflags := -O2 -g -W -Wall

include $(CLEAR_VARS)

LOCAL_CPP_EXTENSION := .cc

LOCAL_C_INCLUDES := $(LOCAL_PATH) $(sgdisk_c_includes)
LOCAL_SRC_FILES := $(sgdisk_src_files)
LOCAL_CFLAGS := $(sgdisk_cflags)

LOCAL_STATIC_LIBRARIES := $(sgdisk_shared_libraries)
LOCAL_STATIC_LIBRARIES += $(sgdisk_system_shared_libraries)

LOCAL_MODULE := sgdisk_msm8996
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_PATH := $(PRODUCT_OUT)/install/bin
LOCAL_FORCE_STATIC_EXECUTABLE := true
include $(BUILD_EXECUTABLE)

