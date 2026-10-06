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
  if (digit > 9) return buffer_.charCount();
  return pressKey(static_cast<uint32_t>('0' + digit));
}

size_t KeypadCore::pressKey(uint32_t codepoint) {
  if (state_ != KeypadState::Armed) {
    return buffer_.charCount();
  }
  const bool allowed = type_ == KeypadType::Full
                           ? isAllowedSecretCodepoint(codepoint)
                           : (codepoint >= '0' && codepoint <= '9');
  if (!allowed) {
    return buffer_.charCount();
  }
  // Enforce the configured maxLength_ in characters. SecureBuffer separately
  // refuses a character whose bytes would pass kMaxSecretLength, so with
  // multi-byte symbols the input can stop short of maxLength_. Both are
  // silent: the press is ignored, not an error.
  if (buffer_.charCount() >= maxLength_) {
    return buffer_.charCount();
  }
  buffer_.append(codepoint);
  return buffer_.charCount();
}

size_t KeypadCore::backspace() {
  if (state_ != KeypadState::Armed) return buffer_.charCount();
  buffer_.pop();
  return buffer_.charCount();
}

void KeypadCore::clearPin() { buffer_.clear(); }

std::string KeypadCore::submit() {
  if (state_ != KeypadState::Armed || !key_) {
    throw EskError(ErrorCode::NotArmed, "keypad is not armed with a key");
  }
  const size_t len = buffer_.charCount();
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

size_t KeypadCore::digitCount() const { return buffer_.charCount(); }

std::array<uint8_t, 10> KeypadCore::shuffledLayout() const {
  return secure_random::shuffledDigits();
}

// Unconditional: the entered value must not outlive the foreground.
void KeypadCore::onHostBackground() { buffer_.clear(); }

}  // namespace esk
