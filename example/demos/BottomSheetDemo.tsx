import BottomSheet, { BottomSheetBackdrop, BottomSheetView } from '@gorhom/bottom-sheet';
import type { BottomSheetBackdropProps } from '@gorhom/bottom-sheet';
import { SecureKeypad, type SecureKeypadHandle } from 'expo-secure-keypad-jsi';
import { useCallback, useRef, useState } from 'react';
import { Button, Pressable, ScrollView, StyleSheet, Text, View } from 'react-native';

import { DEMO_PUBLIC_KEY } from './demoKey';

const MIN_LENGTH = 4;
const MAX_LENGTH = 12;

/**
 * A regular form whose password field is filled via the secure keypad in a
 * bottom sheet. The field never holds the real value — there is no API that
 * could return it. It renders a mask derived from onDigitCountChanged only.
 * Do NOT "improve" this by echoing digits into the input; that would put the
 * secret in the JS heap and defeat the point of the library.
 */
export default function BottomSheetDemo() {
  const [count, setCount] = useState(0);
  const [envelope, setEnvelope] = useState<string | null>(null);
  const [error, setError] = useState<string | null>(null);

  const sheetRef = useRef<BottomSheet>(null);
  const keypadRef = useRef<SecureKeypadHandle>(null);

  const renderBackdrop = useCallback(
    (props: BottomSheetBackdropProps) => (
      <BottomSheetBackdrop {...props} appearsOnIndex={0} disappearsOnIndex={-1} />
    ),
    []
  );

  return (
    <View style={styles.fill}>
      <ScrollView contentContainerStyle={styles.content}>
        <Text style={styles.label}>Password</Text>
        <Pressable
          style={({ pressed }) => [styles.input, styles.passwordField, pressed && styles.pressed]}
          onPress={() => sheetRef.current?.snapToIndex(0)}>
          {count > 0 ? (
            <Text style={styles.maskText}>{'●'.repeat(count)}</Text>
          ) : (
            <Text style={styles.placeholder}>Tap to enter with secure keypad</Text>
          )}
        </Pressable>

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

      {/*
        The sheet content must STAY MOUNTED while the sheet is closed. When the
        keypad view detaches from the window it erases its native digit buffer
        (this is unconditional), so conditionally rendering the sheet —
        or using BottomSheetModal, which unmounts on dismiss — would wipe the
        entry every time the sheet closes. With a persistent BottomSheet at
        index -1 the digits survive close/reopen until Clear or Confirm.
      */}
      <BottomSheet
        ref={sheetRef}
        index={-1}
        enablePanDownToClose
        // The keypad registers a key on ACTION_UP. With content panning on,
        // a slight finger move over a key lets the sheet's pan gesture steal
        // the touch (ACTION_CANCEL) and the tap is dropped. Drag on the
        // handle still closes the sheet.
        enableContentPanningGesture={false}
        backdropComponent={renderBackdrop}
        backgroundStyle={styles.sheetBackground}
        handleIndicatorStyle={styles.sheetHandle}>
        <BottomSheetView style={styles.sheetContent}>
          <SecureKeypad
            ref={keypadRef}
            publicKey={DEMO_PUBLIC_KEY}
            minLength={MIN_LENGTH}
            maxLength={MAX_LENGTH}
            shuffle="mount"
            // autoSubmit would encrypt the moment maxLength is reached; a
            // variable-length password needs an explicit Confirm instead.
            autoSubmit={false}
            theme={{ keyColor: '#2C2C2E', keyTextColor: '#FFFFFF', cornerRadius: 12 }}
            // The sheet covers the password field, so mirror its mask right
            // above the keypad. Only the count is known here — never the value.
            accessory={
              <View style={styles.accessory}>
                <Text style={styles.sheetTitle}>Enter password</Text>
                <View style={[styles.input, styles.passwordField, styles.accessoryField]}>
                  {count > 0 ? (
                    <Text style={styles.maskText}>{'●'.repeat(count)}</Text>
                  ) : (
                    <Text style={styles.placeholder}>
                      {MIN_LENGTH}~{MAX_LENGTH} digits
                    </Text>
                  )}
                </View>
              </View>
            }
            // The keypad has no intrinsic size (Fabric sizes Expo views from
            // style alone), and its default aspectRatio can feed back into the
            // sheet's dynamic sizing — inside a sheet, give the container a
            // fixed height; the keypad takes what the accessory leaves.
            style={styles.keypad}
            onDigitCountChanged={setCount}
            onComplete={(env) => {
              setEnvelope(env);
              setError(null);
              setCount(0);
              sheetRef.current?.close();
            }}
            onError={(e) => setError(`${e.phase}: ${e.code}`)}
          />
          <View style={styles.sheetButtons}>
            <Button title="Clear" color="#FF453A" onPress={() => keypadRef.current?.clear()} />
            <Button
              title="Confirm"
              disabled={count < MIN_LENGTH}
              onPress={() => keypadRef.current?.submit()}
            />
          </View>
        </BottomSheetView>
      </BottomSheet>
    </View>
  );
}

const styles = StyleSheet.create({
  fill: { flex: 1 },
  content: { padding: 20, gap: 10 },
  subtle: { fontSize: 13, color: '#8E8E93' },
  label: { fontSize: 13, color: '#8E8E93', marginTop: 10 },
  input: {
    backgroundColor: '#1C1C1E',
    borderRadius: 10,
    paddingHorizontal: 14,
    height: 48,
    color: '#fff',
    fontSize: 16,
  },
  passwordField: { justifyContent: 'center' },
  pressed: { opacity: 0.7 },
  maskText: { color: '#fff', fontSize: 16, letterSpacing: 4 },
  placeholder: { color: '#48484A', fontSize: 16 },
  card: { backgroundColor: '#1C1C1E', borderRadius: 12, padding: 14, gap: 8, marginTop: 14 },
  cardTitle: { color: '#fff', fontWeight: '600' },
  mono: { color: '#30D158', fontFamily: 'Courier', fontSize: 11 },
  sheetBackground: { backgroundColor: '#1C1C1E' },
  sheetHandle: { backgroundColor: '#48484A' },
  sheetContent: { padding: 16, paddingBottom: 32, gap: 12 },
  sheetTitle: { color: '#fff', fontSize: 17, fontWeight: '600', textAlign: 'center' },
  keypad: { height: 380 },
  accessory: { gap: 12, marginBottom: 12 },
  accessoryField: { backgroundColor: '#2C2C2E' },
  sheetButtons: { flexDirection: 'row', justifyContent: 'space-around' },
});
