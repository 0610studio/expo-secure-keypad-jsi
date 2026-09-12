// Copyright (c) 2026 expo-secure-keypad-jsi contributors. MIT License.
#include "esk/SecureRandom.h"

#include <openssl/rand.h>

#include "esk/Error.h"

namespace esk {
namespace secure_random {

void bytes(uint8_t* buf, size_t len) {
  if (len == 0) return;
  if (buf == nullptr) {
    throw EskError(ErrorCode::Random, "secure_random::bytes null buffer");
  }
  // Never fall back to a weaker source when the CSPRNG refuses.
  if (RAND_bytes(buf, static_cast<int>(len)) != 1) {
    throw EskError(ErrorCode::Random, "RAND_bytes failed");
  }
}

std::array<uint8_t, 10> shuffledDigits() {
  std::array<uint8_t, 10> layout{};
  for (uint8_t i = 0; i < 10; ++i) layout[i] = i;

  // Fisher-Yates. The rejection loop below removes modulo bias — do not
  // simplify it to a plain `r % bound`.
  for (size_t i = 9; i > 0; --i) {
    const uint32_t bound = static_cast<uint32_t>(i) + 1;
    const uint32_t limit = UINT32_MAX - (UINT32_MAX % bound);
    uint32_t r;
    do {
      bytes(reinterpret_cast<uint8_t*>(&r), sizeof(r));
    } while (r >= limit);
    const size_t j = r % bound;
    std::swap(layout[i], layout[j]);
  }
  return layout;
}

}  // namespace secure_random
}  // namespace esk
