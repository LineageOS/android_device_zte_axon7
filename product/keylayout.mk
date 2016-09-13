# Keylayout
PRODUCT_COPY_FILES := $(filter-out frameworks/base/data/keyboards/qwerty.kl:system/usr/keylayout/qwerty.kl, $(PRODUCT_COPY_FILES))

PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/keylayout/synaptics_dsx.kl:system/usr/keylayout/synaptics_dsx.kl \
    $(LOCAL_PATH)/keylayout/qwerty.kl:system/usr/keylayout/qwerty.kl
