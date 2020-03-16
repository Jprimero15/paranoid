#!/bin/sh

rm -rf hardware/qcom/media
git clone https://github.com/IKGapirov/android_hardware_qcom_media -b quartz-8974 hardware/qcom/media

rm -rf hardware/qcom/audio
git clone https://github.com/IKGapirov/android_hardware_qcom_audio -b quartz-8974 hardware/qcom/audio

rm -rf hardware/qcom/display
git clone https://github.com/IKGapirov/android_hardware_qcom_display -b quartz-8974 hardware/qcom/display

rm -rf prebuilts/gcc/linux-x86/arm/arm-linux-androideabi-4.9
git clone https://github.com/LineageOS/android_prebuilts_gcc_linux-x86_arm_arm-linux-androideabi-4.9 -b lineage-17.1 prebuilts/gcc/linux-x86/arm/arm-linux-androideabi-4.9

# home wake
git -C frameworks/base fetch https://github.com/Staydirtyboi/android_frameworks_base-2 fw_base && git -C frameworks/base cherry-pick ac3c14bbd5eabf9c86a049a5dfc25e45b67aca91

# PowerHAL commits
 ./vendor/pa/build/tools/repopick.py 9454 # power: Add back display_boost checks

# AOSP WFD
 ./vendor/pa/build/tools/repopick.py -t quartz-aosp-wfd
 
 # netd RIL fix
git -C system/netd revert --no-edit 
92e8f96e43320efd5183d7452fb90883fd96415e

# wifi: fix for legacy devices
git -C hardware/interfaces fetch https://github.com/Jprimero15/paranoid hardware_interfaces && git -C hardware/interfaces cherry-pick 7a76758ada5efca2efe23a7a61a5927264f3426c

# vendor/pa: pick libfqio and hlte bringup
git -C vendor/pa fetch https://github.com/Jprimero15/paranoid vendor_pa && git -C vendor/pa cherry-pick 5c031fce8132bb434fb8217dbabfee0de18981f2^..f2b74a2e922d9614f97e35c3d233f9ef17e25eda

# wpa aosp
rm -rf external/wpa_supplicant_8
git clone https://android.googlesource.com/platform/external/wpa_supplicant_8 -b android-10.0.0_r31

