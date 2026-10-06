// Copyright (c) 2026 expo-secure-keypad-jsi contributors. MIT License.
//
// Exposes esk_c_api to Swift, which never touches C++ or OpenSSL directly.
#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

/// One native session. Main thread only.
@interface EskKeypadBridge : NSObject

/// keypadType: 0 = digit PIN pad, 1 = full QWERTY.
- (instancetype)initWithMinLength:(int32_t)minLength
                        maxLength:(int32_t)maxLength
                       keypadType:(int32_t)keypadType;

/// nil on success, else an error-code string ("ERR_WEAK_KEY", ...).
- (nullable NSString *)arm:(NSString *)pem;

- (int32_t)press:(uint8_t)digit;
/// Final Unicode code point (the view resolves shift/layer first);
/// out-of-charset input is ignored.
- (int32_t)pressKey:(uint32_t)codePoint;
- (int32_t)backspace;
- (void)clear;

/// Envelope JSON, or nil with *errOut set to an error-code string.
- (nullable NSString *)submit:(NSString *_Nullable *_Nullable)errOut;

- (NSArray<NSNumber *> *)shuffledLayout;

- (void)onBackground;

@end

NS_ASSUME_NONNULL_END
