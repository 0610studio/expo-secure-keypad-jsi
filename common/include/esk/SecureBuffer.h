// Copyright (c) 2026 expo-secure-keypad-jsi contributors. MIT License.
#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <thread>

namespace esk {

// Digit keypad length caps (UI policy; the wire format is shared).
inline constexpr size_t kMaxPinLength = 12;
inline constexpr size_t kMinPinLength = 4;
// Full QWERTY keyboard hard cap and the payload's secret field size. 32 + 64 =
// 96-byte payload stays well inside the 190-byte RSA-2048 OAEP-SHA256 limit.
inline constexpr size_t kMaxSecretLength = 64;

// Only printable ASCII (no space) may enter the buffer; anything else is a
// caller bug, not user input.
inline constexpr bool isAllowedSecretByte(uint8_t ch) {
  return ch > 0x20 && ch < 0x7F;
}

// Secret bytes on a single anonymous mmap'd page — never the heap, where
// realloc would leave un-cleansable copies. Single-owner (UI) thread, so no
// locking. Bytes are stored as the final ASCII characters ('0'..'9' for the
// digit keypad); PinEncryptor copies them into the payload verbatim.
class SecureBuffer {
 public:
  SecureBuffer();
  ~SecureBuffer();

  SecureBuffer(const SecureBuffer&) = delete;
  SecureBuffer& operator=(const SecureBuffer&) = delete;
  SecureBuffer(SecureBuffer&&) = delete;
  SecureBuffer& operator=(SecureBuffer&&) = delete;

  bool append(uint8_t asciiChar);
  bool pop();
  void clear();
  size_t length() const;

  // The only read path for the plaintext. Pointer is valid for the call only.
  void withPlaintext(const std::function<void(const uint8_t*, size_t)>& fn) const;

  // True if mlock() succeeded on the page. False means the swap guarantee is
  // not in effect on this process (low RLIMIT_MEMLOCK); cleanse still runs.
  bool isLocked() const { return locked_; }

 private:
  void assertOwnerThread() const;

  uint8_t* page_ = nullptr;
  size_t pageSize_ = 0;
  size_t length_ = 0;
  bool locked_ = false;
  std::thread::id owner_;
};

}  // namespace esk
