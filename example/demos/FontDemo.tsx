import { Pacifico_400Regular } from '@expo-google-fonts/pacifico';
import { SpaceMono_400Regular } from '@expo-google-fonts/space-mono';
import { useFonts } from 'expo-font';
import {
  SecureKeypad,
  type KeypadTheme,
  type KeypadType,
  type SecureKeypadHandle,
} from 'expo-secure-keypad-jsi';
import { useRef, useState } from 'react';
import {
  ActivityIndicator,
  Button,
  Pressable,
  ScrollView,
  StyleSheet,
  Text,
  View,
} from 'react-native';

import { DEMO_PUBLIC_KEY } from './demoKey';

/**
 * The alias on the left is the only handle the keypad ever sees: `useFonts`
 * registers the .ttf under it (`ReactFontManager` on Android, a
 * `CTFontManager` alias on iOS), and `theme.fontFamily` looks it up there. So
 * whatever name works in a plain `<Text style={{ fontFamily }}>` works here.
 */
const FONT_ALIASES = {
  SpaceMono: SpaceMono_400Regular,
  Pacifico: Pacifico_400Regular,
};

const CHOICES: { label: string; family?: string; note: string }[] = [
  {
    label: 'System',
    family: undefined,
    note: 'Omit fontFamily (or pass undefined) for the platform default — SF Pro on iOS, Roboto on Android.',
  },
  {
    label: 'Space Mono',
    family: 'SpaceMono',
    note: 'A fixed-width slab. Every digit occupies the same advance width, so the keys look mechanically aligned.',
  },
  {
    label: 'Pacifico',
    family: 'Pacifico',
    note: 'A connected script with a tall x-height and heavy slant — about as far from the system font as a Latin face gets.',
  },
  {
    label: 'NotInstalled',
    family: 'ThisFontDoesNotExist',
    note: 'An unresolvable name is not an error: both platforms fall back to the system font rather than throwing or drawing tofu.',
  },
];

const MIN_LENGTH = 4;

export default function FontDemo() {
  // Nothing below renders until the .ttf files are registered natively: hand
  // the keypad an alias that is not loaded yet and it draws the system font.
  const [loaded, error] = useFonts(FONT_ALIASES);

  const [choiceIndex, setChoiceIndex] = useState(1);
  const [keypadType, setKeypadType] = useState<KeypadType>('digit');
  const [count, setCount] = useState(0);
  const [envelope, setEnvelope] = useState<string | null>(null);
  const [keypadError, setKeypadError] = useState<string | null>(null);
  const keypadRef = useRef<SecureKeypadHandle>(null);

  const choice = CHOICES[choiceIndex];
  const maxLength = keypadType === 'full' ? 64 : 12;

  const theme: KeypadTheme = {
    keyColor: '#1C1C1E',
    keyTextColor: '#FFFFFF',
    actionTextColor: '#8E8E93',
    cornerRadius: keypadType === 'full' ? 8 : 12,
    digitTextSize: 32,
    fontFamily: choice.family,
  };

  if (error) {
    return (
      <View style={styles.center}>
        <Text style={styles.cardTitle}>Font loading failed</Text>
        <Text style={styles.subtle}>{String(error)}</Text>
      </View>
    );
  }

  if (!loaded) {
    return (
      <View style={styles.center}>
        <ActivityIndicator />
        <Text style={styles.subtle}>Registering fonts…</Text>
      </View>
    );
  }

  return (
    <ScrollView contentContainerStyle={styles.content}>
      <Text style={styles.subtle}>
        `theme.fontFamily` restyles the glyphs the keypad draws itself. Switching it is free — the
        native PIN buffer is untouched, so the counter below survives every switch. Only publicKey /
        lengths / keypadType start a new session.
      </Text>

      <Row label="theme.fontFamily">
        {CHOICES.map((c, i) => (
          <Chip
            key={c.label}
            label={c.label}
            font={c.family}
            selected={choiceIndex === i}
            onPress={() => setChoiceIndex(i)}
          />
        ))}
      </Row>

      <Text style={styles.note}>{choice.note}</Text>

      <Row label="keypadType">
        {(['digit', 'full'] as KeypadType[]).map((t) => (
          <Chip
            key={t}
            label={t}
            selected={keypadType === t}
            onPress={() => {
              setKeypadType(t);
              setCount(0);
            }}
          />
        ))}
      </Row>

      <View style={styles.maskRow}>
        <Text style={styles.mask}>{'•'.repeat(count) || ' '}</Text>
        <Text style={styles.count}>
          {count}/{maxLength}
        </Text>
      </View>

      <View style={keypadType === 'full' ? styles.keyboardWrapFull : styles.keyboardWrap}>
        <SecureKeypad
          ref={keypadRef}
          publicKey={DEMO_PUBLIC_KEY}
          keypadType={keypadType}
          minLength={MIN_LENGTH}
          maxLength={maxLength}
          shuffle="mount"
          theme={theme}
          style={styles.keyboard}
          onDigitCountChanged={setCount}
          onComplete={(env) => {
            setEnvelope(env);
            setKeypadError(null);
            setCount(0);
          }}
          onError={(e) => setKeypadError(`${e.phase}: ${e.code}`)}
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

      <View style={styles.card}>
        <Text style={styles.cardTitle}>Applied theme prop</Text>
        <Text style={styles.mono} selectable>
          {JSON.stringify(theme, null, 2)}
        </Text>
      </View>

      <View style={styles.card}>
        <Text style={styles.cardTitle}>Two caveats</Text>
        <Text style={styles.subtle}>
          1. The action glyphs (⌫ ✕ ⇧ ⏎) stay in the system font on purpose. Neither font here has a
          glyph for them — Space Mono and Pacifico both stop at U+00FF-ish — and a missing glyph
          would draw as tofu (□) on an unlabelled key.
        </Text>
        <Text style={styles.subtle}>
          2. For keypadType &quot;full&quot;, the font must cover printable ASCII (0x21~0x7E) or
          some keys show tofu. Both fonts above do; a digits-only display face would not.
        </Text>
      </View>

      {envelope ? (
        <View style={styles.card}>
          <Text style={styles.cardTitle}>Ciphertext envelope (send to server)</Text>
          <Text style={[styles.mono, { color: '#30D158' }]} selectable>
            {envelope}
          </Text>
        </View>
      ) : null}

      {keypadError ? (
        <View style={styles.card}>
          <Text style={styles.cardTitle}>Error</Text>
          <Text style={[styles.mono, { color: '#FF453A' }]}>{keypadError}</Text>
        </View>
      ) : null}
    </ScrollView>
  );
}

function Row({ label, children }: { label: string; children: React.ReactNode }) {
  return (
    <View style={styles.controlRow}>
      <Text style={styles.rowLabel}>{label}</Text>
      <View style={styles.chips}>{children}</View>
    </View>
  );
}

function Chip({
  label,
  font,
  selected,
  onPress,
}: {
  label: string;
  font?: string;
  selected: boolean;
  onPress: () => void;
}) {
  return (
    <Pressable
      onPress={onPress}
      style={({ pressed }) => [
        styles.chip,
        { borderColor: selected ? '#0A84FF' : 'transparent' },
        pressed && styles.chipPressed,
      ]}>
      {/* The chip previews itself in the font it selects, so the label is the
          sample. 'NotInstalled' deliberately renders in the fallback. */}
      <Text
        style={[
          styles.chipText,
          { color: selected ? '#0A84FF' : '#FFFFFF' },
          font ? { fontFamily: font } : null,
        ]}>
        {label}
      </Text>
    </Pressable>
  );
}

const styles = StyleSheet.create({
  content: { padding: 20, gap: 14, paddingBottom: 40 },
  center: { flex: 1, alignItems: 'center', justifyContent: 'center', gap: 10 },
  subtle: { fontSize: 13, color: '#8E8E93', lineHeight: 18 },
  note: { fontSize: 11, color: '#8E8E93', lineHeight: 16, fontStyle: 'italic' },
  controlRow: { gap: 8 },
  rowLabel: { fontSize: 12, color: '#8E8E93', textTransform: 'uppercase', letterSpacing: 0.5 },
  chips: { flexDirection: 'row', flexWrap: 'wrap', gap: 8 },
  chip: {
    backgroundColor: '#1C1C1E',
    borderRadius: 10,
    borderWidth: 1.5,
    paddingHorizontal: 12,
    paddingVertical: 8,
  },
  chipPressed: { opacity: 0.6 },
  chipText: { fontSize: 14 },
  sample: { color: '#FFFFFF', fontSize: 26 },
  maskRow: {
    flexDirection: 'row',
    alignItems: 'center',
    justifyContent: 'space-between',
    backgroundColor: '#1C1C1E',
    borderRadius: 10,
    paddingHorizontal: 14,
    paddingVertical: 10,
  },
  mask: { flex: 1, color: '#FFFFFF', fontSize: 18, letterSpacing: 2 },
  count: { color: '#0A84FF', fontSize: 12, fontWeight: '600' },
  keyboardWrap: { height: 340 },
  keyboardWrapFull: { height: 300 },
  keyboard: { flex: 1 },
  row: { flexDirection: 'row', justifyContent: 'space-around' },
  card: { backgroundColor: '#1C1C1E', borderRadius: 12, padding: 14, gap: 8 },
  cardTitle: { color: '#fff', fontWeight: '600' },
  mono: { color: '#8E8E93', fontFamily: 'Courier', fontSize: 11 },
});
