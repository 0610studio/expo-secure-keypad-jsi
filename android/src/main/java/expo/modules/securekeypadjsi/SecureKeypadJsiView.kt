// Copyright (c) 2026 expo-secure-keypad-jsi contributors. MIT License.
package expo.modules.securekeypadjsi

import android.content.Context
import android.graphics.Color
import android.os.Handler
import android.os.Looper
import android.view.View
import android.widget.FrameLayout
import expo.modules.kotlin.AppContext
import expo.modules.kotlin.viewevent.EventDispatcher
import expo.modules.kotlin.views.ExpoView
import java.util.Collections
import java.util.WeakHashMap

/**
 * Hosts the Canvas keypad (digit) or QWERTY keyboard (full) and owns the
 * [PinSessionHandle]. JS receives only the entered-key count and the
 * ciphertext envelope. Screen capture and device integrity are deliberately
 * out of scope — see README.
 */
class SecureKeypadJsiView(context: Context, appContext: AppContext) :
  ExpoView(context, appContext), KeypadCanvasView.Listener, KeyboardCanvasView.Listener {

  companion object {
    // Digit pad policy cap; the full keyboard uses the 64-byte payload cap.
    // Mirrors kMaxPinLength / kMaxSecretLength in the C++ core so the
    // autoSubmit comparison below agrees with what the core actually stores.
    private const val MAX_DIGIT_CAP = 12
    private const val MAX_FULL_CAP = 64

    // Live views, so the module's activity-background hook can reach them.
    // Weak keys: a view that Fabric dropped without OnViewDestroys must not
    // be pinned here. Main thread only.
    private val liveViews: MutableSet<SecureKeypadJsiView> =
      Collections.newSetFromMap(WeakHashMap())

    /** Called by [SecureKeypadJsiModule] when the host activity pauses. */
    fun onActivityEntersBackground() {
      for (v in liveViews.toList()) v.onAppBackground()
    }
  }

  private val onDigitCountChanged by EventDispatcher<Map<String, Any>>()
  private val onComplete by EventDispatcher<Map<String, Any>>()
  private val onError by EventDispatcher<Map<String, Any>>()

  private var digitKeypad: KeypadCanvasView? = null
  private var fullKeyboard: KeyboardCanvasView? = null
  private var handle: PinSessionHandle? = null
  private val mainHandler = Handler(Looper.getMainLooper())

  // Rebuilt on the next frame so a batch of prop updates collapses into one
  // native session.
  private var publicKeyPem: String? = null
  private var keypadType = "digit" // digit | full
  private var minLength = 4
  private var maxLength = 6
  private var autoSubmit = true
  private var shuffleMode = KeypadCanvasView.ShuffleMode.MOUNT
  private var theme: Map<String, Any?>? = null

  private var rebuildScheduled = false
  // Set by dispose(). A rebuild posted before OnViewDestroys must not create a
  // session nobody will ever destroy.
  private var disposed = false

  // Last count handed to JS. A rebuild reports 0 for the fresh session even
  // when JS was already at 0, and that redundant event can land before the JS
  // component has mounted (a React "state update on a component that hasn't
  // mounted yet" warning). Nothing changed, so do not send it.
  private var lastReportedCount = 0

  init {
    setBackgroundColor(Color.TRANSPARENT)
    rebuildInputView()
    applyAccessibilityHardening()
    scheduleRebuild()
    liveViews.add(this)
  }

  /** maxLength after the per-type clamp the C++ core applies. */
  private fun effectiveMaxLength(): Int =
    maxLength.coerceIn(4, if (keypadType == "full") MAX_FULL_CAP else MAX_DIGIT_CAP)

  fun setPublicKeyPem(pem: String) { publicKeyPem = pem; scheduleRebuild() }
  fun setMinLength(v: Int) { minLength = v.coerceIn(4, 64); scheduleRebuild() }
  fun setMaxLength(v: Int) { maxLength = v.coerceIn(4, 64); scheduleRebuild() }
  fun setAutoSubmit(v: Boolean) { autoSubmit = v }

  fun setKeypadType(v: String) {
    val next = if (v == "full") "full" else "digit"
    if (next == keypadType) return
    keypadType = next
    rebuildInputView()
    scheduleRebuild()
  }

  fun setShuffle(mode: String) {
    shuffleMode = when (mode) {
      "perKey" -> KeypadCanvasView.ShuffleMode.PER_KEY
      "off" -> KeypadCanvasView.ShuffleMode.OFF
      else -> KeypadCanvasView.ShuffleMode.MOUNT
    }
    digitKeypad?.let { it.shuffleMode = shuffleMode; it.reshuffle() }
    fullKeyboard?.let { it.shuffleMode = shuffleMode; it.reshuffle() }
  }

  fun setTheme(t: Map<String, Any?>) {
    theme = t
    digitKeypad?.let { applyTheme(it) }
    fullKeyboard?.let { applyTheme(it) }
  }

  // #RRGGBB / #RRGGBBAA, same reading as the iOS side.
  private fun safeColor(s: String, fallback: Int): Int = ThemeColor.parse(s) ?: fallback

  private fun applyTheme(v: ThemedKeypadView) {
    val t = theme ?: return
    (t["keyColor"] as? String)?.let { v.keyColor = safeColor(it, v.keyColor) }
    (t["keyTextColor"] as? String)?.let { v.keyTextColor = safeColor(it, v.keyTextColor) }
    (t["actionTextColor"] as? String)?.let { v.actionTextColor = safeColor(it, v.actionTextColor) }
    (t["cornerRadius"] as? Number)?.let { v.keyCornerRadiusDp = it.toFloat() }
    (t["digitTextSize"] as? Number)?.let { v.digitTextSizeDp = it.toFloat() }
    (t["pressedHighlight"] as? Boolean)?.let { v.pressedHighlight = it }
    // Absent key vs. explicit null: both mean "system font", so this one is
    // assigned unconditionally instead of only on a hit.
    v.fontFamily = (t["fontFamily"] as? String)?.takeIf { it.isNotEmpty() }
    v.invalidate()
  }

  private fun rebuildInputView() {
    removeAllViews()
    digitKeypad = null
    fullKeyboard = null
    val lp = FrameLayout.LayoutParams(
      FrameLayout.LayoutParams.MATCH_PARENT, FrameLayout.LayoutParams.MATCH_PARENT)
    if (keypadType == "full") {
      val v = KeyboardCanvasView(context)
      v.listener = this
      v.shuffleMode = shuffleMode
      v.digitsProvider = { handle?.shuffledLayout() ?: identityLayout() }
      applyTheme(v)
      addView(v, lp)
      layoutChildNow(v)
      fullKeyboard = v
    } else {
      val v = KeypadCanvasView(context)
      v.listener = this
      v.shuffleMode = shuffleMode
      v.layoutProvider = { handle?.shuffledLayout() ?: identityLayout() }
      applyTheme(v)
      addView(v, lp)
      layoutChildNow(v)
      digitKeypad = v
    }
  }

  /**
   * Sizes a child that was added *after* the first layout pass — what a
   * `keypadType` switch does. Fabric drives layout from the shadow tree and
   * ignores the [requestLayout] that `addView` schedules (React Native
   * #17968), so such a child would stay 0x0 and draw nothing. At init the view
   * has no size yet and the normal mount cascade handles it, hence the guard.
   */
  private fun layoutChildNow(child: View) {
    if (width <= 0 || height <= 0) return
    child.measure(
      MeasureSpec.makeMeasureSpec(width, MeasureSpec.EXACTLY),
      MeasureSpec.makeMeasureSpec(height, MeasureSpec.EXACTLY))
    child.layout(0, 0, width, height)
  }

  // A parent resize (rotation, a container animating) arrives as a real layout
  // pass, but the children are laid out from their *measured* size, which is
  // stale after a mid-life swap. Re-measure them against the new bounds.
  override fun onSizeChanged(w: Int, h: Int, oldw: Int, oldh: Int) {
    super.onSizeChanged(w, h, oldw, oldh)
    digitKeypad?.let { layoutChildNow(it) }
    fullKeyboard?.let { layoutChildNow(it) }
  }

  private fun scheduleRebuild() {
    if (rebuildScheduled) return
    rebuildScheduled = true
    mainHandler.post {
      rebuildScheduled = false
      rebuildHandle()
    }
  }

  private fun rebuildHandle() {
    if (disposed) return
    // A new session starts empty; tell JS so a masked-dot UI never shows
    // digits that no longer exist in the buffer.
    val hadSession = handle != null
    handle?.destroy()

    val type = if (keypadType == "full") PinSessionHandle.TYPE_FULL else PinSessionHandle.TYPE_DIGIT
    val h = PinSessionHandle(minLength = minLength, maxLength = maxLength, keypadType = type)
    handle = h
    digitKeypad?.reshuffle()
    fullKeyboard?.reshuffle()
    if (hadSession) reportCount(0)

    publicKeyPem?.let { pem ->
      val err = h.arm(pem)
      if (err != null) {
        onError(mapOf("code" to err, "phase" to "arm"))
      }
    }
  }

  // KeypadCanvasView.Listener (digit pad)

  override fun onDigitPressed(digit: Int) {
    val h = handle ?: return
    val count = h.press(digit)
    // Negative = the core refused the call (ESK_COUNT_ERROR); the buffer is
    // unchanged, so do not tell JS "0 entered".
    if (count < 0) return
    reportCount(count)
    if (autoSubmit && count >= effectiveMaxLength()) submit()
  }

  // KeyboardCanvasView.Listener (full keyboard)

  override fun onKeyPressed(asciiChar: Int) {
    val h = handle ?: return
    val count = h.pressKey(asciiChar)
    if (count < 0) return
    reportCount(count)
    if (autoSubmit && count >= effectiveMaxLength()) submit()
  }

  override fun onDone() = submit()

  // Shared listener members

  override fun onBackspace() {
    val h = handle ?: return
    val count = h.backspace()
    if (count < 0) return
    reportCount(count)
  }

  override fun onClear() {
    val h = handle ?: return
    h.clear()
    reportCount(0)
  }

  // filterTouchesWhenObscured already dropped the touch; this only lets the
  // host explain why the keypad did not respond (overlay app, screen dimmer).
  override fun onObscuredTouch() {
    onError(mapOf("code" to "ERR_OBSCURED_TOUCH", "phase" to "input"))
  }

  private fun reportCount(count: Int) {
    if (count == lastReportedCount) return
    lastReportedCount = count
    onDigitCountChanged(mapOf("count" to count))
  }

  fun submit() {
    val h = handle ?: return
    val result = h.submit()
    if (result.startsWith("{")) {
      onComplete(mapOf("envelope" to result))
      reportCount(0)
    } else {
      onError(mapOf("code" to result, "phase" to "submit"))
    }
  }

  fun clearPin() {
    handle?.clear()
    reportCount(0)
  }

  // Backgrounding = the host activity pausing (home button, app switcher) or
  // this view leaving the window (react-native-screens detaches inactive
  // screens). Either way the buffer is wiped and JS is told the count is 0,
  // so the masked-dot UI cannot drift from the real buffer.
  private fun onAppBackground() {
    val h = handle ?: return
    h.onBackground()
    reportCount(0)
  }

  override fun onDetachedFromWindow() {
    onAppBackground()
    super.onDetachedFromWindow()
  }

  private fun applyAccessibilityHardening() {
    // Unconditional. The keys are canvas glyphs with no child nodes, so a11y
    // services could never read them anyway; this keeps screen readers off the
    // container too. The keypad is not screen-reader usable by design — see README.
    importantForAccessibility = IMPORTANT_FOR_ACCESSIBILITY_NO_HIDE_DESCENDANTS
    if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.O) {
      importantForAutofill = IMPORTANT_FOR_AUTOFILL_NO_EXCLUDE_DESCENDANTS
    }
  }

  fun dispose() {
    disposed = true
    // Drop any rebuild still queued from a prop set in this same frame;
    // otherwise it would run after us and leak a fresh session.
    mainHandler.removeCallbacksAndMessages(null)
    rebuildScheduled = false
    liveViews.remove(this)
    handle?.destroy()
    handle = null
  }
}
