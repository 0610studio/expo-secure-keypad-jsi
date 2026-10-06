// Copyright (c) 2026 expo-secure-keypad-jsi contributors. MIT License.
package expo.modules.securekeypadjsi

import android.annotation.SuppressLint
import android.content.Context
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.graphics.RectF
import android.view.MotionEvent
import android.view.View
import java.security.SecureRandom

/**
 * Full QWERTY secure keyboard drawn with [Canvas] and no child views, so key
 * glyphs never become scrapeable text. Touches resolve to a final code point
 * here (shift and symbol layer included); the on-screen position never leaves
 * this class.
 *
 * Following commercial secure-keyboard practice, the QWERTY arrangement is
 * kept readable: the digit row is fully shuffled (order comes from the native
 * core's CSPRNG), and each character row gets one blank dummy key at a random
 * slot so the same character is not at the same coordinate across sessions.
 */
@SuppressLint("ViewConstructor")
class KeyboardCanvasView(context: Context) : View(context), ThemedKeypadView {

  interface Listener {
    /** Final Unicode code point — shift/layer already applied. */
    fun onKeyPressed(codePoint: Int)
    fun onBackspace()
    fun onClear()
    fun onDone()
    fun onObscuredTouch()
  }

  var listener: Listener? = null

  var shuffleMode: KeypadCanvasView.ShuffleMode = KeypadCanvasView.ShuffleMode.MOUNT

  /** Shuffled 0..9 from the native core; 1..9,0 when shuffle is off. */
  var digitsProvider: (() -> ByteArray)? = null

  private val density = context.resources.displayMetrics.density

  override var keyColor: Int = Color.parseColor("#1C1C1E")
  override var keyTextColor: Int = Color.WHITE
  override var actionTextColor: Int = Color.parseColor("#8E8E93")
  override var keyCornerRadiusDp: Float = 8f
  override var digitTextSizeDp: Float = 32f
  override var fontFamily: String? = null

  override var pressedHighlight: Boolean = true
  override var pressedKeyColor: Int? = null
  override var clearKeyLabel: String? = null
  override var submitKeyLabel: String? = null

  private companion object {
    const val ACTION_SHIFT = -1
    const val ACTION_BACKSPACE = -2
    const val ACTION_CLEAR = -3
    const val ACTION_DONE = -4
    const val ACTION_TOGGLE = -5
    const val DUMMY = -6
    const val ACTION_SYMBOL_PAGE = -7

    const val ROW_QWERTY_TOP = "qwertyuiop"
    const val ROW_QWERTY_MID = "asdfghjkl"
    const val ROW_QWERTY_BOT = "zxcvbnm"
    // Symbol page 1/2: all 32 printable ASCII specials, three rows + a short fourth row.
    const val ROW_SYM_0 = "!@#$%^&*()"
    const val ROW_SYM_1 = "-_=+[]{}\\|"
    const val ROW_SYM_2 = ";:'\",.<>?/"
    const val ROW_SYM_3 = "`~"
    // Symbol page 2/2: the non-ASCII symbols the stock iOS / Gboard / Samsung
    // keyboards show. Must match kExtraSymbols in common/include/esk/SecureBuffer.h,
    // or the core silently drops the key.
    const val ROW_EXT_0 = "₩€£¥¢¤§¶©®"
    const val ROW_EXT_1 = "™✓°•×÷√π∆"
    const val ROW_EXT_2 = "¡¿《》○●□■▪"
    const val ROW_EXT_3 = "◇☆♤♡♧"
  }

  private enum class Layer { LETTERS, SYMBOLS, SYMBOLS_EXTRA }
  private enum class ShiftState { OFF, ONE_SHOT, LOCK }

  private data class Key(val code: Int, val weight: Float = 1f)

  private var layer = Layer.LETTERS
  private var shift = ShiftState.OFF
  private var lastShiftTapMs = 0L

  private val random = SecureRandom()
  private var digitsOrder = identityLayout()
  // One dummy slot per character row, re-drawn on reshuffle(); -1 = none.
  private var dummySlots = IntArray(5) { -1 }

  private var keyRects = ArrayList<Pair<RectF, Key>>(48)

  private val keyPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply { style = Paint.Style.FILL }
  // Every character key, symbols included, uses the theme font. A glyph that
  // font lacks (Space Mono has no ♡) is drawn by the typeface's built-in
  // system fallback, per character, instead of tofu.
  private val textPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply { textAlign = Paint.Align.CENTER }
  private val actionPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply { textAlign = Paint.Align.CENTER }
  private val gap = 6f * density

  private val pressed = PressedKeyState(this)

  init {
    isClickable = true
    filterTouchesWhenObscured = true  // tapjacking
    reshuffle()
  }

  fun reshuffle() {
    val shuffled = shuffleMode != KeypadCanvasView.ShuffleMode.OFF
    digitsOrder = if (shuffled) {
      digitsProvider?.invoke() ?: identityLayout()
    } else {
      identityLayout()
    }
    for (i in dummySlots.indices) {
      // Slot count per row is resolved in buildRows(); draw an index generously
      // and clamp there so one random pull per row suffices.
      dummySlots[i] = if (shuffled) random.nextInt(Int.MAX_VALUE) else -1
    }
    layoutKeys()
    invalidate()
  }

  private fun withDummy(keys: MutableList<Key>, rowIndex: Int) {
    val slotSeed = dummySlots[rowIndex]
    if (slotSeed < 0) return
    keys.add(slotSeed % (keys.size + 1), Key(DUMMY))
  }

  private fun buildRows(): List<List<Key>> {
    val rows = ArrayList<List<Key>>(5)
    if (layer == Layer.LETTERS) {
      rows.add(digitsOrder.map { Key(('0' + it.toInt()).code) })
      rows.add(ArrayList<Key>(ROW_QWERTY_TOP.map { Key(it.code) }).also { withDummy(it, 0) })
      rows.add(ArrayList<Key>(ROW_QWERTY_MID.map { Key(it.code) }).also { withDummy(it, 1) })
      rows.add(ArrayList<Key>().also {
        it.add(Key(ACTION_SHIFT, 1.5f))
        val letters = ArrayList<Key>(ROW_QWERTY_BOT.map { c -> Key(c.code) })
        withDummy(letters, 2)
        it.addAll(letters)
        it.add(Key(ACTION_BACKSPACE, 1.5f))
      })
    } else if (layer == Layer.SYMBOLS) {
      rows.add(ArrayList<Key>(ROW_SYM_0.map { Key(it.code) }).also { withDummy(it, 4) })
      rows.add(ArrayList<Key>(ROW_SYM_1.map { Key(it.code) }).also { withDummy(it, 0) })
      rows.add(ArrayList<Key>(ROW_SYM_2.map { Key(it.code) }).also { withDummy(it, 1) })
      rows.add(ArrayList<Key>().also {
        it.add(Key(ACTION_SYMBOL_PAGE, 1.5f))
        val syms = ArrayList<Key>(ROW_SYM_3.map { c -> Key(c.code, 2f) })
        withDummy(syms, 3)
        it.addAll(syms)
        it.add(Key(ACTION_BACKSPACE, 1.5f))
      })
    } else {
      rows.add(ArrayList<Key>(ROW_EXT_0.map { Key(it.code) }).also { withDummy(it, 4) })
      rows.add(ArrayList<Key>(ROW_EXT_1.map { Key(it.code) }).also { withDummy(it, 0) })
      rows.add(ArrayList<Key>(ROW_EXT_2.map { Key(it.code) }).also { withDummy(it, 1) })
      rows.add(ArrayList<Key>().also {
        it.add(Key(ACTION_SYMBOL_PAGE, 1.5f))
        val syms = ArrayList<Key>(ROW_EXT_3.map { c -> Key(c.code) })
        withDummy(syms, 3)
        it.addAll(syms)
        it.add(Key(ACTION_BACKSPACE, 1.5f))
      })
    }
    rows.add(listOf(Key(ACTION_TOGGLE, 1f), Key(ACTION_CLEAR, 1f), Key(ACTION_DONE, 1f)))
    return rows
  }

  private fun layoutKeys() {
    keyRects = ArrayList(48)
    val w = width.toFloat()
    val h = height.toFloat()
    if (w <= 0f || h <= 0f) return

    val rows = buildRows()
    val rowH = (h - gap * (rows.size - 1)) / rows.size
    var top = 0f
    for (row in rows) {
      val totalWeight = row.sumOf { it.weight.toDouble() }.toFloat()
      val unit = (w - gap * (row.size - 1)) / totalWeight
      var left = 0f
      for (key in row) {
        val kw = unit * key.weight
        keyRects.add(Pair(RectF(left, top, left + kw, top + rowH), key))
        left += kw + gap
      }
      top += rowH + gap
    }
  }

  override fun onSizeChanged(w: Int, h: Int, oldw: Int, oldh: Int) {
    super.onSizeChanged(w, h, oldw, oldh)
    layoutKeys()
  }

  private fun labelFor(key: Key): String? = when (key.code) {
    DUMMY -> null
    ACTION_SHIFT -> "⇧"
    ACTION_BACKSPACE -> "⌫"
    ACTION_CLEAR -> clearKeyLabel ?: "✕"
    ACTION_DONE -> submitKeyLabel ?: "⏎"
    ACTION_TOGGLE -> if (layer == Layer.LETTERS) "!#1" else "abc"
    ACTION_SYMBOL_PAGE -> if (layer == Layer.SYMBOLS) "1/2" else "2/2"
    else -> {
      val c = key.code.toChar()
      if (shift != ShiftState.OFF && c in 'a'..'z') c.uppercaseChar().toString() else c.toString()
    }
  }

  // No background fill: see KeypadCanvasView.onDraw.
  override fun onDraw(canvas: Canvas) {
    textPaint.color = keyTextColor
    textPaint.textSize = digitTextSizeDp * density * 0.6f
    textPaint.typeface = EskFont.resolve(context, fontFamily)
    actionPaint.textSize = digitTextSizeDp * density * 0.5f
    val r = keyCornerRadiusDp * density

    for ((index, entry) in keyRects.withIndex()) {
      val (rect, key) = entry
      val held = pressedHighlight && index == pressed.index
      keyPaint.color = keyFill(held)
      canvas.drawRoundRect(rect, r, r, keyPaint)
      val label = labelFor(key)
      if (label != null) {
        val paint = if (key.code >= 0) textPaint else actionPaint
        // Shift shows as "armed" while one-shot or locked.
        actionPaint.color = if (key.code == ACTION_SHIFT && shift != ShiftState.OFF) {
          keyTextColor
        } else {
          actionTextColor
        }
        val custom = (key.code == ACTION_CLEAR && clearKeyLabel != null) ||
          (key.code == ACTION_DONE && submitKeyLabel != null)
        drawCentered(canvas, label, rect, paint, fit = custom)
      }
    }
  }

  /** Same tapjacking observation point as [KeypadCanvasView]. */
  override fun onFilterTouchEventForSecurity(event: MotionEvent): Boolean {
    val allowed = super.onFilterTouchEventForSecurity(event)
    if (!allowed && event.actionMasked == MotionEvent.ACTION_DOWN) {
      listener?.onObscuredTouch()
    }
    return allowed
  }

  override fun onTouchEvent(event: MotionEvent): Boolean {
    when (event.actionMasked) {
      MotionEvent.ACTION_DOWN -> {
        pressed.press(keyRects.indexOfFirst { it.first.contains(event.x, event.y) })
        return true
      }
      MotionEvent.ACTION_CANCEL -> {
        pressed.clear()
        return true
      }
      MotionEvent.ACTION_UP -> {
        pressed.release()
        val key = keyRects.firstOrNull { it.first.contains(event.x, event.y) }?.second
        if (key != null) handleKey(key)
        performClick()
        return true
      }
      else -> return super.onTouchEvent(event)
    }
  }

  private fun handleKey(key: Key) {
    when (key.code) {
      DUMMY -> {}
      ACTION_SHIFT -> {
        val now = android.os.SystemClock.uptimeMillis()
        shift = when {
          shift == ShiftState.OFF -> ShiftState.ONE_SHOT
          shift == ShiftState.ONE_SHOT && now - lastShiftTapMs < 350L -> ShiftState.LOCK
          else -> ShiftState.OFF
        }
        lastShiftTapMs = now
        invalidate()
      }
      ACTION_BACKSPACE -> listener?.onBackspace()
      ACTION_CLEAR -> listener?.onClear()
      ACTION_DONE -> listener?.onDone()
      ACTION_TOGGLE -> {
        layer = if (layer == Layer.LETTERS) Layer.SYMBOLS else Layer.LETTERS
        shift = ShiftState.OFF
        // A new key set: the lit index would now point at another key.
        pressed.clear()
        layoutKeys()
        invalidate()
      }
      ACTION_SYMBOL_PAGE -> {
        layer = if (layer == Layer.SYMBOLS) Layer.SYMBOLS_EXTRA else Layer.SYMBOLS
        pressed.clear()
        layoutKeys()
        invalidate()
      }
      else -> {
        var c = key.code.toChar()
        if (shift != ShiftState.OFF && c in 'a'..'z') c = c.uppercaseChar()
        if (shift == ShiftState.ONE_SHOT) {
          shift = ShiftState.OFF
          invalidate()
        }
        listener?.onKeyPressed(c.code)
        if (shuffleMode == KeypadCanvasView.ShuffleMode.PER_KEY) reshuffle()
      }
    }
  }

  override fun performClick(): Boolean {
    super.performClick()
    return true
  }
}
