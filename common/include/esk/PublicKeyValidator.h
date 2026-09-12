// Copyright (c) 2026 expo-secure-keypad-jsi contributors. MIT License.
#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <string>

#include <openssl/evp.h>

namespace esk {

// RAII wrapper over EVP_PKEY. A validated key is the precondition for arming.
class PublicKey {
 public:
  // Enforces: RSA, modulus in [minBits, maxBits], exponent odd and >= 65537.
  // Throws EskError on any failure.
  static std::unique_ptr<PublicKey> parse(const std::string& pem,
                                          int minBits = 2048,
                                          int maxBits = 8192);

  ~PublicKey();
  PublicKey(const PublicKey&) = delete;
  PublicKey& operator=(const PublicKey&) = delete;

  EVP_PKEY* evp() const { return pkey_; }

  // Envelope `kid`: hex of the first 8 bytes of the SPKI SHA-256 digest.
  std::string keyId() const;

 private:
  explicit PublicKey(EVP_PKEY* pkey) : pkey_(pkey) {}
  EVP_PKEY* pkey_ = nullptr;
  std::array<uint8_t, 32> spkiSha256_{};
};

}  // namespace esk
