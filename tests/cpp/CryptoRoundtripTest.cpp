// Copyright (c) 2026 expo-secure-keypad-jsi contributors. MIT License.
#include <gtest/gtest.h>
#include <openssl/ec.h>
#include <openssl/obj_mac.h>

#include <cstring>

#include "TestSupport.h"
#include "esk/Base64.h"
#include "esk/Error.h"
#include "esk/KeypadCore.h"
#include "esk/PinEncryptor.h"
#include "esk/SecureRandom.h"

using namespace esk;
using namespace esk_test;

namespace {
KeypadCore makeCore(int64_t now = 1700000000) {
  return KeypadCore([now]() -> int64_t { return now; });
}
}  // namespace

TEST(Base64, Roundtrip) {
  const std::string msg = "hello secure keypad";
  std::string enc =
      base64::encode(reinterpret_cast<const uint8_t*>(msg.data()), msg.size());
  std::string dec;
  ASSERT_TRUE(base64::decode(enc, dec));
  EXPECT_EQ(dec, msg);
}

TEST(SecureRandom, ShuffledDigitsIsPermutation) {
  for (int iter = 0; iter < 50; ++iter) {
    auto layout = secure_random::shuffledDigits();
    bool seen[10] = {false};
    for (uint8_t d : layout) {
      ASSERT_LT(d, 10);
      ASSERT_FALSE(seen[d]);
      seen[d] = true;
    }
  }
}

TEST(Crypto, FullRoundtripRecoversPin) {
  RsaKeyPair kp(2048);
  auto core = makeCore(1700000000);
  ASSERT_FALSE(core.arm(kp.publicPem()).has_value());
  EXPECT_EQ(core.state(), KeypadState::Armed);

  core.pressDigit(1);
  core.pressDigit(3);
  core.pressDigit(3);
  core.pressDigit(7);
  ASSERT_EQ(core.digitCount(), 4u);

  std::string envelope = core.submit();
  EXPECT_EQ(core.digitCount(), 0u);  // cleansed after submit

  auto payload = decryptEnvelope(envelope, kp.pkey);
  ASSERT_EQ(payload.size(), payload::kSize);
  EXPECT_EQ(payload[0], 'S');
  EXPECT_EQ(payload[1], 'K');
  EXPECT_EQ(payload[2], payload::kVersion);
  EXPECT_EQ(payload[3], 4);
  EXPECT_EQ(payload[payload::kOffSecret + 0], '1');
  EXPECT_EQ(payload[payload::kOffSecret + 1], '3');
  EXPECT_EQ(payload[payload::kOffSecret + 2], '3');
  EXPECT_EQ(payload[payload::kOffSecret + 3], '7');
  EXPECT_EQ(payload[payload::kOffSecret + 4], 0);  // zero padded

  uint64_t ts = 0;
  for (int i = 0; i < 8; ++i)
    ts = (ts << 8) | payload[payload::kOffTimestamp + i];
  EXPECT_EQ(ts, 1700000000ull);
}

TEST(Crypto, EnvelopeContainsKidAndNonce) {
  RsaKeyPair kp(2048);
  auto core = makeCore();
  core.arm(kp.publicPem());
  core.pressDigit(1);
  core.pressDigit(2);
  core.pressDigit(3);
  core.pressDigit(4);
  std::string envelope = core.submit();
  EXPECT_EQ(jsonField(envelope, "kid").size(), 16u);
  EXPECT_FALSE(jsonField(envelope, "nonce").empty());
}

TEST(Crypto, TwoSubmitsProduceDifferentCiphertext) {
  // OAEP randomization + fresh nonce => never identical.
  RsaKeyPair kp(2048);
  auto core = makeCore();
  core.arm(kp.publicPem());
  for (int i = 0; i < 4; ++i) core.pressDigit(5);
  std::string a = core.submit();
  core.arm(kp.publicPem());
  for (int i = 0; i < 4; ++i) core.pressDigit(5);
  std::string b = core.submit();
  EXPECT_NE(jsonField(a, "ct"), jsonField(b, "ct"));
  EXPECT_NE(jsonField(a, "nonce"), jsonField(b, "nonce"));
}

TEST(Crypto, WeakKeyRejected) {
  RsaKeyPair small(1024);
  auto core = makeCore();
  auto err = core.arm(small.publicPem());
  ASSERT_TRUE(err.has_value());
  EXPECT_EQ(*err, "ERR_WEAK_KEY");
  EXPECT_EQ(core.state(), KeypadState::Unarmed);
}

TEST(Crypto, GarbageKeyRejected) {
  auto core = makeCore();
  auto err = core.arm("-----BEGIN PUBLIC KEY-----\nnotbase64\n-----END PUBLIC KEY-----\n");
  ASSERT_TRUE(err.has_value());
  EXPECT_EQ(*err, "ERR_KEY_PARSE");
}

TEST(Crypto, SubmitBelowMinLengthThrows) {
  RsaKeyPair kp(2048);
  auto core = makeCore();
  core.arm(kp.publicPem());
  core.pressDigit(1);
  core.pressDigit(2);
  try {
    core.submit();
    FAIL() << "expected throw";
  } catch (const EskError& e) {
    EXPECT_EQ(e.code(), ErrorCode::TooShort);
  }
}

TEST(Crypto, SubmitEmptyThrows) {
  RsaKeyPair kp(2048);
  auto core = makeCore();
  core.arm(kp.publicPem());
  try {
    core.submit();
    FAIL() << "expected throw";
  } catch (const EskError& e) {
    EXPECT_EQ(e.code(), ErrorCode::Empty);
  }
}

TEST(Crypto, SubmitWithoutArmThrows) {
  auto core = makeCore();
  EXPECT_THROW(core.submit(), EskError);
}

TEST(KeypadCore, MaxLengthEnforced) {
  RsaKeyPair kp(2048);
  KeypadCore core([]() -> int64_t { return 0; }, 4, 6);
  core.arm(kp.publicPem());
  for (int i = 0; i < 10; ++i) core.pressDigit(1);
  EXPECT_EQ(core.digitCount(), 6u);  // capped at configured max
}

TEST(KeypadCore, BackgroundClearsBuffer) {
  RsaKeyPair kp(2048);
  auto core = makeCore();
  core.arm(kp.publicPem());
  core.pressDigit(1);
  core.pressDigit(2);
  core.onHostBackground();
  EXPECT_EQ(core.digitCount(), 0u);
}

TEST(KeypadCore, RejectedRearmClearsPreviousSecret) {
  RsaKeyPair good(2048);
  RsaKeyPair weak(1024);
  auto core = makeCore();
  ASSERT_FALSE(core.arm(good.publicPem()).has_value());
  core.pressDigit(1);
  core.pressDigit(2);
  ASSERT_EQ(core.digitCount(), 2u);

  ASSERT_TRUE(core.arm(weak.publicPem()).has_value());
  EXPECT_EQ(core.state(), KeypadState::Unarmed);
  EXPECT_EQ(core.digitCount(), 0u);  // stale digits must not survive
}

// --- Invariants the server relies on -------------------------------------

TEST(Crypto, EnvelopeNonceEqualsPlaintextNonce) {
  // README tells servers to reject when the outer `nonce` differs from the one
  // inside the ciphertext. That only works if we write the same bytes to both.
  RsaKeyPair kp(2048);
  auto core = makeCore();
  core.arm(kp.publicPem());
  for (int i = 0; i < 4; ++i) core.pressDigit(2);
  std::string envelope = core.submit();

  std::string outer;
  ASSERT_TRUE(base64::decode(jsonField(envelope, "nonce"), outer));
  ASSERT_EQ(outer.size(), payload::kNonceSize);

  auto payload = decryptEnvelope(envelope, kp.pkey);
  ASSERT_EQ(payload.size(), payload::kSize);
  EXPECT_EQ(std::memcmp(outer.data(), payload.data() + payload::kOffNonce,
                        payload::kNonceSize),
            0);
}

TEST(Crypto, KidIsSpkiSha256Prefix) {
  RsaKeyPair kp(2048);
  auto key = PublicKey::parse(kp.publicPem());
  auto core = makeCore();
  core.arm(kp.publicPem());
  for (int i = 0; i < 4; ++i) core.pressDigit(2);
  EXPECT_EQ(jsonField(core.submit(), "kid"), key->keyId());
}

// --- PublicKey::parse branches --------------------------------------------

TEST(PublicKey, RejectsNonRsaKey) {
  EVP_PKEY* ec = nullptr;
  EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_EC, nullptr);
  ASSERT_EQ(EVP_PKEY_keygen_init(ctx), 1);
  ASSERT_EQ(EVP_PKEY_CTX_set_ec_paramgen_curve_nid(ctx, NID_X9_62_prime256v1), 1);
  ASSERT_EQ(EVP_PKEY_keygen(ctx, &ec), 1);
  EVP_PKEY_CTX_free(ctx);
  BIO* bio = BIO_new(BIO_s_mem());
  PEM_write_bio_PUBKEY(bio, ec);
  char* data = nullptr;
  long len = BIO_get_mem_data(bio, &data);
  std::string pem(data, static_cast<size_t>(len));
  BIO_free(bio);
  EVP_PKEY_free(ec);

  try {
    PublicKey::parse(pem);
    FAIL() << "EC key must be rejected";
  } catch (const EskError& e) {
    EXPECT_EQ(e.code(), ErrorCode::KeyNotRsa);
  }
}

TEST(PublicKey, RejectsSmallPublicExponent) {
  EVP_PKEY* pkey = nullptr;
  EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, nullptr);
  ASSERT_EQ(EVP_PKEY_keygen_init(ctx), 1);
  ASSERT_EQ(EVP_PKEY_CTX_set_rsa_keygen_bits(ctx, 2048), 1);
  BIGNUM* e = BN_new();
  BN_set_word(e, 3);
  ASSERT_EQ(EVP_PKEY_CTX_set1_rsa_keygen_pubexp(ctx, e), 1);
  BN_free(e);
  ASSERT_EQ(EVP_PKEY_keygen(ctx, &pkey), 1);
  EVP_PKEY_CTX_free(ctx);
  BIO* bio = BIO_new(BIO_s_mem());
  PEM_write_bio_PUBKEY(bio, pkey);
  char* data = nullptr;
  long len = BIO_get_mem_data(bio, &data);
  std::string pem(data, static_cast<size_t>(len));
  BIO_free(bio);
  EVP_PKEY_free(pkey);

  try {
    PublicKey::parse(pem);
    FAIL() << "e=3 must be rejected";
  } catch (const EskError& e2) {
    EXPECT_EQ(e2.code(), ErrorCode::WeakKey);
  }
}

TEST(PublicKey, RejectsModulusAboveMaxBits) {
  // 3072-bit key against a 2048-bit ceiling stands in for the 8192 cap
  // without spending seconds on keygen.
  RsaKeyPair kp(3072);
  try {
    PublicKey::parse(kp.publicPem(), 2048, 2048);
    FAIL() << "oversized modulus must be rejected";
  } catch (const EskError& e) {
    EXPECT_EQ(e.code(), ErrorCode::WeakKey);
  }
  EXPECT_NO_THROW(PublicKey::parse(kp.publicPem(), 2048, 8192));
}

TEST(PublicKey, RejectsPkcs1RsaPublicKeyBlock) {
  // Only SubjectPublicKeyInfo ("BEGIN PUBLIC KEY") is accepted.
  auto core = makeCore();
  auto err = core.arm(
      "-----BEGIN RSA PUBLIC KEY-----\nMIIBCgKCAQEA\n-----END RSA PUBLIC KEY-----\n");
  ASSERT_TRUE(err.has_value());
  EXPECT_EQ(*err, "ERR_KEY_PARSE");
}
