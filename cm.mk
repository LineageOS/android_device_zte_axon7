# Copyright (C) 2016 The CyanogenMod Project
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

$(call inherit-product, device/zte/ailsa_ii/full_ailsa_ii.mk)

# Inherit some common CM stuff.
$(call inherit-product, vendor/cm/config/common_full_phone.mk)

# Enhanced NFC
$(call inherit-product, vendor/cm/config/nfc_enhanced.mk)

PRODUCT_NAME := cm_ailsa_ii

PRODUCT_BUILD_PROP_OVERRIDES += \
    PRODUCT_NAME="P996A01" \
    BUILD_FINGERPRINT="ZTE/P996A01/ailsa_ii:6.0.1/MMB29M/20161014.114339:user/release-keys" \
    PRIVATE_BUILD_DESC="P996A01-user 6.0.1 MMB29M 20161014.114339 release-keys"
