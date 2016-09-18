# Keylayout
PRODUCT_COPY_FILES := $(filter-out frameworks/base/data/keyboards/qwerty.kl:system/usr/keylayout/qwerty.kl, $(PRODUCT_COPY_FILES))
