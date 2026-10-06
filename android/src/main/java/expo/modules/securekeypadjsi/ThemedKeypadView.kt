// Copyright (c) 2026 expo-secure-keypad-jsi contributors. MIT License.
package expo.modules.securekeypadjsi

import android.content.Context
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.graphics.RectF
import android.graphics.Typeface
import android.os.SystemClock
import android.view.View
import com.facebook.react.common.assets.ReactFontManager

/**
 * Unshuffled order, phone-keypad style (1..9 then 0): what `shuffle: 'off'`
 * shows, and the fallback whenever the native CSPRNG layout is unavailable.
 */
internal fun identityLayout(): ByteArray = ByteArray(10) { ((it + 1) % 10).toByte() }

/**
 * The theme surface both canvas views expose, so [SecureKeypadJsiView] applies
 * a JS theme map through one code path instead of one per keypad type.
 */
internal interface ThemedKeypadView {
  var keyColor: Int
  var keyTextColor: Int
  var actionTextColor: Int
  var keyCornerRadiusDp: Float
  var digitTextSizeDp: Float

  /** Digit / character glyphs only; action glyphs stay on the system font. */
  var fontFamily: String?

  /** false hides the press feedback entirely, for apps that cannot block capture. */
  var pressedHighlight: Boolean

  /** Fill of a held key; null keeps the default 70% dim of [keyColor]. */
  var pressedKeyColor: Int?

  /** Text drawn instead of the ✕ glyph; null keeps the glyph. */
  var clearKeyLabel: String?

  /** Text drawn instead of the ⏎ glyph (full keyboard only); null keeps the glyph. */
  var submitKeyLabel: String?

  fun invalidate()
}

/**
 * A held key is drawn at 70% opacity. One rule reads correctly on any theme,
 * so no per-theme luminance branch is needed.
 */
internal fun pressedDim(color: Int, held: Boolean): Int =
  if (held) {
    Color.argb((Color.alpha(color) * 0.7f).toInt(), Color.red(color), Color.green(color), Color.blue(color))
  } else {
    color
  }

/**
 * Which key is lit, kept lit for at least [MIN_LIT_MS] after the press.
 *
 * When DOWN and UP land within one frame — a quick tap, or a touch the system
 * held back and then released whole — unlighting on UP clears the key before a
 * frame is drawn. iOS does this at the left and right screen edges (see the
 * PressedKeyState twin in ios/KeypadCoreGraphicsView.swift); this keeps both
 * platforms showing the same feedback. [View] guards its own pressed state the
 * same way with `ViewConfiguration.getPressedStateDuration`.
 */
internal class PressedKeyState(private val view: View) {
  companion object {
    /** About one ordinary tap, so an edge tap reads like one in the middle. */
    const val MIN_LIT_MS = 100L
  }

  var index = -1
    private set
  private var litAt = 0L
  private val unlight = Runnable { clear() }

  fun press(index: Int) {
    view.removeCallbacks(unlight)
    this.index = index
    litAt = SystemClock.uptimeMillis()
    view.invalidate()
  }

  /** Finger lifted: unlit now, or once the key has been lit for [MIN_LIT_MS]. */
  fun release() {
    view.removeCallbacks(unlight)
    val remaining = MIN_LIT_MS - (SystemClock.uptimeMillis() - litAt)
    if (index >= 0 && remaining > 0) view.postDelayed(unlight, remaining) else clear()
  }

  /** Unlit at once: a cancelled touch, or a key set that changed under it. */
  fun clear() {
    view.removeCallbacks(unlight)
    index = -1
    view.invalidate()
  }
}

/** Key fill: [ThemedKeypadView.pressedKeyColor] while held when set, else the 70% dim. */
internal fun ThemedKeypadView.keyFill(held: Boolean): Int =
  if (held) pressedKeyColor ?: pressedDim(keyColor, true) else keyColor

/**
 * Shared by both canvas views: baseline-corrected centred text. [fit] shrinks
 * the text to the key's width — for theme labels, whose length is the app's.
 */
internal fun drawCentered(canvas: Canvas, text: String, r: RectF, paint: Paint, fit: Boolean = false) {
  val size = paint.textSize
  if (fit) {
    val maxWidth = r.width() * 0.85f
    val width = paint.measureText(text)
    if (width > maxWidth && width > 0f) paint.textSize = size * maxWidth / width
  }
  val cx = r.centerX()
  val cy = r.centerY() - (paint.descent() + paint.ascent()) / 2f
  canvas.drawText(text, cx, cy, paint)
  paint.textSize = size
}


/**
 * Resolves a theme `fontFamily` to a [Typeface], memoised because `onDraw`
 * runs on every press and a miss hits the asset manager.
 *
 * [ReactFontManager] is the same registry `expo-font`'s `loadAsync` writes
 * into, so a family loaded from JS resolves here under the name JS used, and
 * `assets/fonts/<name>.ttf` bundled at build time resolves too. An unknown
 * name degrades to [Typeface.create]'s own fallback (the system font) rather
 * than throwing.
 */
internal object EskFont {
  private val cache = HashMap<String, Typeface>()

  fun resolve(context: Context, family: String?): Typeface? {
    if (family.isNullOrEmpty()) return null
    cache[family]?.let { return it }
    val typeface = try {
      ReactFontManager.getInstance().getTypeface(family, Typeface.NORMAL, context.assets)
    } catch (_: Throwable) {
      // No React font registry on the classpath, or a corrupt font file.
      Typeface.create(family, Typeface.NORMAL)
    }
    return typeface?.also { cache[family] = it }
  }
}
