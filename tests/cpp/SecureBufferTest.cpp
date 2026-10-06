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
  buf.append(0x2661);
  buf.clear();
  EXPECT_EQ(buf.length(), 0u);
  EXPECT_EQ(buf.charCount(), 0u);
}

TEST(SecureBuffer, RejectsOutOfRangeBytes) {
  SecureBuffer buf;
  EXPECT_THROW(buf.append(9), EskError);       // raw digit, not ASCII
  EXPECT_THROW(buf.append(' '), EskError);     // space excluded
  EXPECT_THROW(buf.append(0x7F), EskError);    // DEL
  EXPECT_THROW(buf.append(0x80), EskError);    // C1 control
  EXPECT_THROW(buf.append(0xE9), EskError);    // é: non-ASCII, not listed
  EXPECT_THROW(buf.append(0xD800), EskError);  // lone surrogate
  EXPECT_THROW(buf.append(0x1F600), EskError); // emoji, outside the BMP
}

TEST(SecureBuffer, EncodesExtraSymbolsAsUtf8) {
  SecureBuffer buf;
  EXPECT_TRUE(buf.append(0x00A3));  // £, 2 bytes
  EXPECT_TRUE(buf.append(0x2661));  // ♡, 3 bytes
  EXPECT_TRUE(buf.append('a'));
  EXPECT_EQ(buf.charCount(), 3u);
  EXPECT_EQ(buf.length(), 6u);
  buf.withPlaintext([](const uint8_t* d, size_t n) {
    const uint8_t want[] = {0xC2, 0xA3, 0xE2, 0x99, 0xA1, 'a'};
    ASSERT_EQ(n, sizeof(want));
    for (size_t i = 0; i < n; ++i) EXPECT_EQ(d[i], want[i]) << "byte " << i;
  });
}

TEST(SecureBuffer, AcceptsEveryExtraSymbol) {
  SecureBuffer buf;
  for (uint32_t cp : kExtraSymbols) {
    buf.clear();
    EXPECT_TRUE(buf.append(cp)) << std::hex << cp;
    EXPECT_EQ(buf.charCount(), 1u);
    EXPECT_GE(buf.length(), 2u);
    EXPECT_LE(buf.length(), 3u);
  }
}

TEST(SecureBuffer, PopRemovesWholeCharacterAndCleansesIt) {
  SecureBuffer buf;
  buf.append('x');
  buf.append(0x20A9);  // ₩, 3 bytes
  ASSERT_TRUE(buf.pop());
  EXPECT_EQ(buf.charCount(), 1u);
  EXPECT_EQ(buf.length(), 1u);
  buf.withPlaintext([](const uint8_t* d, size_t n) {
    ASSERT_EQ(n, 1u);
    EXPECT_EQ(d[0], 'x');
    for (size_t i = 1; i < 4; ++i) EXPECT_EQ(d[i], 0u) << "byte " << i;
  });
}

TEST(SecureBuffer, RefusesCharacterThatWouldOverflowByteCap) {
  SecureBuffer buf;
  for (int i = 0; i < 21; ++i) ASSERT_TRUE(buf.append(0x2661));  // 63 bytes
  EXPECT_FALSE(buf.append(0x2661));  // 66 > 64: refused whole, not split
  EXPECT_EQ(buf.length(), 63u);
  EXPECT_EQ(buf.charCount(), 21u);
  EXPECT_TRUE(buf.append('a'));  // one byte still fits
  EXPECT_EQ(buf.length(), kMaxSecretLength);
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
