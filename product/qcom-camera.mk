# Camera
PRODUCT_PACKAGES += \
    Snap

# Permissions
PRODUCT_COPY_FILES += \
    frameworks/native/data/etc/android.hardware.camera.flash-autofocus.xml:system/etc/permissions/android.hardware.camera.flash-autofocus.xml \
    frameworks/native/data/etc/android.hardware.camera.front.xml:system/etc/permissions/android.hardware.camera.front.xml

# Properties
PRODUCT_PROPERTY_OVERRIDES += \
    camera.disable_zsl_mode=1 \
    persist.camera.global.debug \
    persist.camera.gyro.disable=0 \
    persist.camera.preview.ubwc=0 \
    persist.camera.set.afd \
    persist.camera.stats.test=5 \
    persist.camera.video.ubwc=0

