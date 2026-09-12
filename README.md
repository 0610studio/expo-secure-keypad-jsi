# expo-secure-keypad-jsi

한국어 문서: [docs/README.ko.md](docs/README.ko.md)

A secure keypad for Expo apps: a numeric PIN pad (`'digit'`) and a QWERTY
keyboard (`'full'`). What the user types never enters the JavaScript runtime.
Touches are handled by a native view, the entered value accumulates in an
`mlock`'d C++ buffer, and only the **ciphertext** — RSA-OAEP against your
server's public key — is handed to JS.

<p align="center">
  <img src="docs/demo.gif" width="260" alt="Shuffled PIN pad running in the example app">
  <br>
  <em>Shuffled PIN pad in the example app (iOS)</em>
</p>

## Features

- The mapping between touch coordinates and key values exists only in native code. JS cannot know the shuffled layout.
- The entered value lives in an `mlock`'d C++ buffer and is wiped with `OPENSSL_cleanse` immediately after use.
- Only RSA-OAEP (SHA-256) ciphertext reaches JS.
- The envelope carries `kid`, `nonce` and `timestamp` so the server can detect key misconfiguration and replays.
- The buffer is wiped whenever the app backgrounds or the view leaves the window, and JS is told the count is 0. This is unconditional — there is no prop to turn it off, so a half-entered PIN never survives a trip to the app switcher.
- On Android, touches delivered through an obscured window (tapjacking) are rejected.

## Non-goals

This library only keeps the entered value out of the JS runtime. Device
integrity, hooking and debugger detection, screen-capture blocking and JS
bundle integrity are app-wide concerns that belong to the host app and the
server — see [Threat model](#threat-model) for what is and is not defended,
and what to use instead.

> **Trust boundary**: the server public key comes from the JS bundle. An
> attacker who can tamper with the bundle can swap the key or draw their own
> input field instead of this keypad. The defence on that path is bundle
> integrity; this library only helps the server notice a wrong key via `kid`.

## Requirements

- Expo SDK 57+, React Native 0.86+, New Architecture only
- A development build (`expo run:*`) or an EAS build, because the module ships
  native code. Expo Go's prebuilt binary does not contain it, and an OTA update
  replaces only JS, so it cannot add a native module either. Once the module is
  in a build, later JS-only changes can still ship over OTA
- iOS 16.4+
- Android 7.0 (API 24)+ — the Expo SDK 57 default; the module adds no floor of
  its own. Ships `arm64-v8a`, `armeabi-v7a`, `x86`, `x86_64`

## Installation

```sh
pnpm expo install expo-secure-keypad-jsi

# A development build. `expo run:*` runs pod install for you.
pnpm expo run:ios        # or: pnpm expo run:android
```

## Usage

```tsx
import { SecureKeypad } from 'expo-secure-keypad-jsi';
import { useState } from 'react';

export function PinScreen({ serverPublicKeyPem }: { serverPublicKeyPem: string }) {
  const [count, setCount] = useState(0);

  return (
    <SecureKeypad
      publicKey={serverPublicKeyPem}       // RSA-2048 or stronger, PEM (SubjectPublicKeyInfo)
      minLength={6}
      maxLength={6}
      shuffle="mount"
      autoSubmit
      onDigitCountChanged={setCount}        // for the masked-dot UI
      onComplete={(ciphertext) => {
        fetch('/api/verify-pin', { method: 'POST', body: ciphertext });
      }}
      onError={(e) => console.warn(e.phase, e.code)}
    />
  );
}
```

### Props

| Prop | Type | Default | Description |
|---|---|---|---|
| `publicKey` | `string` | required | RSA 2048–8192 bit PEM. Weaker keys are rejected |
| `keypadType` | `'digit' \| 'full'` | `'digit'` | `'full'` is the QWERTY keyboard |
| `minLength` / `maxLength` | `number` | `4` / `6` (digit), `4` / `64` (full) | Valid range is 4–12 (digit) and 4–64 (full). An out-of-range **prop value** is clamped into it (`maxLength={20}` on the digit pad behaves as 12), and a `minLength` above `maxLength` is lowered to it. Once the input reaches `maxLength`, further key presses are ignored without an error |
| `shuffle` | `'mount' \| 'perKey' \| 'off'` | `'mount'` | When the layout is reshuffled |
| `autoSubmit` | `boolean` | `true` (digit), `false` (full) | Encrypt automatically once `maxLength` is reached |
| `theme` | `KeypadTheme` | see [Theme](#theme) | Colours, corner radius and text size |
| `accessory` | `ReactNode` | | React content rendered directly above the keypad. The place for a masked-length indicator or a confirm button when the keypad covers the field it fills (bottom sheets) |
| `style` | `StyleProp<ViewStyle>` | see [Sizing](#sizing) | Applied to the keypad, or to the container when `accessory` is set |

### Accessibility

**This keypad does not support screen readers on either platform, and there is
no option to enable it.** The keys are canvas glyphs with no child views, so no
key is ever an accessibility node, and the container is hidden from a11y
services and autofill unconditionally.

The reason is that an Android `AccessibilityService` can read the node tree and
input events of other apps — the standard keylogging route — and an app cannot
tell a real screen reader from a malicious one. If you must serve screen-reader
users, provide a separate entry path.

### Events

| Event | Argument | Description |
|---|---|---|
| `onComplete` | `string` | The ciphertext envelope JSON. Send it to your server |
| `onDigitCountChanged` | `number` | Current input length |
| `onError` | `{ code, phase }` | `phase`: `'arm'` (public key rejected), `'submit'` (encryption failed), `'input'` (Android rejected a touch delivered through an obscured window; `code` is `ERR_OBSCURED_TOUCH`) |

An `'input'` error is informational, not a threat detection. When an overlay app
such as a screen dimmer is active, Android silently drops the touch — use this
to tell the user why the keypad is not responding.

Ref API: `clear()`, `submit()`

Examples: [InlineDemo](example/demos/InlineDemo.tsx),
[FullKeyboardDemo](example/demos/FullKeyboardDemo.tsx),
[AccessoryDemo](example/demos/AccessoryDemo.tsx),
[BottomSheetDemo](example/demos/BottomSheetDemo.tsx)

### QWERTY keyboard (`keypadType: 'full'`)

A password keyboard for upper- and lowercase letters, digits, and the 32 ASCII
specials (``!@#$%^&*()-_=+[]{}\|;:'",.<>?/`~``). Space is not accepted.

- Layout: digit row / `qwertyuiop` / `asdfghjkl` / `⇧ zxcvbnm ⌫` / `[!#1] [✕] [⏎]`.
  `!#1` switches to the symbol layer
- Shuffle: the letter rows keep the standard QWERTY order while the digit row is
  fully shuffled, and each letter row gets one blank dummy key at a random slot.
  `'perKey'` redraws after every keystroke
- Shift: released after one letter, double tap for caps lock. Shift state and
  the active layer exist only in native code
- Submit: passwords are variable-length, so `autoSubmit` defaults to `false`
  and the `⏎` key submits
- No magnified key preview — it would expose the input to a screen recording

### Theme

All colours are `#RRGGBB` or `#RRGGBBAA` (alpha last, interpreted identically on
both platforms; colour names are rejected). Sizes are density-independent (dp on
Android, points on iOS), so the same number looks the same on both.

| Field | Type | Default | Notes |
|---|---|---|---|
| `keyColor` | `string` | `#1C1C1E` | Key background |
| `keyTextColor` | `string` | `#FFFFFF` | Digit and letter glyphs |
| `actionTextColor` | `string` | `#8E8E93` | Action keys (`⌫`, `✕`, `⇧`, `!#1`, `⏎`) |
| `cornerRadius` | `number` | `12` (digit), `8` (full) | Key corner radius |
| `digitTextSize` | `number` | `32` | Base glyph size; despite the name it drives `keypadType: 'full'` too. Each kind of key scales off it — digit keys 1×, action keys 0.7×; on the QWERTY keyboard characters 0.6× and action keys 0.5×. The factors are the same on both platforms |
| `pressedHighlight` | `boolean` | `true` | A held key dims to 70% opacity. `false` disables the press highlight entirely |
| `fontFamily` | `string` | system font | Font for the digit / character glyphs. Any name the platform already resolves: a family registered by `expo-font` (`useFonts` / `loadAsync`), a font bundled at build time, or a system family. An unresolvable name falls back to the system font instead of throwing |

`fontFamily` covers only the glyphs drawn from a value — digits, letters,
symbols. The action glyphs (`⌫`, `✕`, `⇧`, `⏎`) always render in the system
font: most custom fonts have no glyph for them, and a missing glyph would draw
as tofu (□) on an unlabelled key. For `keypadType: 'full'`, pick a font that
covers all of printable ASCII (0x21~0x7E) or some keys will show tofu.
`example/demos/FontDemo.tsx` is a working screen with two `expo-font` families.

There is no `backgroundColor` theme field: the keypad is a regular view, so its
own `style={{ backgroundColor }}` carries it. Left unset (the default) the view
is transparent and the gaps between keys show whatever is behind the keypad.

### Sizing

If you give neither a height, `flex`, nor `aspectRatio`, the wrapper applies
`aspectRatio: 3/4` for `'digit'` and `4/3` for `'full'`. Fabric sizes views
purely from style, so this default comes from the JS wrapper — if you use the
low-level `SecureKeypadJsiView` directly you must size it yourself.

With an `accessory`, `style` applies to the container that wraps the accessory
and the keypad, and the keypad fills whatever space the accessory leaves.
`accessory` is ordinary React content: it never receives the entered value, and
the only thing you can display is the length from `onDigitCountChanged`.

```tsx
<SecureKeypad
  publicKey={pem}
  style={{ height: 380 }}
  accessory={<Text style={styles.mask}>{'●'.repeat(count)}</Text>}
  onDigitCountChanged={setCount}
/>
```

## Server-side decryption

This is the JSON you receive from `onComplete`:

```json
{ "kid": "97349c2e876fcbf2", "nonce": "…base64…", "ct": "…base64…" }
```

- `kid`: public key identifier, so the server can diagnose a key mismatch
- `nonce`: lets you drop replays before decrypting. It must match the nonce inside the plaintext
- `ct`: RSA-OAEP (SHA-256, MGF1-SHA256) ciphertext. 256 bytes for RSA-2048

There is deliberately no algorithm or version field in the envelope. Anything
outside the ciphertext can be forged, so the server pins the algorithm and reads
the version from the decrypted plaintext.

Decrypting `ct` yields a fixed 96-byte plaintext. `'digit'` and `'full'` share
the same structure.

| Offset | Size | Field | Notes |
|---|---|---|---|
| 0 | 2 | magic `"SK"` | Fixed. Confirms the plaintext came from this library |
| 2 | 1 | version = 2 | Version of this 96-byte layout. Reject anything you don't know |
| 3 | 1 | secretLength (4–64) | Actual input length; where to cut `secret` |
| 4 | 64 | secret (ASCII, zero-padded) | The input. Printable ASCII (0x21–0x7E), rest is zero |
| 68 | 16 | nonce | Single-use random. Must match the envelope's `nonce` |
| 84 | 8 | timestamp (unix seconds, big endian) | When it was encrypted; for the freshness check |
| 92 | 4 | reserved | Always zero. Room for the next version |

Node example (full implementation: [decrypt.mjs](example/scripts/decrypt.mjs)):

```js
import crypto from 'node:crypto';

const msg = JSON.parse(body);
const plain = crypto.privateDecrypt(
  { key: privateKeyPem,
    padding: crypto.constants.RSA_PKCS1_OAEP_PADDING,
    oaepHash: 'sha256' },
  Buffer.from(msg.ct, 'base64')
);
if (plain.subarray(0, 2).toString() !== 'SK' || plain[2] !== 2) throw new Error('bad payload');
const secret = plain.subarray(4, 4 + plain[3]).toString('ascii');
const nonce = plain.subarray(68, 84).toString('base64');
const timestamp = Number(plain.readBigUInt64BE(84));
```

What the server must verify:

- magic and version
- `nonce` not seen before (replay defence), and the JSON `nonce` matches the plaintext nonce
- `timestamp` freshness (say ±10 minutes)
- wipe the plaintext secret as soon as it has been checked

`timestamp` comes from the **device clock** (`time()`). Users with a wrong clock
will fail the freshness check, so return that failure as its own code — it lets
the app show "check your device time" — and pick the window with the trade-off
between replay risk and user drop-off in mind. To keep replay defence off the
device clock entirely, retain nonces for longer than the freshness window.

Other languages: Java `RSA/ECB/OAEPWithSHA-256AndMGF1Padding`, Python
`cryptography` `OAEP(mgf=MGF1(SHA256), algorithm=SHA256)`, Go
`rsa.DecryptOAEP(sha256, …)`.

Checking locally:

```sh
node example/scripts/gen-keys.mjs          # generate a demo keypair, paste the public key into example/demos/demoKey.ts
node example/scripts/decrypt.mjs '<JSON>'  # decrypt an envelope shown by the demo app
```

## Architecture

```
touch → native view (Canvas / CoreGraphics) → key value
      → C++ SecureBuffer (mlock + OPENSSL_cleanse)
      → RSA-OAEP → base64 JSON → JS → server
```

| Layer | Android | iOS |
|---|---|---|
| Rendering | Kotlin `View` + Canvas (no TextView) | Swift `UIView` + CoreGraphics (no UILabel) |
| Bridge | JNI → C ABI | Objective-C++ → C ABI |
| Core | shared C++ (`common/`) | same |
| Crypto | OpenSSL 3.6.2 (`openssl-static` prefab 3.6.2-2, statically linked) | OpenSSL 3.6.2 (`OpenSSL-Universal` 3.6.2000, exact pin) |

No function in the native bridge (`esk_c_api.h`) lets an exception escape.
Internal failures become an error string or `ESK_COUNT_ERROR` (-1), and the
platform views never forward a negative count to JS — the buffer was not
modified, so it must not be reported as "0 entered". If the shuffle CSPRNG
refuses, the layout is left unchanged.

### Verification

| Target | Method | How to run |
|---|---|---|
| React wrapper | 4 jest tests over the JS boundary (public key, background wiping, the three native events), tsc, eslint | `pnpm test`, `pnpm lint`, `pnpm exec tsc --noEmit -p tsconfig.json` |
| C++ core | 48 gtest tests in the documented Debug run (two more are Release-only, guarded by `NDEBUG`); both the core and the tests are instrumented with ASan/UBSan (asserted at configure time) | `pnpm test:cpp` |
| Android module | Kotlin compile, JNI/CMake link across 4 ABIs | `./gradlew :expo-secure-keypad-jsi:assembleDebug` in `example/android`, which `pnpm expo prebuild` generates |
| iOS module | Only compilation can be automated | Build the example app |
| Touch, rendering, lifecycle, on-device behaviour | Not automatable here — manual checks on real devices | The example app on a development build |

Provenance, pinning and the fallback plan for the OpenSSL binaries are in
[docs/BUILD.md](docs/BUILD.md).

#### On-device memory measurement

Measured on both platforms by typing a secret on the keypad and then searching
the process's resident read-write memory for it. The builds carried release
optimization; only debugger access was added.

An ordinary JS string, held in React state as a positive control, was found in
46 (iOS) and 47 (Android) places during the same passes — so the `length_=0`
below is a measurement, not a scanner that read nothing.

```
# Android — the buffer page read through /proc/<pid>/mem
SecureBuffer  page_=0x7aede5c000  pageSize_=4096  length_=12  locked_=1

right after entry   71 76 7a 78 6d 6c 70 67 77 6b 68 64 00 00 00 00 ...
                    b'qvzxmlpgwkhd\x00\x00\x00\x00 ...'
after submit        00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ...   length_=0
after backgrounding 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ...   length_=0
```

```
# iOS — the same page read through lldb
SecureBuffer  page_=0x10cb88000  pageSize_=16384  length_=12  locked_=1

right after entry   page content: b'qvzxmlpgwkhd'
after submit        page content: b''                                     length_=0
after backgrounding page content: b''                                     length_=0

# attributes of the buffer page — its own read-write anonymous region, one dirty
# page. From a separate run, so the address differs from the one above
(lldb) memory region 0x102b64000
[0x0000000102b64000-0x0000000102b68000) rw-
Dirty pages: 0x102b64000.
```

## Threat model

| Threat | Defended | How / why |
|---|---|---|
| JS heap dump, Hermes snapshot | Yes | The value never enters the JS VM |
| Bridge / JSI traffic sniffing | Yes | Key mapping and the value exist only in native code |
| Layout Inspector, accessibility tree scraping | Yes | Glyphs are drawn directly; there are no text nodes, and the container is hidden from a11y services unconditionally |
| Userland memory scan | Mostly | cleanse always, mlock when it succeeds. The value lives for microseconds. Transient copies inside libcrypto do exist |
| Swap leakage | Conditional (Android) | mlock + MADV_DONTDUMP. On devices with a small `RLIMIT_MEMLOCK` mlock fails silently and only cleanse remains. iOS compresses RAM instead of swapping |
| Ciphertext replay | Yes (with server support) | The server verifies nonce + timestamp |
| Tapjacking, overlays | Partial (Android) | `filterTouchesWhenObscured` rejects the input |
| Screenshots, screen recording | No | The value is never rendered, but the press highlight reveals positions. Blocking capture is the app's job; failing that, set `pressedHighlight: false` |
| Hooking, debuggers | No | RASP territory |
| Rooted / jailbroken devices | No | RASP and server-side attestation territory |
| Bundle tampering to swap the public key | No | The defence is OTA code signing |
| Past traffic decrypted after a server private-key leak | No | RSA-OAEP has no forward secrecy. Rotate keys |
| OS keyloggers, kernel compromise, hardware attacks | No | Outside the app's authority |

Recommended counterparts for the rows marked "No": Play Integrity / App Attest
and a RASP SDK for device posture, per-screen `FLAG_SECURE` /
`UIScreen.isCaptured` for capture blocking, and expo-updates code signing plus a
controlled build pipeline for bundle integrity.

## Contributing

Bug reports and PRs are welcome.

The app in [`example/`](example) autolinks the module from the repository root,
so a change under `src/`, `common/`, `android/` or `ios/` shows up directly. Run
`pnpm install && pnpm build` at the root, then `pnpm install && pnpm ios` (or
`pnpm android`) in `example/`. Expo Go cannot load a native module — use a
development build. Re-run `pnpm build` after touching `src/`, because the
example imports the compiled output rather than the TypeScript source.

Each test suite and how to run it is in the [Verification](#verification) table.
Anything touching rendering, touch handling, lifecycle or the iOS build has to
be checked by hand on a device, through the demos in `example/app/`.

The 96-byte plaintext is a contract with every server already decrypting these
envelopes. Changing a field means bumping `payload::kVersion` in
`common/include/esk/PinEncryptor.h`, updating the layout table in both READMEs
and in `example/scripts/decrypt.mjs`, and adding a round-trip test in
`tests/cpp/CryptoRoundtripTest.cpp`. The error-code strings in
`common/src/Error.cpp` reach JS verbatim, so renaming one is breaking too.

Keep the security invariants intact: the entered value must never cross into JS,
the C ABI must stay no-throw, and a negative count must never be forwarded to JS
as "0 entered". Say so in the PR description if a change comes near any of them.

Report a security problem privately through
[GitHub security advisories](https://github.com/0610studio/expo-secure-keypad-jsi/security/advisories/new),
not as a public issue. In scope is anything that lets the entered value or the
key mapping escape native code, survive a wipe, or weaken the envelope; the
rows marked "No" in the threat model above are documented non-goals, not
defects. Never attach a real PIN, password, or private key — a demo keypair
from `example/scripts/gen-keys.mjs` demonstrates anything about the wire
format.

## License

MIT — see [LICENSE](LICENSE).
