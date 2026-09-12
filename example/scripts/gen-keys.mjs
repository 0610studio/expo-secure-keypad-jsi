#!/usr/bin/env node
// Regenerates the DEMO RSA-2048 keypair used by the example app. Writes
// keys/public.pem and keys/private.pem, and prints the public key to paste into
// demos/demoKey.ts (DEMO_PUBLIC_KEY).
//
//   node example/scripts/gen-keys.mjs
//
// These keys are for the demo ONLY. Never ship a private key in an app, and
// never reuse these keys for anything real.
import { mkdirSync, writeFileSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
import { dirname, join } from 'node:path';
import crypto from 'node:crypto';

const __dirname = dirname(fileURLToPath(import.meta.url));
const keysDir = join(__dirname, 'keys');
mkdirSync(keysDir, { recursive: true });

const { publicKey, privateKey } = crypto.generateKeyPairSync('rsa', {
  modulusLength: 2048,
  publicKeyEncoding: { type: 'spki', format: 'pem' },
  privateKeyEncoding: { type: 'pkcs8', format: 'pem' },
});

writeFileSync(join(keysDir, 'public.pem'), publicKey);
writeFileSync(join(keysDir, 'private.pem'), privateKey);

console.log('Wrote keys/public.pem and keys/private.pem\n');
console.log('Paste this into example/demos/demoKey.ts as DEMO_PUBLIC_KEY:\n');
console.log(publicKey);
