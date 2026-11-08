import assert from 'node:assert/strict';
import { readFile, writeFile } from 'node:fs/promises';
import { join } from 'node:path';
import test from 'node:test';
import vm from 'node:vm';
import { pathToFileURL } from 'node:url';
const { CoverLibrary, INDEX_BYTES, decodeBMP8, encodeBMP8, serialBytes, serialText,
        filename, parseFilename, discIdentity, adler32 } = await import(
  process.env.SAROO_WEB_CODEC ? pathToFileURL(process.env.SAROO_WEB_CODEC) : '../tools/covertool/web/src/cover-codec.mjs');

const root = process.env.SAROO_WEB_FIXTURES;
const fixture = name => readFile(join(root, name));
const load = async name => CoverLibrary.open(new Blob([await fixture(name)]));
const bytes = async library => new Uint8Array(await (await library.export()).arrayBuffer());
const source = (data, width = data.length - 1024, height = 1) => ({ data, width, height });
const v = data => new DataView(data.buffer, data.byteOffset, data.byteLength);

test('independent BMP corpus preserves complete palettes, rows and twelve-byte IDs', async () => {
  const cases = JSON.parse(await fixture('fixtures.json')), library = new CoverLibrary();
  for (const [i, meta] of cases.entries()) {
    const image = decodeBMP8(await fixture(i + '.bmp'));
    assert.deepEqual([image.width, image.height], [meta.width, meta.height]);
    assert.deepEqual(image.data, new Uint8Array(await fixture(i + '.payload')));
    assert.deepEqual(decodeBMP8(encodeBMP8(image)), image);
    library.set(library.count, meta, image);
  }
  const output = await bytes(library);
  assert.deepEqual(output, new Uint8Array(await fixture('expected.bin')));
  await writeFile(join(root, 'browser.bin'), output);
});

test('untouched export preserves inactive headers, padding and trailing bytes; accessors own copies', async () => {
  const library = await load('noncanonical.bin'), original = new Uint8Array(await fixture('noncanonical.bin'));
  assert.deepEqual(await bytes(library), original);
  const image = library.image(0); image.data.fill(0);
  const meta = library.meta(0); meta.serial = 'MUTATED';
  assert.deepEqual(await bytes(library), original);
  library.set(0, library.meta(0));
  assert.equal(library.dirty, false);
  library.set(0, { ...library.meta(0), hash: 43 });
  const changed = await bytes(library);
  assert.deepEqual(changed.subarray(0, 12), original.subarray(0, 12));
  assert.deepEqual(changed.subarray(24, 32), original.subarray(24, 32));
  assert.deepEqual(changed.subarray(INDEX_BYTES), original.subarray(INDEX_BYTES, INDEX_BYTES + 1045));
  assert.equal(v(changed).getUint32(12, true), 43);
});

test('exact deduplication distinguishes palette and pixel checksum collisions and byte counts', async () => {
  const palette = new Uint8Array(1024).fill(90), pixels = new Uint8Array(16).fill(100);
  const variants = [];
  const make = (pal, pix, width = 4, height = 4) => {
    const data = new Uint8Array(pal.length + pix.length); data.set(pal); data.set(pix, 1024);
    variants.push(source(data, width, height));
  };
  make(palette, pixels);
  make(new Uint8Array(1024).fill(91), pixels);
  const pixelCollision = pixels.slice(); pixelCollision.set([101, 98, 101]);
  make(palette, pixelCollision);
  const paletteCollision = palette.slice(); paletteCollision.set([91, 88, 91]);
  make(paletteCollision, pixels);
  make(palette, pixels, 8, 2);
  assert.equal(adler32(variants[0].data), adler32(variants[2].data));
  assert.equal(adler32(variants[0].data), adler32(variants[3].data));
  const library = new CoverLibrary();
  variants.forEach((image, i) => library.set(i, { serial: 'G' + i, hash: i }, image));
  const output = await bytes(library), offsets = variants.map((_, i) => v(output).getUint32(i * 32 + 16, true));
  assert.equal(new Set(offsets.slice(0, 4)).size, 4);
  assert.equal(offsets[0], offsets[4]);
  variants.forEach((image, i) => assert.deepEqual(output.subarray(offsets[i], offsets[i] + image.data.length), image.data));
  // Different payload lengths must remain distinct even when a checksum agrees.
  const differentSizes = new CoverLibrary();
  const short = new Uint8Array(1027), long = new Uint8Array(1028);
  short.set([100, 100, 100], 1024); long.set([0, 100, 99, 101], 1024);
  assert.equal(adler32(short), adler32(long));
  differentSizes.set(0, { serial: 'A', hash: 0 }, source(short, 3));
  differentSizes.set(1, { serial: 'B', hash: 0 }, source(long, 4));
  const sized = await bytes(differentSizes);
  assert.notEqual(v(sized).getUint32(16, true), v(sized).getUint32(48, true));
});

test('full index preserves all 16384 entries and rejects overflow without mutation', async () => {
  const library = await load('full.bin');
  assert.equal(library.count, 16384);
  const before = await bytes(library);
  assert.throws(() => library.set(16384, { serial: 'OVERFLOW', hash: 1 }, library.image(0)), /full/);
  assert.deepEqual(await bytes(library), before);
  library.remove(16383);
  library.set(library.count, { serial: 'LAST-1234567', hash: 99 }, library.image(0));
  const output = await bytes(library);
  assert.equal(output.length, INDEX_BYTES + 1025);
  const opened = await CoverLibrary.open(new Blob([output]));
  assert.equal(opened.count, 16384);
  assert.equal(opened.meta(16383).serial, 'LAST-1234567');
  for (let i = 0; i < 16383; i++) assert.equal(opened.meta(i).hash, i);
});

test('invalid additions and replacements leave the whole library unchanged', async () => {
  const library = await load('expected.bin'), original = await bytes(library);
  const meta = library.meta(0), image = library.image(0);
  for (const operation of [
    () => library.set(0, { ...meta, serial: 'THIRTEEN-BYTE' }),
    () => library.set(0, { ...meta, hash: -1 }),
    () => library.set(0, { ...meta, hash: 2 ** 32 }),
    () => library.set(0, library.meta(1)),
    () => library.set(0, meta, { ...image, width: 0 }),
    () => library.set(0, meta, { ...image, height: 193 }),
    () => library.set(0, meta, { ...image, data: image.data.subarray(1) }),
    () => library.remove(-1), () => library.remove(library.count),
  ]) { assert.throws(operation); assert.deepEqual(await bytes(library), original); }
  const fresh = new CoverLibrary();
  fresh.set(0, meta, image); image.data.fill(0);
  assert.notDeepEqual(fresh.image(0).data, image.data);
});

test('rejected archive geometry and duplicate identities do not create partial libraries', async () => {
  const original = new Uint8Array(await fixture('expected.bin'));
  for (const input of [original.subarray(0, INDEX_BYTES - 1), original.subarray(0, original.length - 1)])
    await assert.rejects(CoverLibrary.open(new Blob([input])));
  for (const corrupt of [
    b => v(b).setUint32(16, INDEX_BYTES - 1, true),
    b => v(b).setUint16(20, 0, true), b => v(b).setUint16(22, 193, true),
    b => b.set(b.subarray(0, 16), 32),
  ]) {
    const input = original.slice(); corrupt(input);
    await assert.rejects(CoverLibrary.open(new Blob([input])));
  }
});

test('BMP declared extents, planes and geometry are checked before decoding', async () => {
  const original = new Uint8Array(await fixture('1.bmp'));
  for (const mutate of [
    b => v(b).setUint32(10, 54, true), b => v(b).setUint32(2, b.length + 1, true),
    b => v(b).setUint32(14, 12, true), b => v(b).setInt32(18, -1, true),
    b => v(b).setInt32(22, 0, true), b => v(b).setUint16(26, 2, true),
    b => v(b).setUint32(30, 1, true), b => v(b).setUint32(46, 2, true),
  ]) { const input = original.slice(); mutate(input); assert.throws(() => decodeBMP8(input)); }
  for (const size of [0, 53, 100, original.length - 1]) assert.throws(() => decodeBMP8(original.subarray(0, size)));
  const hint = original.slice(); v(hint).setUint32(34, 17, true);
  assert.deepEqual(decodeBMP8(hint), decodeBMP8(original));
});

test('serial and filename conversions round-trip every nonzero byte without truncation', () => {
  for (let byte = 1; byte <= 255; byte++) {
    const raw = new Uint8Array(12).fill(byte), serial = serialText(raw);
    assert.deepEqual(serialBytes(serial), raw);
    assert.deepEqual(parseFilename(filename({ serial, hash: 0xffffffff })), { serial, hash: 0xffffffff });
  }
  for (const serial of ['', '%00', '%', '%GG', '1234567890123', '😀']) assert.throws(() => serialBytes(serial));
  assert.equal(parseFilename('cover_GAME~1.bmp'), null);
});

test('disc helper preserves native offset, serial and IP checksum semantics', () => {
  for (const offset of [0, 16]) {
    const data = new Uint8Array(256);
    data.set(Buffer.from('SEGA SEGASATURN '), offset);
    data.set(Buffer.from('ABCDEFGHIJKL V1'), offset + 32);
    data.set(new Uint8Array(64).fill(1), offset + 48);
    assert.deepEqual(discIdentity(data), { serial: 'ABCDEFGHIJKL', hash: 0x08600041 });
  }
  assert.throws(() => discIdentity(new Uint8Array(255)));
  assert.throws(() => discIdentity(new Uint8Array(256)));
});

test('cancellation and a superseding edit cannot publish an old mixed snapshot', async () => {
  const library = await load('full.bin'), original = await bytes(library), controller = new AbortController();
  await assert.rejects(library.export({ repack: true, signal: controller.signal,
    onProgress: () => controller.abort() }), { name: 'AbortError' });
  assert.deepEqual(await bytes(library), original);
  let changed = false;
  await assert.rejects(library.export({ repack: true, onProgress: () => {
    if (!changed) { changed = true; library.set(0, { serial: 'CHANGED', hash: 19 }); }
  } }), /changed during export/);
  assert.equal(library.meta(0).serial, 'CHANGED');
  const current = await CoverLibrary.open(new Blob([await bytes(library)]));
  assert.deepEqual(current.meta(0), library.meta(0));
});

test('empty libraries remain a full empty index and can be reopened', async () => {
  const output = await bytes(new CoverLibrary());
  assert.deepEqual(output, new Uint8Array(INDEX_BYTES));
  assert.equal((await CoverLibrary.open(new Blob([output]))).count, 0);
});

test('bounded Blob batches preserve crossing payloads and cancellation discards the export', async () => {
  const library = await load('batch.bin'), original = new Uint8Array(await fixture('batch.bin'));
  const output = new Uint8Array(await (await library.export({ repack: true })).arrayBuffer());
  assert.deepEqual(output, original);
  const controller = new AbortController();
  await assert.rejects(library.export({ repack: true, signal: controller.signal,
    onProgress: () => controller.abort()
  }), { name: 'AbortError' });
  assert.deepEqual(await bytes(library), original);
});

test('canonical browser export matches the independent unchanged-baseline native corpus', async () => {
  const library = await load('native-expected.bin');
  const output = new Uint8Array(await (await library.export({ repack: true })).arrayBuffer());
  assert.deepEqual(output, new Uint8Array(await fixture('native-expected.bin')));
  await writeFile(join(root, 'native-browser.bin'), output);
});

test('original proposal controls expose palette loss, checksum aliasing, serial truncation and short BMP admission', async () => {
  const code = await readFile(new URL('./fixtures/pr374-cover-codec.js', import.meta.url), 'utf8');
  const original = vm.runInNewContext(code + '\n({buildCoverBin,parseCoverBin,decodeBMP8});',
                                     { Uint8Array, Uint32Array, DataView });
  const safe = { serial: 'SAFE', hash: 42, width: 4, height: 4,
                 palette: new Uint32Array(256), indices: new Uint8Array(16).fill(100) };
  const canonical = original.buildCoverBin([safe]);
  // Positive control: an ordinary image within the proposal's narrow behavior.
  assert.deepEqual(original.buildCoverBin(original.parseCoverBin(canonical)), canonical);
  const twelve = original.buildCoverBin([{ ...safe, serial: 'ABCDEFGHIJKL' }]);
  assert.notEqual(Buffer.from(twelve.subarray(0, 12)).toString('ascii'), 'ABCDEFGHIJKL');
  const reserved = canonical.slice(); reserved[INDEX_BYTES + 3] = 0xA5;
  assert.notDeepEqual(original.buildCoverBin(original.parseCoverBin(reserved)), reserved);
  const palette = safe.palette.slice(); palette[0] = 0x123456;
  const collision = original.buildCoverBin([safe, { ...safe, serial: 'OTHER', palette }]);
  assert.equal(v(collision).getUint32(16, true), v(collision).getUint32(48, true));
  const truncated = new Uint8Array(await fixture('4.bmp')).subarray(0, 1078);
  assert.equal(original.decodeBMP8(truncated).indices.every(byte => byte === 0), true);
  assert.throws(() => decodeBMP8(truncated), /extent/);
  assert.equal(original.parseCoverBin(new Uint8Array(32)).length, 0);
  await assert.rejects(CoverLibrary.open(new Blob([new Uint8Array(32)])), /index/);
});
