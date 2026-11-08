/* SPDX-License-Identifier: GPL-3.0-only
 * Authored image preparation for the SAROO browser cover editor.
 * Feature origin: Damián Parrino (bucanero), tpunix/SAROO PR #374.
 * Indexed artwork is preserved; true-color conversion uses weighted median cut.
 */
import { decodeBMP8, PALETTE_BYTES } from './cover-codec.mjs';

const require = (ok, message) => { if (!ok) throw new Error(message); };
function dimensions(width, height) {
  require(Number.isInteger(width) && width > 0 && width <= 128 &&
          Number.isInteger(height) && height > 0 && height <= 192,
          'Cover dimensions must be 1–128 × 1–192.');
}
function canceled(signal) { signal?.throwIfAborted(); }

// Read only the header, palette and rows, even when a valid BMP has a large gap.
// Other formats/large source images are left to the browser's image decoder.
export async function readIndexedBMP(file) {
  const header = new Uint8Array(await file.slice(0, 54).arrayBuffer());
  if (header.length < 2 || header[0] !== 66 || header[1] !== 77) return null;
  require(header.length === 54, 'Truncated BMP header.');
  const v = new DataView(header.buffer), dib = v.getUint32(14, true);
  if (![40, 52, 56, 108, 124].includes(dib) || v.getUint16(28, true) !== 8 ||
      v.getUint32(30, true) !== 0) return null;
  const width = v.getInt32(18, true), height = Math.abs(v.getInt32(22, true));
  if (width > 128 || height > 192) return null;
  dimensions(width, height);
  const prefix = 14 + dib + PALETTE_BYTES, offset = v.getUint32(10, true);
  const length = ((width + 3) & ~3) * height, declared = v.getUint32(2, true);
  require(offset >= prefix && offset + length <= declared && declared <= file.size,
          'Invalid BMP palette or pixel extent.');
  const compact = new Uint8Array(prefix + length);
  const head = new Uint8Array(await file.slice(0, prefix).arrayBuffer());
  const rows = new Uint8Array(await file.slice(offset, offset + length).arrayBuffer());
  require(head.length === prefix && rows.length === length, 'Short BMP read.');
  compact.set(head); compact.set(rows, prefix);
  const cv = new DataView(compact.buffer);
  cv.setUint32(2, compact.length, true); cv.setUint32(10, prefix, true);
  return decodeBMP8(compact);
}

export function rgbaPixels(image) {
  const { width, height, data } = image;
  dimensions(width, height);
  require(data instanceof Uint8Array && data.length === PALETTE_BYTES + width * height,
          'Incomplete indexed image.');
  const rgba = new Uint8ClampedArray(width * height * 4);
  for (let i = 0; i < width * height; i++) {
    const p = data[PALETTE_BYTES + i] * 4, q = i * 4;
    rgba[q] = data[p + 2]; rgba[q + 1] = data[p + 1]; rgba[q + 2] = data[p]; rgba[q + 3] = 255;
  }
  return rgba;
}

// All scratch counts derive from the enforced 24,576-pixel maximum. Uint16
// unique-color indices retain every color; no histogram precision is discarded.
export async function quantizeRGBA(rgba, width, height, { signal } = {}) {
  dimensions(width, height); canceled(signal);
  const count = width * height;
  require((rgba instanceof Uint8Array || rgba instanceof Uint8ClampedArray) && rgba.length === count * 4,
          'Expected exactly width × height RGBA pixels.');
  const colors = new Uint32Array(count), populations = new Uint32Array(count);
  const pixels = new Uint16Array(count), unique = new Map();
  for (let i = 0; i < count; i++) {
    const p = i * 4, a = rgba[p + 3], white = 255 * (255 - a);
    const r = Math.floor((rgba[p] * a + white + 127) / 255);
    const g = Math.floor((rgba[p + 1] * a + white + 127) / 255);
    const b = Math.floor((rgba[p + 2] * a + white + 127) / 255);
    const rgb = (r << 16) | (g << 8) | b;
    let index = unique.get(rgb);
    if (index === undefined) { index = unique.size; unique.set(rgb, index); colors[index] = rgb; }
    populations[index]++; pixels[i] = index;
  }
  const size = unique.size;
  unique.clear(); // Only the histogram arrays are needed during box splitting.
  const order = new Uint16Array(size), paletteIndex = new Uint8Array(size);
  for (let i = 0; i < size; i++) order[i] = i;
  const channel = (index, shift) => (colors[index] >>> shift) & 255;
  function box(start, end) {
    let loR = 255, loG = 255, loB = 255, hiR = 0, hiG = 0, hiB = 0, population = 0;
    for (let i = start; i < end; i++) {
      const id = order[i], rgb = colors[id], r = rgb >>> 16, g = (rgb >>> 8) & 255, b = rgb & 255;
      loR = Math.min(loR, r); hiR = Math.max(hiR, r);
      loG = Math.min(loG, g); hiG = Math.max(hiG, g);
      loB = Math.min(loB, b); hiB = Math.max(hiB, b); population += populations[id];
    }
    const dr = hiR - loR, dg = hiG - loG, db = hiB - loB;
    // Keep the proposal's channel and box tie order, so avoiding its repeated
    // range scans does not change the chosen palette or image approximation.
    const range = Math.max(dr, dg, db), shift = dg >= dr && dg >= db ? 8 : db >= dr && db >= dg ? 0 : 16;
    return { start, end, population, shift, score: end - start > 1 ? range * population : -1 };
  }
  // At <=256 colors, preserve each flattened RGB exactly without sorting.
  const boxes = size <= 256 ? [] : [box(0, size)];
  let last = performance.now();
  while (boxes.length && boxes.length < 256) {
    let best = 0;
    for (let i = 1; i < boxes.length; i++) if (boxes[i].score > boxes[best].score) best = i;
    const current = boxes[best];
    if (current.score < 0) break;
    const { start, end, shift, population } = current;
    order.subarray(start, end).sort((a, b) => channel(a, shift) - channel(b, shift));
    let split = start, weight = 0;
    do { weight += populations[order[split++]]; } while (split < end - 1 && weight < population / 2);
    boxes.splice(best, 1, box(start, split), box(split, end));
    if (performance.now() - last >= 8) {
      await new Promise(resolve => setTimeout(resolve, 0)); canceled(signal); last = performance.now();
    }
  }
  const data = new Uint8Array(PALETTE_BYTES + count);
  function palette(index, r, g, b) {
    data[index * 4] = b; data[index * 4 + 1] = g; data[index * 4 + 2] = r;
  }
  if (!boxes.length) {
    for (let i = 0; i < size; i++) {
      palette(i, colors[i] >>> 16, (colors[i] >>> 8) & 255, colors[i] & 255); paletteIndex[i] = i;
    }
  } else {
    boxes.forEach(({ start, end, population }, index) => {
      let r = 0, g = 0, b = 0;
      for (let i = start; i < end; i++) {
        const id = order[i], n = populations[id], rgb = colors[id];
        r += (rgb >>> 16) * n; g += ((rgb >>> 8) & 255) * n; b += (rgb & 255) * n;
        paletteIndex[id] = index;
      }
      palette(index, Math.round(r / population), Math.round(g / population), Math.round(b / population));
    });
  }
  for (let i = 0; i < count; i++) data[PALETTE_BYTES + i] = paletteIndex[pixels[i]];
  canceled(signal);
  return { width, height, data };
}

export function imageCanvas(image) {
  const canvas = new OffscreenCanvas(image.width, image.height);
  canvas.getContext('2d').putImageData(new ImageData(rgbaPixels(image), image.width, image.height), 0, 0);
  return canvas;
}

export class ImageSource {
  #indexed; #bitmap; #closed = false;
  constructor(indexed, bitmap = null) { this.#indexed = indexed; this.#bitmap = bitmap; }
  get width() { return this.#indexed?.width ?? this.#bitmap.width; }
  get height() { return this.#indexed?.height ?? this.#bitmap.height; }
  get indexed() { return !!this.#indexed; }
  static async open(file, { signal } = {}) {
    canceled(signal);
    const indexed = await readIndexedBMP(file); canceled(signal);
    if (indexed) return new ImageSource(indexed);
    const bitmap = await createImageBitmap(file);
    if (signal?.aborted) { bitmap.close(); canceled(signal); }
    return new ImageSource(null, bitmap);
  }
  close() { this.#bitmap?.close(); this.#bitmap = null; this.#indexed = null; this.#closed = true; }
  async convert({ width, height, fit = 'contain', signal } = {}) {
    require(!this.#closed, 'Image source was closed.'); dimensions(width, height); canceled(signal);
    require(['contain', 'cover', 'stretch'].includes(fit), 'Unknown image fitting mode.');
    if (this.#indexed && width === this.width && height === this.height)
      return { width, height, data: this.#indexed.data.slice() };
    const source = this.#bitmap ?? imageCanvas(this.#indexed);
    const canvas = new OffscreenCanvas(width, height), ctx = canvas.getContext('2d', { willReadFrequently: true });
    ctx.fillStyle = '#fff'; ctx.fillRect(0, 0, width, height);
    ctx.imageSmoothingEnabled = true; ctx.imageSmoothingQuality = 'high';
    let w = width, h = height;
    if (fit !== 'stretch') {
      const scale = (fit === 'contain' ? Math.min : Math.max)(width / source.width, height / source.height);
      w = source.width * scale; h = source.height * scale;
    }
    ctx.drawImage(source, (width - w) / 2, (height - h) / 2, w, h);
    return quantizeRGBA(ctx.getImageData(0, 0, width, height).data, width, height, { signal });
  }
}
