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

  // Return the resulting entered-key count. pressKey takes the final ASCII
  // character (the view resolves shift/layer state before calling); characters
  // outside the type's charset are silently ignored.
  size_t pressDigit(uint8_t digit);
  size_t pressKey(uint8_t asciiChar);
  size_t backspace();
  void clearPin();

  // Encrypts, cleanses the buffer, returns the envelope JSON. Throws EskError
  // on Empty / TooShort / NotArmed.
  std::string submit();

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
