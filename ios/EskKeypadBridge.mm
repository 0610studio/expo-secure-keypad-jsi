// Copyright (c) 2026 expo-secure-keypad-jsi contributors. MIT License.
#import "EskKeypadBridge.h"

#include <ctime>

#include "esk/esk_c_api.h"

static int64_t NowTrampoline(void *user);

@implementation EskKeypadBridge {
  esk_keypad *_kp;
}

- (instancetype)initWithMinLength:(int32_t)minLength
                        maxLength:(int32_t)maxLength
                       keypadType:(int32_t)keypadType {
  if ((self = [super init])) {
    esk_config cfg = {};
    cfg.min_length = minLength;
    cfg.max_length = maxLength;
    cfg.keypad_type = keypadType;
    _kp = esk_keypad_create(&cfg, &NowTrampoline, (__bridge void *)self);
  }
  return self;
}

- (void)dealloc {
  if (_kp) {
    esk_keypad_destroy(_kp);
    _kp = NULL;
  }
}

- (nullable NSString *)arm:(NSString *)pem {
  if (!_kp) return @"ERR_INTERNAL";
  const char *err = esk_keypad_arm(_kp, pem.UTF8String);
  return err == NULL ? nil : @(err);
}

- (int32_t)press:(uint8_t)digit { return _kp ? esk_keypad_press(_kp, digit) : 0; }
- (int32_t)pressKey:(uint32_t)codePoint { return _kp ? esk_keypad_press_key(_kp, codePoint) : 0; }
- (int32_t)backspace { return _kp ? esk_keypad_backspace(_kp) : 0; }
- (void)clear { if (_kp) esk_keypad_clear(_kp); }

- (nullable NSString *)submit:(NSString *_Nullable *_Nullable)errOut {
  if (errOut) *errOut = nil;
  if (!_kp) {
    if (errOut) *errOut = @"ERR_INTERNAL";
    return nil;
  }
  const char *err = NULL;
  char *envelope = esk_keypad_submit(_kp, &err);
  if (envelope != NULL) {
    NSString *out = @(envelope);
    esk_free(envelope);
    return out;
  }
  if (errOut) *errOut = @(err ? err : "ERR_INTERNAL");
  return nil;
}

- (NSArray<NSNumber *> *)shuffledLayout {
  // Unshuffled fallback, same 1..9,0 order as identityDigitLayout in Swift.
  uint8_t layout[10] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 0};
  if (_kp) esk_keypad_shuffled_layout(_kp, layout);
  NSMutableArray<NSNumber *> *out = [NSMutableArray arrayWithCapacity:10];
  for (int i = 0; i < 10; i++) [out addObject:@(layout[i])];
  return out;
}

- (void)onBackground { if (_kp) esk_keypad_on_background(_kp); }

@end

static int64_t NowTrampoline(void *user) {
  (void)user;
  return (int64_t)time(NULL);
}
