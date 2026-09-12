// Copyright (c) 2026 expo-secure-keypad-jsi contributors. MIT License.
// Exercises the flat C ABI the platform layers call.
#include <gtest/gtest.h>

#include <cstring>
#include <string>
#include <thread>

#include "TestSupport.h"
#include "esk/esk_c_api.h"

using namespace esk_test;

namespace {
int64_t fixedNow(void*) { return 1700000000; }
}  // namespace

TEST(CApi, CreateArmSubmitDestroy) {
  RsaKeyPair kp(2048);
  std::string pem = kp.publicPem();

  esk_config cfg{};
  cfg.min_length = 4;
  cfg.max_length = 12;

  esk_keypad* kpd = esk_keypad_create(&cfg, &fixedNow, nullptr);
  ASSERT_NE(kpd, nullptr);

  EXPECT_EQ(esk_keypad_arm(kpd, pem.c_str()), nullptr);  // success

  EXPECT_EQ(esk_keypad_press(kpd, 1), 1);
  EXPECT_EQ(esk_keypad_press(kpd, 2), 2);
  EXPECT_EQ(esk_keypad_press(kpd, 3), 3);
  EXPECT_EQ(esk_keypad_press(kpd, 4), 4);

  const char* err = nullptr;
  char* envelope = esk_keypad_submit(kpd, &err);
  ASSERT_NE(envelope, nullptr);
  EXPECT_EQ(err, nullptr);

  auto payload = decryptEnvelope(std::string(envelope), kp.pkey);
  ASSERT_EQ(payload.size(), 96u);
  EXPECT_EQ(payload[4], '1');
  EXPECT_EQ(payload[7], '4');

  esk_free(envelope);
  esk_keypad_destroy(kpd);
}

TEST(CApi, ArmWeakKeyReturnsErrorString) {
  RsaKeyPair small(1024);
  std::string pem = small.publicPem();
  esk_config cfg{};
  esk_keypad* kpd = esk_keypad_create(&cfg, &fixedNow, nullptr);
  const char* err = esk_keypad_arm(kpd, pem.c_str());
  ASSERT_NE(err, nullptr);
  EXPECT_STREQ(err, "ERR_WEAK_KEY");
  esk_keypad_destroy(kpd);
}

TEST(CApi, SubmitTooShortReturnsErr) {
  RsaKeyPair kp(2048);
  std::string pem = kp.publicPem();
  esk_config cfg{};
  cfg.min_length = 4;
  cfg.max_length = 12;
  esk_keypad* kpd = esk_keypad_create(&cfg, &fixedNow, nullptr);
  esk_keypad_arm(kpd, pem.c_str());
  esk_keypad_press(kpd, 1);
  esk_keypad_press(kpd, 2);
  const char* err = nullptr;
  char* envelope = esk_keypad_submit(kpd, &err);
  EXPECT_EQ(envelope, nullptr);
  ASSERT_NE(err, nullptr);
  EXPECT_STREQ(err, "ERR_TOO_SHORT");
  esk_keypad_destroy(kpd);
}

TEST(CApi, ShuffledLayoutIsPermutation) {
  esk_config cfg{};
  esk_keypad* kpd = esk_keypad_create(&cfg, &fixedNow, nullptr);
  uint8_t layout[10];
  esk_keypad_shuffled_layout(kpd, layout);
  bool seen[10] = {false};
  for (uint8_t d : layout) {
    ASSERT_LT(d, 10);
    ASSERT_FALSE(seen[d]);
    seen[d] = true;
  }
  esk_keypad_destroy(kpd);
}

TEST(CApi, NullKeypadNeverCrashes) {
  // Every entry point is a no-throw boundary and tolerates a null handle.
  EXPECT_STREQ(esk_keypad_arm(nullptr, "x"), "ERR_INTERNAL");
  EXPECT_EQ(esk_keypad_press(nullptr, 1), 0);
  EXPECT_EQ(esk_keypad_press_key(nullptr, 'a'), 0);
  EXPECT_EQ(esk_keypad_backspace(nullptr), 0);
  esk_keypad_clear(nullptr);
  esk_keypad_on_background(nullptr);
  uint8_t layout[10] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
  esk_keypad_shuffled_layout(nullptr, layout);
  EXPECT_EQ(layout[9], 9);
  const char* err = nullptr;
  EXPECT_EQ(esk_keypad_submit(nullptr, &err), nullptr);
  EXPECT_STREQ(err, "ERR_INTERNAL");
  esk_keypad_destroy(nullptr);
}

TEST(CApi, ArmNullPemReturnsInternal) {
  esk_config cfg{};
  esk_keypad* kpd = esk_keypad_create(&cfg, &fixedNow, nullptr);
  ASSERT_NE(kpd, nullptr);
  EXPECT_STREQ(esk_keypad_arm(kpd, nullptr), "ERR_INTERNAL");
  esk_keypad_destroy(kpd);
}

// Release: the core throws, the C boundary turns that into ESK_COUNT_ERROR so
// the caller can tell "refused" from "0 entered". Debug: the core's assert
// fires first; either way the wrong thread never gets a plausible count.
#ifdef NDEBUG
TEST(CApi, CrossThreadCallReturnsErrorNotZero) {
  RsaKeyPair kp(2048);
  std::string pem = kp.publicPem();
  esk_config cfg{};
  esk_keypad* kpd = esk_keypad_create(&cfg, &fixedNow, nullptr);
  ASSERT_EQ(esk_keypad_arm(kpd, pem.c_str()), nullptr);
  EXPECT_EQ(esk_keypad_press(kpd, 7), 1);

  int32_t fromOtherThread = 0;
  std::thread([&] { fromOtherThread = esk_keypad_press(kpd, 8); }).join();
  EXPECT_EQ(fromOtherThread, ESK_COUNT_ERROR);

  // Owner thread still sees exactly the one digit; nothing was appended.
  EXPECT_EQ(esk_keypad_backspace(kpd), 0);
  esk_keypad_destroy(kpd);
}
#else
TEST(CApiDeathTest, CrossThreadCallAbortsInDebug) {
  esk_config cfg{};
  esk_keypad* kpd = esk_keypad_create(&cfg, &fixedNow, nullptr);
  EXPECT_DEATH({ std::thread([&] { esk_keypad_press(kpd, 8); }).join(); },
               "non-owning thread");
  esk_keypad_destroy(kpd);
}
#endif
