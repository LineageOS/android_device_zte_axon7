#
# Copyright (C) 2016 The CyanogenMod Project
# Copyright (C) 2017 The LineageOS Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

# Inherit from those products. Most specific first.
$(call inherit-product, $(SRC_TARGET_DIR)/product/core_64_bit.mk)
$(call inherit-product, $(SRC_TARGET_DIR)/product/full_base_telephony.mk)

# Inherit from ailsa_ii device
$(call inherit-product, device/zte/axon7/device.mk)

# Inherit some common Aex stuff.
$(call inherit-product, vendor/aex/config/common_full_phone.mk)

# Device identifier. This must come after all inclusions.
PRODUCT_NAME := aex_axon7
PRODUCT_DEVICE := axon7
PRODUCT_BRAND := ZTE
PRODUCT_MODEL := ZTE A2017U
PRODUCT_MANUFACTURER := ZTE

PRODUCT_BUILD_PROP_OVERRIDES += \
    PRODUCT_NAME="P996A01_N" \
    PRIVATE_BUILD_DESC="P996A01_N-user 7.1.1 NMF26V 20170725.003053 release-keys"

BUILD_FINGERPRINT := ZTE/P996A01_N/ailsa_ii:7.1.1/NMF26V/20170725.003053:user/release-keys
