# 빌드 노트: OpenSSL 출처와 대체 계획

English: [BUILD.md](BUILD.md)

보안 코어는 양 플랫폼 모두 OpenSSL `libcrypto`를 정적 링크합니다. 버전을 올릴 때마다
이 문서를 다시 검토하십시오.

## 사용 중인 것

| 플랫폼 | 아티팩트 | 버전 | 고정 위치 |
|---|---|---|---|
| Android | `io.github.ronickg:openssl-static` (prefab AAR, Maven Central) | `3.6.2-2` | `android/build.gradle` |
| iOS | `OpenSSL-Universal` (CocoaPods XCFramework) | `3.6.2000` | `SecureKeypadJsi.podspec` |

둘 다 upstream OpenSSL 3.6.2를 제3자가 재패키징한 것이고 OpenSSL 프로젝트 공식 릴리스가 아닙니다. 

## 심볼 격리

`android/CMakeLists.txt`는 `-Wl,--exclude-libs,ALL`과 `-fvisibility=hidden`으로
링크합니다. 정적 libcrypto가 같은 프로세스의 다른 OpenSSL과 심볼을 가로채지 못하게
하기 위함입니다(react-native-quick-crypto issue #1059). 링크 오류를 고치려고 이
플래그를 빼지 마십시오.

## 대체 계획

공급처가 사라지면 upstream 타르볼을 NDK / Xcode 툴체인으로 직접 빌드해
(`no-shared`, `no-tests`, `no-apps`, `no-ssl` — libcrypto만 필요) 로컬 prefab
AAR·XCFramework로 패키지에 넣고 의존성 두 줄을 그쪽으로 바꿉니다.

사용하는 `libcrypto` 심볼은 이것이 전부라 최소 구성 빌드로도 충분합니다:
`EVP_PKEY_*`, `PEM_read_bio_PUBKEY`, `i2d_PUBKEY`, `BIO_*`, `BN_*`, `SHA256`,
`RAND_bytes`, `EVP_EncodeBlock` / `EVP_DecodeBlock`, `OPENSSL_cleanse`,
`OPENSSL_free`.
