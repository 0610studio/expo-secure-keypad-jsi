import { Stack } from 'expo-router';
import { StatusBar } from 'expo-status-bar';
import { GestureHandlerRootView } from 'react-native-gesture-handler';

export default function RootLayout() {
  return (
    <GestureHandlerRootView style={{ flex: 1 }}>
      <StatusBar style="light" />
      <Stack
        screenOptions={{
          headerStyle: { backgroundColor: '#000' },
          headerTintColor: '#fff',
          headerTitleStyle: { fontWeight: '600' },
          contentStyle: { backgroundColor: '#000' },
        }}>
        <Stack.Screen name="index" options={{ title: 'Secure Keypad' }} />
        <Stack.Screen name="inline" options={{ title: 'Inline keypad' }} />
        <Stack.Screen name="bottom-sheet" options={{ title: 'Bottom sheet' }} />
        <Stack.Screen name="full-keyboard" options={{ title: 'Full QWERTY' }} />
        <Stack.Screen name="accessory" options={{ title: 'Accessory row' }} />
        <Stack.Screen name="theme" options={{ title: 'Theming' }} />
        <Stack.Screen name="font" options={{ title: 'Font injection' }} />
      </Stack>
    </GestureHandlerRootView>
  );
}
