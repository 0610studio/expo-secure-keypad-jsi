// Copyright (c) 2026 expo-secure-keypad-jsi contributors. MIT License.
package expo.modules.securekeypadjsi

/**
 * Parses the theme colour format the JS API documents: `#RRGGBB` or
 * `#RRGGBBAA`, alpha last. `android.graphics.Color.parseColor` is deliberately
 * not used — it reads 8-digit values as `#AARRGGBB` and accepts colour names,
 * so the same string would render differently from iOS. Pure Kotlin so the
 * JVM unit test needs no Android runtime.
 */
object ThemeColor {
  /** Packed ARGB int (what Paint.color takes), or null when not `#RRGGBB` / `#RRGGBBAA`. */
  fun parse(s: String): Int? {
    var hex = s.trim()
    if (hex.startsWith("#")) hex = hex.substring(1)
    if (hex.length != 6 && hex.length != 8) return null
    if (!hex.all { it in '0'..'9' || it in 'a'..'f' || it in 'A'..'F' }) return null
    val value = hex.toLong(16)
    val r: Long; val g: Long; val b: Long; val a: Long
    if (hex.length == 6) {
      r = (value shr 16) and 0xff; g = (value shr 8) and 0xff; b = value and 0xff; a = 0xff
    } else {
      r = (value shr 24) and 0xff; g = (value shr 16) and 0xff; b = (value shr 8) and 0xff; a = value and 0xff
    }
    return ((a shl 24) or (r shl 16) or (g shl 8) or b).toInt()
  }
}
