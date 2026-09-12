// Copyright (c) 2026 expo-secure-keypad-jsi contributors. MIT License.
#include "esk/SecureBuffer.h"

#include <gtest/gtest.h>

#include <thread>

#include "esk/Error.h"

using namespace esk;

TEST(SecureBuffer, StartsEmpty) {
  SecureBuffer buf;
  EXPECT_EQ(buf.length(), 0u);
}

TEST(SecureBuffer, AppendAndPop) {
  SecureBuffer buf;
  EXPECT_TRUE(buf.append('1'));
  EXPECT_TRUE(buf.append('2'));
  EXPECT_TRUE(buf.append('3'));
  EXPECT_EQ(buf.length(), 3u);
  EXPECT_TRUE(buf.pop());
  EXPECT_EQ(buf.length(), 2u);
}

TEST(SecureBuffer, PopEmptyReturnsFalse) {
  SecureBuffer buf;
  EXPECT_FALSE(buf.pop());
}

TEST(SecureBuffer, CapsAtMaxSecretLength) {
  SecureBuffer buf;
  for (size_t i = 0; i < kMaxSecretLength; ++i) EXPECT_TRUE(buf.append('7'));
  EXPECT_FALSE(buf.append('7'));  // one past capacity
  EXPECT_EQ(buf.length(), kMaxSecretLength);
}

TEST(SecureBuffer, ClearResetsLength) {
  SecureBuffer buf;
  buf.append('1');
  buf.append('2');
  buf.clear();
  EXPECT_EQ(buf.length(), 0u);
}

TEST(SecureBuffer, RejectsOutOfRangeBytes) {
  SecureBuffer buf;
  EXPECT_THROW(buf.append(9), EskError);      // raw digit, not ASCII
  EXPECT_THROW(buf.append(' '), EskError);    // space excluded
  EXPECT_THROW(buf.append(0x7F), EskError);   // DEL
  EXPECT_THROW(buf.append(0x80), EskError);   // non-ASCII
}

TEST(SecureBuffer, AcceptsFullPrintableRange) {
  SecureBuffer buf;
  EXPECT_TRUE(buf.append('!'));   // 0x21, low edge
  EXPECT_TRUE(buf.append('~'));   // 0x7E, high edge
  EXPECT_TRUE(buf.append('A'));
  EXPECT_TRUE(buf.append('z'));
  EXPECT_EQ(buf.length(), 4u);
}

TEST(SecureBuffer, WithPlaintextExposesAsciiBytes) {
  SecureBuffer buf;
  buf.append('4');
  buf.append('@');
  buf.withPlaintext([](const uint8_t* d, size_t n) {
    ASSERT_EQ(n, 2u);
    EXPECT_EQ(d[0], '4');
    EXPECT_EQ(d[1], '@');
  });
}

TEST(SecureBuffer, PopCleansesVacatedByte) {
  // The vacated slot must read back as zero, not merely be excluded by length.
  // withPlaintext hands out the page pointer; the slot past `n` is still inside
  // the mmap'd page, so peeking at it is well-defined.
  SecureBuffer buf;
  buf.append('9');
  buf.pop();
  buf.withPlaintext([](const uint8_t* d, size_t n) {
    EXPECT_EQ(n, 0u);
    EXPECT_EQ(d[0], 0u);
  });
}

TEST(SecureBuffer, ClearZeroesWholeSecretRegion) {
  SecureBuffer buf;
  for (size_t i = 0; i < kMaxSecretLength; ++i) buf.append('7');
  buf.clear();
  buf.withPlaintext([](const uint8_t* d, size_t n) {
    EXPECT_EQ(n, 0u);
    for (size_t i = 0; i < kMaxSecretLength; ++i) {
      ASSERT_EQ(d[i], 0u) << "byte " << i << " survived clear()";
    }
  });
}

TEST(SecureBuffer, ReportsWhetherPageIsLocked) {
  // The value is platform dependent (RLIMIT_MEMLOCK), so only assert that the
  // getter exists and is stable; README documents the swap guarantee as
  // conditional on this being true.
  SecureBuffer buf;
  const bool a = buf.isLocked();
  buf.append('1');
  EXPECT_EQ(buf.isLocked(), a);
}

// assertOwnerThread() asserts first and throws second, so a Debug build dies
// and a Release build throws. Cover whichever this configuration is.
#ifdef NDEBUG
TEST(SecureBuffer, CrossThreadAccessThrows) {
  SecureBuffer buf;
  buf.append('1');
  bool threw = false;
  std::thread([&] {
    try {
      buf.length();
    } catch (const EskError&) {
      threw = true;
    }
  }).join();
  EXPECT_TRUE(threw);
}
#else
TEST(SecureBufferDeathTest, CrossThreadAccessAbortsInDebug) {
  SecureBuffer buf;
  buf.append('1');
  EXPECT_DEATH(
      { std::thread([&] { (void)buf.length(); }).join(); },
      "non-owning thread");
}
#endif
