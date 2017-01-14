# Platform
TARGET_BOARD_PLATFORM := msm8996

# Architecture
TARGET_ARCH := arm64
TARGET_ARCH_VARIANT := armv8-a
TARGET_CPU_ABI := arm64-v8a
TARGET_CPU_ABI2 :=
TARGET_CPU_VARIANT := kryo

TARGET_2ND_ARCH := arm
TARGET_2ND_ARCH_VARIANT := armv7-a-neon
TARGET_2ND_CPU_ABI := armeabi-v7a
TARGET_2ND_CPU_ABI2 := armeabi
TARGET_2ND_CPU_VARIANT := cortex-a53

#ENABLE_CPUSETS := true

TARGET_USES_64_BIT_BINDER := true

# Init
TARGET_INIT_VENDOR_LIB := libinit_axon7
TARGET_RECOVERY_DEVICE_MODULES := libinit_axon7

# Qualcomm support
BOARD_USES_QCOM_HARDWARE := true
BOARD_USES_QC_TIME_SERVICES := true
TARGET_POWERHAL_VARIANT := qcom
TARGET_RIL_VARIANT := caf
TARGET_TAP_TO_WAKE_NODE := "/proc/touchscreen/wake_gesture"
