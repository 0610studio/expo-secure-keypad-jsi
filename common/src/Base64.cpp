// Copyright (c) 2026 expo-secure-keypad-jsi contributors. MIT License.
#include "esk/Base64.h"

#include <openssl/evp.h>

#include <vector>

namespace esk {
namespace base64 {

std::string encode(const uint8_t* data, size_t len) {
  if (len == 0) return std::string();
  // EVP_EncodeBlock writes 4 chars per 3 bytes, plus a NUL.
  const size_t outLen = 4 * ((len + 2) / 3);
  std::string out(outLen, '\0');
  const int written = EVP_EncodeBlock(reinterpret_cast<unsigned char*>(&out[0]),
                                      data, static_cast<int>(len));
  if (written < 0) return std::string();
  out.resize(static_cast<size_t>(written));
  return out;
}

bool decode(const std::string& in, std::string& out) {
  if (in.empty()) {
    out.clear();
    return true;
  }
  std::vector<unsigned char> buf(3 * (in.size() / 4) + 3);
  const int written = EVP_DecodeBlock(
      buf.data(), reinterpret_cast<const unsigned char*>(in.data()),
      static_cast<int>(in.size()));
  if (written < 0) return false;
  // EVP_DecodeBlock ignores '=' and always emits multiples of 3.
  size_t realLen = static_cast<size_t>(written);
  for (auto it = in.rbegin(); it != in.rend() && *it == '='; ++it) {
    if (realLen > 0) --realLen;
  }
  out.assign(reinterpret_cast<char*>(buf.data()), realLen);
  return true;
}

}  // namespace base64
}  // namespace esk
