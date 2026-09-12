import type { StyleProp, ViewStyle } from 'react-native';

/**
 * Which step failed. 'arm' = public key rejected at mount, 'submit' =
 * encryption failed, 'input' = a touch was rejected before reaching the
 * keypad (Android tapjacking filter; see KeypadError.code). None of these is
 * a security finding — hooking, device posture and screen capture are out of
 * scope, see README.
 */
export type KeypadErrorPhase = 'arm' | 'submit' | 'input';

export type ShuffleMode = 'mount' | 'perKey' | 'off';

/**
 * 'digit' = 3x4 shuffled PIN pad.
 * 'full' = QWERTY keyboard with shift + symbol layers. The digit
 * row is fully shuffled and each character row gets a blank dummy key at a
 * random slot, so the same character is not at the same coordinate across
 * sessions. Layout and shift state live only in native code.
 */
export type KeypadType = 'digit' | 'full';

export interface KeypadError {
  /**
   * Stable code, e.g. "ERR_WEAK_KEY", "ERR_TOO_SHORT". Phase 'input' only
   * emits "ERR_OBSCURED_TOUCH": Android dropped a touch because another window
   * (overlay app, screen dimmer) was drawn over the keypad. The tap was never
   * registered — tell the user to close the overlay.
   */
  code: string;
  phase: KeypadErrorPhase;
}

/**
 * Colors are #RRGGBB or #RRGGBBAA. Sizes are density-independent (dp on
 * Android, points on iOS), so the same number looks the same on both.
 */
export interface KeypadTheme {
  keyColor?: string;
  keyTextColor?: string;
  actionTextColor?: string;
  cornerRadius?: number;
  digitTextSize?: number;
  /**
   * Font for the digit / character glyphs. Any name the platform can already
   * resolve: a family registered by `expo-font`'s `loadAsync` / `useFonts`, a
   * font bundled at build time, or a system family ("Courier", "monospace").
   * Unresolvable names fall back to the system font instead of throwing.
   *
   * NOTE: the action glyphs (⌫ ✕ ⇧ ⏎) always render in the system font. Most
   * custom fonts have no glyph for them, and a missing glyph would draw as
   * tofu (□) on an unlabelled key. For `keypadType: 'full'`, pick a font that
   * covers all of printable ASCII (0x21~0x7E) or some keys will show tofu.
   */
  fontFamily?: string;
  /**
   * Whether a held key dims to 70% opacity (default true). NOTE: any visible
   * press effect lets a screen recording reconstruct the input — block capture
   * at the app level (FLAG_SECURE / UIScreen.isCaptured) or pass false here.
   */
  pressedHighlight?: boolean;
}

export type OnCompleteEvent = { nativeEvent: { envelope: string } };
export type OnDigitCountChangedEvent = { nativeEvent: { count: number } };
export type OnErrorEvent = { nativeEvent: KeypadError };

/** Raw native view props; most apps want `SecureKeypad` instead. */
export interface SecureKeypadJsiViewProps {
  /** Server RSA-2048+ public key, PEM SubjectPublicKeyInfo. */
  publicKey: string;
  /** Default 'digit'. */
  keypadType?: KeypadType;
  /** Clamped to 4~12 ('digit') or 4~64 ('full'). */
  minLength?: number;
  maxLength?: number;
  shuffle?: ShuffleMode;
  autoSubmit?: boolean;
  theme?: KeypadTheme;
  onComplete?: (event: OnCompleteEvent) => void;
  onDigitCountChanged?: (event: OnDigitCountChangedEvent) => void;
  onError?: (event: OnErrorEvent) => void;
  style?: StyleProp<ViewStyle>;
}

export interface SecureKeypadHandle {
  /** Clears the entered digits; does not disarm. */
  clear: () => Promise<void>;
  /** Encrypts now if the PIN meets minLength. */
  submit: () => Promise<void>;
}
