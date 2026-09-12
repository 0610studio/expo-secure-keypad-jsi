import { SecureKeypad, type SecureKeypadHandle } from 'expo-secure-keypad-jsi';
import { useRef, useState } from 'react';
import { Button, ScrollView, StyleSheet, Text, View } from 'react-native';

import { DEMO_PUBLIC_KEY } from './demoKey';

const PIN_LENGTH = 6;

export default function InlineDemo() {
  const [count, setCount] = useState(0);
  const [envelope, setEnvelope] = useState<string | null>(null);
  const [error, setError] = useState<string | null>(null);
  const keypadRef = useRef<SecureKeypadHandle>(null);

  const dots = Array.from({ length: PIN_LENGTH }, (_, i) => i < count);

  return (
    <ScrollView contentContainerStyle={styles.content}>
      <Text style={styles.subtle}>
        Digits never enter JS. You only ever receive an RSA-OAEP ciphertext.
      </Text>

      <View style={styles.dots}>
        {dots.map((filled, i) => (
          <View key={i} style={[styles.dot, filled && styles.dotFilled]} />
        ))}
      </View>

      <View style={styles.keypadWrap}>
        <SecureKeypad
          ref={keypadRef}
          publicKey={DEMO_PUBLIC_KEY}
          minLength={PIN_LENGTH}
          maxLength={PIN_LENGTH}
          shuffle="mount"
          theme={{ keyColor: '#1C1C1E', keyTextColor: '#FFFFFF', cornerRadius: 14 }}
          style={styles.keypad}
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
      </View>

      {envelope ? (
        <View style={styles.card}>
          <Text style={styles.cardTitle}>Ciphertext envelope (send to server)</Text>
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
  dots: { flexDirection: 'row', gap: 14, justifyContent: 'center', marginVertical: 12 },
  dot: {
    width: 16,
    height: 16,
    borderRadius: 8,
    borderWidth: 1.5,
    borderColor: '#8E8E93',
  },
  dotFilled: { backgroundColor: '#0A84FF', borderColor: '#0A84FF' },
  keypadWrap: { height: 360 },
  keypad: { flex: 1 },
  row: { flexDirection: 'row', justifyContent: 'space-around' },
  card: { backgroundColor: '#1C1C1E', borderRadius: 12, padding: 14, gap: 8 },
  cardTitle: { color: '#fff', fontWeight: '600' },
  mono: { color: '#30D158', fontFamily: 'Courier', fontSize: 11 },
});
