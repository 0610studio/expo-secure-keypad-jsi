// Copyright (c) 2026 expo-secure-keypad-jsi contributors. MIT License.
#include "esk/PinEncryptor.h"

#include <openssl/evp.h>
#include <openssl/rsa.h>

#include <array>
#include <cstring>
#include <memory>
#include <vector>

#include "esk/Base64.h"
#include "esk/Cleanse.h"
#include "esk/Error.h"
#include "esk/SecureRandom.h"

namespace esk {

namespace {

// Cleanses on scope exit so a throw mid-encryption cannot leave plaintext.
template <size_t N>
struct ScopedCleanse {
  std::array<uint8_t, N>& ref;
  ~ScopedCleanse() { cleanse(ref.data(), N); }
};

void putUint64BE(uint8_t* p, uint64_t v) {
  for (int i = 7; i >= 0; --i) {
    p[i] = static_cast<uint8_t>(v & 0xff);
    v >>= 8;
  }
}

}  // namespace

std::string PinEncryptor::encrypt(const SecureBuffer& buffer,
                                  const PublicKey& key,
                                  int64_t nowUnixSeconds) {
  std::array<uint8_t, payload::kSize> plain{};
  ScopedCleanse<payload::kSize> guard{plain};

  size_t secretLen = 0;
  buffer.withPlaintext([&](const uint8_t* bytes, size_t len) {
    secretLen = len;
    if (len < kMinPinLength) {
      throw EskError(ErrorCode::TooShort, "secret shorter than minimum length");
    }
    if (len > kMaxSecretLength) {
      throw EskError(ErrorCode::Internal, "secret longer than max length");
    }
    std::memcpy(plain.data() + payload::kOffSecret, bytes, len);
  });

  plain[0] = payload::kMagic0;
  plain[1] = payload::kMagic1;
  plain[2] = payload::kVersion;
  plain[3] = static_cast<uint8_t>(secretLen);

  std::array<uint8_t, payload::kNonceSize> nonce{};
  secure_random::bytes(nonce.data(), nonce.size());
  std::memcpy(plain.data() + payload::kOffNonce, nonce.data(), nonce.size());

  const uint64_t ts =
      nowUnixSeconds > 0 ? static_cast<uint64_t>(nowUnixSeconds) : 0;
  putUint64BE(plain.data() + payload::kOffTimestamp, ts);

  EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new(key.evp(), nullptr);
  if (ctx == nullptr) {
    throw EskError(ErrorCode::Encrypt, "EVP_PKEY_CTX_new failed");
  }
  auto ctxGuard = std::unique_ptr<EVP_PKEY_CTX, decltype(&EVP_PKEY_CTX_free)>(
      ctx, &EVP_PKEY_CTX_free);

  if (EVP_PKEY_encrypt_init(ctx) != 1 ||
      EVP_PKEY_CTX_set_rsa_padding(ctx, RSA_PKCS1_OAEP_PADDING) != 1 ||
      EVP_PKEY_CTX_set_rsa_oaep_md(ctx, EVP_sha256()) != 1 ||
      EVP_PKEY_CTX_set_rsa_mgf1_md(ctx, EVP_sha256()) != 1) {
    throw EskError(ErrorCode::Encrypt, "OAEP parameter setup failed");
  }

  size_t ctLen = 0;
  if (EVP_PKEY_encrypt(ctx, nullptr, &ctLen, plain.data(), plain.size()) != 1) {
    throw EskError(ErrorCode::Encrypt, "encrypt length probe failed");
  }
  std::vector<uint8_t> ct(ctLen);
  if (EVP_PKEY_encrypt(ctx, ct.data(), &ctLen, plain.data(), plain.size()) != 1) {
    throw EskError(ErrorCode::Encrypt, "RSA-OAEP encrypt failed");
  }
  ct.resize(ctLen);

  // Outer envelope: all of it is non-secret. No version or algorithm field on
  // purpose — the server must fix both itself (RSA-OAEP/SHA-256, version byte
  // inside the ciphertext); anything readable here is attacker-controlled.
  const std::string ctB64 = base64::encode(ct.data(), ct.size());
  const std::string nonceB64 = base64::encode(nonce.data(), nonce.size());

  std::string json;
  json.reserve(ctB64.size() + 80);
  json += "{\"kid\":\"";
  json += key.keyId();
  json += "\",\"nonce\":\"";
  json += nonceB64;
  json += "\",\"ct\":\"";
  json += ctB64;
  json += "\"}";
  return json;
}

}  // namespace esk
