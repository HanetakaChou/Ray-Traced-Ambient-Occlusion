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

HIDE := @

LOCAL_PATH := $(realpath $(dir $(lastword $(MAKEFILE_LIST))))
ifeq (true, $(APP_DEBUG))
	BIN_DIR := $(LOCAL_PATH)/bin/debug
	OBJ_DIR := $(LOCAL_PATH)/obj/debug
else
	BIN_DIR := $(LOCAL_PATH)/bin/release
	OBJ_DIR := $(LOCAL_PATH)/obj/release
endif
SOURCE_DIR := $(LOCAL_PATH)/../source
THIRD_PARTY_DIR := $(LOCAL_PATH)/../thirdparty

CC := clang++

C_FLAGS := 
C_FLAGS += -Wall -Werror=return-type
# C_FLAGS += -fvisibility=hidden
C_FLAGS += -fPIE -fPIC
C_FLAGS += -pthread
ifeq (true, $(APP_DEBUG))
	C_FLAGS += -g -O0 -UNDEBUG
else
	C_FLAGS += -O2 -DNDEBUG
endif
C_FLAGS += -Dbrx_init_unknown_device=brx_init_vk_device
C_FLAGS += -Dbrx_destroy_unknown_device=brx_destroy_vk_device
C_FLAGS += -I$(LOCAL_PATH)/../shaders/spirv
C_FLAGS += -DPAL_STDCPP_COMPAT=1
C_FLAGS += -I$(THIRD_PARTY_DIR)/CoreRT/src/Native/inc/unix
C_FLAGS += -I$(THIRD_PARTY_DIR)/DirectXMath/Inc
C_FLAGS += -std=c++17

LD_FLAGS := 
LD_FLAGS += -pthread
LD_FLAGS += -Wl,--no-undefined
LD_FLAGS += -Wl,--enable-new-dtags 
LD_FLAGS += -Wl,-rpath,'$$ORIGIN'
ifneq (true, $(APP_DEBUG))
	LD_FLAGS += -s
endif

all :  \
	$(BIN_DIR)/Ray-Traced-Ambient-Occlusion-Linux

# Link
ifeq (true, $(APP_DEBUG))
$(BIN_DIR)/Ray-Traced-Ambient-Occlusion-Linux: \
	$(OBJ_DIR)/Demo-assets-assets.o \
	$(OBJ_DIR)/Demo-assets-the_white_room_gltf.o \
	$(OBJ_DIR)/Demo-assets-the_white_room_bin.o \
	$(OBJ_DIR)/Demo-assets-the_white_room_1_dds.o \
	$(OBJ_DIR)/Demo-assets-the_white_room_1_s_dds.o \
	$(OBJ_DIR)/Demo-assets-the_white_room_2_dds.o \
	$(OBJ_DIR)/Demo-assets-the_white_room_2_n_dds.o \
	$(OBJ_DIR)/Demo-assets-the_white_room_3_dds.o \
	$(OBJ_DIR)/Demo-assets-the_white_room_4_dds.o \
	$(OBJ_DIR)/Demo-assets-keqing_lolita_love_you_gltf.o \
	$(OBJ_DIR)/Demo-assets-keqing_lolita_love_you_bin.o \
	$(OBJ_DIR)/Demo-assets-keqing_lolita_dds.o \
	$(OBJ_DIR)/Demo-support-camera_controller.o \
	$(OBJ_DIR)/Demo-support-main.o \
	$(OBJ_DIR)/Demo-support-renderer.o \
	$(OBJ_DIR)/Demo-support-tick_count.o \
	$(OBJ_DIR)/Demo-demo.o \
	$(OBJ_DIR)/Demo-thirdparty-DXUT-Optional-DXUTcamera.o \
	$(OBJ_DIR)/libImportAsset.a \
	$(BIN_DIR)/libBRX.so \
	$(BIN_DIR)/libVkLayer_khronos_validation.so \
	$(BIN_DIR)/VkLayer_khronos_validation.json \
	$(LOCAL_PATH)/Demo-Linux.map
else
$(BIN_DIR)/Ray-Traced-Ambient-Occlusion-Linux: \
	$(OBJ_DIR)/Demo-assets-assets.o \
	$(OBJ_DIR)/Demo-assets-the_white_room_gltf.o \
	$(OBJ_DIR)/Demo-assets-the_white_room_bin.o \
	$(OBJ_DIR)/Demo-assets-the_white_room_1_dds.o \
	$(OBJ_DIR)/Demo-assets-the_white_room_1_s_dds.o \
	$(OBJ_DIR)/Demo-assets-the_white_room_2_dds.o \
	$(OBJ_DIR)/Demo-assets-the_white_room_2_n_dds.o \
	$(OBJ_DIR)/Demo-assets-the_white_room_3_dds.o \
	$(OBJ_DIR)/Demo-assets-the_white_room_4_dds.o \
	$(OBJ_DIR)/Demo-assets-keqing_lolita_love_you_gltf.o \
	$(OBJ_DIR)/Demo-assets-keqing_lolita_love_you_bin.o \
	$(OBJ_DIR)/Demo-assets-keqing_lolita_dds.o \
	$(OBJ_DIR)/Demo-support-camera_controller.o \
	$(OBJ_DIR)/Demo-support-main.o \
	$(OBJ_DIR)/Demo-support-renderer.o \
	$(OBJ_DIR)/Demo-support-tick_count.o \
	$(OBJ_DIR)/Demo-demo.o \
	$(OBJ_DIR)/Demo-thirdparty-DXUT-Optional-DXUTcamera.o \
	$(OBJ_DIR)/libImportAsset.a \
	$(BIN_DIR)/libBRX.so \
	$(LOCAL_PATH)/Demo-Linux.map
endif
	$(HIDE) mkdir -p $(BIN_DIR)
	$(HIDE) $(CC) -pie $(LD_FLAGS) \
		-Wl,--version-script=$(LOCAL_PATH)/Demo-Linux.map \
		$(OBJ_DIR)/Demo-assets-assets.o \
		$(OBJ_DIR)/Demo-assets-the_white_room_gltf.o \
		$(OBJ_DIR)/Demo-assets-the_white_room_bin.o \
		$(OBJ_DIR)/Demo-assets-the_white_room_1_dds.o \
		$(OBJ_DIR)/Demo-assets-the_white_room_1_s_dds.o \
		$(OBJ_DIR)/Demo-assets-the_white_room_2_dds.o \
		$(OBJ_DIR)/Demo-assets-the_white_room_2_n_dds.o \
		$(OBJ_DIR)/Demo-assets-the_white_room_3_dds.o \
		$(OBJ_DIR)/Demo-assets-the_white_room_4_dds.o \
		$(OBJ_DIR)/Demo-assets-keqing_lolita_love_you_gltf.o \
		$(OBJ_DIR)/Demo-assets-keqing_lolita_love_you_bin.o \
		$(OBJ_DIR)/Demo-assets-keqing_lolita_dds.o \
		$(OBJ_DIR)/Demo-support-camera_controller.o \
		$(OBJ_DIR)/Demo-support-main.o \
		$(OBJ_DIR)/Demo-support-renderer.o \
		$(OBJ_DIR)/Demo-support-tick_count.o \
		$(OBJ_DIR)/Demo-demo.o \
		$(OBJ_DIR)/Demo-thirdparty-DXUT-Optional-DXUTcamera.o \
		$(OBJ_DIR)/libImportAsset.a \
		-L$(BIN_DIR) -lBRX \
		-lxcb \
		-o $(BIN_DIR)/Ray-Traced-Ambient-Occlusion-Linux

# Compile
$(OBJ_DIR)/Demo-assets-assets.o: $(SOURCE_DIR)/../assets/assets.cpp
	$(HIDE) mkdir -p $(OBJ_DIR)
	$(HIDE) $(CC) -c $(C_FLAGS) $(SOURCE_DIR)/../assets/assets.cpp -MD -MF $(OBJ_DIR)/Demo-assets-assets.d -o $(OBJ_DIR)/Demo-assets-assets.o

$(OBJ_DIR)/Demo-assets-the_white_room_gltf.o: $(SOURCE_DIR)/../assets/the_white_room_gltf.cpp
	$(HIDE) mkdir -p $(OBJ_DIR)
	$(HIDE) $(CC) -c $(C_FLAGS) $(SOURCE_DIR)/../assets/the_white_room_gltf.cpp -MD -MF $(OBJ_DIR)/Demo-assets-the_white_room_gltf.d -o $(OBJ_DIR)/Demo-assets-the_white_room_gltf.o

$(OBJ_DIR)/Demo-assets-the_white_room_bin.o: $(SOURCE_DIR)/../assets/the_white_room_bin.cpp
	$(HIDE) mkdir -p $(OBJ_DIR)
	$(HIDE) $(CC) -c $(C_FLAGS) $(SOURCE_DIR)/../assets/the_white_room_bin.cpp -MD -MF $(OBJ_DIR)/Demo-assets-the_white_room_bin.d -o $(OBJ_DIR)/Demo-assets-the_white_room_bin.o

$(OBJ_DIR)/Demo-assets-the_white_room_1_dds.o: $(SOURCE_DIR)/../assets/the_white_room_1_dds.cpp
	$(HIDE) mkdir -p $(OBJ_DIR)
	$(HIDE) $(CC) -c $(C_FLAGS) $(SOURCE_DIR)/../assets/the_white_room_1_dds.cpp -MD -MF $(OBJ_DIR)/Demo-assets-the_white_room_1_dds.d -o $(OBJ_DIR)/Demo-assets-the_white_room_1_dds.o

$(OBJ_DIR)/Demo-assets-the_white_room_1_s_dds.o: $(SOURCE_DIR)/../assets/the_white_room_1_s_dds.cpp
	$(HIDE) mkdir -p $(OBJ_DIR)
	$(HIDE) $(CC) -c $(C_FLAGS) $(SOURCE_DIR)/../assets/the_white_room_1_s_dds.cpp -MD -MF $(OBJ_DIR)/Demo-assets-the_white_room_1_s_dds.d -o $(OBJ_DIR)/Demo-assets-the_white_room_1_s_dds.o

$(OBJ_DIR)/Demo-assets-the_white_room_2_dds.o: $(SOURCE_DIR)/../assets/the_white_room_2_dds.cpp
	$(HIDE) mkdir -p $(OBJ_DIR)
	$(HIDE) $(CC) -c $(C_FLAGS) $(SOURCE_DIR)/../assets/the_white_room_2_dds.cpp -MD -MF $(OBJ_DIR)/Demo-assets-the_white_room_2_dds.d -o $(OBJ_DIR)/Demo-assets-the_white_room_2_dds.o

$(OBJ_DIR)/Demo-assets-the_white_room_2_n_dds.o: $(SOURCE_DIR)/../assets/the_white_room_2_n_dds.cpp
	$(HIDE) mkdir -p $(OBJ_DIR)
	$(HIDE) $(CC) -c $(C_FLAGS) $(SOURCE_DIR)/../assets/the_white_room_2_n_dds.cpp -MD -MF $(OBJ_DIR)/Demo-assets-the_white_room_2_n_dds.d -o $(OBJ_DIR)/Demo-assets-the_white_room_2_n_dds.o

$(OBJ_DIR)/Demo-assets-the_white_room_3_dds.o: $(SOURCE_DIR)/../assets/the_white_room_3_dds.cpp
	$(HIDE) mkdir -p $(OBJ_DIR)
	$(HIDE) $(CC) -c $(C_FLAGS) $(SOURCE_DIR)/../assets/the_white_room_3_dds.cpp -MD -MF $(OBJ_DIR)/Demo-assets-the_white_room_3_dds.d -o $(OBJ_DIR)/Demo-assets-the_white_room_3_dds.o

$(OBJ_DIR)/Demo-assets-the_white_room_4_dds.o: $(SOURCE_DIR)/../assets/the_white_room_4_dds.cpp
	$(HIDE) mkdir -p $(OBJ_DIR)
	$(HIDE) $(CC) -c $(C_FLAGS) $(SOURCE_DIR)/../assets/the_white_room_4_dds.cpp -MD -MF $(OBJ_DIR)/Demo-assets-the_white_room_4_dds.d -o $(OBJ_DIR)/Demo-assets-the_white_room_4_dds.o

$(OBJ_DIR)/Demo-assets-keqing_lolita_love_you_gltf.o: $(SOURCE_DIR)/../assets/keqing_lolita_love_you_gltf.cpp
	$(HIDE) mkdir -p $(OBJ_DIR)
	$(HIDE) $(CC) -c $(C_FLAGS) $(SOURCE_DIR)/../assets/keqing_lolita_love_you_gltf.cpp -MD -MF $(OBJ_DIR)/Demo-assets-keqing_lolita_love_you_gltf.d -o $(OBJ_DIR)/Demo-assets-keqing_lolita_love_you_gltf.o

$(OBJ_DIR)/Demo-assets-keqing_lolita_love_you_bin.o: $(SOURCE_DIR)/../assets/keqing_lolita_love_you_bin.cpp
	$(HIDE) mkdir -p $(OBJ_DIR)
	$(HIDE) $(CC) -c $(C_FLAGS) $(SOURCE_DIR)/../assets/keqing_lolita_love_you_bin.cpp -MD -MF $(OBJ_DIR)/Demo-assets-keqing_lolita_love_you_bin.d -o $(OBJ_DIR)/Demo-assets-keqing_lolita_love_you_bin.o

$(OBJ_DIR)/Demo-assets-keqing_lolita_dds.o: $(SOURCE_DIR)/../assets/keqing_lolita_dds.cpp
	$(HIDE) mkdir -p $(OBJ_DIR)
	$(HIDE) $(CC) -c $(C_FLAGS) $(SOURCE_DIR)/../assets/keqing_lolita_dds.cpp -MD -MF $(OBJ_DIR)/Demo-assets-keqing_lolita_dds.d -o $(OBJ_DIR)/Demo-assets-keqing_lolita_dds.o

$(OBJ_DIR)/Demo-support-camera_controller.o: $(SOURCE_DIR)/support/camera_controller.cpp
	$(HIDE) mkdir -p $(OBJ_DIR)
	$(HIDE) $(CC) -c $(C_FLAGS) $(SOURCE_DIR)/support/camera_controller.cpp -MD -MF $(OBJ_DIR)/Demo-support-camera_controller.d -o $(OBJ_DIR)/Demo-support-camera_controller.o

$(OBJ_DIR)/Demo-support-main.o: $(SOURCE_DIR)/support/main.cpp
	$(HIDE) mkdir -p $(OBJ_DIR)
	$(HIDE) $(CC) -c $(C_FLAGS) $(SOURCE_DIR)/support/main.cpp -MD -MF $(OBJ_DIR)/Demo-support-main.d -o $(OBJ_DIR)/Demo-support-main.o

$(OBJ_DIR)/Demo-support-renderer.o: $(SOURCE_DIR)/support/renderer.cpp
	$(HIDE) mkdir -p $(OBJ_DIR)
	$(HIDE) $(CC) -c $(C_FLAGS) $(SOURCE_DIR)/support/renderer.cpp -MD -MF $(OBJ_DIR)/Demo-support-renderer.d -o $(OBJ_DIR)/Demo-support-renderer.o

$(OBJ_DIR)/Demo-support-tick_count.o: $(SOURCE_DIR)/support/tick_count.cpp
	$(HIDE) mkdir -p $(OBJ_DIR)
	$(HIDE) $(CC) -c $(C_FLAGS) $(SOURCE_DIR)/support/tick_count.cpp -MD -MF $(OBJ_DIR)/Demo-support-tick_count.d -o $(OBJ_DIR)/Demo-support-tick_count.o

$(OBJ_DIR)/Demo-demo.o: $(SOURCE_DIR)/demo.cpp
	$(HIDE) mkdir -p $(OBJ_DIR)
	$(HIDE) $(CC) -c $(C_FLAGS) $(SOURCE_DIR)/demo.cpp -MD -MF $(OBJ_DIR)/Demo-demo.d -o $(OBJ_DIR)/Demo-demo.o

$(OBJ_DIR)/Demo-thirdparty-DXUT-Optional-DXUTcamera.o: $(SOURCE_DIR)/../thirdparty/DXUT/Optional/DXUTcamera.cpp
	$(HIDE) mkdir -p $(OBJ_DIR)
	$(HIDE) $(CC) -c $(C_FLAGS) $(SOURCE_DIR)/../thirdparty/DXUT/Optional/DXUTcamera.cpp -MD -MF $(OBJ_DIR)/Demo-thirdparty-DXUT-Optional-DXUTcamera.d -o $(OBJ_DIR)/Demo-thirdparty-DXUT-Optional-DXUTcamera.o

# Copy
ifeq (true, $(APP_DEBUG))
CONFIG_NAME := debug
else
CONFIG_NAME := release
endif
$(BIN_DIR)/libBRX.so: $(THIRD_PARTY_DIR)/Brioche/build-linux/bin/$(CONFIG_NAME)/libBRX.so
	$(HIDE) mkdir -p $(BIN_DIR)
	$(HIDE) cp -f $(THIRD_PARTY_DIR)/Brioche/build-linux/bin/$(CONFIG_NAME)/libBRX.so $(BIN_DIR)/libBRX.so

$(OBJ_DIR)/libImportAsset.a: $(THIRD_PARTY_DIR)/Import-Asset/build-linux/bin/$(CONFIG_NAME)/libImportAsset.a
	$(HIDE) mkdir -p $(BIN_DIR)
	$(HIDE) cp -f $(THIRD_PARTY_DIR)/Import-Asset/build-linux/bin/$(CONFIG_NAME)/libImportAsset.a $(OBJ_DIR)/libImportAsset.a

$(BIN_DIR)/libVkLayer_khronos_validation.so: $(THIRD_PARTY_DIR)/Brioche/thirdparty/Vulkan-ValidationLayers/bin/linux/x64/libVkLayer_khronos_validation.so
	$(HIDE) mkdir -p $(BIN_DIR)
	$(HIDE) cp -f $(THIRD_PARTY_DIR)/Brioche/thirdparty/Vulkan-ValidationLayers/bin/linux/x64/libVkLayer_khronos_validation.so $(BIN_DIR)/libVkLayer_khronos_validation.so

$(BIN_DIR)/VkLayer_khronos_validation.json: $(THIRD_PARTY_DIR)/Brioche/thirdparty/Vulkan-ValidationLayers/bin/linux/x64/VkLayer_khronos_validation.json
	$(HIDE) mkdir -p $(BIN_DIR)
	$(HIDE) cp -f $(THIRD_PARTY_DIR)/Brioche/thirdparty/Vulkan-ValidationLayers/bin/linux/x64/VkLayer_khronos_validation.json $(BIN_DIR)/VkLayer_khronos_validation.json

-include \
	$(OBJ_DIR)/Demo-assets-assets.d \
	$(OBJ_DIR)/Demo-assets-the_white_room_gltf.d \
	$(OBJ_DIR)/Demo-assets-the_white_room_bin.d \
	$(OBJ_DIR)/Demo-assets-the_white_room_1_dds.d \
	$(OBJ_DIR)/Demo-assets-the_white_room_1_s_dds.d \
	$(OBJ_DIR)/Demo-assets-the_white_room_2_dds.d \
	$(OBJ_DIR)/Demo-assets-the_white_room_2_n_dds.d \
	$(OBJ_DIR)/Demo-assets-the_white_room_3_dds.d \
	$(OBJ_DIR)/Demo-assets-the_white_room_4_dds.d \
	$(OBJ_DIR)/Demo-assets-keqing_lolita_love_you_gltf.d \
	$(OBJ_DIR)/Demo-assets-keqing_lolita_love_you_bin.d \
	$(OBJ_DIR)/Demo-assets-keqing_lolita_dds.d \
	$(OBJ_DIR)/Demo-support-camera_controller.d \
	$(OBJ_DIR)/Demo-support-main.d \
	$(OBJ_DIR)/Demo-support-renderer.d \
	$(OBJ_DIR)/Demo-support-tick_count.d \
	$(OBJ_DIR)/Demo-demo.d \
	$(OBJ_DIR)/Demo-thirdparty-DXUT-Optional-DXUTcamera.d

clean:
	$(HIDE) rm -f $(BIN_DIR)/Ray-Traced-Ambient-Occlusion-Linux
	$(HIDE) rm -f $(OBJ_DIR)/Demo-assets-assets.o
	$(HIDE) rm -f $(OBJ_DIR)/Demo-assets-the_white_room_gltf.o
	$(HIDE) rm -f $(OBJ_DIR)/Demo-assets-the_white_room_bin.o
	$(HIDE) rm -f $(OBJ_DIR)/Demo-assets-the_white_room_1_dds.o
	$(HIDE) rm -f $(OBJ_DIR)/Demo-assets-the_white_room_1_s_dds.o
	$(HIDE) rm -f $(OBJ_DIR)/Demo-assets-the_white_room_2_dds.o
	$(HIDE) rm -f $(OBJ_DIR)/Demo-assets-the_white_room_2_n_dds.o
	$(HIDE) rm -f $(OBJ_DIR)/Demo-assets-the_white_room_3_dds.o
	$(HIDE) rm -f $(OBJ_DIR)/Demo-assets-the_white_room_4_dds.o
	$(HIDE) rm -f $(OBJ_DIR)/Demo-assets-keqing_lolita_love_you_gltf.o
	$(HIDE) rm -f $(OBJ_DIR)/Demo-assets-keqing_lolita_love_you_bin.o
	$(HIDE) rm -f $(OBJ_DIR)/Demo-assets-keqing_lolita_dds.o
	$(HIDE) rm -f $(OBJ_DIR)/Demo-support-camera_controller.o
	$(HIDE) rm -f $(OBJ_DIR)/Demo-support-main.o
	$(HIDE) rm -f $(OBJ_DIR)/Demo-support-renderer.o
	$(HIDE) rm -f $(OBJ_DIR)/Demo-support-tick_count.o
	$(HIDE) rm -f $(OBJ_DIR)/Demo-demo.o
	$(HIDE) rm -f $(OBJ_DIR)/Demo-thirdparty-DXUT-Optional-DXUTcamera.o
	$(HIDE) rm -f $(OBJ_DIR)/Demo-assets-assets.d
	$(HIDE) rm -f $(OBJ_DIR)/Demo-assets-living_room_living_room_gltf.d
	$(HIDE) rm -f $(OBJ_DIR)/Demo-assets-living_room_living_room_bin.d
	$(HIDE) rm -f $(OBJ_DIR)/Demo-assets-living_room_apple_dds.d
	$(HIDE) rm -f $(OBJ_DIR)/Demo-assets-living_room_book_spines_dds.d
	$(HIDE) rm -f $(OBJ_DIR)/Demo-assets-living_room_carpet_text3a_normal_dds.d
	$(HIDE) rm -f $(OBJ_DIR)/Demo-assets-living_room_carpet_text3b_dds.d
	$(HIDE) rm -f $(OBJ_DIR)/Demo-assets-living_room_cushion_green_circles_dds.d
	$(HIDE) rm -f $(OBJ_DIR)/Demo-assets-living_room_cushion_green_circles_normal_dds.d
	$(HIDE) rm -f $(OBJ_DIR)/Demo-assets-living_room_cushion_purple_yellow_stripe_dds.d
	$(HIDE) rm -f $(OBJ_DIR)/Demo-assets-living_room_cushion_purple_yellow_stripe_normal_dds.d
	$(HIDE) rm -f $(OBJ_DIR)/Demo-assets-living_room_cushion_stripe_purple_dds.d
	$(HIDE) rm -f $(OBJ_DIR)/Demo-assets-living_room_cushion_stripe_purple_normal_dds.d
	$(HIDE) rm -f $(OBJ_DIR)/Demo-assets-living_room_magazine_dds.d
	$(HIDE) rm -f $(OBJ_DIR)/Demo-assets-living_room_overexposed_street2_dds.d
	$(HIDE) rm -f $(OBJ_DIR)/Demo-assets-living_room_photo1_dds.d
	$(HIDE) rm -f $(OBJ_DIR)/Demo-assets-living_room_photo2_dds.d
	$(HIDE) rm -f $(OBJ_DIR)/Demo-assets-living_room_photo3_dds.d
	$(HIDE) rm -f $(OBJ_DIR)/Demo-assets-living_room_photo4_dds.d
	$(HIDE) rm -f $(OBJ_DIR)/Demo-assets-living_room_pic3_dds.d
	$(HIDE) rm -f $(OBJ_DIR)/Demo-assets-living_room_pic5wide_dds.d
	$(HIDE) rm -f $(OBJ_DIR)/Demo-assets-living_room_shade_paper_dds.d
	$(HIDE) rm -f $(OBJ_DIR)/Demo-assets-living_room_shade_stripes_dds.d
	$(HIDE) rm -f $(OBJ_DIR)/Demo-assets-living_room_wood_floorboards_texture_softer_dds.d
	$(HIDE) rm -f $(OBJ_DIR)/Demo-assets-sparkle_sparkle_gltf.d
	$(HIDE) rm -f $(OBJ_DIR)/Demo-assets-sparkle_sparkle_bin.d
	$(HIDE) rm -f $(OBJ_DIR)/Demo-assets-sparkle_clothing_dds.d
	$(HIDE) rm -f $(OBJ_DIR)/Demo-assets-sparkle_eye_dds.d
	$(HIDE) rm -f $(OBJ_DIR)/Demo-assets-sparkle_face_dds.d
	$(HIDE) rm -f $(OBJ_DIR)/Demo-assets-sparkle_hair_dds.d
	$(HIDE) rm -f $(OBJ_DIR)/Demo-support-camera_controller.d
	$(HIDE) rm -f $(OBJ_DIR)/Demo-support-main.d
	$(HIDE) rm -f $(OBJ_DIR)/Demo-support-renderer.d
	$(HIDE) rm -f $(OBJ_DIR)/Demo-support-tick_count.d
	$(HIDE) rm -f $(OBJ_DIR)/Demo-demo.d
	$(HIDE) rm -f $(OBJ_DIR)/Demo-thirdparty-DXUT-Optional-DXUTcamera.d
	$(HIDE) rm -f $(OBJ_DIR)/libImportAsset.a
	$(HIDE) rm -f $(BIN_DIR)/libBRX.so
ifeq (true, $(APP_DEBUG))
	$(HIDE) rm -f $(BIN_DIR)/libVkLayer_khronos_validation.so
	$(HIDE) rm -f $(BIN_DIR)/VkLayer_khronos_validation.json
endif

.PHONY : \
	all \
	clean