# Copyright (C) 2009 The Android Open Source Project
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

# ====================================================================
#
# Host system auto-detection.
#
# ====================================================================
ifeq ($(OS),Windows_NT)
	# On all modern variants of Windows (including Cygwin and Wine)
	# the OS environment variable is defined to 'Windows_NT'
	#
	# The value of PROCESSOR_ARCHITECTURE will be x86 or AMD64
	#
	HOST_OS := windows

	# Trying to detect that we're running from Cygwin is tricky
	# because we can't use $(OSTYPE): It's a Bash shell variable
	# that is not exported to sub-processes, and isn't defined by
	# other shells (for those with really weird setups).
	#
	# Instead, we assume that a program named /bin/uname.exe
	# that can be invoked and returns a valid value corresponds
	# to a Cygwin installation.
	#
	UNAME := $(shell /bin/uname.exe -s 2>NUL)
	ifneq (,$(filter CYGWIN% MINGW32% MINGW64%,$(UNAME)))
		HOST_OS := unix
		_ := $(shell rm -f NUL) # Cleaning up
	endif
else
	HOST_OS := unix
endif

# -----------------------------------------------------------------------------
# Function : host-mkdir
# Arguments: 1: directory path
# Usage    : $(call host-mkdir,<path>
# Rationale: This function expands to the host-specific shell command used
#            to create a path if it doesn't exist.
# -----------------------------------------------------------------------------
ifeq ($(HOST_OS),windows)
host-mkdir = md $(subst /,\,"$1") >NUL 2>NUL || rem
else
host-mkdir = mkdir -p $1
endif

# -----------------------------------------------------------------------------
# Function : host-rm
# Arguments: 1: list of files
# Usage    : $(call host-rm,<files>)
# Rationale: This function expands to the host-specific shell command used
#            to remove some files.
# -----------------------------------------------------------------------------
ifeq ($(HOST_OS),windows)
host-rm = \
	$(eval __host_rm_files := $(foreach __host_rm_file,$1,$(subst /,\,$(wildcard $(__host_rm_file)))))\
	$(if $(__host_rm_files),del /f/q $(__host_rm_files) >NUL 2>NUL || rem)
else
host-rm = rm -f $1
endif

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
SHADERS_DIR := $(LOCAL_PATH)/../shaders
ifeq (true, $(APP_DEBUG))
	DXIL_DIR := $(LOCAL_PATH)/../shaders/dxil/debug
else
	DXIL_DIR := $(LOCAL_PATH)/../shaders/dxil/release
endif
THIRD_PARTY_DIR := $(LOCAL_PATH)/../thirdparty
ifeq ($(OS),Windows_NT)
	HLSL_COMPILER_PATH := $(THIRD_PARTY_DIR)/DirectXShaderCompiler/bin/win32/x64/dxc.exe
else
	HLSL_COMPILER_PATH := $(THIRD_PARTY_DIR)/DirectXShaderCompiler/bin/linux/x64/dxc
endif

HLSL_COMPILER_DEBUG_FLAGS :=
ifeq (true, $(APP_DEBUG))
	HLSL_COMPILER_DEBUG_FLAGS += -Od -Zi -Qembed_debug
else
	HLSL_COMPILER_DEBUG_FLAGS += 
endif

all : \
	$(DXIL_DIR)/_internal_full_screen_transfer_vertex.inl \
	$(DXIL_DIR)/_internal_full_screen_transfer_fragment.inl \
	$(DXIL_DIR)/_internal_skin_compute.inl \
	$(DXIL_DIR)/_internal_gbuffer_compute.inl \
	$(DXIL_DIR)/_internal_ambient_occlusion_compute.inl

$(DXIL_DIR)/_internal_full_screen_transfer_vertex.inl $(DXIL_DIR)/_internal_full_screen_transfer_vertex.d : $(SHADERS_DIR)/full_screen_transfer_vertex.sl
	$(HIDE) $(call host-mkdir,$(DXIL_DIR))
	$(HIDE) "$(HLSL_COMPILER_PATH)" $(HLSL_COMPILER_DEBUG_FLAGS) -T vs_6_8 -MD -MF "$(DXIL_DIR)/_internal_full_screen_transfer_vertex.d" -Fo "$(DXIL_DIR)/_internal_full_screen_transfer_vertex.inl" "$(SHADERS_DIR)/full_screen_transfer_vertex.sl"
	$(HIDE) "$(HLSL_COMPILER_PATH)" $(HLSL_COMPILER_DEBUG_FLAGS) -T vs_6_8 -Fh "$(DXIL_DIR)/_internal_full_screen_transfer_vertex.inl" -Vn "full_screen_transfer_vertex_shader_module_code" "$(SHADERS_DIR)/full_screen_transfer_vertex.sl"

$(DXIL_DIR)/_internal_full_screen_transfer_fragment.inl $(DXIL_DIR)/_internal_full_screen_transfer_fragment.d : $(SHADERS_DIR)/full_screen_transfer_fragment.sl
	$(HIDE) $(call host-mkdir,$(DXIL_DIR))
	$(HIDE) "$(HLSL_COMPILER_PATH)" $(HLSL_COMPILER_DEBUG_FLAGS) -T ps_6_8 -MD -MF "$(DXIL_DIR)/_internal_full_screen_transfer_fragment.d" -Fo "$(DXIL_DIR)/_internal_full_screen_transfer_fragment.inl" "$(SHADERS_DIR)/full_screen_transfer_fragment.sl"
	$(HIDE) "$(HLSL_COMPILER_PATH)" $(HLSL_COMPILER_DEBUG_FLAGS) -T ps_6_8 -Fh "$(DXIL_DIR)/_internal_full_screen_transfer_fragment.inl" -Vn "full_screen_transfer_fragment_shader_module_code" "$(SHADERS_DIR)/full_screen_transfer_fragment.sl"

$(DXIL_DIR)/_internal_skin_compute.inl $(DXIL_DIR)/_internal_skin_compute.d : $(SHADERS_DIR)/skin_compute.sl
	$(HIDE) $(call host-mkdir,$(DXIL_DIR))
	$(HIDE) "$(HLSL_COMPILER_PATH)" $(HLSL_COMPILER_DEBUG_FLAGS) -T cs_6_8 -MD -MF "$(DXIL_DIR)/_internal_skin_compute.d" -Fo "$(DXIL_DIR)/_internal_skin_compute.inl" "$(SHADERS_DIR)/skin_compute.sl"
	$(HIDE) "$(HLSL_COMPILER_PATH)" $(HLSL_COMPILER_DEBUG_FLAGS) -T cs_6_8 -Fh "$(DXIL_DIR)/_internal_skin_compute.inl" -Vn "skin_compute_shader_module_code" "$(SHADERS_DIR)/skin_compute.sl"

$(DXIL_DIR)/_internal_gbuffer_compute.inl $(DXIL_DIR)/_internal_gbuffer_compute.d : $(SHADERS_DIR)/gbuffer_compute.sl
	$(HIDE) $(call host-mkdir,$(DXIL_DIR))
	$(HIDE) "$(HLSL_COMPILER_PATH)" $(HLSL_COMPILER_DEBUG_FLAGS) -T cs_6_8 -MD -MF "$(DXIL_DIR)/_internal_gbuffer_compute.d" -Fo "$(DXIL_DIR)/_internal_gbuffer_compute.inl" "$(SHADERS_DIR)/gbuffer_compute.sl"
	$(HIDE) "$(HLSL_COMPILER_PATH)" $(HLSL_COMPILER_DEBUG_FLAGS) -T cs_6_8 -Fh "$(DXIL_DIR)/_internal_gbuffer_compute.inl" -Vn "gbuffer_compute_shader_module_code" "$(SHADERS_DIR)/gbuffer_compute.sl"

$(DXIL_DIR)/_internal_ambient_occlusion_compute.inl $(DXIL_DIR)/_internal_ambient_occlusion_compute.d : $(SHADERS_DIR)/ambient_occlusion_compute.sl
	$(HIDE) $(call host-mkdir,$(DXIL_DIR))
	$(HIDE) "$(HLSL_COMPILER_PATH)" $(HLSL_COMPILER_DEBUG_FLAGS) -T cs_6_8 -MD -MF "$(DXIL_DIR)/_internal_ambient_occlusion_compute.d" -Fo "$(DXIL_DIR)/_internal_ambient_occlusion_compute.inl" "$(SHADERS_DIR)/ambient_occlusion_compute.sl"
	$(HIDE) "$(HLSL_COMPILER_PATH)" $(HLSL_COMPILER_DEBUG_FLAGS) -T cs_6_8 -Fh "$(DXIL_DIR)/_internal_ambient_occlusion_compute.inl" -Vn "ambient_occlusion_compute_shader_module_code" "$(SHADERS_DIR)/ambient_occlusion_compute.sl"

-include \
	$(DXIL_DIR)/_internal_full_screen_transfer_vertex.d \
	$(DXIL_DIR)/_internal_full_screen_transfer_fragment.d \
	$(DXIL_DIR)/_internal_skin_compute.d \
	$(DXIL_DIR)/_internal_gbuffer_compute.d \
	$(DXIL_DIR)/_internal_ambient_occlusion_compute.d

clean:
	$(HIDE) $(call host-rm,$(DXIL_DIR)/_internal_full_screen_transfer_vertex.inl)
	$(HIDE) $(call host-rm,$(DXIL_DIR)/_internal_full_screen_transfer_fragment.inl)
	$(HIDE) $(call host-rm,$(DXIL_DIR)/_internal_skin_compute.inl)
	$(HIDE) $(call host-rm,$(DXIL_DIR)/_internal_gbuffer_compute.inl)
	$(HIDE) $(call host-rm,$(DXIL_DIR)/_internal_ambient_occlusion_compute.inl)
	$(HIDE) $(call host-rm,$(DXIL_DIR)/_internal_full_screen_transfer_vertex.d)
	$(HIDE) $(call host-rm,$(DXIL_DIR)/_internal_full_screen_transfer_fragment.d)
	$(HIDE) $(call host-rm,$(DXIL_DIR)/_internal_skin_compute.d)
	$(HIDE) $(call host-rm,$(DXIL_DIR)/_internal_gbuffer_compute.d)
	$(HIDE) $(call host-rm,$(DXIL_DIR)/_internal_ambient_occlusion_compute.d)


.PHONY : \
	all \
	clean
