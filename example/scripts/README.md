# Demo key scripts

- `gen-keys.mjs` writes an RSA-2048 keypair to `keys/` (git-ignored) and prints
  the public key to paste into `../demos/demoKey.ts`.
- `decrypt.mjs '<envelope JSON>'` decrypts an envelope shown by the demo app
  with `keys/private.pem` and prints the 96-byte payload fields.

The private key never leaves this folder. `demoKey.ts` holds only the public
half; a real app fetches its public key from the server or ships it in the
bundle protected by OTA code signing.
