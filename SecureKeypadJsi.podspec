# Copyright (c) 2026 expo-secure-keypad-jsi contributors. MIT License.
#
# This podspec lives at the PACKAGE ROOT (not ios/) so its file patterns can
# reach the shared C++ core in common/ — CocoaPods file patterns cannot use
# `../`. Expo Apple autolinking discovers podspecs anywhere in the package.
require 'json'

package = JSON.parse(File.read(File.join(__dir__, 'package.json')))

Pod::Spec.new do |s|
  s.name           = 'SecureKeypadJsi'
  s.version        = package['version']
  s.summary        = package['description']
  s.description    = package['description']
  s.license        = package['license']
  s.author         = package['author']
  s.homepage       = package['homepage']
  s.platforms      = { :ios => '16.4' }
  s.swift_version  = '5.9'
  s.source         = { git: 'https://github.com/0610studio/expo-secure-keypad-jsi' }
  s.static_framework = true

  s.dependency 'ExpoModulesCore'
  # Precompiled OpenSSL 3.6.2 XCFramework — same upstream version as Android.
  # Exact pin, not a range: a range would let an unreviewed repackage in.
  # Bump deliberately — see docs/BUILD.md.
  s.dependency 'OpenSSL-Universal', '3.6.2000'

  s.pod_target_xcconfig = {
    'DEFINES_MODULE' => 'YES',
    'CLANG_CXX_LANGUAGE_STANDARD' => 'c++20',
    'HEADER_SEARCH_PATHS' => '"$(PODS_TARGET_SRCROOT)/common/include"',
    # Do not expose OpenSSL headers to Swift; only the ObjC++ bridge uses them.
    'SWIFT_INCLUDE_PATHS' => '"$(PODS_TARGET_SRCROOT)/common/include"',
  }

  # Swift + ObjC++ bridge + the shared C++ core.
  s.source_files = 'ios/**/*.{h,m,mm,swift}',
                   'common/src/**/*.cpp',
                   'common/include/**/*.h'

  # Keep the C++ core headers private to the pod (only EskKeypadBridge.h is the
  # Swift-facing surface, exposed via the generated umbrella header).
  s.private_header_files = 'common/include/**/*.h'
end
