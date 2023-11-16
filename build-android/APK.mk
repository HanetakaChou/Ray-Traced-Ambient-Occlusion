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

# -----------------------------------------------------------------------------
# Macro    : my-dir
# Returns  : the directory of the current Makefile
# Usage    : $(my-dir)
# -----------------------------------------------------------------------------
my-dir = $(call parent-dir,$(lastword $(MAKEFILE_LIST)))

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
# Function : parent-dir
# Arguments: 1: path
# Returns  : Parent dir or path of $1, with final separator removed.
# -----------------------------------------------------------------------------
ifeq ($(HOST_OS),windows)
	# On Windows, defining parent-dir is a bit more tricky because the
	# GNU Make $(dir ...) function doesn't return an empty string when it
	# reaches the top of the directory tree, and we want to enforce this to
	# avoid infinite loops.
	#
	#   $(dir C:)     -> C:       (empty expected)
	#   $(dir C:/)    -> C:/      (empty expected)
	#   $(dir C:\)    -> C:\      (empty expected)
	#   $(dir C:/foo) -> C:/      (correct)
	#   $(dir C:\foo) -> C:\      (correct)
	#
	parent-dir = $(patsubst %/,%,$(strip \
		$(eval __dir_node := $(patsubst %/,%,$(subst \,/,$1)))\
		$(eval __dir_parent := $(dir $(__dir_node)))\
		$(filter-out $1,$(__dir_parent))\
		))
else
	parent-dir = $(patsubst %/,%,$(dir $(1:%/=%)))
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
# Function : host-cp
# Arguments: 1: source file
#            2: target file
# Usage    : $(call host-cp,<src-file>,<dst-file>)
# Rationale: This function expands to the host-specific shell command used
#            to copy a single file
# -----------------------------------------------------------------------------
ifeq ($(HOST_OS),windows)
host-cp = copy /b/y $(subst /,\,"$1" "$2") > NUL 2>NUL || rem
else
host-cp = cp -f $1 $2
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

# ANDROID_SDK_BUILD_TOOLS_VERSION := 33.0.2
# APP_PLATFORM:=android-28

HIDE := @

LOCAL_PATH := $(realpath $(call my-dir))
KEYSTORE_DIR := $(LOCAL_PATH)/keystore
RES_DIR := $(LOCAL_PATH)/res
FLAT_DIR := $(LOCAL_PATH)/flat
APK_DIR := $(LOCAL_PATH)/apk
AAPT2_PATH := $(ANDROID_SDK_DIR)/build-tools/$(ANDROID_SDK_BUILD_TOOLS_VERSION)/aapt2
ZIPALIGN_PATH := $(ANDROID_SDK_DIR)/build-tools/$(ANDROID_SDK_BUILD_TOOLS_VERSION)/zipalign
# TODO: 
# GitHub-hosted runner
# /usr/bin/bash: line 1: C:\Android\android-sdk\./build-tools/33.0.2/apksigner: No such file or directory
# Try to find "apksigner.bat"
APKSIGNER_PATH := $(wildcard $(ANDROID_SDK_DIR)/build-tools/$(ANDROID_SDK_BUILD_TOOLS_VERSION)/apksigner*)
ANDROID_JAR_PATH := $(ANDROID_SDK_DIR)/platforms/$(APP_PLATFORM)/android.jar
JAR_PATH := $(JDK_DIR)/bin/jar
KEYTOOL_PATH := $(JDK_DIR)/bin/keytool

all : $(APK_DIR)/demo-android.apk

$(KEYSTORE_DIR)/demo-android-debug.keystore :
	$(HIDE) $(call host-mkdir,$(KEYSTORE_DIR))
	$(HIDE) "$(KEYTOOL_PATH)" -genkey -v -keystore "$(KEYSTORE_DIR)/demo-android-debug.keystore" -storepass demo-android -storetype PKCS12 -keypass demo-android -alias demo-android-debug -keyalg RSA -keysize 3072 -validity 1024 -dname "CN=Demo-Android"

$(FLAT_DIR)/values_styles.arsc.flat : $(RES_DIR)/values/styles.xml
	$(HIDE) $(call host-mkdir,$(FLAT_DIR))
	$(HIDE) "$(AAPT2_PATH)" compile -o "$(FLAT_DIR)" "$(RES_DIR)\values\styles.xml"

$(APK_DIR)/demo-android-unaligned.apk : $(wildcard $(LOCAL_PATH)/lib/*/libdemo-android.so) $(FLAT_DIR)/values_styles.arsc.flat $(LOCAL_PATH)/AndroidManifest.xml 
	$(HIDE) $(call host-mkdir,$(APK_DIR))
	$(HIDE) "$(AAPT2_PATH)" link --debug-mode -I "$(ANDROID_JAR_PATH)" "$(FLAT_DIR)/values_styles.arsc.flat" -o "$(APK_DIR)/demo-android-unaligned.apk" --manifest "$(LOCAL_PATH)/AndroidManifest.xml"
	$(HIDE) "$(JAR_PATH)" uf "$(APK_DIR)/demo-android-unaligned.apk" -C "$(LOCAL_PATH)" lib

$(APK_DIR)/demo-android-unsigned.apk : $(APK_DIR)/demo-android-unaligned.apk
	$(HIDE) $(call host-mkdir,$(APK_DIR))
	$(HIDE) "$(ZIPALIGN_PATH)" -f 4 "$(APK_DIR)/demo-android-unaligned.apk" "$(APK_DIR)/demo-android-unsigned.apk"

$(APK_DIR)/demo-android.apk : $(KEYSTORE_DIR)/demo-android-debug.keystore $(APK_DIR)/demo-android-unsigned.apk
	$(HIDE) $(call host-mkdir,$(APK_DIR))
	$(HIDE) $(call host-cp,$(APK_DIR)/demo-android-unsigned.apk,$(APK_DIR)/demo-android.apk)
	$(HIDE) "$(APKSIGNER_PATH)" sign -v --ks "$(KEYSTORE_DIR)/demo-android-debug.keystore" --ks-pass pass:demo-android -ks-key-alias demo-android-debug "$(APK_DIR)/demo-android.apk"

clean:
	$(HIDE) $(call host-rm,$(KEYSTORE_DIR)/demo-android-debug.keystore)
	$(HIDE) $(call host-rm,$(FLAT_DIR)/values_styles.arsc.flat)
	$(HIDE) $(call host-rm,$(APK_DIR)/demo-android-unaligned.apk)
	$(HIDE) $(call host-rm,$(APK_DIR)/demo-android-unsigned.apk)
	$(HIDE) $(call host-rm,$(APK_DIR)/demo-android.apk)

.PHONY : \
	all