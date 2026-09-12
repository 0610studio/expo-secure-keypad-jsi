import { SecureKeypad, type SecureKeypadHandle } from 'expo-secure-keypad-jsi';
import { useRef, useState } from 'react';
import { Button, ScrollView, StyleSheet, Text, View } from 'react-native';

import { DEMO_PUBLIC_KEY } from './demoKey';

export default function FullKeyboardDemo() {
  const [count, setCount] = useState(0);
  const [envelope, setEnvelope] = useState<string | null>(null);
  const [error, setError] = useState<string | null>(null);
  const keypadRef = useRef<SecureKeypadHandle>(null);

  return (
    <ScrollView contentContainerStyle={styles.content}>
      <Text style={styles.subtle}>
        Full QWERTY with shift + symbol layers. Characters never enter JS — the Enter key (⏎)
        encrypts and hands you a v2 envelope.
      </Text>

      <View style={styles.maskRow}>
        <Text style={styles.mask}>{'•'.repeat(count) || ' '}</Text>
        <Text style={styles.count}>{count}/64</Text>
      </View>

      <View style={styles.keyboardWrap}>
        <SecureKeypad
          ref={keypadRef}
          publicKey={DEMO_PUBLIC_KEY}
          keypadType="full"
          minLength={4}
          shuffle="mount"
          theme={{ keyColor: '#1C1C1E', keyTextColor: '#FFFFFF', cornerRadius: 8 }}
          style={styles.keyboard}
          onDigitCountChanged={setCount}
          onComplete={(env) => {
            setEnvelope(env);
            setCount(0);
          }}
          onError={(e) => setError(`${e.phase}: ${e.code}`)}
        />
      </View>

      <View style={styles.row}>
        <Button title="Clear" onPress={() => keypadRef.current?.clear()} />
        <Button title="Submit" onPress={() => keypadRef.current?.submit()} />
      </View>

      {envelope ? (
        <View style={styles.card}>
          <Text style={styles.cardTitle}>Ciphertext envelope v2 (send to server)</Text>
          <Text style={styles.mono} selectable>
            {envelope}
          </Text>
          <Text style={styles.subtle}>
            Verify locally: node example/scripts/decrypt.mjs {'<envelope>'}
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
  );
}

const styles = StyleSheet.create({
  content: { padding: 20, gap: 16 },
  subtle: { fontSize: 13, color: '#8E8E93' },
  maskRow: {
    flexDirection: 'row',
    alignItems: 'center',
    justifyContent: 'space-between',
    backgroundColor: '#1C1C1E',
    borderRadius: 10,
    paddingHorizontal: 14,
    paddingVertical: 10,
  },
  mask: { color: '#FFFFFF', fontSize: 18, letterSpacing: 2 },
  count: { color: '#8E8E93', fontSize: 12 },
  keyboardWrap: { height: 300 },
  keyboard: { flex: 1 },
  row: { flexDirection: 'row', justifyContent: 'space-around' },
  card: { backgroundColor: '#1C1C1E', borderRadius: 12, padding: 14, gap: 8 },
  cardTitle: { color: '#fff', fontWeight: '600' },
  mono: { color: '#30D158', fontFamily: 'Courier', fontSize: 11 },
});
