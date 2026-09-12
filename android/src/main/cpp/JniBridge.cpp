// Copyright (c) 2026 expo-secure-keypad-jsi contributors. MIT License.
//
// JNI glue for PinSessionHandle.kt. The esk_keypad* lives in a Kotlin `long`.
#include <jni.h>

#include <cstdint>
#include <ctime>
#include <string>

#include "esk/esk_c_api.h"

namespace {

int64_t nowCb(void* /*user*/) {
  return static_cast<int64_t>(::time(nullptr));
}

std::string jstr(JNIEnv* env, jstring s) {
  if (s == nullptr) return {};
  const char* c = env->GetStringUTFChars(s, nullptr);
  std::string out(c ? c : "");
  if (c) env->ReleaseStringUTFChars(s, c);
  return out;
}

esk_keypad* asKeypad(jlong ptr) {
  return reinterpret_cast<esk_keypad*>(static_cast<uintptr_t>(ptr));
}

}  // namespace

extern "C" {

JNIEXPORT jlong JNICALL
Java_expo_modules_securekeypadjsi_PinSessionHandle_nativeCreate(
    JNIEnv*, jobject, jint minLength, jint maxLength, jint keypadType) {
  esk_config cfg{};
  cfg.min_length = minLength;
  cfg.max_length = maxLength;
  cfg.keypad_type = keypadType;

  esk_keypad* kp = esk_keypad_create(&cfg, &nowCb, nullptr);
  return static_cast<jlong>(reinterpret_cast<uintptr_t>(kp));
}

JNIEXPORT void JNICALL
Java_expo_modules_securekeypadjsi_PinSessionHandle_nativeDestroy(JNIEnv*,
                                                                 jobject,
                                                                 jlong ptr) {
  esk_keypad_destroy(asKeypad(ptr));
}

JNIEXPORT jstring JNICALL
Java_expo_modules_securekeypadjsi_PinSessionHandle_nativeArm(JNIEnv* env,
                                                             jobject, jlong ptr,
                                                             jstring pem) {
  const std::string pemStr = jstr(env, pem);
  const char* err = esk_keypad_arm(asKeypad(ptr), pemStr.c_str());
  return err == nullptr ? nullptr : env->NewStringUTF(err);
}

JNIEXPORT jint JNICALL
Java_expo_modules_securekeypadjsi_PinSessionHandle_nativePress(JNIEnv*, jobject,
                                                               jlong ptr,
                                                               jint digit) {
  return esk_keypad_press(asKeypad(ptr), static_cast<uint8_t>(digit));
}

JNIEXPORT jint JNICALL
Java_expo_modules_securekeypadjsi_PinSessionHandle_nativePressKey(
    JNIEnv*, jobject, jlong ptr, jint asciiChar) {
  return esk_keypad_press_key(asKeypad(ptr), static_cast<uint8_t>(asciiChar));
}

JNIEXPORT jint JNICALL
Java_expo_modules_securekeypadjsi_PinSessionHandle_nativeBackspace(JNIEnv*,
                                                                   jobject,
                                                                   jlong ptr) {
  return esk_keypad_backspace(asKeypad(ptr));
}

JNIEXPORT void JNICALL
Java_expo_modules_securekeypadjsi_PinSessionHandle_nativeClear(JNIEnv*, jobject,
                                                               jlong ptr) {
  esk_keypad_clear(asKeypad(ptr));
}

JNIEXPORT jstring JNICALL
Java_expo_modules_securekeypadjsi_PinSessionHandle_nativeSubmit(JNIEnv* env,
                                                                jobject,
                                                                jlong ptr) {
  const char* err = nullptr;
  char* envelope = esk_keypad_submit(asKeypad(ptr), &err);
  if (envelope != nullptr) {
    jstring out = env->NewStringUTF(envelope);
    esk_free(envelope);
    return out;  // starts with '{'
  }
  return env->NewStringUTF(err ? err : "ERR_INTERNAL");  // starts with "ERR_"
}

JNIEXPORT jbyteArray JNICALL
Java_expo_modules_securekeypadjsi_PinSessionHandle_nativeShuffledLayout(
    JNIEnv* env, jobject, jlong ptr) {
  uint8_t layout[10] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
  esk_keypad_shuffled_layout(asKeypad(ptr), layout);
  jbyteArray arr = env->NewByteArray(10);
  env->SetByteArrayRegion(arr, 0, 10, reinterpret_cast<jbyte*>(layout));
  return arr;
}

JNIEXPORT void JNICALL
Java_expo_modules_securekeypadjsi_PinSessionHandle_nativeOnBackground(
    JNIEnv*, jobject, jlong ptr) {
  esk_keypad_on_background(asKeypad(ptr));
}

}  // extern "C"
