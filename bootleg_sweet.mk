#
# Copyright (C) 2021 The LineageOS Project
#
# SPDX-License-Identifier: Apache-2.0
#

# Inherit from those products. Most specific first.
$(call inherit-product, $(SRC_TARGET_DIR)/product/core_64_bit.mk)
$(call inherit-product, $(SRC_TARGET_DIR)/product/full_base_telephony.mk)

# Inherit from sweet device
$(call inherit-product, device/xiaomi/sweet/device.mk)

# Inherit common Bootleggers OS  Stuff.
$(call inherit-product, vendor/bootleggers/config/common_full_phone.mk)
TARGET_BOOT_ANIMATION_RES := 2160
PRODUCT_NAME := bootleg_sweet
PRODUCT_DEVICE := sweet
PRODUCT_BRAND := Xiaomi
PRODUCT_MODEL := Redmi Note 10 Pro
PRODUCT_MANUFACTURER := Xiaomi
PRODUCT_GMS_CLIENTID_BASE := android-xiaomi
TARGET_USES_BLUR := false

# Maintainer Prop
PRODUCT_PROPERTY_OVERRIDES += \
DEVICE_MAINTAINERS := SoloSaravanan
BOOTLEGGERS_BUILD_TYPE := Unshishufied
WITH_GAPPS := false

#Build Flags
TARGET_FACE_UNLOCK_SUPPORTED := false
TARGET_USES_BLUR := false
TARGET_INCLUDE_LIVE_WALLPAPERS := false
TARGET_INCLUDE_STOCK_ARCORE := false
