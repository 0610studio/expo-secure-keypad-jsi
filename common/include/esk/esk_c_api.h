// Copyright (c) 2026 expo-secure-keypad-jsi contributors. MIT License.
//
// Flat C ABI over KeypadCore — the only surface the JNI and Objective-C++
// bridges call. Every function is a no-throw boundary: internal failures
// surface as error strings, a degraded return value, or are swallowed, never
// as an unwinding exception. Strings are NUL-terminated UTF-8.
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct esk_keypad esk_keypad;

// Unix seconds. Injected so the platform owns the clock.
typedef int64_t (*esk_now_cb)(void* user);

typedef struct {
  int32_t min_length;  // clamped to [4,12] (digit) or [4,64] (full)
  int32_t max_length;  // clamped to [4,12] (digit) or [4,64] (full)
  int32_t keypad_type; // 0 = digit PIN pad, 1 = full QWERTY
} esk_config;

// NULL only on allocation failure.
esk_keypad* esk_keypad_create(const esk_config* config, esk_now_cb now_cb,
                              void* user);
void esk_keypad_destroy(esk_keypad* kp);

// NULL on success, else a static error-code string — do NOT free it.
const char* esk_keypad_arm(esk_keypad* kp, const char* pem);

// Key ops. Return the resulting entered-character count, or ESK_COUNT_ERROR
// (-1) if the core refused the call — in practice only a cross-thread access,
// which is a caller bug. Callers must not forward a negative count to JS as
// "0 entered": the buffer was NOT modified and still holds the previous input.
// esk_keypad_press takes a digit value 0..9; esk_keypad_press_key takes the
// final Unicode code point (printable ASCII without space, or one of the extra
// symbols in SecureBuffer.h — the view resolves shift/layer state first).
// Out-of-charset input is ignored, not an error.
#define ESK_COUNT_ERROR (-1)
int32_t esk_keypad_press(esk_keypad* kp, uint8_t digit);
int32_t esk_keypad_press_key(esk_keypad* kp, uint32_t codepoint);
int32_t esk_keypad_backspace(esk_keypad* kp);
void esk_keypad_clear(esk_keypad* kp);

// Envelope JSON, caller frees with esk_free. NULL on failure, with a static
// error-code string written to *err_out (do NOT free that one).
char* esk_keypad_submit(esk_keypad* kp, const char** err_out);

// out10 must hold 10 bytes and be pre-filled with the identity layout; it is
// left untouched if the CSPRNG fails.
void esk_keypad_shuffled_layout(esk_keypad* kp, uint8_t* out10);

void esk_keypad_on_background(esk_keypad* kp);

void esk_free(char* s);

#ifdef __cplusplus
}  // extern "C"
#endif
