// Copyright (c) 2026 expo-secure-keypad-jsi contributors. MIT License.
#pragma once

#include <array>
#include <functional>
#include <memory>
#include <optional>
#include <string>

#include "esk/PublicKeyValidator.h"
#include "esk/SecureBuffer.h"

namespace esk {

enum class KeypadState { Unarmed, Armed };

// Digit = numeric PIN pad, Full = QWERTY keyboard. Same wire format either way.
enum class KeypadType { Digit, Full };

// Owned by the platform native view. All methods must run on the owning (UI)
// thread — SecureBuffer enforces this.
class KeypadCore {
 public:
  // nowProvider is injected so the platform owns the clock and tests stay
  // deterministic. Lengths clamp to [4,12] for Digit and [4,64] for Full.
  explicit KeypadCore(std::function<int64_t()> nowProvider,
                      size_t minLength = kMinPinLength,
                      size_t maxLength = kMaxPinLength,
                      KeypadType type = KeypadType::Digit);
  ~KeypadCore();

  KeypadCore(const KeypadCore&) = delete;
  KeypadCore& operator=(const KeypadCore&) = delete;

  // nullopt on success; otherwise the error-code name (e.g. "ERR_WEAK_KEY").
  std::optional<std::string> arm(const std::string& publicKeyPem);

  // Return the resulting entered-character count. pressKey takes the final
  // Unicode code point (the view resolves shift/layer state before calling);
  // characters outside the type's charset are silently ignored, and so is a
  // character whose UTF-8 bytes would overflow the 64-byte secret field.
  size_t pressDigit(uint8_t digit);
  size_t pressKey(uint32_t codepoint);
  size_t backspace();
  void clearPin();

  // Encrypts, cleanses the buffer, returns the envelope JSON. Throws EskError
  // on Empty / TooShort / NotArmed.
  std::string submit();

  // Characters, not bytes — a multi-byte symbol counts once.
  size_t digitCount() const;
  KeypadState state() const { return state_; }
  KeypadType type() const { return type_; }

  // The view uses this to place glyphs; the mapping never leaves native code.
  std::array<uint8_t, 10> shuffledLayout() const;

  void onHostBackground();

 private:
  SecureBuffer buffer_;
  std::unique_ptr<PublicKey> key_;
  std::function<int64_t()> nowProvider_;
  size_t minLength_;
  size_t maxLength_;
  KeypadType type_ = KeypadType::Digit;
  KeypadState state_ = KeypadState::Unarmed;
};

}  // namespace esk
