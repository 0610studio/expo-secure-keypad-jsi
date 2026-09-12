// Copyright (c) 2026 expo-secure-keypad-jsi contributors. MIT License.
#include "esk/Error.h"

namespace esk {

const char* errorCodeName(ErrorCode code) {
  switch (code) {
    case ErrorCode::KeyParse:       return "ERR_KEY_PARSE";
    case ErrorCode::KeyNotRsa:      return "ERR_KEY_NOT_RSA";
    case ErrorCode::WeakKey:        return "ERR_WEAK_KEY";
    case ErrorCode::NotArmed:       return "ERR_NOT_ARMED";
    case ErrorCode::Empty:          return "ERR_EMPTY";
    case ErrorCode::TooShort:       return "ERR_TOO_SHORT";
    case ErrorCode::Encrypt:        return "ERR_ENCRYPT";
    case ErrorCode::Random:         return "ERR_RANDOM";
    case ErrorCode::Internal:       return "ERR_INTERNAL";
  }
  return "ERR_UNKNOWN";
}

}  // namespace esk
