# Copyright (c) 2026 expo-secure-keypad-jsi contributors. MIT License.
cmake_minimum_required(VERSION 3.22)
project(expo-secure-keypad-jsi LANGUAGES CXX)

# OpenSSL comes from the openssl-static prefab AAR (io.github.ronickg:
# openssl-static). Its prefab package name is `opensslstatic` and it exposes the
# `crypto` and `ssl` modules — we link only `opensslstatic::crypto`.
find_package(opensslstatic REQUIRED CONFIG)
set(ESK_OPENSSL_TARGET opensslstatic::crypto)

# Build the shared core (defines target `esk_core`).
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../common esk_core_build)

# JNI glue -> flat C API -> esk_core.
add_library(expo-secure-keypad-jsi SHARED
  src/main/cpp/JniBridge.cpp)

target_link_libraries(expo-secure-keypad-jsi
  esk_core
  android
  log)

# Hide every symbol pulled in from the static OpenSSL so it can never interpose
# with another OpenSSL present in the app process (see quick-crypto #1059), and
# keep 16KB page alignment for Android 15+.
target_link_options(expo-secure-keypad-jsi PRIVATE
  "-Wl,--exclude-libs,ALL"
  "-Wl,-z,max-page-size=16384")
