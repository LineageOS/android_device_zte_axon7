LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE       := calib.cfg
LOCAL_MODULE_TAGS  := optional
LOCAL_MODULE_CLASS := ETC
LOCAL_SRC_FILES    := calib.cfg
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE       := qdcm_calib_data_zteSAM_S6E3HA3_SAM_25601440_5P5Inch.xml
LOCAL_MODULE_TAGS  := optional
LOCAL_MODULE_CLASS := ETC
LOCAL_SRC_FILES    := qdcm_calib_data_zteSAM_S6E3HA3_SAM_25601440_5P5Inch.xml
include $(BUILD_PREBUILT)

