// Copyright (c) 2026 expo-secure-keypad-jsi contributors. MIT License.
#pragma once

#include <stdexcept>
#include <string>

namespace esk {

// Reaches JS as the "ERR_..." strings in Error.cpp, so the names are API:
// renaming one is a breaking change.
enum class ErrorCode {
  KeyParse,
  KeyNotRsa,
  WeakKey,
  NotArmed,
  Empty,       // submit() with no digits
  TooShort,    // submit() below minLength
  Encrypt,
  Random,
  Internal,
};

const char* errorCodeName(ErrorCode code);

class EskError : public std::runtime_error {
 public:
  EskError(ErrorCode code, const std::string& message)
      : std::runtime_error(message), code_(code) {}

  ErrorCode code() const noexcept { return code_; }
  const char* codeName() const noexcept { return errorCodeName(code_); }

 private:
  ErrorCode code_;
};

}  // namespace esk
