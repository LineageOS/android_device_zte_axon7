# Fingerprint sensor
PRODUCT_PACKAGES += \
    init.fingerprint.goodix_fp.rc

# Permissions
PRODUCT_COPY_FILES += \
    frameworks/native/data/etc/android.hardware.fingerprint.xml:system/etc/permissions/android.hardware.fingerprint.xml
