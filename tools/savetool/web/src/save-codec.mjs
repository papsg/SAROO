/* SPDX-License-Identifier: GPL-3.0-only
 * Authored transactional archive core for SAROO browser-tool PR #374.
 * Native format compatibility: corrected sr_bup, sr_mems and ss_bup readers.
 * No original archive bytes leave this module except through owned copies or
 * immutable export Blobs. All fallible edit work precedes private byte writes.
 */
import { be16, be32, put16, put32, matches, equalBytes, fixedBytes, saveMetadata,
  unsigned, MAX_SAVE_BYTES } from './save-fields.mjs';

export const SLOT_BYTES = 65536, MAX_SLOTS = 4095, MEMS_BYTES = 8388608;
const require = (ok, message) => { if (!ok) throw new Error(message); };
const bit = (bytes, n) => (bytes[n >>> 3] >>> (n & 7)) & 1;
const mark = (bytes, n, used) => { if (used) bytes[n >>> 3] |= 1 << (n & 7); else bytes[n >>> 3] &= ~(1 << (n & 7)); };
const pause = () => new Promise(resolve => setTimeout(resolve, 0));

// Stage only touched blocks, including their untouched bytes. Planning owns all
// allocations. prepare() removes no-op writes before candidate validation; its
// returned fixed typed-array pairs allow a synchronous, bounds-proven commit.
class BlockPlan {
  #read; #copies = new Map();
  constructor(read) { this.#read = read; }
  get stagedBytes() {
    let bytes = 0; for (const patch of this.#copies.values()) bytes += patch.bytes.length; return bytes;
  }
  read = block => this.#copies.get(block)?.bytes ?? this.#read(block);
  write(block) {
    let patch = this.#copies.get(block);
    if (!patch) { const target = this.#read(block); patch = { target, bytes: target.slice() }; this.#copies.set(block, patch); }
    return patch.bytes;
  }
  prepare() {
    const writes = [];
    for (const patch of this.#copies.values()) if (!equalBytes(patch.target, patch.bytes)) writes.push(patch);
    return writes;
  }
}

function validateSlot(read) {
  const global = read(0), bitmap = global.subarray(64, 128), seen = new Uint8Array(64);
  require(matches(global, 0, 'SaroSave') && be32(global, 8) === SLOT_BYTES && be16(global, 12) === 128 && bit(bitmap, 0),
          'Invalid SAROO slot header; existing slot contents were not reformatted.');
  const claim = block => {
    require(block > 0 && block < 512 && bit(bitmap, block) && !bit(seen, block), 'Invalid or shared SAROO allocation.');
    mark(seen, block, true);
  };
  const records = [];
  for (let block = be16(global, 62); block; block = be16(read(block), 62)) {
    claim(block); records.push({ block });
  }
  for (const record of records) {
    const header = read(record.block), dataBitmap = header.subarray(64), size = be32(header, 12), needed = Math.ceil(size / 128);
    require(header[0] && size <= 510 * 128, 'Invalid SAROO save name or size.');
    const blocks = new Uint16Array(needed); let count = 0;
    for (let block = 0; block < 512; block++) if (bit(dataBitmap, block)) {
      claim(block); require(count < needed, 'SAROO allocation exceeds the save size.'); blocks[count++] = block;
    }
    require(count === needed, 'SAROO payload is not completely allocated.');
    Object.assign(record, { size, blocks });
  }
  let free = 0;
  for (let block = 1; block < 512; block++) if (!bit(bitmap, block)) free++;
  return { records, free, storedFree: be16(global, 14), total: 512, reserved: 1 };
}

function validateMems(read) {
  const global = read(0), bitmap = global.subarray(16), seen = new Uint8Array(1008), records = [];
  require(matches(global, 0, 'SaroMems') && be32(global, 8) === MEMS_BYTES && bitmap[0] === 255, 'Invalid MEMS header or reserved blocks.');
  const claim = block => {
    require(block >= 8 && block < 8064 && bit(bitmap, block) && !bit(seen, block), 'Invalid or shared MEMS allocation.');
    mark(seen, block, true);
  };
  for (let entry = 0; entry < 448; entry++) {
    const header = read(1 + (entry >>> 6)), offset = (entry & 63) * 16;
    if (!header[offset]) continue;
    const block = be16(header, offset + 14); claim(block); records.push({ entry, block });
  }
  for (const record of records) {
    const header = read(record.block), directory = read(1 + (record.entry >>> 6)), start = (record.entry & 63) * 16;
    for (let i = 0; i < 11; i++) {
      require(header[i] === directory[start + i], 'MEMS filename and directory identity disagree.');
    }
    const size = be32(header, 12);
    require(size <= 479 * 1024, 'MEMS save exceeds the 479-block pointer table.');
    const needed = size <= 960 ? 0 : Math.ceil(size / 1024), blocks = new Uint16Array(needed);
    for (let i = 0; i < needed; i++) { const block = be16(header, 64 + i * 2); claim(block); blocks[i] = block; }
    if (needed) require(be16(header, 64 + needed * 2) === 0, 'MEMS table terminator is missing.');
    Object.assign(record, { size, blocks });
  }
  let free = 0;
  for (let block = 8; block < 8064; block++) if (!bit(bitmap, block)) free++;
  return { records, free, storedFree: be16(global, 12), total: 8064, reserved: 8 };
}

function validateSaturn(read) {
  const owned = new Uint8Array(64), table = new Uint16Array(510), records = []; owned[0] = 3;
  require(matches(read(0), 0, 'BackUpRam Format'), 'Invalid Saturn backup signature.');
  for (let block = 2; block < 512; block++) if (be32(read(block), 0) === 0x80000000) {
    mark(owned, block, true); records.push({ block });
  }
  for (const record of records) {
    const first = read(record.block), size = be32(first, 30);
    require(first[4] && size <= 32768, 'Invalid Saturn backup name or size.');
    table[0] = record.block;
    let cursor = 0, offset = 34, count = 1;
    for (;;) {
      if (offset === 64) { cursor++; offset = 4; }
      require(cursor < count, 'Saturn block table has no complete terminator.');
      const next = be16(read(table[cursor]), offset); offset += 2;
      if (!next) break;
      require(next >= 2 && next < 512 && count < 510 && !bit(owned, next), 'Invalid or shared Saturn block table.');
      mark(owned, next, true); table[count++] = next;
    }
    require(size <= 64 - offset + (count - cursor - 1) * 60, 'Saturn payload exceeds its complete block list.');
    Object.assign(record, { size, blocks: table.slice(0, count), cursor, offset });
  }
  let free = 0;
  for (let block = 2; block < 512; block++) if (!bit(owned, block)) free++;
  return { records, free, storedFree: null, total: 512, reserved: 2 };
}

const CONSTRUCT = Symbol('SaveArchive');
export class SaveArchive {
  #source; #extra = []; #logical = null; #type; #cache = null; #revision = 0;
  #volumes = new Uint16Array();
  constructor(token) { require(token === CONSTRUCT, 'Use SaveArchive.open() or SaveArchive.create().'); }
  get type() { return this.#type; }
  get readOnly() { return this.#type === 'saturn'; }
  get revision() { return this.#revision; }
  get dirty() { return this.#revision !== 0; }
  get bytes() { return this.#source.length + this.#extra.length * SLOT_BYTES; }
  get volumeCount() { return this.#type === 'saroo' ? this.#volumes.length : 1; }

  static async open(file, { signal, onProgress } = {}) {
    require(file instanceof Blob && file.size >= 32768, 'Archive is too short.');
    signal?.throwIfAborted();
    // Reject impossible extents before allocating the complete input. The odd
    // physical layout needs 32 bytes to identify its 16 logical signature bytes.
    const prefix = new Uint8Array(await file.slice(0, 32).arrayBuffer()); signal?.throwIfAborted();
    if (matches(prefix, 0, 'Saroo Save File\0')) {
      require(file.size % SLOT_BYTES === 0 && file.size <= 4096 * SLOT_BYTES,
              'SAROO archives require complete 64 KiB slots, up to 4,095 games.');
    } else if (matches(prefix, 0, 'SaroMems')) {
      require(file.size >= MEMS_BYTES && be32(prefix, 8) === MEMS_BYTES, 'Invalid MEMS archive extent.');
    } else {
      const odd = Uint8Array.from({ length: 16 }, (_, i) => prefix[2 * i + 1]);
      require((file.size === 32768 && matches(prefix, 0, 'BackUpRam Format')) ||
              (file.size === 65536 && matches(odd, 0, 'BackUpRam Format')), 'Expected a complete Saturn backup signature and extent.');
    }
    const result = new SaveArchive(CONSTRUCT); result.#source = new Uint8Array(await file.arrayBuffer()); signal?.throwIfAborted();
    const source = result.#source;
    if (matches(source, 0, 'Saroo Save File\0')) {
      require(source.length % SLOT_BYTES === 0 && source.length <= 4096 * SLOT_BYTES, 'SAROO archives require complete 64 KiB slots, up to 4,095 games.');
      result.#type = 'saroo'; result.#index();
    } else if (matches(source, 0, 'SaroMems')) {
      require(source.length >= MEMS_BYTES, 'MEMS archive is shorter than 8 MiB.');
      result.#type = 'mems'; result.#select(0);
    } else {
      const plain = matches(source, 0, 'BackUpRam Format');
      require(source.length === (plain ? 32768 : 65536), 'Expected an exact 32 KiB or odd-byte 64 KiB Saturn backup.');
      result.#type = 'saturn';
      if (!plain) { result.#logical = new Uint8Array(32768); for (let i = 0; i < 32768; i++) result.#logical[i] = source[i * 2 + 1]; }
      result.#select(0);
    }
    onProgress?.({ phase: 'open', done: source.length, total: source.length });
    await pause(); signal?.throwIfAborted(); return result;
  }

  static create(type = 'saroo') {
    require(type === 'saroo' || type === 'mems', 'New archives must be SAROO or MEMS.');
    const archive = new SaveArchive(CONSTRUCT); archive.#type = type;
    archive.#source = new Uint8Array(type === 'saroo' ? SLOT_BYTES : MEMS_BYTES);
    archive.#source.set(new TextEncoder().encode(type === 'saroo' ? 'Saroo Save File' : 'SaroMems'));
    if (type === 'mems') { put32(archive.#source, 8, MEMS_BYTES); put16(archive.#source, 12, 8056); archive.#source[16] = 255; }
    archive.#revision = 1; return archive;
  }

  #index() {
    const slots = this.bytes / SLOT_BYTES, ids = new Uint16Array(Math.max(0, slots - 1)); let count = 0;
    for (let slot = 1; slot < 4096; slot++) if (this.#source[slot * 16]) {
      require(slot < slots, 'SAROO directory refers beyond the complete archive.'); ids[count++] = slot;
    }
    this.#volumes = ids.subarray(0, count);
  }
  volume(index) {
    unsigned(index, this.volumeCount - 1, 'Volume index');
    const id = this.#type === 'saroo' ? this.#volumes[index] : 0;
    return { id, rawName: this.#type === 'saroo' ? this.#source.slice(id * 16, id * 16 + 16) :
      new TextEncoder().encode(this.#type === 'mems' ? 'SaroMems' : 'Saturn Backup RAM') };
  }
  #slot(slot) {
    const offset = slot * SLOT_BYTES;
    return offset < this.#source.length ? this.#source.subarray(offset, offset + SLOT_BYTES) : this.#extra[slot - this.#source.length / SLOT_BYTES];
  }
  #reader(volume) {
    let data, width, total;
    if (this.#type === 'saroo') {
      require(Number.isInteger(volume) && volume > 0 && volume < this.bytes / SLOT_BYTES && this.#source[volume * 16], 'SAROO slot does not exist.');
      data = this.#slot(volume); width = 128; total = 512;
    } else {
      require(volume === 0, 'This archive has one volume.');
      data = this.#logical ?? this.#source; width = this.#type === 'mems' ? 1024 : 64; total = this.#type === 'mems' ? 8192 : 512;
    }
    return block => {
      require(Number.isInteger(block) && block >= 0 && block < total, 'Archive block is outside its volume.');
      return data.subarray(block * width, (block + 1) * width);
    };
  }
  #validate(read) { return this.#type === 'saroo' ? validateSlot(read) : this.#type === 'mems' ? validateMems(read) : validateSaturn(read); }
  #select(volume) {
    if (this.#cache?.volume === volume) return this.#cache;
    const parsed = this.#validate(this.#reader(volume)); this.#cache = { volume, ...parsed }; return this.#cache;
  }
  stats(volume) {
    const { records, free, storedFree, total, reserved } = this.#select(volume);
    return { saves: records.length, free, storedFree, total, reserved, readOnly: this.readOnly };
  }
  #record(volume, index) {
    const selected = this.#select(volume); unsigned(index, selected.records.length - 1, 'Save index');
    return selected.records[index];
  }
  metadata(volume, index) {
    const record = this.#record(volume, index), header = this.#reader(volume)(record.block), saturn = this.#type === 'saturn';
    return { id: index, size: record.size, blocks: this.#type === 'saturn' ? record.blocks.length : record.blocks.length + 1,
      rawFilename: header.slice(saturn ? 4 : 0, saturn ? 15 : 11), rawComment: header.slice(16, 26),
      language: header[saturn ? 15 : 27], date: be32(header, saturn ? 26 : 28) };
  }
  read(volume, index, offset = 0, length = undefined) {
    const record = this.#record(volume, index), read = this.#reader(volume);
    unsigned(offset, record.size, 'Payload offset'); length ??= record.size - offset;
    unsigned(length, record.size - offset, 'Payload length');
    const output = new Uint8Array(length); if (!length) return output;
    if (this.#type === 'mems' && !record.blocks.length) { output.set(read(record.block).subarray(64 + offset, 64 + offset + length)); return output; }
    let position = 0, done = 0, cursor = this.#type === 'saturn' ? record.cursor : 0;
    let start = this.#type === 'saturn' ? record.offset : 0;
    while (done < length) {
      const bytes = read(record.blocks[cursor]), available = bytes.length - start;
      if (offset < position + available) {
        const skip = Math.max(0, offset - position), count = Math.min(available - skip, length - done);
        output.set(bytes.subarray(start + skip, start + skip + count), done); done += count;
      }
      position += available; cursor++; start = this.#type === 'saturn' ? 4 : 0;
    }
    return output;
  }
  save(volume, index) { return { ...this.metadata(volume, index), data: this.read(volume, index) }; }

  #commit(volume, plan) {
    const writes = plan.prepare(), stagedBytes = plan.stagedBytes;
    if (!writes.length) return { changed: false, stagedBytes, writtenBytes: 0, blocks: 0 };
    const cache = { volume, ...this.#validate(plan.read) };
    // No callbacks, await, user objects, resizing or allocation-bearing planning
    // follows this point. Both arrays in every pair are private fixed Uint8Arrays.
    for (let i = 0; i < writes.length; i++) writes[i].target.set(writes[i].bytes);
    this.#cache = cache; this.#revision++;
    return { changed: true, stagedBytes, writtenBytes: writes.length * writes[0].bytes.length, blocks: writes.length };
  }

  update(volume, index, changes) {
    require(!this.readOnly, 'Saturn backup archives are read-only.');
    const old = index === null ? null : this.#record(volume, index), selected = this.#select(volume);
    const meta = saveMetadata({ ...(old ? this.metadata(volume, index) : {}), ...changes });
    const payload = changes.data;
    require(old || payload !== undefined, 'A new save needs a payload, including an empty byte array.');
    require(payload === undefined || (payload instanceof Uint8Array && payload.length <= MAX_SAVE_BYTES), 'Invalid save payload.');
    const size = payload === undefined ? old.size : payload.length;
    const mems = this.#type === 'mems', width = mems ? 1024 : 128, limit = mems ? 8064 : 512, reserved = mems ? 8 : 1;
    require(size <= (mems ? 479 * 1024 : 510 * 128), 'Replacement exceeds this format’s per-save capacity.');
    const data = payload?.slice();
    const plan = new BlockPlan(this.#reader(volume));
    let block = old?.block, entry = old?.entry, blocks = old?.blocks;
    if (mems && !old) {
      entry = -1;
      for (let i = 0; i < 448; i++) if (!plan.read(1 + (i >>> 6))[(i & 63) * 16]) { entry = i; break; }
      require(entry >= 0, 'MEMS directory is full (448 saves).');
    }
    if (!old || size !== old.size) {
      const global = plan.write(0), bitmap = global.subarray(mems ? 16 : 64);
      if (old) for (const item of old.blocks) mark(bitmap, item, false);
      if (!old) {
        for (let i = reserved; i < limit; i++) if (!bit(bitmap, i)) { block = i; mark(bitmap, i, true); break; }
        require(block !== undefined, 'No free header block is available.');
      }
      const needed = mems && size <= 960 ? 0 : Math.ceil(size / width);
      blocks = new Uint16Array(needed); let count = 0, free = 0;
      for (let i = reserved; i < limit; i++) if (!bit(bitmap, i)) {
        if (count < needed) { blocks[count++] = i; mark(bitmap, i, true); } else free++;
      }
      require(count === needed, 'Replacement does not fit; the existing save is unchanged.');
      const header = plan.write(block);
      if (!old) header.fill(0); else header.fill(0, 64);
      if (mems) {
        for (let i = 0; i < blocks.length; i++) put16(header, 64 + i * 2, blocks[i]);
      } else {
        const dataBitmap = header.subarray(64);
        for (const item of blocks) mark(dataBitmap, item, true);
        if (!old) put16(plan.write(selected.records.at(-1)?.block ?? 0), 62, block);
      }
      put16(global, mems ? 12 : 14, free);
    }
    const header = plan.write(block); header.set(meta.rawFilename, 0); header.set(meta.rawComment, 16);
    header[27] = meta.language; put32(header, 28, meta.date); put32(header, 12, size);
    if (mems && (!old || !equalBytes(meta.rawFilename, this.metadata(volume, index).rawFilename))) {
      const directory = plan.write(1 + (entry >>> 6)), offset = (entry & 63) * 16;
      directory.set(meta.rawFilename, offset); put16(directory, offset + 14, block);
    }
    if (data) {
      if (mems && !blocks.length) header.set(data, 64);
      else for (let i = 0; i < blocks.length; i++) plan.write(blocks[i]).set(data.subarray(i * width, (i + 1) * width));
    }
    return this.#commit(volume, plan);
  }

  remove(volume, index) {
    require(!this.readOnly, 'Saturn backup archives are read-only.');
    const record = this.#record(volume, index), selected = this.#select(volume), mems = this.#type === 'mems';
    const plan = new BlockPlan(this.#reader(volume)), global = plan.write(0), bitmap = global.subarray(mems ? 16 : 64);
    mark(bitmap, record.block, false); for (const block of record.blocks) mark(bitmap, block, false);
    put16(global, mems ? 12 : 14, selected.free + record.blocks.length + 1);
    if (mems) {
      const directory = plan.write(1 + (record.entry >>> 6)), start = (record.entry & 63) * 16; directory.fill(0, start, start + 16);
    } else put16(plan.write(index ? selected.records[index - 1].block : 0), 62, be16(plan.read(record.block), 62));
    return this.#commit(volume, plan);
  }

  createSlot(gameId) {
    require(this.#type === 'saroo', 'Only SAROO archives have game slots.');
    const id = fixedBytes(gameId, 16, 'Game ID'); require(id[0], 'A game ID is required.');
    let empty = 0;
    for (let slot = 1; slot < 4096; slot++) {
      const existing = this.#source.subarray(slot * 16, slot * 16 + 16);
      if (!existing[0]) { if (!empty) empty = slot; }
      else if (equalBytes(existing, id)) return { id: slot, created: false, stagedBytes: 0 };
    }
    require(empty && empty <= this.bytes / SLOT_BYTES, 'SAROO slot directory is full.');
    const bytes = new Uint8Array(SLOT_BYTES); bytes.set(new TextEncoder().encode('SaroSave'));
    put32(bytes, 8, SLOT_BYTES); put16(bytes, 12, 128); put16(bytes, 14, 511); bytes.set(id, 32); bytes[64] = 1;
    const append = empty === this.bytes / SLOT_BYTES, extra = append ? this.#extra.concat([bytes]) : this.#extra;
    const volumes = new Uint16Array(this.#volumes.length + 1); let index = 0;
    for (let slot = 1; slot < 4096; slot++) if (slot === empty || this.#source[slot * 16]) volumes[index++] = slot;
    if (!append) this.#slot(empty).set(bytes);
    this.#source.set(id, empty * 16); this.#extra = extra; this.#volumes = volumes; this.#cache = null; this.#revision++;
    return { id: empty, created: true, stagedBytes: SLOT_BYTES + 16 };
  }

  async export({ signal, onProgress } = {}) {
    const revision = this.#revision, parts = []; let done = 0;
    const check = () => { signal?.throwIfAborted(); require(revision === this.#revision, 'Archive changed during export; export the current version again.'); };
    const source = this.#source, extra = this.#extra; let checkpoint = performance.now();
    for (const segment of [source, ...extra]) {
      for (let start = 0; start < segment.length; start += 1048576) {
        check(); const end = Math.min(start + 1048576, segment.length);
        parts.push(new Blob([segment.subarray(start, end)])); done += end - start;
        check();
        if (performance.now() - checkpoint >= 8) {
          onProgress?.({ phase: 'export', done, total: this.bytes }); await pause(); check(); checkpoint = performance.now();
        }
      }
    }
    onProgress?.({ phase: 'export', done, total: this.bytes }); await pause(); check();
    return new Blob(parts, { type: 'application/octet-stream' });
  }
}
