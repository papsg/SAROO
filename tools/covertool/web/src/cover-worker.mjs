/* SPDX-License-Identifier: GPL-3.0-only
 * Single-owner library and image preparation worker for PR #374 adaptation.
 */
import { CoverLibrary, MAX_COVERS, hex8, filename, parseFilename, discIdentity } from './cover-codec.mjs';
import { ImageSource, imageCanvas } from './cover-image.mjs';
import { encodeBMP8 } from './cover-codec.mjs';

export const PAGE_SIZE = 24;
const require = (ok, message) => { if (!ok) throw new Error(message); };
const abort = () => new DOMException('Operation canceled.', 'AbortError');

export function createCoverWorker(post) {
  let library = new CoverLibrary(), epoch = 0, busy = null, editor = null;
  let searchGeneration = 0, searchCache = null;
  const state = () => ({ epoch, revision: library.revision, count: library.count, dirty: library.dirty });
  function checkStamp(stamp) {
    require(stamp?.epoch === epoch && stamp.revision === library.revision,
            'The library changed; reopen this edit on the current library.');
  }
  function closeEditor() { editor?.source?.close(); editor = null; }
  function changed() { searchCache = null; searchGeneration++; }
  function checkEditor(message) {
    checkStamp(message.stamp);
    require(editor && editor.token === message.token, 'This image dialog is no longer active.');
    return editor;
  }
  function copyImage(image) { return { width: image.width, height: image.height, data: image.data.slice() }; }
  function progress(id, value) { post({ id, progress: value }); }
  function reply(id, value) {
    const transfer = [];
    if (value?.image) transfer.push(value.image.data.buffer);
    for (const item of value?.items ?? []) if (item.image) transfer.push(item.image.data.buffer);
    post({ id, value, state: state() }, transfer);
  }
  async function list(message) {
    const generation = ++searchGeneration, stamp = state();
    const query = String(message.query ?? '').trim().toUpperCase();
    require(query.length <= 128, 'Search is too long.');
    let matches = searchCache?.query === query ? searchCache.matches : null;
    if (query && !matches) {
      const found = new Uint16Array(library.count); let count = 0;
      for (let i = 0; i < library.count; i++) {
        const meta = library.meta(i);
        if (meta.serial.toUpperCase().includes(query) || hex8(meta.hash).includes(query)) found[count++] = i;
        if (i % 256 === 255) {
          await new Promise(resolve => setTimeout(resolve, 0));
          if (generation !== searchGeneration) throw abort(); checkStamp(stamp);
        }
      }
      matches = found.subarray(0, count); searchCache = { query, matches };
    }
    checkStamp(stamp);
    const total = query ? matches.length : library.count;
    const page = Math.max(0, Math.min(Math.floor(Number(message.page) || 0), Math.max(0, Math.ceil(total / PAGE_SIZE) - 1)));
    const items = [];
    for (let i = page * PAGE_SIZE; i < Math.min(total, (page + 1) * PAGE_SIZE); i++) {
      const index = query ? matches[i] : i;
      items.push({ index, ...library.meta(index), image: library.image(index) });
    }
    return { page, total, items, pageSize: PAGE_SIZE };
  }
  async function dispatch(message, signal) {
    const { op, id } = message;
    const options = { signal, onProgress: value => progress(id, value) };
    if (op === 'init') return { pageSize: PAGE_SIZE, capacity: MAX_COVERS,
      imagesSupported: typeof OffscreenCanvas !== 'undefined' && typeof createImageBitmap !== 'undefined' };
    checkStamp(message.stamp);
    switch (op) {
      case 'new': closeEditor(); library = new CoverLibrary(); epoch++; changed(); return {};
      case 'open': {
        const candidate = await CoverLibrary.open(message.file, options); signal.throwIfAborted();
        closeEditor(); library = candidate; epoch++; changed(); return {};
      }
      case 'remove': library.remove(message.index); closeEditor(); changed(); return {};
      case 'export': return { blob: await library.export({ ...options, repack: !!message.repack }), name: 'cover.bin' };
      case 'editorStart': {
        const index = message.index ?? library.count;
        const image = index === library.count ? null : library.image(index);
        require(image || library.count < MAX_COVERS, 'The cover index is full (16,384 entries).');
        closeEditor();
        editor = { token: message.token, index, source: image ? new ImageSource(image) : null, image };
        return { meta: image ? library.meta(index) : null, image: image ? copyImage(image) : null };
      }
      case 'editorLoad': {
        const owner = checkEditor(message);
        const source = await ImageSource.open(message.file, options);
        try {
          const size = source.indexed ? { width: source.width, height: source.height } : message.size;
          const image = await source.convert({ ...size, fit: message.fit, signal });
          signal.throwIfAborted(); checkEditor(message);
          let identity = null, identityError = null;
          try { identity = parseFilename(message.file.name); } catch (error) { identityError = error.message; }
          owner.source?.close(); owner.source = source; owner.image = image;
          return { image: copyImage(image), identity, identityError,
            source: { width: source.width, height: source.height, indexed: source.indexed } };
        } catch (error) { source.close(); throw error; }
      }
      case 'editorConvert': {
        const owner = checkEditor(message); require(owner.source, 'Choose an image first.');
        const image = await owner.source.convert({ ...message.size, fit: message.fit, signal });
        signal.throwIfAborted(); checkEditor(message); owner.image = image;
        return { image: copyImage(image) };
      }
      case 'editorSave': {
        const owner = checkEditor(message); require(owner.image, 'Choose an image first.');
        library.set(owner.index, message.meta, owner.image); closeEditor(); changed(); return {};
      }
      case 'editorDelete': {
        const owner = checkEditor(message);
        library.remove(owner.index); closeEditor(); changed(); return {};
      }
      case 'disc': {
        checkEditor(message);
        const bytes = new Uint8Array(await message.file.slice(0, 256).arrayBuffer());
        signal.throwIfAborted(); checkEditor(message);
        return { identity: discIdentity(bytes) };
      }
      case 'imageExport': {
        const owner = checkEditor(message); require(owner.image, 'Choose an image first.');
        require(Number.isInteger(message.meta.hash) && message.meta.hash >= 0 && message.meta.hash <= 0xffffffff,
                'IP hash must be an unsigned 32-bit integer.');
        const name = filename(message.meta, message.extension);
        const blob = message.extension === 'bmp'
          ? new Blob([encodeBMP8(owner.image)], { type: 'image/bmp' })
          : await imageCanvas(owner.image).convertToBlob({ type: 'image/png' });
        signal.throwIfAborted(); checkEditor(message); return { blob, name };
      }
      case 'batch': {
        require(Array.isArray(message.files) && message.files.length > 0 && message.files.length <= MAX_COVERS,
                'Select between 1 and 16,384 images.');
        closeEditor(); let added = 0, canceled = false, processed = 0; const skipped = [];
        for (let i = 0; i < message.files.length; i++) {
          if (signal.aborted) { canceled = true; break; }
          let source = null;
          try {
            require(library.count < MAX_COVERS, 'The cover index is full.');
            const meta = parseFilename(message.files[i].name);
            require(meta, 'Needs a serial and IP hash; open individually to identify it.');
            require(!library.has(meta), `Duplicate cover identity: ${meta.serial}~${hex8(meta.hash)}`);
            source = await ImageSource.open(message.files[i], options);
            const size = source.indexed ? { width: source.width, height: source.height } : message.size;
            const image = await source.convert({ ...size, fit: message.fit, signal });
            signal.throwIfAborted(); library.set(library.count, meta, image); added++; changed();
          } catch (error) {
            if (signal.aborted) { canceled = true; break; }
            skipped.push({ index: i, name: message.files[i].name, reason: error.message });
          } finally { source?.close(); }
          processed = i + 1; progress(id, { phase: 'import', done: processed, total: message.files.length });
          await new Promise(resolve => setTimeout(resolve, 0));
        }
        return { added, skipped, processed, canceled, total: message.files.length };
      }
      default: throw new Error('Unknown editor operation.');
    }
  }
  return async message => {
    const { id, op } = message;
    if (op === 'cancel') { if (busy?.id === message.target) busy.controller.abort(abort()); return; }
    if (op === 'editorClose') {
      if (editor?.token === message.token) {
        if (busy?.token === message.token) busy.controller.abort(abort());
        closeEditor();
      }
      return;
    }
    if (op === 'list') {
      try { reply(id, await list(message)); }
      catch (error) { post({ id, error: error.message, name: error.name, state: state() }); }
      return;
    }
    if (busy) { post({ id, error: 'An operation is already running.', state: state() }); return; }
    const job = { id, token: message.token, controller: new AbortController() }; busy = job;
    try { reply(id, await dispatch(message, job.controller.signal)); }
    catch (error) { post({ id, error: error.message, name: error.name, state: state() }); }
    finally { if (busy === job) busy = null; }
  };
}

if (typeof self !== 'undefined' && typeof self.postMessage === 'function') {
  const receive = createCoverWorker((value, transfer) => self.postMessage(value, transfer));
  self.onmessage = event => { void receive(event.data); };
}
