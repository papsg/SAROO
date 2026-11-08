/* Authored checks use independent Python encoders and the maintained native CLI. */
import assert from 'node:assert/strict';
import { test } from 'node:test';
import { readFile, writeFile } from 'node:fs/promises';
import { createHash } from 'node:crypto';
import { pathToFileURL } from 'node:url';
import vm from 'node:vm';
import * as fields from '../tools/savetool/web/src/save-fields.mjs';
const { SaveArchive } = await import(process.env.SAROO_WEB_SAVE_CODEC
  ? pathToFileURL(process.env.SAROO_WEB_SAVE_CODEC) : '../tools/savetool/web/src/save-codec.mjs');
const { put16, put32, encodeRawSave, parseRawSave, crc32, byteText, textBytes, saveFilename } = fields;
const root = process.env.SAROO_WEB_SAVE_FIXTURES;
const fixture = name => readFile(`${root}/${name}`).then(bytes => new Uint8Array(bytes));
const open = async name => SaveArchive.open(new Blob([await fixture(name)]));
const bytes = async archive => new Uint8Array(await (await archive.export()).arrayBuffer());
const hash = data => createHash('sha256').update(data).digest('hex');
const name = text => new TextEncoder().encode(text);
const sequence = (size, seed = 0) => Uint8Array.from({ length: size }, (_, i) => (i * 7 + seed) % 251);
const manifest = JSON.parse(await readFile(`${root}/fixtures.json`, 'utf8'));
const interchange = [];
async function emit(archive, volume, file) {
  await writeFile(`${root}/${file}.bin`, await bytes(archive));
  const output = { file: `${file}.bin`, type: archive.type, volume, saves: [] };
  for (let index = 0; index < archive.stats(volume).saves; index++) {
    const save = archive.save(volume, index), item = { prefix: `${file}-${index}` };
    for (const format of ['SRO', 'BUP']) {
      await writeFile(`${root}/${item.prefix}.${format}`, encodeRawSave(save, format));
      item[format] = saveFilename(save.rawFilename, format);
    }
    output.saves.push(item);
  }
  interchange.push(output);
  await writeFile(`${root}/interchange.json`, JSON.stringify(interchange));
}

test('all supported formats, boundary lengths and fragmented Saturn tables read independent data', async () => {
  for (const item of manifest.reads) {
    const archive = await open(item.file), volume = archive.volume(0).id;
    assert.equal(archive.type, item.type); assert.equal(archive.dirty, false);
    const save = archive.save(volume, 0);
    assert.equal(save.size, item.size); assert.equal(hash(save.data), item.payload_sha256, item.file);
    assert.equal(byteText(save.rawFilename), 'SAVE_A00001'); assert.equal(byteText(save.rawComment), 'Generated!');
    assert.equal(save.language, 1); assert.equal(save.date, 0x01020304);
    for (const offset of new Set([0, Math.min(item.size, 27), Math.min(item.size, 64), Math.max(0, item.size - 1)])) {
      const count = Math.min(133, item.size - offset);
      assert.deepEqual(archive.read(volume, 0, offset, count), save.data.slice(offset, offset + count), item.file);
    }
    assert.deepEqual(await bytes(archive), await fixture(item.file), item.file);
    if (item.size === 29549) await emit(archive, volume, `saturn-${Number(item.odd)}-native`);
  }
});

test('damaged inputs are rejected without reformatting; independent healthy slots remain readable', async () => {
  for (const item of manifest.rejects) {
    const input = await fixture(item.file), blob = new Blob([input]);
    if (item.type === 'saroo') {
      const archive = await SaveArchive.open(blob);
      for (const operation of [() => archive.stats(1), () => archive.save(1, 0),
        () => archive.update(1, 0, { date: 1 }), () => archive.remove(1, 0)]) assert.throws(operation, undefined, item.file);
      assert.equal(archive.stats(3).saves, 0);
      assert.deepEqual(await bytes(archive), input);
    } else await assert.rejects(SaveArchive.open(blob), undefined, item.file);
    assert.deepEqual(new Uint8Array(await blob.arrayBuffer()), input);
  }
});

test('outer archive extents, missing slots and read ranges are enforced', async () => {
  const archive = await open('saroo-empty.bin'), input = await bytes(archive);
  for (const size of [0, 32767, 65535, input.length - 1]) await assert.rejects(SaveArchive.open(new Blob([input.subarray(0, size)])));
  input[32] = 65; await assert.rejects(SaveArchive.open(new Blob([input])));
  for (const id of [0, 2, -1, 4096, NaN]) assert.throws(() => archive.stats(id));
  assert.throws(() => new SaveArchive());
  const mems = await open('mems-961-0.bin');
  for (const [offset, length] of [[-1, 1], [962, 0], [960, 2], [0, NaN], [0, -1]]) assert.throws(() => mems.read(0, 0, offset, length));
});

test('impossible input extents fail before any complete-file allocation', async () => {
  let reads = 0;
  class CountedBlob extends Blob { async arrayBuffer() { reads++; throw new Error('Unexpected complete read'); } }
  const unit = new Blob([new Uint8Array(65536)]), directory = (await fixture('saroo-empty.bin')).subarray(0, 65536);
  const tooManySlots = new CountedBlob([directory, ...Array(4096).fill(unit)]);
  await assert.rejects(SaveArchive.open(tooManySlots), /4,095 games/);
  const unknown = new CountedBlob([unit, ...Array(4096).fill(unit)]);
  await assert.rejects(SaveArchive.open(unknown), /signature and extent/);
  assert.equal(reads, 0);
});

for (const type of ['saroo', 'mems']) {
  const volume = type === 'saroo' ? 1 : 0;
  test(`${type}: same-size update preserves every unrelated byte, raw padding, stale counters and neighbors`, async () => {
    const archive = await open(`${type}-preserve.bin`), original = await bytes(archive);
    const meta = archive.metadata(volume, 0), stored = structuredClone(meta);
    meta.rawFilename.fill(88); meta.rawComment.fill(89); archive.read(volume, 0).fill(90);
    assert.deepEqual(archive.metadata(volume, 0), stored);
    const revision = archive.revision, noOp = archive.update(volume, 0, {});
    assert.equal(noOp.changed, false); assert.equal(noOp.writtenBytes, 0); assert(noOp.stagedBytes > 0);
    assert.equal(archive.revision, revision); assert.deepEqual(await bytes(archive), original);
    const payload = Uint8Array.from({ length: 129 }, (_, i) => (i * 5 + 9) % 256);
    const edited = archive.update(volume, 0, { date: 0xffffffff, data: payload }); payload.fill(0);
    assert.equal(edited.changed, true); assert.equal(archive.stats(volume).storedFree, 0);
    assert.deepEqual(await bytes(archive), await fixture(`${type}-preserve-expected.bin`));
    await emit(archive, volume, `${type}-preserve-native`);
  });

  test(`${type}: replacement resize, metadata and payload commit together across all allocation boundaries`, async () => {
    const archive = await open(`${type}-preserve.bin`), neighbor = archive.save(volume, 1);
    const sizes = type === 'saroo' ? [0, 1, 128, 129, 4097, 507 * 128, 32] : [0, 1, 960, 961, 1024, 1025, 479 * 1024, 32];
    for (const size of sizes) {
      const data = sequence(size, size % 251);
      archive.update(volume, 0, { rawFilename: name('RENAMED0001'), rawComment: name('Edited'), language: 255, date: size, data });
      const save = archive.save(volume, 0); assert.deepEqual(save.data, data);
      assert.equal(byteText(save.rawFilename), 'RENAMED0001'); assert.equal(save.date, size); assert.equal(save.language, 255);
      assert.deepEqual(archive.save(volume, 1), neighbor);
      const reopened = await SaveArchive.open(new Blob([await bytes(archive)]));
      assert.deepEqual(reopened.save(volume, 0), save); assert.deepEqual(reopened.save(volume, 1), neighbor);
      const complete = await bytes(archive);
      if (type === 'saroo') assert.deepEqual(complete.subarray(2 * 65536), (await fixture(`${type}-preserve.bin`)).subarray(2 * 65536));
      else assert.deepEqual(complete.subarray(8388608), (await fixture(`${type}-preserve.bin`)).subarray(8388608));
    }
    await emit(archive, volume, `${type}-resize-native`);
  });

  test(`${type}: exhausted allocation and invalid combined edits preserve the complete archive`, async () => {
    const archive = await open(`${type}-full.bin`), original = await bytes(archive), revision = archive.revision;
    const limit = type === 'saroo' ? 65280 : 490496;
    for (const operation of [
      () => archive.update(volume, 0, { rawFilename: name('CHANGED'), data: sequence(type === 'saroo' ? 257 : 961) }),
      () => archive.update(volume, null, { rawFilename: name('NEW'), data: new Uint8Array() }),
      () => archive.update(volume, 0, { data: new Uint8Array(limit + 1) }),
      () => archive.update(volume, 0, { rawFilename: name('TWELVE-BYTES') }),
      () => archive.update(volume, 0, { rawComment: new Uint8Array(11) }),
      () => archive.update(volume, 0, { language: 256 }),
      () => archive.update(volume, 0, { date: -1 }),
      () => archive.update(volume, 0, { data: null }),
      () => archive.update(volume, 0, { rawFilename: new Uint8Array(11) }),
    ]) { assert.throws(operation); assert.equal(archive.revision, revision); assert.deepEqual(await bytes(archive), original); }
    // Own payload blocks may be reused even when no globally free blocks remain.
    archive.update(volume, 0, { data: sequence(type === 'saroo' ? 128 : 32) });
    assert.equal(archive.save(volume, 0).size, type === 'saroo' ? 128 : 32);
  });

  test(`${type}: full directory, delete/unlink, append and empty saves preserve indexing and capacity`, async () => {
    const archive = await open(`${type}-directory-full.bin`), count = type === 'saroo' ? 511 : 448;
    assert.equal(archive.stats(volume).saves, count);
    const original = await bytes(archive);
    assert.throws(() => archive.update(volume, null, { rawFilename: name('NEW'), data: new Uint8Array() }));
    assert.deepEqual(await bytes(archive), original);
    for (const index of [count - 1, 0, 12]) archive.remove(volume, index);
    assert.equal(archive.stats(volume).saves, count - 3);
    for (let i = 0; i < 3; i++) archive.update(volume, null, { rawFilename: name(`NEW${i}`), data: new Uint8Array() });
    assert.equal(archive.stats(volume).saves, count);
    const reopened = await SaveArchive.open(new Blob([await bytes(archive)]));
    assert.deepEqual(reopened.stats(volume), archive.stats(volume));
    for (let i = 0; i < count; i++) assert.equal(reopened.read(volume, i).length, 0);
  });

  test(`${type}: creating an archive and importing maximum payload retains the full format capacity`, async () => {
    const archive = SaveArchive.create(type), data = sequence(type === 'saroo' ? 65280 : 490496, 27);
    if (type === 'saroo') archive.createSlot(name('GAME'));
    assert(archive.dirty); archive.update(volume, null, { rawFilename: name('MAXIMUM0001'), data });
    assert.deepEqual(archive.read(volume, 0), data);
    await emit(archive, volume, `${type}-maximum-native`);
  });
}

test('slot creation explicitly reuses holes, preserves damaged named slots, appends and recognizes all 16 ID bytes', async () => {
  const archive = await open('saroo-preserve.bin'), original = await bytes(archive);
  assert.deepEqual(archive.createSlot(name('GAME')), { id: 1, created: false, stagedBytes: 0 });
  assert.deepEqual(await bytes(archive), original);
  assert.equal(archive.createSlot(name('0123456789ABCDEF')).id, 2);
  assert.equal(archive.bytes, original.length); assert.equal(archive.stats(2).saves, 0);
  assert.equal(byteText(archive.volume(1).rawName), '0123456789ABCDEF');
  assert.equal(archive.createSlot(name('0123456789ABCDEG')).id, 4);
  assert.equal(archive.bytes, original.length + 65536);
  assert.equal(archive.createSlot(name('0123456789ABCDEG')).created, false);
  const complete = await bytes(archive); assert.deepEqual(complete.subarray(65536, 2 * 65536), original.subarray(65536, 2 * 65536));
  assert.deepEqual(complete.subarray(3 * 65536, 4 * 65536), original.subarray(3 * 65536));
  assert.throws(() => archive.createSlot(name('0123456789ABCDEFG')));
  const reopened = await SaveArchive.open(new Blob([complete]));
  assert.equal(reopened.volumeCount, 4); assert.equal(reopened.stats(4).saves, 0);
});

test('Saturn backups are read-only through every mutation entry point', async () => {
  const archive = await open('saturn-1-1.bin'), original = await bytes(archive);
  assert.equal(archive.readOnly, true);
  for (const operation of [() => archive.update(0, 0, { date: 1 }), () => archive.remove(0, 0),
    () => archive.createSlot(name('GAME')), () => archive.update(0, null, { rawFilename: name('NEW'), data: new Uint8Array() })]) assert.throws(operation);
  assert.deepEqual(await bytes(archive), original);
});

test('all 510 Saturn header blocks remain addressable with bounded table workspace', async () => {
  const archive = await open('saturn-directory-full.bin');
  assert.equal(archive.stats(0).saves, 510); assert.equal(archive.stats(0).free, 0);
  const names = new Set();
  for (let i = 0; i < 510; i++) { names.add(byteText(archive.metadata(0, i).rawFilename)); assert.equal(archive.read(0, i).length, 0); }
  assert.equal(names.size, 510); assert.deepEqual(await bytes(archive), await fixture('saturn-directory-full.bin'));
});

test('SRO/BUP complete extents, CRC, maximum input payload and raw field bytes match independent envelopes', async () => {
  assert.equal(crc32(name('123456789')), 0xcbf43926); assert.equal(crc32(new Uint8Array()), 0);
  for (const format of ['SRO', 'BUP']) {
    const original = await fixture(`input.${format}`), save = parseRawSave(original);
    assert.equal(save.data.length, 1048576); assert.equal(save.format, format);
    assert.deepEqual(encodeRawSave(save, format), original);
    for (const size of [0, 63, 64, original.length - 1]) assert.throws(() => parseRawSave(original.subarray(0, size)));
    const padded = new Uint8Array(original.length + 1); padded.set(original); assert.throws(() => parseRawSave(padded));
    const wrong = original.slice(); put32(wrong, format === 'SRO' ? 28 : 44, 0xffffffff); assert.throws(() => parseRawSave(wrong));
    if (format === 'SRO') {
      const wrongCRC = original.slice(); wrongCRC[64] ^= 1; assert.throws(() => parseRawSave(wrongCRC));
      put32(wrongCRC, 12, 0); assert.equal(parseRawSave(wrongCRC).data[0], 1);
    }
  }
});

test('raw-byte editing and native download filenames preserve every byte without silent truncation', () => {
  const all = Uint8Array.from({ length: 256 }, (_, i) => i);
  assert.deepEqual(textBytes(byteText(all), 256), all);
  for (const text of ['%', '%0', '%GG', 'too many']) assert.throws(() => textBytes(text, 3));
  assert.throws(() => textBytes('😀', 11));
  assert.equal(saveFilename(Uint8Array.of(47, 92, 37, 0x82, 0xa0, 0, 90), 'SRO'), '%2F%5C%25%82%A0.SRO');
});

test('cancelled opens/exports and superseding edits cannot publish stale or mixed output', async () => {
  const archive = await open('mems-preserve.bin'), original = await bytes(archive), controller = new AbortController();
  controller.abort(); await assert.rejects(SaveArchive.open(new Blob([original]), { signal: controller.signal }));
  await assert.rejects(archive.export({ signal: controller.signal }));
  const cancelled = new AbortController();
  await assert.rejects(archive.export({ signal: cancelled.signal, onProgress: () => cancelled.abort() }));
  let changed = false;
  await assert.rejects(archive.export({ onProgress: () => { if (!changed) { changed = true; archive.update(0, 0, { date: 1 }); } } }));
  assert.equal(archive.metadata(0, 0).date, 1);
  const stable = await archive.export(); archive.update(0, 0, { date: 2 });
  assert.equal((await SaveArchive.open(stable)).metadata(0, 0).date, 1);
});


test('the original PR loses a save on rejected growth; the corrected transaction preserves it', async () => {
  const source=await readFile(new URL('./fixtures/pr374-save-codec.js',import.meta.url),'utf8');
  const Original=vm.runInNewContext(source+'\nContainer', {Uint8Array,TextEncoder,TextDecoder,atob,btoa}, {timeout:2000});
  for(const [file,volume,size] of [['saroo-directory-full.bin',1,0],['mems-all-owned-full.bin',0,129]]) {
    const input=await fixture(file),valid=new Original(input.slice());
    valid.overwrite(volume,0,{dataSize:size,date:123,data:sequence(size)});
    const old=new Original(input.slice());
    assert.throws(()=>old.overwrite(volume,0,{dataSize:1757,date:123,data:sequence(1757)}),/Not enough free space/);
    assert.notDeepEqual(old.bytes,input,'the original delete-before-admission path must reproduce');
    const current=await SaveArchive.open(new Blob([input])),revision=current.revision;
    assert.throws(()=>current.update(volume,0,{data:sequence(1757),date:123}),/does not fit|directory is full/);
    assert.equal(current.revision,revision);assert.deepEqual(await bytes(current),input);
  }
});
