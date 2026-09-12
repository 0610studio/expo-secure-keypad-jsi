// Copyright (c) 2026 expo-secure-keypad-jsi contributors. MIT License.
import UIKit

/// Full QWERTY secure keyboard drawn with CoreGraphics and no UILabels, so key
/// glyphs never become accessibility text. Touches resolve to a final ASCII
/// value here (shift and symbol layer included); the on-screen coordinate
/// never leaves this class.
///
/// Following commercial secure-keyboard practice, the QWERTY arrangement is
/// kept readable: the digit row is fully shuffled (order comes from the native
/// core's CSPRNG), and each character row gets one blank dummy key at a random
/// slot so the same character is not at the same coordinate across sessions.
final class KeyboardCoreGraphicsView: UIView, ThemableKeypadView {

  protocol Listener: AnyObject {
    /// Final ASCII character — shift/layer already applied.
    func onKeyPressed(_ asciiChar: Int)
    func onBackspace()
    func onClear()
    func onDone()
  }

  weak var listener: Listener?
  var shuffleMode: KeypadCoreGraphicsView.ShuffleMode = .mount

  /// Shuffled 0..9 from the native core; identity order when shuffle is off.
  var digitsProvider: (() -> [Int])?

  var keyColor: UIColor = UIColor(white: 0.11, alpha: 1)
  var keyTextColor: UIColor = .white
  var actionTextColor: UIColor = UIColor(white: 0.56, alpha: 1)
  var keyCornerRadius: CGFloat = 8
  var digitTextSize: CGFloat = 32
  var fontFamily: String?

  /// false hides the press feedback entirely, for apps that cannot block capture.
  var pressedHighlight = true

  private enum Code {
    static let shift = -1
    static let backspace = -2
    static let clear = -3
    static let done = -4
    static let toggle = -5
    static let dummy = -6
  }

  private enum Layer { case letters, symbols }
  private enum ShiftState { case off, oneShot, lock }

  private struct Key {
    let code: Int
    var weight: CGFloat = 1
  }

  private static let rowQwertyTop = "qwertyuiop"
  private static let rowQwertyMid = "asdfghjkl"
  private static let rowQwertyBot = "zxcvbnm"
  // All 32 printable ASCII specials, three rows + a short fourth row.
  private static let rowSym0 = "!@#$%^&*()"
  private static let rowSym1 = "-_=+[]{}\\|"
  private static let rowSym2 = ";:'\",.<>?/"
  private static let rowSym3 = "`~"

  private var layer_ = Layer.letters
  private var shift = ShiftState.off
  private var lastShiftTap: TimeInterval = 0

  private var digitsOrder: [Int] = Array(0...9)
  // One dummy-slot seed per character row, re-drawn on reshuffle(); -1 = none.
  private var dummySlots = [Int](repeating: -1, count: 5)

  private var keyRects: [(CGRect, Key)] = []
  private var pressedIndex = -1
  private let gap: CGFloat = 6

  override init(frame: CGRect) {
    super.init(frame: frame)
    backgroundColor = .clear
    isAccessibilityElement = false
    reshuffle()
  }

  required init?(coder: NSCoder) { fatalError("init(coder:) not supported") }

  func reshuffle() {
    let shuffled = shuffleMode != .off
    digitsOrder = shuffled ? (digitsProvider?() ?? Array(0...9)) : Array(0...9)
    for i in dummySlots.indices {
      dummySlots[i] = shuffled ? Int.random(in: 0..<Int.max, using: &systemRng) : -1
    }
    layoutKeys()
    setNeedsDisplay()
  }

  // SystemRandomNumberGenerator is cryptographically secure on Apple platforms.
  private var systemRng = SystemRandomNumberGenerator()

  private func withDummy(_ keys: inout [Key], _ rowIndex: Int) {
    let seed = dummySlots[rowIndex]
    guard seed >= 0 else { return }
    keys.insert(Key(code: Code.dummy), at: seed % (keys.count + 1))
  }

  private func charKeys(_ s: String) -> [Key] {
    s.unicodeScalars.map { Key(code: Int($0.value)) }
  }

  private func buildRows() -> [[Key]] {
    var rows: [[Key]] = []
    if layer_ == .letters {
      rows.append(digitsOrder.map { Key(code: Int(UnicodeScalar("0").value) + $0) })
      var top = charKeys(Self.rowQwertyTop); withDummy(&top, 0); rows.append(top)
      var mid = charKeys(Self.rowQwertyMid); withDummy(&mid, 1); rows.append(mid)
      var bot: [Key] = [Key(code: Code.shift, weight: 1.5)]
      var letters = charKeys(Self.rowQwertyBot); withDummy(&letters, 2)
      bot.append(contentsOf: letters)
      bot.append(Key(code: Code.backspace, weight: 1.5))
      rows.append(bot)
    } else {
      var s0 = charKeys(Self.rowSym0); withDummy(&s0, 4); rows.append(s0)
      var s1 = charKeys(Self.rowSym1); withDummy(&s1, 0); rows.append(s1)
      var s2 = charKeys(Self.rowSym2); withDummy(&s2, 1); rows.append(s2)
      var s3 = Self.rowSym3.unicodeScalars.map { Key(code: Int($0.value), weight: 2) }
      withDummy(&s3, 3)
      s3.append(Key(code: Code.backspace, weight: 2))
      rows.append(s3)
    }
    rows.append([Key(code: Code.toggle), Key(code: Code.clear), Key(code: Code.done)])
    return rows
  }

  private func layoutKeys() {
    keyRects = []
    let w = bounds.width
    let h = bounds.height
    guard w > 0, h > 0 else { return }

    let rows = buildRows()
    let rowH = (h - gap * CGFloat(rows.count - 1)) / CGFloat(rows.count)
    var top: CGFloat = 0
    for row in rows {
      let totalWeight = row.reduce(CGFloat(0)) { $0 + $1.weight }
      let unit = (w - gap * CGFloat(row.count - 1)) / totalWeight
      var left: CGFloat = 0
      for key in row {
        let kw = unit * key.weight
        keyRects.append((CGRect(x: left, y: top, width: kw, height: rowH), key))
        left += kw + gap
      }
      top += rowH + gap
    }
  }

  override func layoutSubviews() {
    super.layoutSubviews()
    layoutKeys()
    setNeedsDisplay()
  }

  private func labelFor(_ key: Key) -> String? {
    switch key.code {
    case Code.dummy: return nil
    case Code.shift: return "\u{21E7}"
    case Code.backspace: return "\u{232B}"
    case Code.clear: return "\u{2715}"
    case Code.done: return "\u{23CE}"
    case Code.toggle: return layer_ == .letters ? "!#1" : "abc"
    default:
      let c = Character(UnicodeScalar(key.code)!)
      if shift != .off, c.isLowercase { return String(c).uppercased() }
      return String(c)
    }
  }

  // No background fill: see KeypadCoreGraphicsView.draw.
  override func draw(_ rect: CGRect) {
    for (index, entry) in keyRects.enumerated() {
      let (frame, key) = entry
      let path = UIBezierPath(roundedRect: frame, cornerRadius: keyCornerRadius)
      pressedDim(keyColor, pressedHighlight && index == pressedIndex).setFill()
      path.fill()
      if let label = labelFor(key) {
        let isChar = key.code >= 0
        // Shift shows as "armed" while one-shot or locked.
        let color: UIColor
        if key.code == Code.shift && shift != .off {
          color = keyTextColor
        } else {
          color = isChar ? keyTextColor : actionTextColor
        }
        let size = isChar ? digitTextSize * 0.6 : digitTextSize * 0.5
        drawCentered(label, in: frame, size: size, color: color,
                     family: isChar ? fontFamily : nil)
      }
    }
  }

  override func touchesBegan(_ touches: Set<UITouch>, with event: UIEvent?) {
    guard let touch = touches.first else { return }
    let p = touch.location(in: self)
    pressedIndex = keyRects.firstIndex(where: { $0.0.contains(p) }) ?? -1
    setNeedsDisplay()
  }

  override func touchesCancelled(_ touches: Set<UITouch>, with event: UIEvent?) {
    pressedIndex = -1
    setNeedsDisplay()
  }

  override func touchesEnded(_ touches: Set<UITouch>, with event: UIEvent?) {
    pressedIndex = -1
    setNeedsDisplay()
    guard let touch = touches.first else { return }
    let p = touch.location(in: self)
    guard let key = keyRects.first(where: { $0.0.contains(p) })?.1 else { return }
    handleKey(key)
  }

  private func handleKey(_ key: Key) {
    switch key.code {
    case Code.dummy:
      break
    case Code.shift:
      let now = ProcessInfo.processInfo.systemUptime
      switch shift {
      case .off: shift = .oneShot
      case .oneShot: shift = (now - lastShiftTap < 0.35) ? .lock : .off
      case .lock: shift = .off
      }
      lastShiftTap = now
      setNeedsDisplay()
    case Code.backspace:
      listener?.onBackspace()
    case Code.clear:
      listener?.onClear()
    case Code.done:
      listener?.onDone()
    case Code.toggle:
      layer_ = layer_ == .letters ? .symbols : .letters
      shift = .off
      layoutKeys()
      setNeedsDisplay()
    default:
      var code = key.code
      if shift != .off, code >= 0x61, code <= 0x7A {
        code -= 0x20  // lowercase -> uppercase
      }
      if shift == .oneShot {
        shift = .off
        setNeedsDisplay()
      }
      listener?.onKeyPressed(code)
      if shuffleMode == .perKey { reshuffle() }
    }
  }
}
