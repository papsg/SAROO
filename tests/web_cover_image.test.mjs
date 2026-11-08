/* SPDX-License-Identifier: GPL-3.0-only */
import test from 'node:test';
import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import vm from 'node:vm';
import { quantizeRGBA, rgbaPixels, readIndexedBMP, ImageSource } from '../tools/covertool/web/src/cover-image.mjs';
import { CoverLibrary, INDEX_BYTES } from '../tools/covertool/web/src/cover-codec.mjs';
import { createCoverWorker } from '../tools/covertool/web/src/cover-worker.mjs';

function bmp(width = 3, height = 2, offset = 1078) {
  const stride = (width + 3) & ~3, bytes = new Uint8Array(offset + stride * height), v = new DataView(bytes.buffer);
  v.setUint16(0, 0x4d42, true); v.setUint32(2, bytes.length, true); v.setUint32(10, offset, true);
  v.setUint32(14, 40, true); v.setInt32(18, width, true); v.setInt32(22, height, true);
  v.setUint16(26, 1, true); v.setUint16(28, 8, true); v.setUint32(46, 256, true);
  for (let i = 0; i < 1024; i++) bytes[54 + i] = (i * 43) & 255;
  for (let y = 0; y < height; y++) for (let x = 0; x < width; x++) bytes[offset + y * stride + x] = x + y * width;
  return bytes;
}
function driver(onProgress = null) {
  const replies = [], waiters = new Map(); let id = 0, stamp = { epoch: 0, revision: 0 };
  const receive = createCoverWorker(value => {
    replies.push(value); if (value.progress) { onProgress?.(value, receive); return; }
    if (value.state) stamp = value.state;
    const waiter = waiters.get(value.id); if (!waiter) return;
    waiters.delete(value.id);
    if (value.error) { const error = new Error(value.error); error.name = value.name; waiter.reject(error); }
    else waiter.resolve(value.value);
  });
  const start = (op, data = {}) => {
    const job = ++id;
    const promise = new Promise((resolve, reject) => waiters.set(job, { resolve, reject }));
    void receive({ id: job, op, stamp: { ...stamp }, ...data });
    return { id: job, promise };
  };
  return { start, call: (op, data) => start(op, data).promise, receive, replies, stamp: () => ({ ...stamp }) };
}

test('all 256 colors remain exact across the maximum pixel count', async () => {
  const rgba = new Uint8ClampedArray(128 * 192 * 4);
  for (let i = 0; i < rgba.length / 4; i++) rgba.set([i % 256, 255 - i % 256, (i * 17) % 256, 255], i * 4);
  const image = await quantizeRGBA(rgba, 128, 192);
  assert.deepEqual(rgbaPixels(image), rgba); assert.equal(image.data.length, 25600);
});
test('alpha becomes white and identical flattened colors share a palette entry', async () => {
  const rgba = new Uint8ClampedArray([0, 0, 0, 0, 255, 255, 255, 255, 0, 0, 0, 128, 127, 127, 127, 255]);
  const image = await quantizeRGBA(rgba, 4, 1);
  assert.deepEqual([...rgbaPixels(image)], [255, 255, 255, 255, 255, 255, 255, 255, 127, 127, 127, 255, 127, 127, 127, 255]);
  assert.equal(image.data[1024], image.data[1025]); assert.equal(image.data[1026], image.data[1027]);
});
test('maximum unique-color input is deterministic with useful color fidelity', async () => {
  const rgba = new Uint8ClampedArray(128 * 192 * 4);
  for (let y = 0; y < 192; y++) for (let x = 0; x < 128; x++) rgba.set([x * 2, Math.floor(y * 255 / 191), (x * 7 + y * 3) & 255, 255], (y * 128 + x) * 4);
  const image = await quantizeRGBA(rgba, 128, 192), again = await quantizeRGBA(rgba, 128, 192);
  assert.deepEqual(image.data, again.data);
  const output = rgbaPixels(image); let squared = 0;
  for (let i = 0; i < rgba.length; i++) if (i % 4 !== 3) squared += (rgba[i] - output[i]) ** 2;
  assert.ok(Math.sqrt(squared / (128 * 192 * 3)) < 16, 'Independent RGB RMS quality floor');
  assert.equal(new Set(image.data.subarray(1024)).size, 256);
});
test('invalid dimensions and canceled conversion reject before publication', async () => {
  await assert.rejects(quantizeRGBA(new Uint8Array(0), 0, 1));
  await assert.rejects(quantizeRGBA(new Uint8Array(4), 1, 2));
  const abort = new AbortController(); abort.abort();
  await assert.rejects(quantizeRGBA(new Uint8Array(4), 1, 1, { signal: abort.signal }), { name: 'AbortError' });
});
test('indexed BMP keeps raw palette and top-down pixels, avoiding a large gap read', async () => {
  const base = bmp(), header = base.slice(0, 1078), v = new DataView(header.buffer), gap = 100000000;
  v.setUint32(10, gap, true); v.setUint32(2, gap + 8, true);
  const reads = [], file = { size: gap + 8, slice(start, end) {
    reads.push([start, end]); return new Blob([start === gap ? base.slice(1078) : header.slice(start, end)]);
  } };
  const image = await readIndexedBMP(file);
  assert.deepEqual(image.data.subarray(0, 1024), base.subarray(54, 1078));
  assert.deepEqual([...image.data.subarray(1024)], [3, 4, 5, 0, 1, 2]);
  assert.deepEqual(reads, [[0, 54], [0, 1078], [gap, gap + 8]]);
  const source = new ImageSource(image), copy = await source.convert({ width: 3, height: 2 });
  assert.deepEqual(copy.data, image.data); copy.data[0] ^= 255; assert.notEqual(copy.data[0], image.data[0]);
  source.close(); await assert.rejects(source.convert({ width: 3, height: 2 }), /closed/);
});
test('short indexed BMPs fail and generic formats defer to the image decoder', async () => {
  await assert.rejects(readIndexedBMP(new Blob([bmp().slice(0, -1)])), /extent/);
  await assert.rejects(readIndexedBMP(new Blob([new Uint8Array([66, 77])])), /header/);
  assert.equal(await readIndexedBMP(new Blob(['valid source of another format'])), null);
});
test('worker paginates full capacity and invalidates filtered indices after mutation', async () => {
  const bytes = new Uint8Array(INDEX_BYTES + 1025), v = new DataView(bytes.buffer);
  for (let i = 0; i < 16384; i++) {
    bytes.set(new TextEncoder().encode(`GAME${i}`), i * 32); v.setUint32(i * 32 + 12, i, true);
    v.setUint32(i * 32 + 16, INDEX_BYTES, true); v.setUint16(i * 32 + 20, 1, true); v.setUint16(i * 32 + 22, 1, true);
  }
  const d = driver(); await d.call('open', { file: new Blob([bytes]) });
  const page = await d.call('list', { page: 682 }); assert.equal(page.total, 16384); assert.equal(page.items.length, 16);
  assert.equal(page.items.at(-1).serial, 'GAME16383');
  const filtered = await d.call('list', { query: 'GAME16383' }); assert.equal(filtered.items[0].index, 16383);
  await d.call('remove', { index: 0 });
  const updated = await d.call('list', { query: 'GAME16383' }); assert.equal(updated.items[0].index, 16382);
});
test('worker rejects stale workspace edits and preserves the old library on failed open', async () => {
  const d = driver(), old = d.stamp(); await d.call('new');
  await assert.rejects(d.call('remove', { stamp: old, index: 0 }), /changed/);
  await d.call('editorStart', { token: 1 });
  await d.call('editorLoad', { token: 1, file: new File([bmp()], 'cover_GAME~00000001.bmp'), size: { width: 128, height: 192 }, fit: 'contain' });
  await d.call('editorSave', { token: 1, meta: { serial: 'GAME', hash: 1 } });
  const before = await (await d.call('export')).blob.arrayBuffer();
  await assert.rejects(d.call('open', { file: new Blob(['short']) }), /index/);
  assert.deepEqual(await (await d.call('export')).blob.arrayBuffer(), before);
});
test('closing a dialog during image read discards the late completion', async () => {
  const d = driver(); await d.call('editorStart', { token: 1 });
  let release;
  const file = { name: 'cover_GAME~00000001.bmp', size: bmp().length, slice(start, end) {
    return { arrayBuffer: () => new Promise(resolve => { release = () => resolve(bmp().slice(start, end).buffer); }) };
  } };
  const job = d.start('editorLoad', { token: 1, file, size: { width: 128, height: 192 } });
  const rejected = assert.rejects(job.promise, { name: 'AbortError' });
  await d.receive({ op: 'editorClose', token: 1 }); release();
  // Header completion may enter the two bounded reads before the final cancel check.
  for (let i = 0; i < 4; i++) { await new Promise(resolve => setTimeout(resolve, 0)); release(); }
  await rejected; assert.equal(d.stamp().count, 0);
  await d.call('editorStart', { token: 2 });
  await assert.rejects(d.call('editorSave', { token: 1, meta: { serial: 'OLD', hash: 1 } }), /active/);
});
test('batch import reports duplicate and missing identities without replacing artwork', async () => {
  const d = driver(), one = new File([bmp()], 'cover_GAME~00000001.bmp');
  const result = await d.call('batch', { files: [one, one, new File([bmp()], 'unidentified.bmp')], size: { width: 128, height: 192 }, fit: 'contain' });
  assert.equal(result.added, 1); assert.equal(result.skipped.length, 2); assert.equal(d.stamp().count, 1);
  assert.match(result.skipped[0].reason, /Duplicate/); assert.match(result.skipped[1].reason, /serial/);
  const bytes = await (await d.call('export')).blob.arrayBuffer(), library = await CoverLibrary.open(new Blob([bytes]));
  assert.deepEqual(library.image(0).data, (await readIndexedBMP(one)).data);
  assert.equal(library.has({ serial: 'GAME', hash: 1 }), true);
  assert.equal(library.has({ serial: 'GAME', hash: 2 }), false);
  assert.throws(() => library.has({ serial: 'GAME', hash: -1 }));
  const neverRead = { name: one.name, slice() { throw new Error('Duplicate image must not be decoded'); } };
  const repeated = await d.call('batch', { files: [neverRead] });
  assert.match(repeated.skipped[0].reason, /Duplicate/);
});
test('batch cancellation reports the exact completed prefix and retains it', async () => {
  const d = driver((value, receive) => {
    if (value.progress.phase === 'import' && value.progress.done === 1) void receive({ op: 'cancel', target: value.id });
  });
  const files = ['FIRST', 'SECOND', 'THIRD'].map(name => new File([bmp()], `cover_${name}~00000001.bmp`));
  const result = await d.call('batch', { files, size: { width: 128, height: 192 }, fit: 'contain' });
  assert.equal(result.canceled, true); assert.equal(result.added, 1); assert.equal(result.processed, 1);
  assert.equal(d.stamp().count, 1); assert.equal((await d.call('list')).items[0].serial, 'FIRST');
});

test('complete palettes and indices match the original quantizer on six independent workloads', async () => {
  const source = await readFile(new URL('./fixtures/pr374-cover-codec.js', import.meta.url), 'utf8');
  const original = vm.runInNewContext(source + '\nquantize;');
  for (const [w, h, kind] of [[1,1,0], [128,192,0], [128,192,1], [128,192,2], [128,192,3], [128,192,4]]) {
    const rgba = new Uint8ClampedArray(w*h*4); let random = 0x12345678;
    for (let i = 0; i < w*h; i++) {
      random = (Math.imul(random,1664525)+1013904223)>>>0;
      const x=i%w, y=Math.floor(i/w);
      rgba.set(kind===0 ? [3,47,89,255] : kind===1 ? [i%256,255-i%256,(i*17)%256,255] :
               kind===2 ? [x*2,Math.floor(y*255/191),(x*7+y*3)&255,255] :
               [random&255,(random>>>8)&255,(random>>>16)&255,kind===4 ? random>>>24 : 255],i*4);
    }
    const result = original(rgba,w,h), expected = new Uint8Array(1024+w*h), view = new DataView(expected.buffer);
    for (let i=0;i<256;i++) view.setUint32(i*4,result.palette[i],true);
    expected.set(result.indices,1024);
    assert.deepEqual((await quantizeRGBA(rgba,w,h)).data, expected, `${w}x${h} workload ${kind}`);
  }
});
