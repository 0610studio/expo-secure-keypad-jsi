import { SecureKeypad, type SecureKeypadHandle } from 'expo-secure-keypad-jsi';
import { useRef, useState } from 'react';
import { Button, ScrollView, StyleSheet, Text, View } from 'react-native';

import { DEMO_PUBLIC_KEY } from './demoKey';

const MIN_LENGTH = 4;
const MAX_LENGTH = 8;

/**
 * The keypad is pinned to the bottom like a system keyboard, so it can cover
 * the field it fills. `accessory` puts a mask and the actions directly above
 * the keys, where the user can always see them. The accessory only ever knows
 * the count — there is no API that could hand it the digits.
 */
export default function AccessoryDemo() {
  const [count, setCount] = useState(0);
  const [ciphertext, setCiphertext] = useState<string | null>(null);
  const [error, setError] = useState<string | null>(null);
  const keypadRef = useRef<SecureKeypadHandle>(null);

  return (
    <View style={styles.fill}>
      <ScrollView contentContainerStyle={styles.content}>
        <Text style={styles.subtle}>
          The keypad sits at the bottom and may cover this form. The row above the keys mirrors the
          PIN field's mask and holds Clear / Done, so nothing important is hidden behind the keypad.
        </Text>

        <Text style={styles.label}>PIN</Text>
        <View style={styles.input}>
          {count > 0 ? (
            <Text style={styles.maskText}>{'●'.repeat(count)}</Text>
          ) : (
            <Text style={styles.placeholder}>
              {MIN_LENGTH}~{MAX_LENGTH} digits
            </Text>
          )}
        </View>

        {ciphertext ? (
          <View style={styles.card}>
            <Text style={styles.cardTitle}>Ciphertext (send to server)</Text>
            <Text style={styles.mono} selectable>
              {ciphertext}
            </Text>
          </View>
        ) : null}

        {error ? (
          <View style={styles.card}>
            <Text style={styles.cardTitle}>Error</Text>
            <Text style={styles.mono}>{error}</Text>
          </View>
        ) : null}
      </ScrollView>

      <SecureKeypad
        ref={keypadRef}
        publicKey={DEMO_PUBLIC_KEY}
        minLength={MIN_LENGTH}
        maxLength={MAX_LENGTH}
        shuffle="mount"
        autoSubmit={false}
        theme={{ keyColor: '#2C2C2E', keyTextColor: '#FFFFFF', cornerRadius: 12 }}
        // `style` sizes the container (accessory + keypad); the keypad fills
        // what the accessory leaves.
        style={styles.keypad}
        accessory={
          <View style={styles.accessory}>
            <Text style={styles.accessoryMask}>{count > 0 ? '●'.repeat(count) : 'Enter PIN'}</Text>
            <View style={styles.accessoryButtons}>
              <Button title="Clear" color="#FF453A" onPress={() => keypadRef.current?.clear()} />
              <Button
                title="Done"
                disabled={count < MIN_LENGTH}
                onPress={() => keypadRef.current?.submit()}
              />
            </View>
          </View>
        }
        onDigitCountChanged={setCount}
        onComplete={(ct) => {
          setCiphertext(ct);
          setError(null);
          setCount(0);
        }}
        onError={(e) => setError(`${e.phase}: ${e.code}`)}
      />
    </View>
  );
}

const styles = StyleSheet.create({
  fill: { flex: 1 },
  content: { padding: 20, gap: 10 },
  subtle: { fontSize: 13, color: '#8E8E93', lineHeight: 18 },
  label: { fontSize: 13, color: '#8E8E93', marginTop: 10 },
  input: {
    backgroundColor: '#1C1C1E',
    borderRadius: 10,
    paddingHorizontal: 14,
    height: 48,
    justifyContent: 'center',
  },
  maskText: { color: '#fff', fontSize: 16, letterSpacing: 4 },
  placeholder: { color: '#48484A', fontSize: 16 },
  card: { backgroundColor: '#1C1C1E', borderRadius: 12, padding: 14, gap: 8, marginTop: 14 },
  cardTitle: { color: '#fff', fontWeight: '600' },
  mono: { color: '#30D158', fontFamily: 'Courier', fontSize: 11 },
  keypad: {
    height: 420,
    backgroundColor: '#1C1C1E',
    borderTopLeftRadius: 16,
    borderTopRightRadius: 16,
    padding: 12,
    paddingBottom: 32,
  },
  accessory: {
    flexDirection: 'row',
    alignItems: 'center',
    justifyContent: 'space-between',
    paddingHorizontal: 4,
    marginBottom: 12,
  },
  accessoryMask: { color: '#fff', fontSize: 18, letterSpacing: 4 },
  accessoryButtons: { flexDirection: 'row', gap: 4 },
});
