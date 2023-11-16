::
:: Copyright (C) YuqiaoZhang(HanetakaChou)
:: 
:: This program is free software: you can redistribute it and\or modify
:: it under the terms of the GNU Lesser General Public License as published
:: by the Free Software Foundation, either version 3 of the License, or
:: (at your option) any later version.
:: 
:: This program is distributed in the hope that it will be useful,
:: but WITHOUT ANY WARRANTY; without even the implied warranty of
:: MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
:: GNU Lesser General Public License for more details.
:: 
:: You should have received a copy of the GNU Lesser General Public License
:: along with this program.  If not, see <https:\\www.gnu.org\licenses\>.
::

@echo off

:: SET ANDROID_NDK_ROOT=C:\Users\Public\Documents\android-sdk\ndk\25.2.9519653
:: SET ANDROID_SDK_ROOT=C:\Users\Public\Documents\android-sdk
:: SET JAVA_HOME=C:\Users\Public\Documents\jdk-17

pushd %~dp0
call "%ANDROID_NDK_ROOT%\prebuilt\windows-x86_64\bin\make" -C "%~dp0." -f ".\APK.mk" clean
call "%ANDROID_NDK_ROOT%\ndk-build" -C "%~dp0." NDK_PROJECT_PATH:=null "NDK_APPLICATION_MK:=.\Application.mk" "APP_BUILD_SCRIPT:=.\Android.mk" "NDK_OUT:=.\obj" "NDK_LIBS_OUT:=.\lib" "APP_ABI:=armeabi-v7a arm64-v8a x86 x86_64" "APP_PLATFORM:=android-28" clean
call "%ANDROID_NDK_ROOT%\prebuilt\windows-x86_64\bin\make" -C "%~dp0." -f ".\GLSL.mk" clean
call "%~dp0..\thirdparty\PAL\build-android\clean-android.cmd"
popd