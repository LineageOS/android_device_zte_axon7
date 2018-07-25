LOCAL_PATH := external/e2fsprogs/e2fsck

e2fsck_src_files := \
	e2fsck.c \
	super.c \
	pass1.c \
	pass1b.c \
	pass2.c \
	pass3.c \
	pass4.c \
	pass5.c \
	logfile.c \
	journal.c \
	recovery.c \
	revoke.c \
	badblocks.c \
	util.c \
	unix.c \
	dirinfo.c \
	dx_dirinfo.c \
	ehandler.c \
	problem.c \
	message.c \
	ea_refcount.c \
	quota.c \
	rehash.c \
	region.c \
	sigcatcher.c \
	readahead.c \
	extents.c

e2fsck_c_includes := external/e2fsprogs/lib

e2fsck_cflags := -O2 -g -W -Wall

e2fsck_shared_libraries := \
	libext2fs \
	libext2_blkid \
	libext2_uuid \
	libext2_quota \
	libext2_com_err \
	libext2_e2p

e2fsck_system_shared_libraries := libc

include $(CLEAR_VARS)

LOCAL_SRC_FILES := $(e2fsck_src_files)
LOCAL_C_INCLUDES := $(e2fscks_c_includes)
LOCAL_CFLAGS := $(e2fsck_cflags)
LOCAL_STATIC_LIBRARIES := $(e2fsck_shared_libraries)
LOCAL_STATIC_LIBRARIES += $(e2fsck_system_shared_libraries)
LOCAL_MODULE := e2fsck_msm8996
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_PATH := $(PRODUCT_OUT)/install/bin
LOCAL_FORCE_STATIC_EXECUTABLE := true
include $(BUILD_EXECUTABLE)
