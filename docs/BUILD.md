# Build notes: OpenSSL provenance and fallback

한국어 문서: [BUILD.ko.md](BUILD.ko.md)

The security core links OpenSSL `libcrypto` statically on both platforms.
Review this file whenever the version changes.

## What is used

| Platform | Artifact | Version | Pinned in |
|---|---|---|---|
| Android | `io.github.ronickg:openssl-static` (prefab AAR, Maven Central) | `3.6.2-2` | `android/build.gradle` |
| iOS | `OpenSSL-Universal` (CocoaPods XCFramework) | `3.6.2000` | `SecureKeypadJsi.podspec` |

Both are third-party repackagings of upstream OpenSSL 3.6.2, not
OpenSSL-project releases.

## Symbol isolation

`android/CMakeLists.txt` links with `-Wl,--exclude-libs,ALL` and
`-fvisibility=hidden` so the static libcrypto can never interpose with another
OpenSSL in the same process (react-native-quick-crypto issue #1059). Do not
remove those flags to "fix" a link error.

## Fallback plan

If either artifact is pulled, build OpenSSL from the upstream tarball with the
NDK / Xcode toolchains (`no-shared`, `no-tests`, `no-apps`, `no-ssl` — only
libcrypto is needed), ship the result as a local prefab AAR (Android) and an
XCFramework (iOS) inside this package, and switch the two dependency lines to
those.

Only these `libcrypto` symbols are used, which keeps a minimal custom build
viable: `EVP_PKEY_*`, `PEM_read_bio_PUBKEY`, `i2d_PUBKEY`, `BIO_*`, `BN_*`,
`SHA256`, `RAND_bytes`, `EVP_EncodeBlock` / `EVP_DecodeBlock`,
`OPENSSL_cleanse`, `OPENSSL_free`.
