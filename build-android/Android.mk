#
# Copyright (C) YuqiaoZhang(HanetakaChou)
# 
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU Lesser General Public License as published
# by the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Lesser General Public License for more details.
# 
# You should have received a copy of the GNU Lesser General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.
#

# https://developer.android.com/ndk/guides/android_mk

LOCAL_PATH := $(call my-dir)

# Android Application Library

include $(CLEAR_VARS)

LOCAL_MODULE := NativeActivity

LOCAL_SRC_FILES := \
	$(LOCAL_PATH)/../assets/assets.cpp \
	$(LOCAL_PATH)/../assets/the_white_room_gltf.cpp \
	$(LOCAL_PATH)/../assets/the_white_room_bin.cpp \
	$(LOCAL_PATH)/../assets/the_white_room_1_dds.cpp \
	$(LOCAL_PATH)/../assets/the_white_room_1_s_dds.cpp \
	$(LOCAL_PATH)/../assets/the_white_room_2_dds.cpp \
	$(LOCAL_PATH)/../assets/the_white_room_2_n_dds.cpp \
	$(LOCAL_PATH)/../assets/the_white_room_3_dds.cpp \
	$(LOCAL_PATH)/../assets/the_white_room_4_dds.cpp \
	$(LOCAL_PATH)/../assets/keqing_lolita_love_you_gltf.cpp \
	$(LOCAL_PATH)/../assets/keqing_lolita_love_you_bin.cpp \
	$(LOCAL_PATH)/../assets/keqing_lolita_dds.cpp \
	$(LOCAL_PATH)/../source/support/camera_controller.cpp \
	$(LOCAL_PATH)/../source/support/main.cpp \
	$(LOCAL_PATH)/../source/support/renderer.cpp \
	$(LOCAL_PATH)/../source/support/tick_count.cpp \
	$(LOCAL_PATH)/../source/demo.cpp \
	$(LOCAL_PATH)/../thirdparty/DXUT/Optional/DXUTcamera.cpp

ifeq (arm, $(TARGET_ARCH))
LOCAL_ARM_MODE := arm
LOCAL_ARM_NEON := true
endif

LOCAL_CFLAGS :=
LOCAL_CFLAGS += -finput-charset=UTF-8 -fexec-charset=UTF-8
# LOCAL_CFLAGS += -fvisibility=hidden
LOCAL_CFLAGS += -Wall -Werror=return-type
LOCAL_CFLAGS += -Dbrx_init_unknown_device=brx_init_vk_device
LOCAL_CFLAGS += -Dbrx_destroy_unknown_device=brx_destroy_vk_device
LOCAL_CFLAGS += -DPAL_STDCPP_COMPAT=1

# LOCAL_CPPFLAGS :=
# LOCAL_CPPFLAGS += -std=c++11

LOCAL_C_INCLUDES :=
LOCAL_C_INCLUDES += $(LOCAL_PATH)/../shaders/spirv
LOCAL_C_INCLUDES += $(LOCAL_PATH)/../thirdparty/CoreRT/src/Native/inc/unix
LOCAL_C_INCLUDES += $(LOCAL_PATH)/../thirdparty/DirectXMath/Inc

LOCAL_LDFLAGS :=
LOCAL_LDFLAGS += -finput-charset=UTF-8 -fexec-charset=UTF-8
# the "dynamic linker" can't recognize the old dtags
LOCAL_LDFLAGS += -Wl,--enable-new-dtags
# the "chrpath" can only make path shorter
# LOCAL_LDFLAGS += -Wl,-rpath,XORIGIN 
LOCAL_LDFLAGS += -Wl,--version-script,$(LOCAL_PATH)/libNativeActivity.map

LOCAL_LDLIBS :=
LOCAL_LDLIBS += -landroid

LOCAL_STATIC_LIBRARIES :=
LOCAL_STATIC_LIBRARIES += ImportAsset

LOCAL_SHARED_LIBRARIES :=
LOCAL_SHARED_LIBRARIES += BRX

include $(BUILD_SHARED_LIBRARY)

# ImportAsset

include $(CLEAR_VARS)

LOCAL_MODULE := ImportAsset

LOCAL_SRC_FILES := $(LOCAL_PATH)/../thirdparty/Import-Asset/build-android/debug_$(APP_DEBUG)/obj/local/$(TARGET_ARCH_ABI)/libImportAsset$(TARGET_LIB_EXTENSION)

include $(PREBUILT_STATIC_LIBRARY)

# BRX

include $(CLEAR_VARS)

LOCAL_MODULE := BRX

LOCAL_SRC_FILES := $(LOCAL_PATH)/../thirdparty/Brioche/build-android/debug_$(APP_DEBUG)/lib/$(TARGET_ARCH_ABI)/libBRX$(TARGET_SONAME_EXTENSION)

include $(PREBUILT_SHARED_LIBRARY)

# VkLayer_khronos_validation

ifeq (true, $(APP_DEBUG))

include $(CLEAR_VARS)

LOCAL_MODULE := VkLayer_khronos_validation

LOCAL_SRC_FILES := $(LOCAL_PATH)/../thirdparty/Brioche/thirdparty/Vulkan-ValidationLayers/bin/android/$(TARGET_ARCH_ABI)/libVkLayer_khronos_validation$(TARGET_SONAME_EXTENSION)

include $(PREBUILT_SHARED_LIBRARY)

endif
