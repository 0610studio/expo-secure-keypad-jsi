// Copyright (c) 2026 expo-secure-keypad-jsi contributors. MIT License.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace esk {

// Thin wrapper over OpenSSL's CSPRNG (RAND_bytes). Throws EskError on failure
// rather than returning weak/predictable bytes.
namespace secure_random {

// Fills buf with cryptographically secure random bytes. Throws on RAND failure.
void bytes(uint8_t* buf, size_t len);

// Returns a shuffled 0..9 layout using Fisher-Yates seeded from RAND_bytes.
// The layout is generated natively and never leaves the native side except as
// on-screen glyph positions, so JS never learns which coordinate maps to which
// digit.
std::array<uint8_t, 10> shuffledDigits();

}  // namespace secure_random
}  // namespace esk
