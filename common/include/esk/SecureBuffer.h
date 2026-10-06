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
// Full QWERTY keyboard hard cap and the payload's secret field size, in UTF-8
// bytes. 32 + 64 = 96-byte payload stays well inside the 190-byte RSA-2048
// OAEP-SHA256 limit.
inline constexpr size_t kMaxSecretLength = 64;

// Non-ASCII symbols on the full keyboard's second symbol page: the union of
// what the stock iOS, Gboard and Samsung keyboards show on their symbol layers
// (long-press alternates excluded). All BMP, so each is 2 or 3 UTF-8 bytes.
// The platform views carry the same list as their layout strings.
inline constexpr uint32_t kExtraSymbols[] = {
    0x20A9, 0x20AC, 0x00A3, 0x00A5, 0x00A2, 0x00A4,  // ₩ € £ ¥ ¢ ¤
    0x00A7, 0x00B6, 0x00A9, 0x00AE, 0x2122, 0x2713,  // § ¶ © ® ™ ✓
    0x00B0, 0x2022, 0x00D7, 0x00F7, 0x221A, 0x03C0,  // ° • × ÷ √ π
    0x2206, 0x00A1, 0x00BF, 0x300A, 0x300B, 0x25CB,  // ∆ ¡ ¿ 《 》 ○
    0x25CF, 0x25A1, 0x25A0, 0x25AA, 0x25C7, 0x2606,  // ● □ ■ ▪ ◇ ☆
    0x2664, 0x2661, 0x2667,                          // ♤ ♡ ♧
};

// Printable ASCII (no space) plus kExtraSymbols may enter the buffer; anything
// else is a caller bug, not user input.
inline constexpr bool isAllowedSecretCodepoint(uint32_t cp) {
  if (cp > 0x20 && cp < 0x7F) return true;
  for (uint32_t s : kExtraSymbols) {
    if (s == cp) return true;
  }
  return false;
}

// Secret bytes on a single anonymous mmap'd page — never the heap, where
// realloc would leave un-cleansable copies. Single-owner (UI) thread, so no
// locking. Characters are stored UTF-8 encoded ('0'..'9' for the digit
// keypad, so ASCII input is one byte each); PinEncryptor copies the bytes into
// the payload verbatim.
class SecureBuffer {
 public:
  SecureBuffer();
  ~SecureBuffer();

  SecureBuffer(const SecureBuffer&) = delete;
  SecureBuffer& operator=(const SecureBuffer&) = delete;
  SecureBuffer(SecureBuffer&&) = delete;
  SecureBuffer& operator=(SecureBuffer&&) = delete;

  // Appends one character; false (buffer unchanged) when its encoding would
  // pass kMaxSecretLength bytes. Throws on a code point outside the charset.
  bool append(uint32_t codepoint);
  // Removes the last whole character, not just its final byte.
  bool pop();
  void clear();
  // Bytes, i.e. the payload's secretLength.
  size_t length() const;
  // Characters, i.e. what the user typed and what min/maxLength count.
  size_t charCount() const;

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
  size_t chars_ = 0;
  bool locked_ = false;
  std::thread::id owner_;
};

}  // namespace esk
