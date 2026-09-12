// Copyright (c) 2026 expo-secure-keypad-jsi contributors. MIT License.
#include "esk/KeypadCore.h"

#include <algorithm>

#include "esk/Error.h"
#include "esk/PinEncryptor.h"
#include "esk/SecureRandom.h"

namespace esk {

KeypadCore::KeypadCore(std::function<int64_t()> nowProvider, size_t minLength,
                       size_t maxLength, KeypadType type)
    : nowProvider_(std::move(nowProvider)), type_(type) {
  const size_t cap =
      type_ == KeypadType::Full ? kMaxSecretLength : kMaxPinLength;
  minLength_ = std::clamp<size_t>(minLength, kMinPinLength, cap);
  maxLength_ = std::clamp<size_t>(maxLength, kMinPinLength, cap);
  if (minLength_ > maxLength_) minLength_ = maxLength_;
}

KeypadCore::~KeypadCore() = default;

std::optional<std::string> KeypadCore::arm(const std::string& publicKeyPem) {
  try {
    key_ = PublicKey::parse(publicKeyPem, 2048, 8192);
  } catch (const EskError& e) {
    // A rejected key must not leave the previous session's secret behind.
    key_.reset();
    buffer_.clear();
    state_ = KeypadState::Unarmed;
    return std::string(e.codeName());
  }
  buffer_.clear();
  state_ = KeypadState::Armed;
  return std::nullopt;
}

size_t KeypadCore::pressDigit(uint8_t digit) {
  if (digit > 9) return buffer_.length();
  return pressKey(static_cast<uint8_t>('0' + digit));
}

size_t KeypadCore::pressKey(uint8_t asciiChar) {
  if (state_ != KeypadState::Armed) {
    return buffer_.length();
  }
  const bool allowed = type_ == KeypadType::Full
                           ? isAllowedSecretByte(asciiChar)
                           : (asciiChar >= '0' && asciiChar <= '9');
  if (!allowed) {
    return buffer_.length();
  }
  // Enforce the configured maxLength_ (SecureBuffer only enforces the hard
  // kMaxSecretLength cap). Silently ignore presses past the configured length.
  if (buffer_.length() >= maxLength_) {
    return buffer_.length();
  }
  buffer_.append(asciiChar);
  return buffer_.length();
}

size_t KeypadCore::backspace() {
  if (state_ != KeypadState::Armed) return buffer_.length();
  buffer_.pop();
  return buffer_.length();
}

void KeypadCore::clearPin() { buffer_.clear(); }

std::string KeypadCore::submit() {
  if (state_ != KeypadState::Armed || !key_) {
    throw EskError(ErrorCode::NotArmed, "keypad is not armed with a key");
  }
  const size_t len = buffer_.length();
  if (len == 0) {
    throw EskError(ErrorCode::Empty, "no digits entered");
  }
  if (len < minLength_) {
    throw EskError(ErrorCode::TooShort, "PIN below configured minimum length");
  }

  const int64_t now = nowProvider_ ? nowProvider_() : 0;
  std::string envelope;
  try {
    envelope = PinEncryptor::encrypt(buffer_, *key_, now);
  } catch (...) {
    buffer_.clear();
    throw;
  }
  // Plaintext lifetime ends here.
  buffer_.clear();
  return envelope;
}

size_t KeypadCore::digitCount() const { return buffer_.length(); }

std::array<uint8_t, 10> KeypadCore::shuffledLayout() const {
  return secure_random::shuffledDigits();
}

// Unconditional: the entered value must not outlive the foreground.
void KeypadCore::onHostBackground() { buffer_.clear(); }

}  // namespace esk
