import * as React from 'react';
import { StyleSheet, View, type StyleProp, type ViewStyle } from 'react-native';

import type {
  KeypadError,
  KeypadTheme,
  KeypadType,
  SecureKeypadHandle,
  ShuffleMode,
} from './SecureKeypadJsi.types';
import SecureKeypadJsiView from './SecureKeypadJsiView';

export interface SecureKeypadProps {
  /** Server RSA-2048+ public key, PEM SubjectPublicKeyInfo. Required. */
  publicKey: string;
  /**
   * 'digit' = 3x4 shuffled PIN pad. 'full' = QWERTY keyboard with shift and
   * symbol layers (its Enter key submits). Default 'digit'.
   */
  keypadType?: KeypadType;
  /** Defaults: 4 / 6 ('digit') or 4 / 64 ('full'). Clamped to 4~12 / 4~64. */
  minLength?: number;
  maxLength?: number;
  shuffle?: ShuffleMode;
  /**
   * Auto-encrypt at maxLength. Default true for 'digit', false for 'full' —
   * passwords are variable-length, so the keyboard's Enter key submits.
   */
  autoSubmit?: boolean;
  theme?: KeypadTheme;
  /**
   * Applied to the keypad, or to the container when `accessory` is set.
   */
  style?: StyleProp<ViewStyle>;
  /**
   * Rendered directly above the keypad, inside the same container — the place
   * for a masked-length indicator or a confirm button when the keypad covers
   * the field it fills (bottom sheets, overlays). Ordinary React content: it
   * never receives the entered value, only what you pass it.
   */
  accessory?: React.ReactNode;

  /** Ciphertext envelope JSON — send this to your server. */
  onComplete?: (envelope: string) => void;
  /** Drive your masked-dot UI from this. */
  onDigitCountChanged?: (count: number) => void;
  /**
   * Key rejected at mount ('arm'), encryption failed on submit ('submit'), or
   * Android dropped an obscured touch ('input', "ERR_OBSCURED_TOUCH").
   */
  onError?: (error: KeypadError) => void;
}

// Yoga sizes Expo views purely from style — their shadow node has no measure
// function, so a keypad with no height renders at zero. Applied only when the
// caller gave nothing to size it by. 3x4 near-square keys for the digit pad;
// the QWERTY keyboard is 5 rows of ~10 keys, so it is wider than tall.
const digitSizing: ViewStyle = { aspectRatio: 3 / 4 };
const fullSizing: ViewStyle = { aspectRatio: 4 / 3 };
// With an accessory the caller's size lands on the container; the keypad
// then fills whatever the accessory leaves.
const fillRest: ViewStyle = { flex: 1 };

function hasOwnSize(style: StyleProp<ViewStyle>): boolean {
  const flat = StyleSheet.flatten(style);
  if (!flat) return false;
  return (
    flat.height != null ||
    flat.aspectRatio != null ||
    flat.flex != null ||
    flat.flexGrow != null ||
    flat.flexBasis != null
  );
}

/**
 * Unwraps `nativeEvent` for the caller. PIN digits never enter JavaScript —
 * `onComplete` yields only the ciphertext envelope. The buffer is wiped
 * whenever the app backgrounds or the view leaves the window; that is not
 * configurable.
 */
export const SecureKeypad = React.forwardRef<SecureKeypadHandle, SecureKeypadProps>(
  (props, ref) => {
    const {
      publicKey,
      keypadType = 'digit',
      minLength = 4,
      maxLength = keypadType === 'full' ? 64 : 6,
      shuffle = 'mount',
      autoSubmit = keypadType !== 'full',
      theme,
      style,
      accessory,
      onComplete,
      onDigitCountChanged,
      onError,
    } = props;

    const sized = React.useMemo(() => hasOwnSize(style), [style]);
    const defaultSizing = keypadType === 'full' ? fullSizing : digitSizing;
    const hasAccessory = accessory != null && accessory !== false;

    let keypadStyle: StyleProp<ViewStyle>;
    if (hasAccessory) {
      keypadStyle = sized ? fillRest : defaultSizing;
    } else {
      keypadStyle = sized ? style : [defaultSizing, style];
    }

    const keypad = (
      <SecureKeypadJsiView
        ref={ref}
        publicKey={publicKey}
        keypadType={keypadType}
        minLength={minLength}
        maxLength={maxLength}
        shuffle={shuffle}
        autoSubmit={autoSubmit}
        theme={theme}
        style={keypadStyle}
        onComplete={(e) => onComplete?.(e.nativeEvent.envelope)}
        onDigitCountChanged={(e) => onDigitCountChanged?.(e.nativeEvent.count)}
        onError={(e) => onError?.(e.nativeEvent)}
      />
    );

    if (!hasAccessory) return keypad;

    return (
      <View style={style}>
        {accessory}
        {keypad}
      </View>
    );
  }
);

SecureKeypad.displayName = 'SecureKeypad';

export default SecureKeypad;
