// Copyright (c) 2026 expo-secure-keypad-jsi contributors. MIT License.
#pragma once

#include <cstdint>
#include <string>

#include "esk/PublicKeyValidator.h"
#include "esk/SecureBuffer.h"

namespace esk {

// Wire format — servers decode against this, so it cannot change without a
// version bump. README carries the same table. Both keypad types emit it; the
// digit pad simply never fills more than 12 of the 64 secret bytes.
//
// v2 (96 bytes):
//   0 2 magic {'S','K'} | 2 1 version | 3 1 secretLength
//   4 64 secret ASCII, zero-padded | 68 16 nonce | 84 8 unix seconds BE
//   | 92 4 zero
namespace payload {
inline constexpr uint8_t kMagic0 = 'S';
inline constexpr uint8_t kMagic1 = 'K';
inline constexpr uint8_t kVersion = 2;
inline constexpr size_t kSize = 96;
inline constexpr size_t kOffSecret = 4;
inline constexpr size_t kNonceSize = 16;
inline constexpr size_t kOffNonce = 68;
inline constexpr size_t kOffTimestamp = 84;
}  // namespace payload

class PinEncryptor {
 public:
  // Returns the envelope JSON; throws EskError on failure. Does NOT clear
  // `buffer` — KeypadCore owns that and cleanses after this returns.
  static std::string encrypt(const SecureBuffer& buffer, const PublicKey& key,
                             int64_t nowUnixSeconds);
};

}  // namespace esk
