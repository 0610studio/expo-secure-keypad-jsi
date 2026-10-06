import { SecureKeypad, type KeypadTheme, type SecureKeypadHandle } from 'expo-secure-keypad-jsi';
import { useRef, useState } from 'react';
import {
  Button,
  Pressable,
  ScrollView,
  StyleSheet,
  Switch,
  Text,
  View,
  useColorScheme,
} from 'react-native';

import { DEMO_PUBLIC_KEY } from './demoKey';

type Mode = 'light' | 'dark';

const MIN_LENGTH = 4;
const MAX_LENGTH = 64;

/**
 * Every value below is a plain `#RRGGBB` / `#RRGGBBAA` string handed to the
 * `theme` prop. The keypad parses it natively and identically on both
 * platforms — Android deliberately does not use `Color.parseColor`, which
 * would read an 8-digit value as #AARRGGBB and drift from iOS.
 */
const SURFACE: Record<Mode, { screen: string; card: string; text: string; subtle: string }> = {
  light: { screen: '#F2F2F7', card: '#FFFFFF', text: '#1C1C1E', subtle: '#6E6E73' },
  dark: { screen: '#000000', card: '#1C1C1E', text: '#FFFFFF', subtle: '#8E8E93' },
};

const PALETTES: {
  name: string;
  swatch: string;
  keys: Record<Mode, Pick<KeypadTheme, 'keyColor' | 'keyTextColor' | 'actionTextColor'>>;
}[] = [
  {
    name: 'Slate',
    swatch: '#8E8E93',
    keys: {
      light: { keyColor: '#E5E5EA', keyTextColor: '#1C1C1E', actionTextColor: '#6E6E73' },
      dark: { keyColor: '#2C2C2E', keyTextColor: '#FFFFFF', actionTextColor: '#8E8E93' },
    },
  },
  {
    name: 'Ocean',
    swatch: '#0A84FF',
    keys: {
      light: { keyColor: '#D6E9FF', keyTextColor: '#00366B', actionTextColor: '#0A6BCC' },
      dark: { keyColor: '#0B3D66', keyTextColor: '#EAF4FF', actionTextColor: '#64B5FF' },
    },
  },
  {
    name: 'Sunset',
    swatch: '#FF9F0A',
    keys: {
      light: { keyColor: '#FFE6C7', keyTextColor: '#5A3200', actionTextColor: '#C97A00' },
      dark: { keyColor: '#5A3200', keyTextColor: '#FFF1DE', actionTextColor: '#FFB86B' },
    },
  },
  {
    name: 'Forest',
    swatch: '#30D158',
    keys: {
      light: { keyColor: '#D6F5DE', keyTextColor: '#0B3D1E', actionTextColor: '#1E8E4A' },
      dark: { keyColor: '#12351F', keyTextColor: '#E8FFEF', actionTextColor: '#4FD97A' },
    },
  },
];

const RADIUS_STEPS = [0, 4, 8, 14, 999];
const TEXT_SIZE_STEPS = [24, 28, 32, 38];
// null = the default 70% dim of keyColor.
const PRESSED_COLORS: (string | null)[] = [null, '#0A84FF', '#FF453A'];

export default function ThemeDemo() {
  const systemMode: Mode = useColorScheme() === 'dark' ? 'dark' : 'light';
  // null = follow the OS live, so flipping appearance restyles the keyboard
  // with no remount. An explicit choice pins it until System is picked again.
  const [override, setOverride] = useState<Mode | null>(null);
  const mode = override ?? systemMode;

  const [paletteIndex, setPaletteIndex] = useState(0);
  const [radiusIndex, setRadiusIndex] = useState(2);
  const [sizeIndex, setSizeIndex] = useState(2);
  const [opaqueBackground, setOpaqueBackground] = useState(false);
  const [pressedHighlight, setPressedHighlight] = useState(true);
  const [pressedIndex, setPressedIndex] = useState(0);
  const [textLabels, setTextLabels] = useState(false);

  const [count, setCount] = useState(0);
  const [envelope, setEnvelope] = useState<string | null>(null);
  const [error, setError] = useState<string | null>(null);
  const keypadRef = useRef<SecureKeypadHandle>(null);

  const surface = SURFACE[mode];
  const palette = PALETTES[paletteIndex];

  // The whole point of the screen: one object, swapped live. Changing `theme`
  // never rebuilds the native session, so what you have already typed survives
  // a restyle — only publicKey / lengths / keypadType start a new session.
  const theme: KeypadTheme = {
    ...palette.keys[mode],
    cornerRadius: RADIUS_STEPS[radiusIndex],
    digitTextSize: TEXT_SIZE_STEPS[sizeIndex],
    pressedHighlight,
    ...(PRESSED_COLORS[pressedIndex] ? { pressedKeyColor: PRESSED_COLORS[pressedIndex]! } : null),
    ...(textLabels ? { clearKeyLabel: '취소', submitKeyLabel: '완료' } : null),
  };

  return (
    <ScrollView style={{ backgroundColor: surface.screen }} contentContainerStyle={styles.content}>
      <Text style={[styles.subtle, { color: surface.subtle }]}>
        The full QWERTY keyboard, restyled live. Restyling is free: the native buffer is untouched,
        so whatever you have already typed stays typed — watch the counter survive every switch
        below. ⏎ encrypts.
      </Text>

      <Text style={[styles.note, { color: surface.subtle }]}>
        Heads up: this example app pins userInterfaceStyle to &quot;light&quot; in app.json, so
        useColorScheme() always reports light and System is effectively light-only. Set it to
        &quot;automatic&quot; and rebuild to make System follow the OS.
      </Text>

      <Row label="Appearance" color={surface.subtle}>
        {(['light', 'dark'] as Mode[]).map((m) => (
          <Chip
            key={m}
            label={m === 'light' ? 'Light' : 'Dark'}
            selected={override === m}
            surface={surface}
            onPress={() => setOverride(m)}
          />
        ))}
        <Chip
          label={`System (${systemMode})`}
          selected={override === null}
          surface={surface}
          onPress={() => setOverride(null)}
        />
      </Row>

      <Row label="Palette" color={surface.subtle}>
        {PALETTES.map((p, i) => (
          <Chip
            key={p.name}
            label={p.name}
            swatch={p.swatch}
            selected={paletteIndex === i}
            surface={surface}
            onPress={() => setPaletteIndex(i)}
          />
        ))}
      </Row>

      {/* Not a theme key: the keypad's own `style` carries its background, the
          same as any other React Native view. Transparent (the native default)
          lets the gaps between keys show the page behind them. */}
      <Row label="style backgroundColor" color={surface.subtle}>
        <Chip
          label="Transparent"
          selected={!opaqueBackground}
          surface={surface}
          onPress={() => setOpaqueBackground(false)}
        />
        <Chip
          label="Panel"
          selected={opaqueBackground}
          surface={surface}
          onPress={() => setOpaqueBackground(true)}
        />
      </Row>

      <Row label="cornerRadius" color={surface.subtle}>
        {RADIUS_STEPS.map((r, i) => (
          <Chip
            key={r}
            label={r === 999 ? 'pill' : String(r)}
            selected={radiusIndex === i}
            surface={surface}
            onPress={() => setRadiusIndex(i)}
          />
        ))}
      </Row>

      <Row label="digitTextSize" color={surface.subtle}>
        {TEXT_SIZE_STEPS.map((s, i) => (
          <Chip
            key={s}
            label={String(s)}
            selected={sizeIndex === i}
            surface={surface}
            onPress={() => setSizeIndex(i)}
          />
        ))}
      </Row>

      <Row label="pressedKeyColor" color={surface.subtle}>
        {PRESSED_COLORS.map((c, i) => (
          <Chip
            key={c ?? 'dim'}
            label={c ?? 'Dim (default)'}
            swatch={c ?? undefined}
            selected={pressedIndex === i}
            surface={surface}
            onPress={() => setPressedIndex(i)}
          />
        ))}
      </Row>

      <Row label="clearKeyLabel / submitKeyLabel" color={surface.subtle}>
        <Chip
          label="✕ / ⏎ (default)"
          selected={!textLabels}
          surface={surface}
          onPress={() => setTextLabels(false)}
        />
        <Chip
          label="취소 / 완료"
          selected={textLabels}
          surface={surface}
          onPress={() => setTextLabels(true)}
        />
      </Row>

      <View style={[styles.switchRow, { backgroundColor: surface.card }]}>
        <View style={styles.switchText}>
          <Text style={[styles.switchLabel, { color: surface.text }]}>pressedHighlight</Text>
          <Text style={[styles.switchHint, { color: surface.subtle }]}>
            Off removes the held-key dim entirely. Any visible press effect lets a screen recording
            reconstruct the input, so turn it off when the app cannot block capture.
          </Text>
        </View>
        <Switch value={pressedHighlight} onValueChange={setPressedHighlight} />
      </View>

      <View style={[styles.maskRow, { backgroundColor: surface.card }]}>
        <Text style={[styles.mask, { color: surface.text }]} numberOfLines={1}>
          {'•'.repeat(count) || ' '}
        </Text>
        <Text style={[styles.count, { color: palette.swatch }]}>
          {count}/{MAX_LENGTH}
        </Text>
      </View>

      <View style={styles.keyboardWrap}>
        <SecureKeypad
          ref={keypadRef}
          publicKey={DEMO_PUBLIC_KEY}
          keypadType="full"
          minLength={MIN_LENGTH}
          maxLength={MAX_LENGTH}
          shuffle="mount"
          theme={theme}
          style={[styles.keyboard, opaqueBackground && { backgroundColor: surface.card }]}
          onDigitCountChanged={setCount}
          onComplete={(env) => {
            setEnvelope(env);
            setError(null);
            setCount(0);
          }}
          onError={(e) => setError(`${e.phase}: ${e.code}`)}
        />
      </View>

      <View style={styles.row}>
        <Button title="Clear" color="#FF453A" onPress={() => keypadRef.current?.clear()} />
        <Button
          title="Submit"
          disabled={count < MIN_LENGTH}
          onPress={() => keypadRef.current?.submit()}
        />
      </View>

      <View style={[styles.card, { backgroundColor: surface.card }]}>
        <Text style={[styles.cardTitle, { color: surface.text }]}>Applied theme prop</Text>
        <Text style={[styles.mono, { color: surface.subtle }]} selectable>
          {JSON.stringify(theme, null, 2)}
        </Text>
      </View>

      {envelope ? (
        <View style={[styles.card, { backgroundColor: surface.card }]}>
          <Text style={[styles.cardTitle, { color: surface.text }]}>
            Ciphertext envelope (send to server)
          </Text>
          <Text style={[styles.mono, { color: '#30D158' }]} selectable>
            {envelope}
          </Text>
        </View>
      ) : null}

      {error ? (
        <View style={[styles.card, { backgroundColor: surface.card }]}>
          <Text style={[styles.cardTitle, { color: surface.text }]}>Error</Text>
          <Text style={[styles.mono, { color: '#FF453A' }]}>{error}</Text>
        </View>
      ) : null}
    </ScrollView>
  );
}

function Row({
  label,
  color,
  children,
}: {
  label: string;
  color: string;
  children: React.ReactNode;
}) {
  return (
    <View style={styles.controlRow}>
      <Text style={[styles.rowLabel, { color }]}>{label}</Text>
      <View style={styles.chips}>{children}</View>
    </View>
  );
}

function Chip({
  label,
  swatch,
  selected,
  surface,
  onPress,
}: {
  label: string;
  swatch?: string;
  selected: boolean;
  surface: { card: string; text: string; subtle: string };
  onPress: () => void;
}) {
  return (
    <Pressable
      onPress={onPress}
      style={({ pressed }) => [
        styles.chip,
        { backgroundColor: surface.card, borderColor: selected ? '#0A84FF' : 'transparent' },
        pressed && styles.chipPressed,
      ]}>
      {swatch ? <View style={[styles.swatch, { backgroundColor: swatch }]} /> : null}
      <Text style={[styles.chipText, { color: selected ? '#0A84FF' : surface.text }]}>{label}</Text>
    </Pressable>
  );
}

const styles = StyleSheet.create({
  content: { padding: 20, gap: 14, paddingBottom: 40 },
  subtle: { fontSize: 13, lineHeight: 18 },
  note: { fontSize: 11, lineHeight: 16, fontStyle: 'italic' },
  controlRow: { gap: 8 },
  rowLabel: { fontSize: 12, textTransform: 'uppercase', letterSpacing: 0.5 },
  chips: { flexDirection: 'row', flexWrap: 'wrap', gap: 8 },
  chip: {
    flexDirection: 'row',
    alignItems: 'center',
    gap: 6,
    paddingHorizontal: 12,
    paddingVertical: 8,
    borderRadius: 10,
    borderWidth: 1.5,
  },
  chipPressed: { opacity: 0.6 },
  chipText: { fontSize: 13, fontWeight: '600' },
  swatch: { width: 12, height: 12, borderRadius: 6 },
  switchRow: {
    flexDirection: 'row',
    alignItems: 'center',
    gap: 12,
    borderRadius: 12,
    padding: 14,
  },
  switchText: { flex: 1, gap: 4 },
  switchLabel: { fontSize: 14, fontWeight: '600' },
  switchHint: { fontSize: 11, lineHeight: 15 },
  maskRow: {
    flexDirection: 'row',
    alignItems: 'center',
    justifyContent: 'space-between',
    gap: 12,
    borderRadius: 10,
    paddingHorizontal: 14,
    paddingVertical: 10,
  },
  mask: { flex: 1, fontSize: 18, letterSpacing: 2 },
  count: { fontSize: 12, fontWeight: '600' },
  keyboardWrap: { height: 320 },
  keyboard: { flex: 1 },
  row: { flexDirection: 'row', justifyContent: 'space-around' },
  card: { borderRadius: 12, padding: 14, gap: 8 },
  cardTitle: { fontWeight: '600' },
  mono: { fontFamily: 'Courier', fontSize: 11 },
});
