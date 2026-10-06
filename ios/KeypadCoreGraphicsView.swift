// Copyright (c) 2026 expo-secure-keypad-jsi contributors. MIT License.
import UIKit

/// Draws with CoreGraphics and no UILabels, so digit glyphs never become
/// accessibility text. Touches resolve to a digit *value* here; the on-screen
/// coordinate never leaves this class.
final class KeypadCoreGraphicsView: UIView, ThemableKeypadView {

  enum ShuffleMode { case mount, perKey, off }

  protocol Listener: AnyObject {
    func onDigitPressed(_ digit: Int)
    func onBackspace()
    func onClear()
  }

  weak var listener: Listener?
  var shuffleMode: ShuffleMode = .mount

  var keyColor: UIColor = UIColor(white: 0.11, alpha: 1)
  var keyTextColor: UIColor = .white
  var actionTextColor: UIColor = UIColor(white: 0.56, alpha: 1)
  var keyCornerRadius: CGFloat = 12
  var digitTextSize: CGFloat = 32
  var fontFamily: String?

  /// false hides the press feedback entirely, for apps that cannot block capture.
  var pressedHighlight = true
  var pressedKeyColor: UIColor?
  var clearKeyLabel: String?
  // No submit key on the digit pad (autoSubmit / ref.submit()); kept for the protocol.
  var submitKeyLabel: String?

  var layoutProvider: (() -> [Int])?

  // 3 cols x 4 rows. Digits occupy 0..8 and 10; 9 = clear, 11 = backspace.
  private let digitCells = [0, 1, 2, 3, 4, 5, 6, 7, 8, 10]
  private let clearCell = 9
  private let backspaceCell = 11
  private var digitAtCell = [Int](repeating: -1, count: 12)
  private var pressedCell = -1
  private let gap: CGFloat = 12

  override init(frame: CGRect) {
    super.init(frame: frame)
    backgroundColor = .clear
    isAccessibilityElement = false
    reshuffle()
  }

  required init?(coder: NSCoder) { fatalError("init(coder:) not supported") }

  func reshuffle() {
    let layout: [Int]
    if shuffleMode == .off {
      layout = identityDigitLayout
    } else {
      layout = layoutProvider?() ?? identityDigitLayout
    }
    digitAtCell = [Int](repeating: -1, count: 12)
    for (slot, cell) in digitCells.enumerated() where slot < layout.count {
      digitAtCell[cell] = layout[slot]
    }
    setNeedsDisplay()
  }

  private func cellSize() -> (w: CGFloat, h: CGFloat) {
    let w = (bounds.width - gap * 2) / 3
    let h = (bounds.height - gap * 3) / 4
    return (w, h)
  }

  // No background fill: the host view's `style` carries it, so the gaps
  // between keys show whatever is behind the keypad.
  override func draw(_ rect: CGRect) {
    let (cw, ch) = cellSize()

    for cell in 0..<12 {
      let col = CGFloat(cell % 3)
      let row = CGFloat(cell / 3)
      let frame = CGRect(x: col * (cw + gap), y: row * (ch + gap), width: cw, height: ch)
      let digit = digitAtCell[cell]
      let held = pressedHighlight && cell == pressedCell
      let path = UIBezierPath(roundedRect: frame, cornerRadius: keyCornerRadius)
      // Action cells have no key fill. A held one gets the pressedKeyColor fill
      // when the theme sets it; otherwise the glyph itself carries the dim.
      let actionFill = held && digit < 0 && pressedKeyColor != nil
      let actionColor = actionFill ? actionTextColor : pressedDim(actionTextColor, held)
      if actionFill && (cell == backspaceCell || cell == clearCell) {
        keyFill(held).setFill()
        path.fill()
      }
      if digit >= 0 {
        keyFill(held).setFill()
        path.fill()
        drawCentered("\(digit)", in: frame, size: digitTextSize, color: keyTextColor, family: fontFamily)
      } else if cell == backspaceCell {
        drawCentered("\u{232B}", in: frame, size: digitTextSize * 0.7, color: actionColor)
      } else if cell == clearCell {
        drawCentered(clearKeyLabel ?? "\u{2715}", in: frame, size: digitTextSize * 0.7,
                     color: actionColor, fit: clearKeyLabel != nil)
      }
    }
  }

  private func cellAt(_ p: CGPoint) -> Int {
    // Mirrors KeyboardCoreGraphicsView.layoutKeys: never divide by a
    // zero/negative cell during a collapsed layout pass.
    guard bounds.width > 0, bounds.height > 0 else { return -1 }
    let (cw, ch) = cellSize()
    guard cw + gap > 0, ch + gap > 0 else { return -1 }
    let col = max(0, min(2, Int(p.x / (cw + gap))))
    let row = max(0, min(3, Int(p.y / (ch + gap))))
    return row * 3 + col
  }

  override func touchesBegan(_ touches: Set<UITouch>, with event: UIEvent?) {
    guard let touch = touches.first else { return }
    pressedCell = cellAt(touch.location(in: self))
    setNeedsDisplay()
  }

  override func touchesCancelled(_ touches: Set<UITouch>, with event: UIEvent?) {
    pressedCell = -1
    setNeedsDisplay()
  }

  override func touchesEnded(_ touches: Set<UITouch>, with event: UIEvent?) {
    pressedCell = -1
    setNeedsDisplay()
    guard let touch = touches.first else { return }
    let cell = cellAt(touch.location(in: self))
    guard cell >= 0, cell < 12 else { return }
    if digitAtCell[cell] >= 0 {
      listener?.onDigitPressed(digitAtCell[cell])
      if shuffleMode == .perKey { reshuffle() }
    } else if cell == backspaceCell {
      listener?.onBackspace()
    } else if cell == clearCell {
      listener?.onClear()
    }
  }
}

/// The theme surface both keypad views expose, so `SecureKeypadJsiView` applies
/// a JS theme dictionary through one code path instead of one per keypad type.
protocol ThemableKeypadView: AnyObject {
  var keyColor: UIColor { get set }
  var keyTextColor: UIColor { get set }
  var actionTextColor: UIColor { get set }
  var keyCornerRadius: CGFloat { get set }
  var digitTextSize: CGFloat { get set }
  /// Digit / character glyphs only; action glyphs stay on the system font.
  var fontFamily: String? { get set }
  /// false hides the press feedback entirely, for apps that cannot block capture.
  var pressedHighlight: Bool { get set }
  /// Fill of a held key; nil keeps the default 70% dim of `keyColor`.
  var pressedKeyColor: UIColor? { get set }
  /// Text drawn instead of the ✕ glyph; nil keeps the glyph.
  var clearKeyLabel: String? { get set }
  /// Text drawn instead of the ⏎ glyph (full keyboard only); nil keeps the glyph.
  var submitKeyLabel: String? { get set }

  func setNeedsDisplay()
}

/// Unshuffled order, phone-keypad style (1..9 then 0): what `shuffle: 'off'`
/// shows, and the fallback whenever the native CSPRNG layout is unavailable.
let identityDigitLayout = [1, 2, 3, 4, 5, 6, 7, 8, 9, 0]

extension ThemableKeypadView {
  /// Key fill: `pressedKeyColor` while held when set, else the 70% dim.
  func keyFill(_ held: Bool) -> UIColor {
    guard held else { return keyColor }
    return pressedKeyColor ?? keyColor.withAlphaComponent(keyColor.cgColor.alpha * 0.7)
  }
}

extension UIView {
  /// A held key is drawn at 70% opacity. One rule reads correctly on any theme,
  /// so no per-theme luminance branch is needed.
  func pressedDim(_ c: UIColor, _ held: Bool) -> UIColor {
    held ? c.withAlphaComponent(c.cgColor.alpha * 0.7) : c
  }

  /// Shared by both keypad views: centred text with no UILabel in the tree.
  /// `family` nil (or unresolvable) draws in the system font — action glyphs
  /// always pass nil, since most custom fonts have no glyph for them. `fit`
  /// shrinks the text to the key's width — for theme labels, whose length is
  /// the app's.
  func drawCentered(_ text: String, in frame: CGRect, size: CGFloat, color: UIColor,
                    family: String? = nil, fit: Bool = false) {
    var fontSize = size
    if fit {
      let probe = NSAttributedString(string: text,
                                     attributes: [.font: EskFont.resolve(family, size: size)])
      let maxWidth = frame.width * 0.85
      let width = probe.size().width
      if width > maxWidth, width > 0 { fontSize = size * maxWidth / width }
    }
    let attrs: [NSAttributedString.Key: Any] = [
      .font: EskFont.resolve(family, size: fontSize),
      .foregroundColor: color,
    ]
    let str = NSAttributedString(string: text, attributes: attrs)
    let textSize = str.size()
    let origin = CGPoint(x: frame.midX - textSize.width / 2,
                         y: frame.midY - textSize.height / 2)
    str.draw(at: origin)
  }
}


/// Resolves a theme `fontFamily` to a `UIFont`, memoised because `draw(_:)`
/// runs on every press and a miss walks the whole registered font list.
enum EskFont {
  private static var cache = [String: UIFont]()

  static func resolve(_ family: String?, size: CGFloat) -> UIFont {
    let fallback = UIFont.systemFont(ofSize: size, weight: .medium)
    guard let family, !family.isEmpty else { return fallback }
    let key = "\(family)@\(size)"
    if let hit = cache[key] { return hit }
    // A PostScript / full font name resolves directly. An `expo-font` alias
    // does not: expo swizzles `fontNames(forFamilyName:)`, not `init(name:)`,
    // so the family lookup is the second attempt rather than the first.
    var resolved = UIFont(name: family, size: size)
    if resolved == nil {
      for name in UIFont.fontNames(forFamilyName: family) {
        if let f = UIFont(name: name, size: size) { resolved = f; break }
      }
    }
    let font = resolved ?? fallback
    cache[key] = font
    return font
  }
}
