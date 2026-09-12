// Copyright (c) 2026 expo-secure-keypad-jsi contributors. MIT License.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace esk {
namespace base64 {

// Standard base64 (with padding) encode. Used only for ciphertext/nonce, which
// are not secret, so no constant-time requirement here.
std::string encode(const uint8_t* data, size_t len);

// Decode standard base64. Returns false on malformed input. Used by the host
// test suite's reference decryptor; the runtime paths only ever encode.
bool decode(const std::string& in, std::string& out);

}  // namespace base64
}  // namespace esk
