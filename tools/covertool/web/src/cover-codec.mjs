/* SPDX-License-Identifier: GPL-3.0-only
 * Checked browser cover format implementation.
 * Feature proposal: Damián Parrino (bucanero), tpunix/SAROO PR #374.
 * Format authority: maintained native covertool and its independent fixtures.
 * This module owns imported bytes; callers only receive metadata or copies.
 */
export const MAX_COVERS = 16384, INDEX_BYTES = MAX_COVERS * 32;
export const PALETTE_BYTES = 1024;
const view = bytes => new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
const require = (ok, message) => { if (!ok) throw new Error(message); };
const pause = () => new Promise(resolve => setTimeout(resolve, 0));
export const hex8 = value => value.toString(16).toUpperCase().padStart(8, '0');

function dimensions(width, height) {
  require(Number.isInteger(width) && width >= 1 && width <= 128 &&
          Number.isInteger(height) && height >= 1 && height <= 192,
          'Cover dimensions must be 1–128 × 1–192.');
}

export function serialBytes(text) {
  require(typeof text === 'string', 'Serial must be text.');
  const bytes = [];
  for (let i = 0; i < text.length; i++) {
    let byte = text.charCodeAt(i);
    if (byte === 37) {
      const pair = text.slice(i + 1, i + 3);
      require(/^[0-9a-f]{2}$/i.test(pair), 'Use %HH for an escaped serial byte.');
      byte = parseInt(pair, 16); i += 2;
    }
    require(byte > 0 && byte <= 255, 'Serial bytes must be nonzero; use %HH for raw bytes.');
    bytes.push(byte);
    require(bytes.length <= 12, 'Serial exceeds twelve bytes.');
  }
  require(bytes.length > 0, 'Serial is required.');
  return Uint8Array.from(bytes);
}

export function serialText(bytes) {
  let result = '';
  for (const byte of bytes.subarray(0, 12)) {
    if (!byte) break;
    result += (byte >= 65 && byte <= 90) || (byte >= 48 && byte <= 57) ||
              byte === 45 || byte === 46 || byte === 95
      ? String.fromCharCode(byte) : '%' + byte.toString(16).toUpperCase().padStart(2, '0');
  }
  return result;
}

export function filename(meta, extension = 'bmp') {
  require(extension === 'bmp' || extension === 'png', 'Unsupported artwork extension.');
  return `cover_${serialText(serialBytes(meta.serial))}~${hex8(meta.hash)}.${extension}`;
}

export function parseFilename(name) {
  const match = /^cover_(.+)~([0-9a-f]{8})\.(bmp|png)$/i.exec(name);
  if (!match) return null;
  return { serial: serialText(serialBytes(match[1])), hash: parseInt(match[2], 16) };
}

export function adler32(bytes) {
  let a = 1, b = 0;
  for (let start = 0; start < bytes.length; start += 5552) {
    const end = Math.min(start + 5552, bytes.length);
    for (let i = start; i < end; i++) { a += bytes[i]; b += a; }
    a %= 65521; b %= 65521;
  }
  return ((b << 16) | a) >>> 0;
}

export function discIdentity(bytes) {
  require(bytes instanceof Uint8Array && bytes.length >= 256, 'Disc header is shorter than 256 bytes.');
  const signature = 'SEGA SEGASATURN ';
  const matches = offset => [...signature].every((c, i) => bytes[offset + i] === c.charCodeAt(0));
  const offset = matches(0) ? 0 : 16;
  require(matches(offset), 'No Saturn disc header at offset 0 or 16.');
  let serial = '';
  for (const byte of bytes.subarray(offset + 32, offset + 48)) {
    if (!byte) break;
    serial += String.fromCharCode(byte);
  }
  const version = serial.lastIndexOf('V');
  if (version >= 0) serial = serial.slice(0, version);
  const space = serial.indexOf(' ');
  if (space >= 0) serial = serial.slice(0, space);
  const raw = Uint8Array.from(serial, c => c.charCodeAt(0));
  require(raw.length > 0 && raw.length <= 12, 'Disc serial must contain 1–12 bytes.');
  return { serial: serialText(raw), hash: adler32(bytes.subarray(offset + 48, offset + 112)) };
}

// RGBQUAD's fourth byte is preserved, although rendering uses only B/G/R.
export function decodeBMP8(bytes) {
  require(bytes instanceof Uint8Array && bytes.length >= 54, 'Truncated BMP header.');
  const v = view(bytes), dib = v.getUint32(14, true), offset = v.getUint32(10, true);
  require(v.getUint16(0, true) === 0x4d42 && [40, 52, 56, 108, 124].includes(dib),
          'Expected a Windows indexed BMP.');
  const width = v.getInt32(18, true), signedHeight = v.getInt32(22, true);
  const height = Math.abs(signedHeight), stride = (width + 3) & ~3;
  dimensions(width, height);
  require(v.getUint16(26, true) === 1 && v.getUint16(28, true) === 8 &&
          v.getUint32(30, true) === 0 && [0, 256].includes(v.getUint32(46, true)),
          'Expected uncompressed 8-bit BMP with a complete 256-color palette.');
  const end = offset + stride * height, declared = v.getUint32(2, true);
  require(offset >= 14 + dib + PALETTE_BYTES && end <= bytes.length &&
          declared >= end && declared <= bytes.length, 'Invalid BMP palette or pixel extent.');
  // BI_RGB row geometry establishes the extent. Like the native utility, do
  // not restrict compatibility based on the unused biSizeImage hint.
  const data = new Uint8Array(PALETTE_BYTES + width * height);
  data.set(bytes.subarray(14 + dib, 14 + dib + PALETTE_BYTES));
  for (let y = 0; y < height; y++) {
    const row = offset + (signedHeight < 0 ? y : height - y - 1) * stride;
    data.set(bytes.subarray(row, row + width), PALETTE_BYTES + y * width);
  }
  return { width, height, data };
}

function checkImage(image) {
  dimensions(image.width, image.height);
  require(image.data instanceof Uint8Array &&
          image.data.length === PALETTE_BYTES + image.width * image.height,
          'Image must have a complete palette and exactly width × height indices.');
}

export function encodeBMP8(image) {
  checkImage(image);
  const { width, height, data } = image, stride = (width + 3) & ~3;
  const bytes = new Uint8Array(1078 + stride * height), v = view(bytes);
  v.setUint16(0, 0x4d42, true); v.setUint32(2, bytes.length, true);
  v.setUint32(10, 1078, true); v.setUint32(14, 40, true);
  v.setInt32(18, width, true); v.setInt32(22, height, true);
  v.setUint16(26, 1, true); v.setUint16(28, 8, true);
  v.setUint32(34, stride * height, true);
  v.setUint32(38, 2835, true); v.setUint32(42, 2835, true); v.setUint32(46, 256, true);
  bytes.set(data.subarray(0, PALETTE_BYTES), 54);
  for (let y = 0; y < height; y++) {
    const start = PALETTE_BYTES + (height - y - 1) * width;
    bytes.set(data.subarray(start, start + width), 1078 + y * stride);
  }
  return bytes;
}

function compareBytes(a, b) {
  if (a === b) return 0;
  if (a.length !== b.length) return a.length - b.length;
  for (let i = 0; i < a.length; i++) if (a[i] !== b[i]) return a[i] - b[i];
  return 0;
}
function identity(header) { return serialText(header) + '~' + hex8(view(header).getUint32(12, true)); }
function recordMeta(record) {
  const v = view(record.header);
  return { serial: serialText(record.header), hash: v.getUint32(12, true),
           width: v.getUint16(20, true), height: v.getUint16(22, true) };
}

export class CoverLibrary {
  #records = []; #identities = new Map(); #source = null; #revision = 0;
  get count() { return this.#records.length; }
  get revision() { return this.#revision; }
  get dirty() { return this.#revision !== 0; }

  static async open(file, options = {}) {
    require(file && typeof file.arrayBuffer === 'function' && file.size >= INDEX_BYTES,
            'cover.bin must contain the complete 512 KiB index.');
    const library = new CoverLibrary();
    const checkpoint = library.#checkpoints(options, 0);
    const bytes = new Uint8Array(await file.arrayBuffer()), v = view(bytes);
    require(bytes.length >= INDEX_BYTES, 'Truncated cover index.');
    const shared = new Map();
    for (let i = 0; i < MAX_COVERS && bytes[i * 32]; i++) {
      const base = i * 32, width = v.getUint16(base + 20, true), height = v.getUint16(base + 22, true);
      dimensions(width, height);
      const offset = v.getUint32(base + 16, true), size = PALETTE_BYTES + width * height;
      require(offset >= INDEX_BYTES && offset + size <= bytes.length, `Cover ${i + 1} has an invalid image extent.`);
      const header = bytes.subarray(base, base + 32), key = identity(header);
      require(!library.#identities.has(key), `Duplicate cover identity: ${key}`);
      const extent = offset + ':' + size;
      let payload = shared.get(extent);
      if (!payload) { payload = { bytes: bytes.subarray(offset, offset + size), hash: null }; shared.set(extent, payload); }
      const record = { header, payload };
      library.#records.push(record); library.#identities.set(key, record);
      if (i % 256 === 255) await checkpoint('open', i + 1, MAX_COVERS);
    }
    await checkpoint('open', library.count, library.count, true);
    library.#source = bytes;
    return library;
  }

  #record(index) {
    require(Number.isInteger(index) && index >= 0 && index < this.count, 'Cover index is out of range.');
    return this.#records[index];
  }
  meta(index) { return recordMeta(this.#record(index)); }
  has(metadata) {
    const serial = serialText(serialBytes(metadata.serial));
    require(Number.isInteger(metadata.hash) && metadata.hash >= 0 && metadata.hash <= 0xffffffff,
            'IP hash must be an unsigned 32-bit integer.');
    return this.#identities.has(serial + '~' + hex8(metadata.hash));
  }
  image(index) {
    const record = this.#record(index), { width, height } = recordMeta(record);
    return { width, height, data: record.payload.bytes.slice() };
  }

  // Validate and allocate everything before changing the library or its index.
  set(index, metadata, image) {
    const old = index === this.count ? null : this.#record(index);
    require(old || this.count < MAX_COVERS, 'The cover index is full (16,384 entries).');
    const serial = serialBytes(metadata.serial);
    require(Number.isInteger(metadata.hash) && metadata.hash >= 0 && metadata.hash <= 0xffffffff,
            'IP hash must be an unsigned 32-bit integer.');
    require(image || old, 'A new cover needs image data.');
    if (image) checkImage(image);
    const header = old ? old.header.slice() : new Uint8Array(32), v = view(header);
    if (serialText(header) !== serialText(serial)) { header.fill(0, 0, 12); header.set(serial); }
    v.setUint32(12, metadata.hash, true);
    if (image) { v.setUint16(20, image.width, true); v.setUint16(22, image.height, true); }
    const key = identity(header), existing = this.#identities.get(key);
    require(!existing || existing === old, `Duplicate cover identity: ${key}`);
    const payload = image ? { bytes: image.data.slice(), hash: null } : old.payload;
    if (old && !compareBytes(header, old.header) && !compareBytes(payload.bytes, old.payload.bytes)) return;
    const record = { header, payload };
    // Map insertion can allocate; do it before removing the old identity.
    this.#identities.set(key, record);
    try { this.#records[index] = record; }
    catch (error) {
      if (existing) this.#identities.set(key, existing); else this.#identities.delete(key);
      throw error;
    }
    if (old && key !== identity(old.header)) this.#identities.delete(identity(old.header));
    this.#source = null; this.#revision++;
  }
  remove(index) {
    const record = this.#record(index);
    this.#records.splice(index, 1); this.#identities.delete(identity(record.header));
    this.#source = null; this.#revision++;
  }

  #checkpoints(options, revision) {
    let last = performance.now();
    const check = () => {
      if (options.signal?.aborted) throw new DOMException('Operation canceled.', 'AbortError');
      require(this.#revision === revision, 'Library changed during export; export the current version again.');
    };
    return async (phase, done, total, force = false) => {
      check();
      if (!force && performance.now() - last < 8) return;
      options.onProgress?.({ phase, done, total });
      await pause(); last = performance.now(); check();
    };
  }

  async export(options = {}) {
    const revision = this.#revision, count = this.count, records = this.#records;
    const checkpoint = this.#checkpoints(options, revision);
    await checkpoint('export', 0, count);
    // An untouched round trip preserves padding, inactive records, gaps and trailing bytes.
    if (!revision && this.#source && !options.repack) return new Blob([this.#source]);
    let order = new Uint32Array(count), scratch = new Uint32Array(count);
    const hashes = new Uint32Array(count);
    for (let i = 0; i < count; i++) {
      order[i] = i;
      const payload = records[i].payload;
      if (payload.hash === null) payload.hash = adler32(payload.bytes);
      hashes[i] = payload.hash;
      if (i % 32 === 31) await checkpoint('hash', i + 1, count);
    }
    // A bounded merge sort avoids quadratic chains of same-checksum candidates.
    const compare = (a, b) => hashes[a] - hashes[b] ||
      compareBytes(records[a].payload.bytes, records[b].payload.bytes);
    let comparisons = 0;
    for (let width = 1; width < count; width *= 2) {
      for (let start = 0; start < count; start += width * 2) {
        const middle = Math.min(start + width, count), end = Math.min(middle + width, count);
        let a = start, b = middle;
        for (let out = start; out < end; out++) {
          scratch[out] = a < middle && (b >= end || compare(order[a], order[b]) <= 0) ? order[a++] : order[b++];
          if (++comparisons % 32 === 0) await checkpoint('deduplicate', end, count);
        }
      }
      [order, scratch] = [scratch, order];
    }
    // Equal payloads are stable in original record order. Reuse sort scratch for
    // their first representative, then reuse hashes for the resulting offsets.
    let representative = 0;
    for (let i = 0; i < count; i++) {
      if (!i || compare(order[i - 1], order[i])) representative = order[i];
      scratch[order[i]] = representative;
      if (i % 64 === 63) await checkpoint('group', i + 1, count);
    }
    const index = new Uint8Array(INDEX_BYTES), v = view(index), parts = [index];
    let offset = INDEX_BYTES;
    for (let i = 0; i < count; i++) {
      if (scratch[i] === i) {
        hashes[i] = offset; parts.push(records[i].payload.bytes); offset += records[i].payload.bytes.length;
      } else hashes[i] = hashes[scratch[i]];
      index.set(records[i].header, i * 32); v.setUint32(i * 32 + 16, hashes[i], true);
      if (i % 256 === 255) await checkpoint('pack', i + 1, count);
    }
    await checkpoint('export', count, count, true);
    if (parts.length <= 2) return new Blob(parts, { type: 'application/octet-stream' });
    // Many tiny Blob leaves impose avoidable stream/export overhead. Snapshot
    // bounded batches instead of allocating a second complete output buffer.
    const batch = new Uint8Array(Math.min(1024 * 1024, offset - INDEX_BYTES));
    const blobs = [index]; let used = 0, copied = 0;
    for (let i = 1; i < parts.length; i++) {
      const data = parts[i];
      for (let position = 0; position < data.length;) {
        const size = Math.min(batch.length - used, data.length - position);
        batch.set(data.subarray(position, position + size), used);
        used += size; position += size; copied += size;
        if (used === batch.length) { blobs.push(new Blob([batch])); used = 0; }
      }
      if (i % 32 === 0) await checkpoint('snapshot', copied, offset - INDEX_BYTES);
    }
    if (used) blobs.push(new Blob([batch.subarray(0, used)]));
    await checkpoint('snapshot', copied, copied, true);
    return new Blob(blobs, { type: 'application/octet-stream' });
  }
}
