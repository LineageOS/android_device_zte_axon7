LOCAL_PATH := external/e2fsprogs/misc

#########################################################################
# Build statically linked mke2fs for recovery
mke2fs_src_files := \
       mke2fs.c \
       mk_hugefiles.c \
       default_profile.c \
       create_inode.c \

mke2fs_c_includes := \
       external/e2fsprogs/e2fsck

mke2fs_cflags := -W -Wall -Wno-macro-redefined

mke2fs_static_libraries := \
       libext2_blkid \
       libext2_uuid \
       libext2_quota \
       libext2_com_err \
       libext2_e2p \
       libsparse \
       libz \

mke2fs_whole_static_libraries := \
       libbase \
       libext2fs \

include $(CLEAR_VARS)

LOCAL_SRC_FILES := $(mke2fs_src_files)
LOCAL_C_INCLUDES := $(mke2fs_c_includes)
LOCAL_CFLAGS := $(mke2fs_cflags)
LOCAL_WHOLE_STATIC_LIBRARIES := $(mke2fs_whole_static_libraries)
LOCAL_STATIC_LIBRARIES := $(mke2fs_static_libraries)
LOCAL_MODULE := mke2fs_msm8996
LOCAL_MODULE_PATH := $(PRODUCT_OUT)/install/bin
LOCAL_FORCE_STATIC_EXECUTABLE := true

include $(BUILD_EXECUTABLE)

