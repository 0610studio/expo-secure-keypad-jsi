// Copyright (c) 2026 expo-secure-keypad-jsi contributors. MIT License.
import ExpoModulesCore
import UIKit

/// Hosts the CoreGraphics keypad (digit) or QWERTY keyboard (full). JS
/// receives only the entered-key count and the ciphertext envelope. Screen
/// capture and device integrity are deliberately out of scope — see README.
class SecureKeypadJsiView: ExpoView, KeypadCoreGraphicsView.Listener,
                           KeyboardCoreGraphicsView.Listener {

  private let onDigitCountChanged = EventDispatcher()
  private let onComplete = EventDispatcher()
  private let onError = EventDispatcher()

  private var digitKeypad: KeypadCoreGraphicsView?
  private var fullKeyboard: KeyboardCoreGraphicsView?
  private var bridge: EskKeypadBridge?

  private var publicKeyPem: String?
  private var keypadType = "digit" // digit | full
  private var minLength: Int32 = 4
  private var maxLength: Int32 = 6
  private var autoSubmit = true
  private var shuffleMode: KeypadCoreGraphicsView.ShuffleMode = .mount
  private var theme: [String: Any]?
  private var rebuildScheduled = false

  /// Last count handed to JS. A rebuild reports 0 for the fresh session even
  /// when JS was already at 0; that redundant event carries no information, so
  /// it is not sent. Keeps both platforms emitting the same event stream.
  private var lastReportedCount: Int = 0

  // Mirrors kMaxPinLength / kMaxSecretLength in the C++ core so the autoSubmit
  // comparison agrees with what the core actually stores.
  private static let maxDigitCap: Int32 = 12
  private static let maxFullCap: Int32 = 64

  /// maxLength after the per-type clamp the C++ core applies.
  private var effectiveMaxLength: Int32 {
    let cap = keypadType == "full" ? Self.maxFullCap : Self.maxDigitCap
    return min(max(4, maxLength), cap)
  }

  required init(appContext: AppContext? = nil) {
    super.init(appContext: appContext)
    clipsToBounds = true
    backgroundColor = .clear

    rebuildInputView()
    registerNotifications()
    scheduleRebuild()
  }

  deinit {
    NotificationCenter.default.removeObserver(self)
  }

  // MARK: - Input view lifecycle

  private func rebuildInputView() {
    digitKeypad?.removeFromSuperview()
    fullKeyboard?.removeFromSuperview()
    digitKeypad = nil
    fullKeyboard = nil

    if keypadType == "full" {
      let v = KeyboardCoreGraphicsView(frame: bounds)
      v.listener = self
      v.shuffleMode = shuffleMode
      v.digitsProvider = { [weak self] in
        (self?.bridge?.shuffledLayout())?.map { $0.intValue } ?? identityDigitLayout
      }
      v.autoresizingMask = [.flexibleWidth, .flexibleHeight]
      applyTheme(to: v)
      addSubview(v)
      fullKeyboard = v
    } else {
      let v = KeypadCoreGraphicsView(frame: bounds)
      v.listener = self
      v.shuffleMode = shuffleMode
      v.layoutProvider = { [weak self] in
        (self?.bridge?.shuffledLayout())?.map { $0.intValue } ?? identityDigitLayout
      }
      v.autoresizingMask = [.flexibleWidth, .flexibleHeight]
      applyTheme(to: v)
      addSubview(v)
      digitKeypad = v
    }
  }

  override func layoutSubviews() {
    super.layoutSubviews()
    digitKeypad?.frame = bounds
    fullKeyboard?.frame = bounds
  }

  // MARK: - Prop setters

  func setPublicKeyPem(_ pem: String) { publicKeyPem = pem; scheduleRebuild() }
  func setMinLength(_ v: Int) { minLength = Int32(max(4, min(64, v))); scheduleRebuild() }
  func setMaxLength(_ v: Int) { maxLength = Int32(max(4, min(64, v))); scheduleRebuild() }
  func setAutoSubmit(_ v: Bool) { autoSubmit = v }

  func setKeypadType(_ v: String) {
    let next = v == "full" ? "full" : "digit"
    if next == keypadType { return }
    keypadType = next
    rebuildInputView()
    scheduleRebuild()
  }

  func setShuffle(_ mode: String) {
    switch mode {
    case "perKey": shuffleMode = .perKey
    case "off": shuffleMode = .off
    default: shuffleMode = .mount
    }
    if let v = digitKeypad { v.shuffleMode = shuffleMode; v.reshuffle() }
    if let v = fullKeyboard { v.shuffleMode = shuffleMode; v.reshuffle() }
  }

  func setTheme(_ t: [String: Any]) {
    theme = t
    if let v = digitKeypad { applyTheme(to: v); v.setNeedsDisplay() }
    if let v = fullKeyboard { applyTheme(to: v); v.setNeedsDisplay() }
  }

  private func applyTheme(to v: ThemableKeypadView) {
    guard let t = theme else { return }
    if let s = t["keyColor"] as? String { v.keyColor = UIColor(hex: s) ?? v.keyColor }
    if let s = t["keyTextColor"] as? String { v.keyTextColor = UIColor(hex: s) ?? v.keyTextColor }
    if let s = t["actionTextColor"] as? String { v.actionTextColor = UIColor(hex: s) ?? v.actionTextColor }
    if let n = t["cornerRadius"] as? NSNumber { v.keyCornerRadius = CGFloat(truncating: n) }
    if let n = t["digitTextSize"] as? NSNumber { v.digitTextSize = CGFloat(truncating: n) }
    // Absent key vs. explicit null: both mean "system font", so this one is
    // assigned unconditionally instead of only on a hit.
    v.fontFamily = (t["fontFamily"] as? String).flatMap { $0.isEmpty ? nil : $0 }
    if let b = t["pressedHighlight"] as? Bool { v.pressedHighlight = b }
    // Same absent-or-null rule as fontFamily: back to the default (70% dim,
    // the ✕ / ⏎ glyphs).
    v.pressedKeyColor = (t["pressedKeyColor"] as? String).flatMap { UIColor(hex: $0) }
    v.clearKeyLabel = (t["clearKeyLabel"] as? String).flatMap { $0.isEmpty ? nil : $0 }
    v.submitKeyLabel = (t["submitKeyLabel"] as? String).flatMap { $0.isEmpty ? nil : $0 }
  }

  // MARK: - Session lifecycle

  private func scheduleRebuild() {
    if rebuildScheduled { return }
    rebuildScheduled = true
    DispatchQueue.main.async { [weak self] in
      self?.rebuildScheduled = false
      self?.rebuildBridge()
    }
  }

  private func rebuildBridge() {
    // A new session starts empty; tell JS so a masked-dot UI never shows
    // digits that no longer exist in the buffer.
    let hadSession = bridge != nil
    let type: Int32 = keypadType == "full" ? 1 : 0
    let b = EskKeypadBridge(minLength: minLength, maxLength: maxLength, keypadType: type)
    bridge = b
    digitKeypad?.reshuffle()
    fullKeyboard?.reshuffle()
    if hadSession { reportCount(0) }

    if let pem = publicKeyPem {
      if let err = b.arm(pem) {
        onError(["code": err, "phase": "arm"])
      }
    }
  }

  // MARK: - KeypadCoreGraphicsView.Listener (digit pad)

  func onDigitPressed(_ digit: Int) {
    guard let b = bridge else { return }
    let count = b.press(UInt8(digit))
    // Negative = the core refused the call (ESK_COUNT_ERROR); buffer unchanged.
    if count < 0 { return }
    reportCount(Int(count))
    if autoSubmit && count >= effectiveMaxLength { submit() }
  }

  // MARK: - KeyboardCoreGraphicsView.Listener (full keyboard)

  func onKeyPressed(_ codePoint: Int) {
    // The range guard only keeps UInt32() from trapping; the core owns the charset.
    guard let b = bridge, codePoint > 0, codePoint <= 0x10FFFF else { return }
    let count = b.pressKey(UInt32(codePoint))
    if count < 0 { return }
    reportCount(Int(count))
    if autoSubmit && count >= effectiveMaxLength { submit() }
  }

  func onDone() { submit() }

  // MARK: - Shared listener members

  func onBackspace() {
    guard let b = bridge else { return }
    let count = b.backspace()
    if count < 0 { return }
    reportCount(Int(count))
  }

  func onClear() {
    bridge?.clear()
    reportCount(0)
  }

  private func reportCount(_ count: Int) {
    if count == lastReportedCount { return }
    lastReportedCount = count
    onDigitCountChanged(["count": count])
  }

  func submit() {
    guard let b = bridge else { return }
    var err: NSString?
    if let envelope = b.submit(&err) {
      onComplete(["envelope": envelope])
      reportCount(0)
    } else {
      onError(["code": (err as String?) ?? "ERR_INTERNAL", "phase": "submit"])
    }
  }

  func clearPin() {
    bridge?.clear()
    reportCount(0)
  }

  // MARK: - Backgrounding

  private func registerNotifications() {
    NotificationCenter.default.addObserver(
      self, selector: #selector(didEnterBackground),
      name: UIApplication.didEnterBackgroundNotification, object: nil)
  }

  // Backgrounding = the app resigning to the background or this view leaving
  // the window (react-native-screens detaches inactive screens). Either way
  // the buffer is wiped and JS is told the count is 0, so the masked-dot UI
  // cannot drift from the real buffer.
  private func onAppBackground() {
    guard let b = bridge else { return }
    b.onBackground()
    reportCount(0)
  }

  @objc private func didEnterBackground() { onAppBackground() }

  override func didMoveToWindow() {
    super.didMoveToWindow()
    if window == nil { onAppBackground() }
  }
}

// #RRGGBB / #RRGGBBAA theme props.
extension UIColor {
  convenience init?(hex: String) {
    var s = hex.trimmingCharacters(in: .whitespacesAndNewlines)
    if s.hasPrefix("#") { s.removeFirst() }
    guard let value = UInt64(s, radix: 16) else { return nil }
    let r, g, b, a: CGFloat
    switch s.count {
    case 6:
      r = CGFloat((value >> 16) & 0xff) / 255
      g = CGFloat((value >> 8) & 0xff) / 255
      b = CGFloat(value & 0xff) / 255
      a = 1
    case 8:
      r = CGFloat((value >> 24) & 0xff) / 255
      g = CGFloat((value >> 16) & 0xff) / 255
      b = CGFloat((value >> 8) & 0xff) / 255
      a = CGFloat(value & 0xff) / 255
    default:
      return nil
    }
    self.init(red: r, green: g, blue: b, alpha: a)
  }
}
