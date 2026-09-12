import { useRouter } from 'expo-router';
import { Pressable, StyleSheet, Text, View } from 'react-native';

const DEMOS = [
  {
    href: '/inline' as const,
    title: 'Inline keypad',
    description: 'The keypad rendered directly in the screen. autoSubmit encrypts at maxLength.',
  },
  {
    href: '/bottom-sheet' as const,
    title: 'Bottom sheet',
    description:
      'A sign-in form whose password field opens the keypad in a bottom sheet. The field only ever shows a count-derived mask.',
  },
  {
    href: '/full-keyboard' as const,
    title: 'Full QWERTY keyboard',
    description:
      'keypadType="full": shuffled digit row, dummy keys, shift + symbol layers. Enter encrypts.',
  },
  {
    href: '/accessory' as const,
    title: 'Accessory row',
    description:
      'A keypad pinned to the bottom with a mask + Clear / Done row rendered above the keys via the accessory prop.',
  },
  {
    href: '/theme' as const,
    title: 'Theming & dark mode',
    description:
      'Swap light/dark, palettes, cornerRadius, digitTextSize and pressedHighlight live. Restyling never resets the entered digits.',
  },
  {
    href: '/font' as const,
    title: 'Font injection',
    description:
      'theme.fontFamily with two expo-font families (Space Mono, Pacifico), an unresolvable name to show the fallback, and both keypad types.',
  },
];

export default function Home() {
  const router = useRouter();

  return (
    <View style={styles.container}>
      <Text style={styles.subtle}>
        Digits never enter JS — you only ever receive an RSA-OAEP ciphertext.
      </Text>
      {/* Leaving a demo page unmounts its keypad, which erases any entered
          digits (the native buffer is cleared on window detach). Intended
          security behavior, not a bug. */}
      {DEMOS.map((demo) => (
        <Pressable
          key={demo.href}
          onPress={() => router.push(demo.href)}
          style={({ pressed }) => [styles.card, pressed && styles.pressed]}>
          <Text style={styles.cardTitle}>{demo.title}</Text>
          <Text style={styles.cardDescription}>{demo.description}</Text>
        </Pressable>
      ))}
    </View>
  );
}

const styles = StyleSheet.create({
  container: { flex: 1, padding: 20, gap: 14 },
  subtle: { fontSize: 13, color: '#8E8E93', marginBottom: 6 },
  card: { backgroundColor: '#1C1C1E', borderRadius: 12, padding: 16, gap: 6 },
  pressed: { opacity: 0.7 },
  cardTitle: { color: '#fff', fontSize: 17, fontWeight: '600' },
  cardDescription: { color: '#8E8E93', fontSize: 13, lineHeight: 18 },
});
