// Copyright (c) 2026 expo-secure-keypad-jsi contributors. MIT License.
#pragma once

#include <cstddef>
#include <openssl/crypto.h>

namespace esk {

// Never memset() plaintext: the compiler is allowed to elide it, OPENSSL_cleanse
// is not.
inline void cleanse(void* ptr, size_t len) {
  if (ptr != nullptr && len > 0) {
    OPENSSL_cleanse(ptr, len);
  }
}

}  // namespace esk
