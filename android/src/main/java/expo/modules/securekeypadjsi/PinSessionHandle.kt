// Copyright (c) 2026 expo-secure-keypad-jsi contributors. MIT License.
package expo.modules.securekeypadjsi

/**
 * Owns the native pointer for its lifetime — call [destroy] exactly once, on
 * the UI thread. `external` names must match the symbols in JniBridge.cpp.
 */
class PinSessionHandle(minLength: Int, maxLength: Int, keypadType: Int = TYPE_DIGIT) {
  companion object {
    const val TYPE_DIGIT = 0
    const val TYPE_FULL = 1

    init {
      System.loadLibrary("expo-secure-keypad-jsi")
    }
  }

  private var ptr: Long = nativeCreate(minLength, maxLength, keypadType)

  /** null on success, else an error-code string (e.g. "ERR_WEAK_KEY"). */
  fun arm(pem: String): String? = if (ptr != 0L) nativeArm(ptr, pem) else "ERR_INTERNAL"

  fun press(digit: Int): Int = if (ptr != 0L) nativePress(ptr, digit) else 0

  /** Final ASCII char (view resolves shift/layer first). Out-of-charset is ignored. */
  fun pressKey(asciiChar: Int): Int = if (ptr != 0L) nativePressKey(ptr, asciiChar) else 0
  fun backspace(): Int = if (ptr != 0L) nativeBackspace(ptr) else 0
  fun clear() { if (ptr != 0L) nativeClear(ptr) }

  /** Envelope JSON starting with '{', or an "ERR_..." string. */
  fun submit(): String = if (ptr != 0L) nativeSubmit(ptr) else "ERR_INTERNAL"

  fun shuffledLayout(): ByteArray =
    if (ptr != 0L) nativeShuffledLayout(ptr) else identityLayout()

  fun onBackground() { if (ptr != 0L) nativeOnBackground(ptr) }

  fun destroy() {
    val p = ptr
    ptr = 0L
    if (p != 0L) nativeDestroy(p)
  }

  private external fun nativeCreate(minLength: Int, maxLength: Int, keypadType: Int): Long
  private external fun nativeDestroy(ptr: Long)
  private external fun nativeArm(ptr: Long, pem: String): String?
  private external fun nativePress(ptr: Long, digit: Int): Int
  private external fun nativePressKey(ptr: Long, asciiChar: Int): Int
  private external fun nativeBackspace(ptr: Long): Int
  private external fun nativeClear(ptr: Long)
  private external fun nativeSubmit(ptr: Long): String
  private external fun nativeShuffledLayout(ptr: Long): ByteArray
  private external fun nativeOnBackground(ptr: Long)
}
