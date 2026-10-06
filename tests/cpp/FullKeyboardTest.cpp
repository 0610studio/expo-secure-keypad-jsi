// Copyright (c) 2026 expo-secure-keypad-jsi contributors. MIT License.
// Full QWERTY keyboard mode: charset, length caps, and the v2 wire format.
#include <gtest/gtest.h>

#include <string>

#include "TestSupport.h"
#include "esk/Error.h"
#include "esk/KeypadCore.h"
#include "esk/PinEncryptor.h"
#include "esk/esk_c_api.h"

using namespace esk;
using namespace esk_test;

namespace {

KeypadCore makeFullCore(size_t minLen = 4, size_t maxLen = 64,
                        int64_t now = 1700000000) {
  return KeypadCore([now]() -> int64_t { return now; }, minLen, maxLen,
                    KeypadType::Full);
}

void pressString(KeypadCore& core, const std::string& s) {
  for (char c : s) core.pressKey(static_cast<uint8_t>(c));
}

}  // namespace

TEST(FullKeyboard, RoundtripRecoversMixedSecret) {
  RsaKeyPair kp(2048);
  auto core = makeFullCore();
  ASSERT_FALSE(core.arm(kp.publicPem()).has_value());

  const std::string secret = "aB9!@#$%^&*()_+~`";
  pressString(core, secret);
  ASSERT_EQ(core.digitCount(), secret.size());

  std::string envelope = core.submit();
  EXPECT_EQ(core.digitCount(), 0u);  // cleansed after submit

  auto payload = decryptEnvelope(envelope, kp.pkey);
  ASSERT_EQ(payload.size(), payload::kSize);
  EXPECT_EQ(payload[0], 'S');
  EXPECT_EQ(payload[1], 'K');
  EXPECT_EQ(payload[2], payload::kVersion);
  EXPECT_EQ(payload[3], secret.size());
  for (size_t i = 0; i < secret.size(); ++i) {
    EXPECT_EQ(payload[payload::kOffSecret + i],
              static_cast<uint8_t>(secret[i]));
  }
  EXPECT_EQ(payload[payload::kOffSecret + secret.size()], 0);  // zero padded

  uint64_t ts = 0;
  for (int i = 0; i < 8; ++i)
    ts = (ts << 8) | payload[payload::kOffTimestamp + i];
  EXPECT_EQ(ts, 1700000000ull);
}

TEST(FullKeyboard, EnvelopeCarriesOnlyKidNonceCt) {
  RsaKeyPair kp(2048);
  auto core = makeFullCore();
  core.arm(kp.publicPem());
  pressString(core, "hunter2!");
  std::string envelope = core.submit();
  EXPECT_EQ(envelope.rfind("{\"kid\":\"", 0), 0u);
  EXPECT_EQ(envelope.find("\"v\""), std::string::npos);
  EXPECT_EQ(envelope.find("\"alg\""), std::string::npos);
  EXPECT_FALSE(jsonField(envelope, "nonce").empty());
  EXPECT_FALSE(jsonField(envelope, "ct").empty());
}

TEST(FullKeyboard, DigitModeSharesTheWireFormat) {
  RsaKeyPair kp(2048);
  KeypadCore core([]() -> int64_t { return 0; });
  core.arm(kp.publicPem());
  for (int i = 0; i < 4; ++i) core.pressDigit(5);
  std::string envelope = core.submit();
  auto payload = decryptEnvelope(envelope, kp.pkey);
  ASSERT_EQ(payload.size(), payload::kSize);
  EXPECT_EQ(payload[2], payload::kVersion);
  EXPECT_EQ(payload[3], 4);
  EXPECT_EQ(payload[payload::kOffSecret + 3], '5');
  EXPECT_EQ(payload[payload::kOffSecret + 4], 0);  // zero padded
}

TEST(FullKeyboard, MaxSecretLengthRoundtripsOnRsa2048) {
  // 64-char secret in a 96-byte payload must fit RSA-2048 OAEP (190-byte cap).
  RsaKeyPair kp(2048);
  auto core = makeFullCore();
  core.arm(kp.publicPem());
  const std::string secret(64, 'x');
  pressString(core, secret);
  ASSERT_EQ(core.digitCount(), 64u);
  auto payload = decryptEnvelope(core.submit(), kp.pkey);
  ASSERT_EQ(payload.size(), payload::kSize);
  EXPECT_EQ(payload[3], 64);
  EXPECT_EQ(payload[payload::kOffSecret + 63], 'x');
}

TEST(FullKeyboard, IgnoresSpaceAndNonPrintable) {
  RsaKeyPair kp(2048);
  auto core = makeFullCore();
  core.arm(kp.publicPem());
  EXPECT_EQ(core.pressKey(' '), 0u);
  EXPECT_EQ(core.pressKey('\n'), 0u);
  EXPECT_EQ(core.pressKey(0x7F), 0u);
  EXPECT_EQ(core.pressKey(0x80), 0u);
  EXPECT_EQ(core.pressKey(0xE9), 0u);     // é: not on the keyboard
  EXPECT_EQ(core.pressKey(0x1F600), 0u);  // emoji
  EXPECT_EQ(core.pressKey(0xFFE6), 0u);   // fullwidth ￦: the keyboards emit ₩
  EXPECT_EQ(core.pressKey('a'), 1u);
}

TEST(FullKeyboard, DigitModeIgnoresLetters) {
  RsaKeyPair kp(2048);
  KeypadCore core([]() -> int64_t { return 0; });
  core.arm(kp.publicPem());
  EXPECT_EQ(core.pressKey('a'), 0u);
  EXPECT_EQ(core.pressKey('!'), 0u);
  EXPECT_EQ(core.pressKey(0x20A9), 0u);  // ₩: full keyboard only
  EXPECT_EQ(core.pressKey('7'), 1u);
}

TEST(FullKeyboard, ExtraSymbolsRoundtripAsUtf8) {
  RsaKeyPair kp(2048);
  auto core = makeFullCore();
  core.arm(kp.publicPem());
  EXPECT_EQ(core.pressKey(0x20A9), 1u);  // ₩
  EXPECT_EQ(core.pressKey(0x2661), 2u);  // ♡
  EXPECT_EQ(core.pressKey(0x00B0), 3u);  // °
  EXPECT_EQ(core.pressKey('a'), 4u);
  ASSERT_EQ(core.digitCount(), 4u);  // characters, not bytes

  auto payload = decryptEnvelope(core.submit(), kp.pkey);
  ASSERT_EQ(payload.size(), payload::kSize);
  const uint8_t want[] = {0xE2, 0x82, 0xA9, 0xE2, 0x99, 0xA1, 0xC2, 0xB0, 'a'};
  EXPECT_EQ(payload[3], sizeof(want));  // secretLength is in bytes
  for (size_t i = 0; i < sizeof(want); ++i) {
    EXPECT_EQ(payload[payload::kOffSecret + i], want[i]) << "byte " << i;
  }
  EXPECT_EQ(payload[payload::kOffSecret + sizeof(want)], 0);
}

TEST(FullKeyboard, BackspaceRemovesWholeSymbol) {
  RsaKeyPair kp(2048);
  auto core = makeFullCore();
  core.arm(kp.publicPem());
  pressString(core, "abc");
  core.pressKey(0x2606);  // ☆
  EXPECT_EQ(core.backspace(), 3u);
  core.pressKey('d');

  auto payload = decryptEnvelope(core.submit(), kp.pkey);
  ASSERT_EQ(payload.size(), payload::kSize);
  EXPECT_EQ(payload[3], 4);
  EXPECT_EQ(std::string(payload.begin() + payload::kOffSecret,
                        payload.begin() + payload::kOffSecret + 4),
            "abcd");
}

TEST(FullKeyboard, MinLengthCountsCharactersNotBytes) {
  RsaKeyPair kp(2048);
  auto core = makeFullCore(4, 64);
  core.arm(kp.publicPem());
  // Three 3-byte symbols = 9 bytes, but only 3 characters: still too short.
  for (int i = 0; i < 3; ++i) core.pressKey(0x2661);
  try {
    core.submit();
    FAIL() << "expected throw";
  } catch (const EskError& e) {
    EXPECT_EQ(e.code(), ErrorCode::TooShort);
  }
}

TEST(FullKeyboard, ByteCapCanStopInputBeforeMaxLength) {
  RsaKeyPair kp(2048);
  auto core = makeFullCore(4, 64);
  core.arm(kp.publicPem());
  for (int i = 0; i < 30; ++i) core.pressKey(0x2661);
  EXPECT_EQ(core.digitCount(), 21u);  // 21 x 3 = 63 bytes; the 22nd won't fit
  EXPECT_EQ(core.pressKey('a'), 22u);
  auto payload = decryptEnvelope(core.submit(), kp.pkey);
  ASSERT_EQ(payload.size(), payload::kSize);
  EXPECT_EQ(payload[3], 64);
}

TEST(FullKeyboard, ConfiguredMaxLengthEnforced) {
  RsaKeyPair kp(2048);
  auto core = makeFullCore(4, 8);
  core.arm(kp.publicPem());
  pressString(core, "abcdefghijkl");
  EXPECT_EQ(core.digitCount(), 8u);  // capped at configured max
}

TEST(FullKeyboard, LengthsClampTo64NotTwelve) {
  RsaKeyPair kp(2048);
  auto core = makeFullCore(4, 64);
  core.arm(kp.publicPem());
  const std::string secret(30, 'q');
  pressString(core, secret);
  EXPECT_EQ(core.digitCount(), 30u);  // digit-mode 12 cap must not apply
}

TEST(FullKeyboard, CApiPressKeyAndKeypadType) {
  RsaKeyPair kp(2048);
  std::string pem = kp.publicPem();

  esk_config cfg{};
  cfg.min_length = 4;
  cfg.max_length = 64;
  cfg.keypad_type = 1;

  esk_keypad* kpd =
      esk_keypad_create(&cfg, [](void*) -> int64_t { return 1700000000; },
                        nullptr);
  ASSERT_NE(kpd, nullptr);
  EXPECT_EQ(esk_keypad_arm(kpd, pem.c_str()), nullptr);

  EXPECT_EQ(esk_keypad_press_key(kpd, 'P'), 1);
  EXPECT_EQ(esk_keypad_press_key(kpd, '@'), 2);
  EXPECT_EQ(esk_keypad_press_key(kpd, 's'), 3);
  EXPECT_EQ(esk_keypad_press_key(kpd, '5'), 4);
  EXPECT_EQ(esk_keypad_press_key(kpd, ' '), 4);  // ignored
  EXPECT_EQ(esk_keypad_press_key(kpd, 0x2661), 5);  // ♡, 3 bytes

  const char* err = nullptr;
  char* envelope = esk_keypad_submit(kpd, &err);
  ASSERT_NE(envelope, nullptr);
  EXPECT_EQ(err, nullptr);

  auto payload = decryptEnvelope(std::string(envelope), kp.pkey);
  ASSERT_EQ(payload.size(), payload::kSize);
  EXPECT_EQ(payload[2], payload::kVersion);
  EXPECT_EQ(payload[payload::kOffSecret + 0], 'P');
  EXPECT_EQ(payload[payload::kOffSecret + 1], '@');
  EXPECT_EQ(payload[payload::kOffSecret + 2], 's');
  EXPECT_EQ(payload[payload::kOffSecret + 3], '5');
  EXPECT_EQ(payload[3], 7);  // 4 ASCII bytes + 3 for ♡
  EXPECT_EQ(payload[payload::kOffSecret + 4], 0xE2);

  esk_free(envelope);
  esk_keypad_destroy(kpd);
}

TEST(FullKeyboard, BackgroundClearsSecret) {
  RsaKeyPair kp(2048);
  auto core = makeFullCore();
  core.arm(kp.publicPem());
  pressString(core, "Secret1!");
  core.onHostBackground();
  EXPECT_EQ(core.digitCount(), 0u);
}
