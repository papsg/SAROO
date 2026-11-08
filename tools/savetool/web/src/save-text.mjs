/* SPDX-License-Identifier: GPL-3.0-only
 * Lossless field editing for SAROO PR #374. Original raw fields stay authoritative.
 * Shift-JIS decoding uses the browser's Encoding Standard implementation.
 */
import { fixedBytes, unsigned, textBytes, byteText } from './save-fields.mjs';
const require = (ok, message) => { if (!ok) throw new Error(message); };
const EPOCH = Date.UTC(1980, 0, 1), MINUTE = 60000;
const pause = () => new Promise(resolve => setTimeout(resolve, 0));

export function dateFields(value) {
  unsigned(value, 0xffffffff, 'Date');
  const date = new Date(EPOCH + value * MINUTE);
  return { year: date.getUTCFullYear(), month: date.getUTCMonth() + 1, day: date.getUTCDate(),
    hour: date.getUTCHours(), minute: date.getUTCMinutes() };
}
export function dateMinutes(value) {
  const limits = { year: 10146, month: 12, day: 31, hour: 23, minute: 59 };
  for (const [key, limit] of Object.entries(limits)) unsigned(value[key], limit, key);
  require(value.year >= 1980 && value.month && value.day, 'Date must be on or after 1980-01-01.');
  const stamp = Date.UTC(value.year, value.month - 1, value.day, value.hour, value.minute);
  const result = unsigned((stamp - EPOCH) / MINUTE, 0xffffffff, 'Date');
  const actual = dateFields(result);
  require(Object.keys(limits).every(key => actual[key] === value[key]), 'Enter a valid Gregorian calendar date.');
  return result;
}
export function dateText(value) {
  if (!value) return '';
  const date = dateFields(value), pad = number => String(number).padStart(2, '0');
  return `${String(date.year).padStart(4, '0')}-${pad(date.month)}-${pad(date.day)}T${pad(date.hour)}:${pad(date.minute)}`;
}
export function parseDateText(text) {
  if (text === '') return 0;
  const match = /^(\d{4,5})-(\d\d)-(\d\d)[T ](\d\d):(\d\d)$/.exec(text);
  require(match, 'Use YYYY-MM-DD HH:MM, or leave the date unset.');
  return dateMinutes(Object.fromEntries(['year', 'month', 'day', 'hour', 'minute'].map((key, i) => [key, Number(match[i + 1])])));
}

function visibleBytes(bytes) {
  const end = bytes.indexOf(0); return end < 0 ? bytes : bytes.subarray(0, end);
}
function scalarText(text) {
  require(typeof text === 'string', 'Expected text.');
  for (const character of text) {
    const point = character.codePointAt(0);
    require(point < 0xd800 || point > 0xdfff, 'Text contains an incomplete Unicode character.');
  }
}

export class SaveText {
  #shift = null; #utf8 = new TextDecoder('utf-8', { fatal: true }); #reverse = null;
  constructor() { try { this.#shift = new TextDecoder('shift-jis', { fatal: true }); } catch { /* Raw bytes still work. */ } }
  get shiftJISSupported() { return this.#shift !== null; }
  get reverseBytes() { return this.#reverse?.buffer.byteLength ?? 0; }
  display(bytes, encoding = 'shift-jis') {
    if (encoding === 'bytes') return { text: byteText(bytes), valid: true };
    require(encoding === 'shift-jis' || encoding === 'utf-8', 'Unknown field encoding.');
    const decoder = encoding === 'shift-jis' ? this.#shift : this.#utf8;
    if (!decoder) return { text: byteText(bytes), valid: false };
    try { return { text: decoder.decode(visibleBytes(bytes)), valid: true }; }
    catch { return { text: byteText(bytes), valid: false }; }
  }
  async #prepare(signal) {
    if (this.#reverse) return;
    require(this.#shift, 'Shift-JIS is unavailable in this browser; use raw bytes.');
    // At most 60 lead bytes × 188 trail bytes. Each admitted mapping is one BMP
    // scalar and two encoded bytes; enforce that bound before packing into u32.
    const pairs = new Uint32Array(60 * 188), row = new Uint8Array(188 * 3);
    const decoder = new TextDecoder('shift-jis');
    for (let i = 0; i < 188; i++) row[i * 3 + 1] = i + (i < 63 ? 0x40 : 0x41);
    let count = 0, checkpoint = performance.now();
    for (let lead = 0x81; lead <= 0xfc; lead++) {
      if (lead === 0xa0) lead = 0xe0;
      for (let i = 0; i < 188; i++) row[i * 3] = lead;
      // NUL separates each trial pair, including invalid ones. A valid pair
      // yields exactly one BMP scalar before its delimiter. No exception object
      // or decoder call is needed for each of the 11,280 candidate pairs.
      const decoded = decoder.decode(row); let start = 0;
      for (let i = 0; i < 188; i++) {
        const end = decoded.indexOf('\0', start), point = decoded.charCodeAt(start);
        require(end >= start, 'Shift-JIS decoder lost a trial delimiter.');
        if (end - start === 1 && point !== 0xfffd) {
          require(point < 0xd800 || point > 0xdfff, 'Unexpected non-BMP Shift-JIS mapping.');
          pairs[count++] = point * 65536 + lead * 256 + row[i * 3 + 1];
        } else require(!(point >= 0xd800 && point <= 0xdbff), 'Unexpected non-BMP Shift-JIS mapping.');
        start = end + 1;
      }
      signal?.throwIfAborted();
      if (performance.now() - checkpoint >= 8) { await pause(); signal?.throwIfAborted(); checkpoint = performance.now(); }
    }
    const table = pairs.subarray(0, count); table.sort(); signal?.throwIfAborted(); this.#reverse = table;
  }
  async encode(text, maximum, { signal } = {}) {
    scalarText(text); unsigned(maximum, 16, 'Field capacity');
    const output = new Uint8Array(maximum); let used = 0;
    for (const character of text) {
      let point = character.codePointAt(0), code;
      if (point <= 0x80) code = point;
      else if (point >= 0xff61 && point <= 0xff9f) code = point - 0xff61 + 0xa1;
      else {
        await this.#prepare(signal);
        let low = 0, high = this.#reverse.length;
        while (low < high) { const middle = (low + high) >>> 1;
          if ((this.#reverse[middle] >>> 16) < point) low = middle + 1; else high = middle; }
        require(low < this.#reverse.length && (this.#reverse[low] >>> 16) === point,
                `Character ${character} cannot be represented in Shift-JIS.`);
        code = this.#reverse[low] & 65535;
      }
      const count = code > 255 ? 2 : 1;
      require(used + count <= maximum, `Text exceeds the field's ${maximum}-byte capacity.`);
      if (count === 2) output[used++] = code >>> 8;
      output[used++] = code;
    }
    signal?.throwIfAborted(); return output.subarray(0, used);
  }
  async edit(original, text, encoding, maximum, { signal } = {}) {
    signal?.throwIfAborted();
    const display = this.display(original, encoding);
    // Do not regenerate even an equivalent encoding: raw padding and duplicate
    // Shift-JIS spellings are meaningful bytes owned by the original save.
    if (text === display.text) return fixedBytes(original, maximum);
    let encoded;
    if (encoding === 'bytes') encoded = textBytes(text, maximum);
    else {
      scalarText(text);
      require(!text.includes('\0'), 'Use raw-byte mode to deliberately enter NUL bytes.');
      encoded = encoding === 'utf-8' ? new TextEncoder().encode(text) : await this.encode(text, maximum, { signal });
    }
    return fixedBytes(encoded, maximum);
  }
}
