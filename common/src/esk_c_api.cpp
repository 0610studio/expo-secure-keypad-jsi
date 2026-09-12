// Copyright (c) 2026 expo-secure-keypad-jsi contributors. MIT License.
#include "esk/esk_c_api.h"

#include <cstdlib>
#include <cstring>
#include <string>

#include "esk/Error.h"
#include "esk/KeypadCore.h"

using namespace esk;

struct esk_keypad {
  std::unique_ptr<KeypadCore> core;
  esk_now_cb nowCb = nullptr;
  void* user = nullptr;
};

namespace {

char* dupString(const std::string& s) {
  char* out = static_cast<char*>(malloc(s.size() + 1));
  if (out == nullptr) return nullptr;
  std::memcpy(out, s.data(), s.size());
  out[s.size()] = '\0';
  return out;
}

// Every entry point below is a no-throw boundary: the JNI and Objective-C++
// callers cannot unwind a C++ exception, so one escaping here would be
// std::terminate. Count-returning calls return ESK_COUNT_ERROR on failure so
// the caller can tell "refused" from "zero entered" (the buffer is untouched
// either way); void calls swallow.

template <typename F>
int32_t countOr(F&& fn) noexcept {
  try {
    return static_cast<int32_t>(fn());
  } catch (...) {
    return ESK_COUNT_ERROR;
  }
}

}  // namespace

extern "C" {

esk_keypad* esk_keypad_create(const esk_config* config, esk_now_cb now_cb,
                              void* user) {
  auto* kp = new (std::nothrow) esk_keypad();
  if (kp == nullptr) return nullptr;
  kp->nowCb = now_cb;
  kp->user = user;

  size_t minLen = kMinPinLength, maxLen = kMaxPinLength;
  KeypadType type = KeypadType::Digit;
  if (config != nullptr) {
    if (config->min_length > 0) minLen = static_cast<size_t>(config->min_length);
    if (config->max_length > 0) maxLen = static_cast<size_t>(config->max_length);
    if (config->keypad_type == 1) type = KeypadType::Full;
  }

  auto nowProvider = [kp]() -> int64_t {
    return kp->nowCb ? kp->nowCb(kp->user) : 0;
  };

  try {
    kp->core = std::make_unique<KeypadCore>(std::move(nowProvider), minLen,
                                            maxLen, type);
  } catch (...) {
    delete kp;
    return nullptr;
  }
  return kp;
}

void esk_keypad_destroy(esk_keypad* kp) { delete kp; }

const char* esk_keypad_arm(esk_keypad* kp, const char* pem) {
  if (kp == nullptr || pem == nullptr) return "ERR_INTERNAL";
  try {
    auto err = kp->core->arm(std::string(pem));
    if (!err.has_value()) return nullptr;
    // The name must come back as a static string the caller can hold.
    for (int i = 0; i <= static_cast<int>(ErrorCode::Internal); ++i) {
      const char* name = errorCodeName(static_cast<ErrorCode>(i));
      if (*err == name) return name;
    }
  } catch (...) {
  }
  return "ERR_INTERNAL";
}

int32_t esk_keypad_press(esk_keypad* kp, uint8_t digit) {
  if (kp == nullptr) return 0;
  // KeypadCore::pressDigit ignores out-of-range digits itself.
  return countOr([&] { return kp->core->pressDigit(digit); });
}

int32_t esk_keypad_press_key(esk_keypad* kp, uint8_t ascii_char) {
  if (kp == nullptr) return 0;
  // KeypadCore ignores out-of-charset characters itself.
  return countOr([&] { return kp->core->pressKey(ascii_char); });
}

int32_t esk_keypad_backspace(esk_keypad* kp) {
  if (kp == nullptr) return 0;
  return countOr([&] { return kp->core->backspace(); });
}

void esk_keypad_clear(esk_keypad* kp) {
  if (kp == nullptr) return;
  try {
    kp->core->clearPin();
  } catch (...) {
  }
}

char* esk_keypad_submit(esk_keypad* kp, const char** err_out) {
  if (err_out) *err_out = nullptr;
  if (kp == nullptr) {
    if (err_out) *err_out = "ERR_INTERNAL";
    return nullptr;
  }
  try {
    std::string envelope = kp->core->submit();
    return dupString(envelope);
  } catch (const EskError& e) {
    if (err_out) *err_out = e.codeName();
    return nullptr;
  } catch (...) {
    if (err_out) *err_out = "ERR_INTERNAL";
    return nullptr;
  }
}

void esk_keypad_shuffled_layout(esk_keypad* kp, uint8_t* out10) {
  if (kp == nullptr || out10 == nullptr) return;
  try {
    auto layout = kp->core->shuffledLayout();
    std::memcpy(out10, layout.data(), 10);
  } catch (...) {
    // RAND_bytes refused. Leave the caller's identity layout untouched rather
    // than crash: an unshuffled pad is a degraded UI, not a leaked secret.
  }
}

void esk_keypad_on_background(esk_keypad* kp) {
  if (kp == nullptr) return;
  try {
    kp->core->onHostBackground();
  } catch (...) {
  }
}

void esk_free(char* s) { free(s); }

}  // extern "C"
