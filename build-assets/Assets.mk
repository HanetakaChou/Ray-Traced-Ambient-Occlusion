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
ASSETS_DIR := $(LOCAL_PATH)/../assets
HEADERS_DIR := $(LOCAL_PATH)/../assets/bin2h
PYTHON_PATH := python
BIN2H_PATH := $(ASSETS_DIR)/bin2h.py

all : \
	$(HEADERS_DIR)/_internal_the-white-room.gltf.inl \
	$(HEADERS_DIR)/_internal_the-white-room.bin.inl \
	$(HEADERS_DIR)/_internal_the-white-room-1.dds.inl \
	$(HEADERS_DIR)/_internal_the-white-room-1_s.dds.inl \
	$(HEADERS_DIR)/_internal_the-white-room-2.dds.inl \
	$(HEADERS_DIR)/_internal_the-white-room-2_n.dds.inl \
	$(HEADERS_DIR)/_internal_the-white-room-3.dds.inl \
	$(HEADERS_DIR)/_internal_the-white-room-4.dds.inl \
	$(HEADERS_DIR)/_internal_keqing-lolita-love-you.gltf.inl \
	$(HEADERS_DIR)/_internal_keqing-lolita-love-you.bin.inl \
	$(HEADERS_DIR)/_internal_keqing-lolita.dds.inl

$(HEADERS_DIR)/_internal_the-white-room.gltf.inl : $(ASSETS_DIR)/the-white-room/the-white-room.gltf
	$(HIDE) $(call host-mkdir,$(HEADERS_DIR))
	$(HIDE) "$(PYTHON_PATH)" "$(BIN2H_PATH)" "$(ASSETS_DIR)/the-white-room/the-white-room.gltf" "$(HEADERS_DIR)/_internal_the-white-room.gltf.inl" 

$(HEADERS_DIR)/_internal_the-white-room.bin.inl : $(ASSETS_DIR)/the-white-room/the-white-room.bin
	$(HIDE) $(call host-mkdir,$(HEADERS_DIR))
	$(HIDE) "$(PYTHON_PATH)" "$(BIN2H_PATH)" "$(ASSETS_DIR)/the-white-room/the-white-room.bin" "$(HEADERS_DIR)/_internal_the-white-room.bin.inl" 

$(HEADERS_DIR)/_internal_the-white-room-1.dds.inl : $(ASSETS_DIR)/the-white-room/the-white-room-1.dds
	$(HIDE) $(call host-mkdir,$(HEADERS_DIR))
	$(HIDE) "$(PYTHON_PATH)" "$(BIN2H_PATH)" "$(ASSETS_DIR)/the-white-room/the-white-room-1.dds" "$(HEADERS_DIR)/_internal_the-white-room-1.dds.inl"  

$(HEADERS_DIR)/_internal_the-white-room-1_s.dds.inl : $(ASSETS_DIR)/the-white-room/the-white-room-1_s.dds
	$(HIDE) $(call host-mkdir,$(HEADERS_DIR))
	$(HIDE) "$(PYTHON_PATH)" "$(BIN2H_PATH)" "$(ASSETS_DIR)/the-white-room/the-white-room-1_s.dds" "$(HEADERS_DIR)/_internal_the-white-room-1_s.dds.inl"  

$(HEADERS_DIR)/_internal_the-white-room-2.dds.inl : $(ASSETS_DIR)/the-white-room/the-white-room-2.dds
	$(HIDE) $(call host-mkdir,$(HEADERS_DIR))
	$(HIDE) "$(PYTHON_PATH)" "$(BIN2H_PATH)" "$(ASSETS_DIR)/the-white-room/the-white-room-2.dds" "$(HEADERS_DIR)/_internal_the-white-room-2.dds.inl"  

$(HEADERS_DIR)/_internal_the-white-room-2_n.dds.inl : $(ASSETS_DIR)/the-white-room/the-white-room-2_n.dds
	$(HIDE) $(call host-mkdir,$(HEADERS_DIR))
	$(HIDE) "$(PYTHON_PATH)" "$(BIN2H_PATH)" "$(ASSETS_DIR)/the-white-room/the-white-room-2_n.dds" "$(HEADERS_DIR)/_internal_the-white-room-2_n.dds.inl"  

$(HEADERS_DIR)/_internal_the-white-room-3.dds.inl : $(ASSETS_DIR)/the-white-room/the-white-room-3.dds
	$(HIDE) $(call host-mkdir,$(HEADERS_DIR))
	$(HIDE) "$(PYTHON_PATH)" "$(BIN2H_PATH)" "$(ASSETS_DIR)/the-white-room/the-white-room-3.dds" "$(HEADERS_DIR)/_internal_the-white-room-3.dds.inl"  

$(HEADERS_DIR)/_internal_the-white-room-4.dds.inl : $(ASSETS_DIR)/the-white-room/the-white-room-4.dds
	$(HIDE) $(call host-mkdir,$(HEADERS_DIR))
	$(HIDE) "$(PYTHON_PATH)" "$(BIN2H_PATH)" "$(ASSETS_DIR)/the-white-room/the-white-room-4.dds" "$(HEADERS_DIR)/_internal_the-white-room-4.dds.inl"  

$(HEADERS_DIR)/_internal_keqing-lolita-love-you.gltf.inl : $(ASSETS_DIR)/keqing-lolita/keqing-lolita-love-you.gltf
	$(HIDE) $(call host-mkdir,$(HEADERS_DIR))
	$(HIDE) "$(PYTHON_PATH)" "$(BIN2H_PATH)" "$(ASSETS_DIR)/keqing-lolita/keqing-lolita-love-you.gltf" "$(HEADERS_DIR)/_internal_keqing-lolita-love-you.gltf.inl" 

$(HEADERS_DIR)/_internal_keqing-lolita-love-you.bin.inl : $(ASSETS_DIR)/keqing-lolita/keqing-lolita-love-you.bin
	$(HIDE) $(call host-mkdir,$(HEADERS_DIR))
	$(HIDE) "$(PYTHON_PATH)" "$(BIN2H_PATH)" "$(ASSETS_DIR)/keqing-lolita/keqing-lolita-love-you.bin" "$(HEADERS_DIR)/_internal_keqing-lolita-love-you.bin.inl" 

$(HEADERS_DIR)/_internal_keqing-lolita.dds.inl : $(ASSETS_DIR)/keqing-lolita/keqing-lolita.dds
	$(HIDE) $(call host-mkdir,$(HEADERS_DIR))
	$(HIDE) "$(PYTHON_PATH)" "$(BIN2H_PATH)" "$(ASSETS_DIR)/keqing-lolita/keqing-lolita.dds" "$(HEADERS_DIR)/_internal_keqing-lolita.dds.inl"  

clean:
	$(HIDE) $(call host-rm,$(HEADERS_DIR)/_internal_the-white-room.gltf.inl)
	$(HIDE) $(call host-rm,$(HEADERS_DIR)/_internal_the-white-room.bin.inl)
	$(HIDE) $(call host-rm,$(HEADERS_DIR)/_internal_the-white-room-1.dds.inl)
	$(HIDE) $(call host-rm,$(HEADERS_DIR)/_internal_the-white-room-1_s.dds.inl)
	$(HIDE) $(call host-rm,$(HEADERS_DIR)/_internal_the-white-room-2.dds.inl)
	$(HIDE) $(call host-rm,$(HEADERS_DIR)/_internal_the-white-room-2_n.dds.inl)
	$(HIDE) $(call host-rm,$(HEADERS_DIR)/_internal_the-white-room-3.dds.inl)
	$(HIDE) $(call host-rm,$(HEADERS_DIR)/_internal_the-white-room-4.dds.inl)
	$(HIDE) $(call host-rm,$(HEADERS_DIR)/_internal_keqing-lolita-love-you.gltf.inl)
	$(HIDE) $(call host-rm,$(HEADERS_DIR)/_internal_keqing-lolita-love-you.bin.inl)
	$(HIDE) $(call host-rm,$(HEADERS_DIR)/_internal_keqing-lolita.dds.inl)

.PHONY : \
	all \
	clean
