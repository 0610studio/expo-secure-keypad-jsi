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

/**
 * Draws with [Canvas] and no child views, so digit glyphs never become
 * scrapeable text. Touches resolve to a digit *value* here; the on-screen
 * position never leaves this class.
 */
@SuppressLint("ViewConstructor")
class KeypadCanvasView(context: Context) : View(context), ThemedKeypadView {

  interface Listener {
    fun onDigitPressed(digit: Int)
    fun onBackspace()
    fun onClear()
    fun onObscuredTouch()
  }

  var listener: Listener? = null

  enum class ShuffleMode { MOUNT, PER_KEY, OFF }

  var shuffleMode: ShuffleMode = ShuffleMode.MOUNT

  // Canvas draws in pixels, so every dimension below is stored in dp and scaled
  // at draw time. Without that, a JS theme value would mean a different physical
  // size than the identical value on iOS, which works in points.
  private val density = context.resources.displayMetrics.density

  override var keyColor: Int = Color.parseColor("#1C1C1E")
  override var keyTextColor: Int = Color.WHITE
  override var actionTextColor: Int = Color.parseColor("#8E8E93")
  override var keyCornerRadiusDp: Float = 12f
  override var digitTextSizeDp: Float = 32f
  override var fontFamily: String? = null

  override var pressedHighlight: Boolean = true

  var layoutProvider: (() -> ByteArray)? = null

  // 3 cols x 4 rows. Digits occupy 0..8 and 10; 9 = clear, 11 = backspace.
  private val digitCells = intArrayOf(0, 1, 2, 3, 4, 5, 6, 7, 8, 10)
  private val clearCell = 9
  private val backspaceCell = 11

  // Indexed by cell; -1 marks an action cell.
  private val digitAtCell = IntArray(12) { -1 }

  private val keyPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply { style = Paint.Style.FILL }
  private val textPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply { textAlign = Paint.Align.CENTER }
  private val actionPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply { textAlign = Paint.Align.CENTER }
  private val rect = RectF()

  private var pressedCell = -1

  private var cellW = 0f
  private var cellH = 0f
  private val gap = 12f * density

  init {
    isClickable = true
    filterTouchesWhenObscured = true  // tapjacking
    reshuffle()
  }

  fun reshuffle() {
    val layout = if (shuffleMode == ShuffleMode.OFF) {
      identityLayout()
    } else {
      layoutProvider?.invoke() ?: identityLayout()
    }
    for (i in digitAtCell.indices) digitAtCell[i] = -1
    for (slot in digitCells.indices) {
      digitAtCell[digitCells[slot]] = layout[slot].toInt()
    }
    invalidate()
  }

  override fun onSizeChanged(w: Int, h: Int, oldw: Int, oldh: Int) {
    super.onSizeChanged(w, h, oldw, oldh)
    // Same guard as KeyboardCanvasView.layoutKeys: a 0-size pass (hidden tab,
    // first layout) must not leave negative cell sizes behind.
    if (w <= 0 || h <= 0) { cellW = 0f; cellH = 0f; return }
    cellW = (w - gap * 2) / 3f
    cellH = (h - gap * 3) / 4f
  }

  // No background fill: the host view's `style` carries it, so the gaps
  // between keys show whatever is behind the keypad.
  override fun onDraw(canvas: Canvas) {
    textPaint.color = keyTextColor
    textPaint.textSize = digitTextSizeDp * density
    textPaint.typeface = EskFont.resolve(context, fontFamily)
    actionPaint.textSize = digitTextSizeDp * density * 0.7f

    for (cell in 0 until 12) {
      val col = cell % 3
      val row = cell / 3
      val left = col * (cellW + gap)
      val top = row * (cellH + gap)
      rect.set(left, top, left + cellW, top + cellH)

      val digit = digitAtCell[cell]
      val held = pressedHighlight && cell == pressedCell
      keyPaint.color = pressedDim(keyColor, held)
      // Action cells have no key fill, so the glyph itself carries the dim.
      actionPaint.color = pressedDim(actionTextColor, held)
      when {
        digit >= 0 -> {
          val r = keyCornerRadiusDp * density
          canvas.drawRoundRect(rect, r, r, keyPaint)
          drawCentered(canvas, digit.toString(), rect, textPaint)
        }
        cell == backspaceCell -> drawCentered(canvas, "⌫", rect, actionPaint)
        cell == clearCell -> drawCentered(canvas, "✕", rect, actionPaint)
      }
    }
  }

  /**
   * The only place a tapjacking attempt is observable: filterTouchesWhenObscured
   * drops the event before [onTouchEvent] runs. ACTION_DOWN only, or one gesture
   * would emit a finding per event.
   */
  override fun onFilterTouchEventForSecurity(event: MotionEvent): Boolean {
    val allowed = super.onFilterTouchEventForSecurity(event)
    if (!allowed && event.actionMasked == MotionEvent.ACTION_DOWN) {
      listener?.onObscuredTouch()
    }
    return allowed
  }

  private fun cellAt(x: Float, y: Float): Int {
    val col = (x / (cellW + gap)).toInt().coerceIn(0, 2)
    val row = (y / (cellH + gap)).toInt().coerceIn(0, 3)
    return row * 3 + col
  }

  override fun onTouchEvent(event: MotionEvent): Boolean {
    when (event.actionMasked) {
      MotionEvent.ACTION_DOWN -> {
        pressedCell = cellAt(event.x, event.y)
        invalidate()
        return true
      }
      MotionEvent.ACTION_CANCEL -> {
        pressedCell = -1
        invalidate()
        return true
      }
      MotionEvent.ACTION_UP -> {
        pressedCell = -1
        invalidate()
        val cell = cellAt(event.x, event.y)
        when {
          cell in 0..11 && digitAtCell[cell] >= 0 -> {
            listener?.onDigitPressed(digitAtCell[cell])
            if (shuffleMode == ShuffleMode.PER_KEY) reshuffle()
          }
          cell == backspaceCell -> listener?.onBackspace()
          cell == clearCell -> listener?.onClear()
        }
        performClick()
        return true
      }
      else -> return super.onTouchEvent(event)
    }
  }

  override fun performClick(): Boolean {
    super.performClick()
    return true
  }
}
