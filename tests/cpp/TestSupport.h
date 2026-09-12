// Copyright (c) 2026 expo-secure-keypad-jsi contributors. MIT License.
// Shared helpers for the host C++ test suite: throwaway RSA key generation and
// a reference envelope decryptor that mirrors what a server would do.
#pragma once

#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>

#include <cassert>
#include <cstdint>
#include <string>
#include <vector>

#include "esk/Base64.h"

namespace esk_test {

// RAII wrapper for a generated RSA keypair.
struct RsaKeyPair {
  EVP_PKEY* pkey = nullptr;

  explicit RsaKeyPair(int bits) {
    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, nullptr);
    EVP_PKEY_keygen_init(ctx);
    EVP_PKEY_CTX_set_rsa_keygen_bits(ctx, bits);
    EVP_PKEY_keygen(ctx, &pkey);
    EVP_PKEY_CTX_free(ctx);
  }
  ~RsaKeyPair() { if (pkey) EVP_PKEY_free(pkey); }
  RsaKeyPair(const RsaKeyPair&) = delete;
  RsaKeyPair& operator=(const RsaKeyPair&) = delete;

  std::string publicPem() const {
    BIO* bio = BIO_new(BIO_s_mem());
    PEM_write_bio_PUBKEY(bio, pkey);
    char* data = nullptr;
    long len = BIO_get_mem_data(bio, &data);
    std::string s(data, static_cast<size_t>(len));
    BIO_free(bio);
    return s;
  }
};

// Extracts a "key":"value" string field from a flat JSON object.
inline std::string jsonField(const std::string& json, const char* key) {
  std::string pat = std::string("\"") + key + "\":\"";
  size_t a = json.find(pat);
  if (a == std::string::npos) return {};
  a += pat.size();
  size_t b = json.find('"', a);
  return json.substr(a, b - a);
}

// Decrypts an envelope's `ct` with the private key and returns the raw payload.
inline std::vector<uint8_t> decryptEnvelope(const std::string& json,
                                            EVP_PKEY* priv) {
  std::string ctB64 = jsonField(json, "ct");
  std::string ct;
  esk::base64::decode(ctB64, ct);

  EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new(priv, nullptr);
  EVP_PKEY_decrypt_init(ctx);
  EVP_PKEY_CTX_set_rsa_padding(ctx, RSA_PKCS1_OAEP_PADDING);
  EVP_PKEY_CTX_set_rsa_oaep_md(ctx, EVP_sha256());
  EVP_PKEY_CTX_set_rsa_mgf1_md(ctx, EVP_sha256());
  size_t outLen = 0;
  const uint8_t* in = reinterpret_cast<const uint8_t*>(ct.data());
  EVP_PKEY_decrypt(ctx, nullptr, &outLen, in, ct.size());
  std::vector<uint8_t> out(outLen);
  int rc = EVP_PKEY_decrypt(ctx, out.data(), &outLen, in, ct.size());
  EVP_PKEY_CTX_free(ctx);
  if (rc != 1) return {};
  out.resize(outLen);
  return out;
}

}  // namespace esk_test
