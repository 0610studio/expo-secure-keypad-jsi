# Copyright (c) 2026 expo-secure-keypad-jsi contributors. MIT License.
#
# Host test build. Run from the repo root:
#   cmake -S tests/cpp -B build-cpp -DCMAKE_BUILD_TYPE=Debug
#   cmake --build build-cpp
#   ctest --test-dir build-cpp --output-on-failure
#
# On macOS with Homebrew OpenSSL, point CMake at it:
#   cmake -S tests/cpp -B build-cpp -DOPENSSL_ROOT_DIR=$(brew --prefix openssl@3)

cmake_minimum_required(VERSION 3.22)
project(esk_core_tests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Host OpenSSL provides the OpenSSL::Crypto target consumed by esk_core.
find_package(OpenSSL REQUIRED)
set(ESK_OPENSSL_TARGET OpenSSL::Crypto)

# Sanitizers on non-Windows for the memory-safety assertions to mean something.
# This block MUST precede add_subdirectory(): add_compile_options only affects
# targets created after it, and the whole point is to instrument esk_core, not
# just the test translation units. The configure-time check below guards it.
if(NOT MSVC)
  add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer -g)
  add_link_options(-fsanitize=address,undefined)
endif()

# Build the core library from the sibling common/ directory.
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../common esk_core_build)

# Fail configure loudly if the core ever loses instrumentation again.
if(NOT MSVC)
  get_directory_property(_esk_dir_opts DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../../common COMPILE_OPTIONS)
  if(NOT "${_esk_dir_opts}" MATCHES "fsanitize")
    message(FATAL_ERROR "esk_core is not built with sanitizers; check the option ordering in tests/cpp/CMakeLists.txt")
  endif()
endif()

# GoogleTest via FetchContent so CI needs no system gtest.
include(FetchContent)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG        v1.15.2
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()

add_executable(esk_tests
  SecureBufferTest.cpp
  CryptoRoundtripTest.cpp
  FullKeyboardTest.cpp
  CApiTest.cpp)

target_include_directories(esk_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(esk_tests PRIVATE esk_core GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(esk_tests)
