// Copyright (c) 2026 expo-secure-keypad-jsi contributors. MIT License.
#include "esk/PublicKeyValidator.h"

#include <openssl/bio.h>
#include <openssl/bn.h>
#include <openssl/pem.h>
#include <openssl/sha.h>
#include <openssl/x509.h>

#include "esk/Error.h"

namespace esk {

namespace {

// Computes SHA-256 over the DER-encoded SubjectPublicKeyInfo (i2d_PUBKEY),
// which is exactly the bytes inside a PEM PUBLIC KEY block.
std::array<uint8_t, 32> computeSpkiSha256(EVP_PKEY* pkey) {
  unsigned char* der = nullptr;
  const int derLen = i2d_PUBKEY(pkey, &der);
  if (derLen <= 0 || der == nullptr) {
    throw EskError(ErrorCode::KeyParse, "i2d_PUBKEY failed");
  }
  std::array<uint8_t, 32> digest{};
  SHA256(der, static_cast<size_t>(derLen), digest.data());
  OPENSSL_free(der);
  return digest;
}

}  // namespace

std::unique_ptr<PublicKey> PublicKey::parse(const std::string& pem, int minBits,
                                           int maxBits) {
  BIO* bio = BIO_new_mem_buf(pem.data(), static_cast<int>(pem.size()));
  if (bio == nullptr) {
    throw EskError(ErrorCode::KeyParse, "BIO alloc failed");
  }
  EVP_PKEY* pkey = PEM_read_bio_PUBKEY(bio, nullptr, nullptr, nullptr);
  BIO_free(bio);
  if (pkey == nullptr) {
    throw EskError(ErrorCode::KeyParse,
                   "could not parse PEM SubjectPublicKeyInfo");
  }

  // RAII from here: any throw must free pkey.
  auto fail = [&](ErrorCode code, const char* msg) {
    EVP_PKEY_free(pkey);
    throw EskError(code, msg);
  };

  if (EVP_PKEY_base_id(pkey) != EVP_PKEY_RSA) {
    fail(ErrorCode::KeyNotRsa, "public key is not RSA");
  }

  const int bits = EVP_PKEY_bits(pkey);
  if (bits < minBits) {
    fail(ErrorCode::WeakKey, "RSA modulus below minimum bit length");
  }
  if (bits > maxBits) {
    fail(ErrorCode::WeakKey, "RSA modulus above maximum bit length");
  }

  // e=3 is OAEP-safe in principle; rejected anyway and deliberately so.
  {
    BIGNUM* e = nullptr;
    if (EVP_PKEY_get_bn_param(pkey, "e", &e) != 1 || e == nullptr) {
      fail(ErrorCode::WeakKey, "could not read RSA public exponent");
    }
    const bool odd = BN_is_odd(e);
    BIGNUM* min_e = BN_new();
    BN_set_word(min_e, 65537);
    const bool tooSmall = BN_cmp(e, min_e) < 0;
    BN_free(min_e);
    BN_free(e);
    if (!odd || tooSmall) {
      fail(ErrorCode::WeakKey, "RSA public exponent must be odd and >= 65537");
    }
  }

  std::array<uint8_t, 32> spki;
  try {
    spki = computeSpkiSha256(pkey);
  } catch (...) {
    EVP_PKEY_free(pkey);
    throw;
  }

  std::unique_ptr<PublicKey> key(new PublicKey(pkey));
  key->spkiSha256_ = spki;
  return key;
}

PublicKey::~PublicKey() {
  if (pkey_ != nullptr) {
    EVP_PKEY_free(pkey_);
    pkey_ = nullptr;
  }
}

std::string PublicKey::keyId() const {
  static const char hex[] = "0123456789abcdef";
  std::string out;
  out.reserve(16);
  for (size_t i = 0; i < 8; ++i) {
    out.push_back(hex[spkiSha256_[i] >> 4]);
    out.push_back(hex[spkiSha256_[i] & 0x0f]);
  }
  return out;
}

}  // namespace esk
