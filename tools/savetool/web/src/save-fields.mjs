/* SPDX-License-Identifier: GPL-3.0-only
 * Authored save metadata and SRO/BUP transport for SAROO PR #374 adaptation.
 * Format reference: maintained native savetool; proposal by Damián Parrino.
 */
export const MAX_SAVE_BYTES = 1048576;
const require = (ok, message) => { if (!ok) throw new Error(message); };
export const be16 = (bytes, offset) => (bytes[offset] << 8) | bytes[offset + 1];
export const be32 = (bytes, offset) => ((bytes[offset] << 24) | (bytes[offset + 1] << 16) |
  (bytes[offset + 2] << 8) | bytes[offset + 3]) >>> 0;
export function put16(bytes, offset, value) { bytes[offset] = value >>> 8; bytes[offset + 1] = value; }
export function put32(bytes, offset, value) {
  bytes[offset] = value >>> 24; bytes[offset + 1] = value >>> 16;
  bytes[offset + 2] = value >>> 8; bytes[offset + 3] = value;
}
export function matches(bytes, offset, text) {
  if (offset < 0 || offset + text.length > bytes.length) return false;
  for (let i = 0; i < text.length; i++) if (bytes[offset + i] !== text.charCodeAt(i)) return false;
  return true;
}
export function equalBytes(a, b) {
  if (a.length !== b.length) return false;
  for (let i = 0; i < a.length; i++) if (a[i] !== b[i]) return false;
  return true;
}
export function fixedBytes(value, length, label = 'Field') {
  require(value instanceof Uint8Array && value.length <= length, `${label} exceeds ${length} bytes.`);
  const bytes = new Uint8Array(length); bytes.set(value); return bytes;
}
export function unsigned(value, maximum, label) {
  require(Number.isInteger(value) && value >= 0 && value <= maximum, `${label} is out of range.`);
  return value;
}
export function saveMetadata(value) {
  const rawFilename = fixedBytes(value.rawFilename, 11, 'Filename');
  require(rawFilename[0] !== 0, 'A save needs a nonempty filename.');
  return { rawFilename, rawComment: fixedBytes(value.rawComment ?? new Uint8Array(), 10, 'Comment'),
    language: unsigned(value.language ?? 0, 255, 'Language'), date: unsigned(value.date ?? 0, 0xffffffff, 'Date') };
}

// The reversible raw-byte editor representation includes bytes after NUL. The
// display decoder and deliberate character encoding are separate operations.
export function byteText(bytes) {
  let result = '';
  for (const byte of bytes) result += byte >= 32 && byte <= 126 && byte !== 37
    ? String.fromCharCode(byte) : '%' + byte.toString(16).toUpperCase().padStart(2, '0');
  return result;
}
export function textBytes(text, limit) {
  require(typeof text === 'string', 'Expected byte text.');
  const result = [];
  for (let i = 0; i < text.length; i++) {
    let byte = text.charCodeAt(i);
    if (byte === 37) {
      const pair = text.slice(i + 1, i + 3);
      require(/^[a-f0-9]{2}$/i.test(pair), 'Use %HH for an escaped byte.');
      byte = parseInt(pair, 16); i += 2;
    }
    require(byte <= 255, 'Use encoded bytes or a supported character encoding.');
    result.push(byte); require(result.length <= limit, `Field exceeds ${limit} bytes.`);
  }
  return Uint8Array.from(result);
}
export function saveFilename(raw, extension) {
  require(['SRO', 'BUP', 'XML', 'bin'].includes(extension), 'Unknown save extension.');
  let name = '';
  for (const byte of raw.subarray(0, 11)) {
    if (!byte) break;
    name += (byte >= 65 && byte <= 90) || (byte >= 97 && byte <= 122) || (byte >= 48 && byte <= 57) ||
      [32, 46, 45, 95].includes(byte) ? String.fromCharCode(byte) : '%' + byte.toString(16).toUpperCase().padStart(2, '0');
  }
  require(name.length > 0, 'A save needs a filename.'); return name + '.' + extension;
}

// A 16-entry nibble table keeps the transport checksum small while eliminating
// the proposal's eight branch/shift steps per byte. Exact CRC32/ISO-HDLC.
const CRC_NIBBLES = Uint32Array.from({ length: 16 }, (_, i) => {
  for (let bit = 0; bit < 4; bit++) i = (i >>> 1) ^ ((i & 1) ? 0xedb88320 : 0);
  return i >>> 0;
});
export function crc32(bytes) {
  let value = 0xffffffff;
  for (const byte of bytes) {
    value ^= byte;
    value = (value >>> 4) ^ CRC_NIBBLES[value & 15];
    value = (value >>> 4) ^ CRC_NIBBLES[value & 15];
  }
  return (~value) >>> 0;
}
export function parseRawSave(bytes) {
  require(bytes instanceof Uint8Array && bytes.length >= 64 && bytes.length <= MAX_SAVE_BYTES + 64,
          'Save must contain a complete 64-byte header and at most 1 MiB of data.');
  const bup = matches(bytes, 0, 'Vmem');
  require(bup || matches(bytes, 0, 'SSAVERAW\0'), 'Expected an SRO or BUP save.');
  const size = be32(bytes, bup ? 44 : 28);
  require(size === bytes.length - 64, 'Declared save size does not match the complete payload.');
  if (!bup) {
    const expected = be32(bytes, 12);
    require(!expected || expected === crc32(bytes.subarray(16)), 'SRO checksum mismatch.');
  }
  return { format: bup ? 'BUP' : 'SRO', rawFilename: bytes.slice(16, 27),
    rawComment: bytes.slice(bup ? 28 : 32, bup ? 38 : 42), language: bytes[bup ? 39 : 43],
    date: be32(bytes, bup ? 40 : 44), data: bytes.slice(64) };
}
export function encodeRawSave(value, format = 'SRO') {
  const meta = saveMetadata(value), data = value.data;
  require(data instanceof Uint8Array && data.length <= MAX_SAVE_BYTES, 'Invalid save payload.');
  require(format === 'SRO' || format === 'BUP', 'Choose SRO or BUP.');
  const output = new Uint8Array(data.length + 64), bup = format === 'BUP';
  output.set(new TextEncoder().encode(bup ? 'Vmem' : 'SSAVERAW'));
  output.set(meta.rawFilename, 16); output.set(meta.rawComment, bup ? 28 : 32);
  output[bup ? 39 : 43] = meta.language; put32(output, bup ? 40 : 44, meta.date);
  put32(output, bup ? 44 : 28, data.length); output.set(data, 64);
  if (bup) put32(output, 52, meta.date); else put32(output, 12, crc32(output.subarray(16)));
  return output;
}
