# Copyright (c) 2026 expo-secure-keypad-jsi contributors. MIT License.
#
# Reusable static library `esk_core` holding the platform-agnostic security
# core. Consumed by android/CMakeLists.txt (with prefab OpenSSL) and by
# tests/cpp/CMakeLists.txt (with host OpenSSL).
#
# The caller must provide the OpenSSL crypto target name via ESK_OPENSSL_TARGET
# before add_subdirectory():
#   * host / CI:   OpenSSL::Crypto   (find_package(OpenSSL))
#   * Android:     openssl::crypto   (prefab from the openssl-static AAR)

cmake_minimum_required(VERSION 3.22)
project(esk_core LANGUAGES CXX)

if(NOT DEFINED ESK_OPENSSL_TARGET)
  set(ESK_OPENSSL_TARGET OpenSSL::Crypto)
endif()

# Listed explicitly, not globbed: a glob is cached at configure time, so adding
# or deleting a file leaves stale entries in an incremental NDK build.
set(ESK_CORE_SOURCES
  src/Base64.cpp
  src/Error.cpp
  src/KeypadCore.cpp
  src/PinEncryptor.cpp
  src/PublicKeyValidator.cpp
  src/SecureBuffer.cpp
  src/SecureRandom.cpp
  src/esk_c_api.cpp)
list(TRANSFORM ESK_CORE_SOURCES PREPEND "${CMAKE_CURRENT_SOURCE_DIR}/")

add_library(esk_core STATIC ${ESK_CORE_SOURCES})

target_include_directories(esk_core PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/include")

target_compile_features(esk_core PUBLIC cxx_std_20)

set_target_properties(esk_core PROPERTIES
  CXX_STANDARD 20
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
  POSITION_INDEPENDENT_CODE ON)

target_compile_options(esk_core PRIVATE
  -Wall -Wextra -Wpedantic
  $<$<CONFIG:Release>:-O2>)

target_link_libraries(esk_core PUBLIC ${ESK_OPENSSL_TARGET})
