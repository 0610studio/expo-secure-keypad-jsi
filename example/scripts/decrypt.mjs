#!/usr/bin/env node
// Reference server-side decryptor for the demo. Decrypts an envelope produced
// by <SecureKeypad /> using the DEMO private key and prints the recovered
// secret and metadata. This mirrors what your backend does with its real
// private key.
//
//   node example/scripts/decrypt.mjs '<envelope-json>'
//
// Inner payload layout, v2, 96 bytes (see common/include/esk/PinEncryptor.h):
//   magic "SK"(2) | version(1) | secretLength(1) | secret ascii(64,
//   zero-padded) | nonce(16) | timestamp big-endian seconds(8) | reserved(4)
// Both keypad types emit this layout.
import { readFileSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
import { dirname, join } from 'node:path';
import crypto from 'node:crypto';

const __dirname = dirname(fileURLToPath(import.meta.url));
const privateKeyPem = readFileSync(join(__dirname, 'keys', 'private.pem'), 'utf8');

const arg = process.argv[2];
if (!arg) {
  console.error("usage: node decrypt.mjs '<envelope-json>'");
  process.exit(1);
}

// The envelope carries no algorithm or version field: the server fixes
// RSA-OAEP/SHA-256 itself and reads the version byte from the decrypted payload.
const envelope = JSON.parse(arg);

const ciphertext = Buffer.from(envelope.ct, 'base64');
const payload = crypto.privateDecrypt(
  {
    key: privateKeyPem,
    padding: crypto.constants.RSA_PKCS1_OAEP_PADDING,
    oaepHash: 'sha256',
  },
  ciphertext
);

const magic = payload.subarray(0, 2).toString('ascii');
if (magic !== 'SK') {
  console.error(`bad magic: ${magic}`);
  process.exit(1);
}

const VERSION = 2;
const SIZE = 96;
const OFF_NONCE = 68;
const OFF_TIMESTAMP = 84;

const version = payload[2];
if (version !== VERSION) {
  console.error(`unknown payload version: ${version}`);
  process.exit(1);
}
if (payload.length !== SIZE) {
  console.error(`unexpected payload length: ${payload.length} (expected ${SIZE})`);
  process.exit(1);
}

const secretLength = payload[3];
const secret = payload.subarray(4, 4 + secretLength).toString('ascii');
const nonce = payload.subarray(OFF_NONCE, OFF_NONCE + 16).toString('base64');
const timestamp = Number(payload.readBigUInt64BE(OFF_TIMESTAMP));

console.log('magic       :', magic);
console.log('version     :', version);
console.log('secretLength:', secretLength);
console.log('secret      :', secret);
console.log('nonce       :', nonce);
console.log('kid         :', envelope.kid);
console.log('nonceMatch  :', nonce === envelope.nonce);
console.log('timestamp   :', timestamp, `(${new Date(timestamp * 1000).toISOString()})`);
