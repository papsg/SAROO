/* SPDX-License-Identifier: GPL-3.0-only
 * Authored offline UI for tpunix/SAROO PR #374. DOM metadata is literal text.
 */
export function bootCoverApp(workerSource) {
  const $ = id => document.getElementById(id);
  let worker, state = { epoch: 0, revision: 0, count: 0, dirty: false };
  let sequence = 0, active = null, page = 0, totalPages = 1, listGeneration = 0, gridOwner = null;
  let modal = null, modalSequence = 0, debounce = null, libraryURL = null, imageURL = null;
  let batchFiles = [], reviewItems = [], reviewPage = 0, ready = false, fatal = false;
  const pending = new Map();
  const fileChoices = new Map();
  function chooseFile(id, owner) { fileChoices.set(id, owner); $(id).click(); }
  function takeChoice(id) { const owner = fileChoices.get(id); fileChoices.delete(id); return owner; }
  const workspaceChoice = () => ({ epoch: state.epoch, revision: state.revision });
  const sameWorkspace = owner => owner && owner.epoch === state.epoch && owner.revision === state.revision;
  const currentChoice = owner => sameWorkspace(owner) && !modal && !$('batch').open;
  function message(text, error = false) { $('message').textContent = text; $('message').hidden = !text; $('message').classList.toggle('error', error); }
  function clearDownload(kind) {
    if (kind === 'library') { if (libraryURL) URL.revokeObjectURL(libraryURL); libraryURL = null; $('library-download').hidden = true; }
    else { if (imageURL) URL.revokeObjectURL(imageURL); imageURL = null; $('image-download').hidden = true; }
  }
  function updateState(next) {
    if (!next || next.epoch < state.epoch || (next.epoch === state.epoch && next.revision < state.revision)) return;
    if (next.epoch !== state.epoch || next.revision !== state.revision) clearDownload('library');
    state = next;
    $('library-state').textContent = `${state.count.toLocaleString()} / 16,384 covers · ${state.dirty ? 'Edited — export to keep changes' : 'No changes'}`;
  }
  function controls() {
    for (const button of document.querySelectorAll('[data-mutate]')) button.disabled = !ready || fatal || active !== null;
    for (const button of $('grid').querySelectorAll('.card')) button.disabled ||= !sameWorkspace(gridOwner);
    $('activity').hidden = active === null; $('cancel').disabled = active === null;
    $('previous').disabled = page === 0; $('next').disabled = page + 1 >= totalPages;
  }
  function rpc(op, data = {}) {
    const id = ++sequence;
    const promise = new Promise((resolve, reject) => { pending.set(id, { resolve, reject }); });
    try { worker.postMessage({ id, op, stamp: { ...state }, ...data }); }
    catch (error) { pending.get(id).reject(error); pending.delete(id); }
    return { id, promise };
  }
  async function run(op, data = {}) {
    if (active !== null) throw new Error('Wait for the current operation or cancel it.');
    if (fatal) throw new Error('The worker stopped. Reopen the editor to continue.');
    const request = rpc(op, data); active = request.id;
    $('activity-text').textContent = 'Working…'; $('progress').removeAttribute('value'); controls();
    try { return await request.promise; }
    finally { if (active === request.id) active = null; controls(); }
  }
  async function perform(action) {
    const owner = modal;
    try { await action(); }
    catch (error) {
      if (error.name === 'AbortError') return;
      if (owner && modal !== owner) return;
      if (modal) $('editor-error').textContent = error.message; else message(error.message, true);
    }
  }
  const on = (id, event, action) => $(id).addEventListener(event, e => { void perform(() => action(e)); });
  function draw(canvas, image) {
    canvas.width = image.width; canvas.height = image.height;
    const ctx = canvas.getContext('2d'), pixels = ctx.createImageData(image.width, image.height), bytes = image.data;
    for (let i = 0; i < image.width * image.height; i++) {
      const p = bytes[1024 + i] * 4, q = i * 4;
      pixels.data[q] = bytes[p + 2]; pixels.data[q + 1] = bytes[p + 1]; pixels.data[q + 2] = bytes[p]; pixels.data[q + 3] = 255;
    }
    ctx.putImageData(pixels, 0, 0);
  }
  async function refresh() {
    const generation = ++listGeneration, owner = workspaceChoice();
    const result = await rpc('list', { page, query: $('search').value }).promise;
    if (generation !== listGeneration || !sameWorkspace(owner)) return;
    page = result.page; totalPages = Math.max(1, Math.ceil(result.total / result.pageSize));
    const fragment = document.createDocumentFragment();
    for (const item of result.items) {
      const button = document.createElement('button'); button.className = 'card'; button.dataset.mutate = '';
      const art = document.createElement('span'); art.className = 'art';
      const canvas = document.createElement('canvas'); draw(canvas, item.image); item.image = null; art.append(canvas);
      const serial = document.createElement('strong'); serial.className = 'serial'; serial.textContent = item.serial;
      const detail = document.createElement('small'); detail.textContent = `${item.hash.toString(16).toUpperCase().padStart(8, '0')} · ${item.width} × ${item.height}`;
      button.append(art, serial, detail); button.setAttribute('aria-label', `Edit ${item.serial}`);
      button.addEventListener('click', () => { if (currentChoice(owner)) void perform(() => openEditor(item.index)); }); fragment.append(button);
    }
    gridOwner = owner; $('grid').replaceChildren(fragment); $('empty').hidden = result.total !== 0;
    if (!result.total) $('empty').querySelector('h2').textContent = $('search').value ? 'No matching covers' : 'Your artwork, together.';
    $('range').textContent = result.total ? `${page * result.pageSize + 1}–${Math.min(result.total, (page + 1) * result.pageSize)} of ${result.total.toLocaleString()}` : '';
    $('page-label').textContent = `Page ${page + 1} of ${totalPages}`; $('pagination').hidden = totalPages <= 1; controls();
  }
  function canDiscard() { return !state.dirty || confirm('Replace this edited library? Keep an export first if you need its changes.'); }
  function setSize(width, height) {
    $('width').value = width; $('height').value = height;
    $('size').value = width === 128 && [128, 192].includes(height) ? `128x${height}` : 'custom';
  }
  function size() {
    const width = Number($('width').value), height = Number($('height').value);
    if (!Number.isInteger(width) || width < 1 || width > 128 || !Number.isInteger(height) || height < 1 || height > 192)
      throw new Error('Cover dimensions must be 1–128 × 1–192.');
    return { width, height };
  }
  function metadata() {
    if (!/^[0-9a-f]{8}$/i.test($('hash').value)) throw new Error('Enter an IP hash with exactly eight hexadecimal digits.');
    return { serial: $('serial').value, hash: parseInt($('hash').value, 16) };
  }
  function identity(value) {
    $('serial').value = value.serial; $('hash').value = value.hash.toString(16).toUpperCase().padStart(8, '0');
  }
  function preview(image) {
    draw($('preview'), image); $('preview').hidden = false; $('preview-empty').hidden = true;
    setSize(image.width, image.height); $('image-info').textContent = `${image.width} × ${image.height} · 256-color format`;
    if (modal) modal.hasImage = true;
  }
  async function openEditor(index, file = null, reviewItem = null) {
    const token = ++modalSequence;
    const result = await run('editorStart', { index, token });
    modal = { token, dirty: false, hasImage: !!result.image, reviewItem };
    $('editor-title').textContent = result.meta ? 'Edit cover' : 'Add cover'; $('delete').hidden = !result.meta;
    $('editor-error').textContent = ''; $('serial').value = ''; $('hash').value = ''; $('image-info').textContent = '';
    $('preview').hidden = true; $('preview-empty').hidden = false; $('fit').value = 'contain'; setSize(128, 192);
    if (result.meta) identity(result.meta); if (result.image) preview(result.image);
    $('editor').showModal(); if (file) await loadImage(file);
  }
  function closeEditor(force = false) {
    if (!modal || (!force && modal.dirty && !confirm('Discard the changes in this cover dialog?'))) return;
    worker.postMessage({ op: 'editorClose', token: modal.token }); modal = null;
    fileChoices.delete('image-file'); fileChoices.delete('disc-file');
    $('editor').close(); clearDownload('image'); $('preview').width = 1; $('preview').height = 1;
  }
  async function loadImage(file) {
    const owner = modal; if (!owner) return;
    const result = await run('editorLoad', { token: owner.token, file, size: size(), fit: $('fit').value });
    if (modal !== owner) return;
    preview(result.image); if (result.identity) identity(result.identity);
    owner.dirty = true; clearDownload('image');
    $('editor-error').textContent = result.identityError ?? (result.identity ? '' : 'Enter the serial and IP hash, or identify this cover from a disc image.');
  }
  async function convert() {
    const owner = modal; if (!owner?.hasImage) return;
    try {
      const result = await run('editorConvert', { token: owner.token, size: size(), fit: $('fit').value });
      if (modal !== owner) return; preview(result.image); owner.dirty = true; clearDownload('image');
    } catch (error) {
      if (modal === owner) setSize($('preview').width, $('preview').height);
      throw error;
    }
  }
  async function openLibrary(file) {
    if (!canDiscard()) return;
    await run('open', { file }); $('library-name').textContent = file.name; page = 0; $('search').value = '';
    reviewItems = []; batchFiles = []; renderReview(); message(''); await refresh();
  }
  function renderReview() {
    const pages = Math.max(1, Math.ceil(reviewItems.length / 24)); reviewPage = Math.min(reviewPage, pages - 1);
    $('review').hidden = !reviewItems.length;
    const fragment = document.createDocumentFragment();
    for (const item of reviewItems.slice(reviewPage * 24, reviewPage * 24 + 24)) {
      const li = document.createElement('li'), text = document.createElement('span'), title = document.createElement('strong'), why = document.createElement('small');
      title.textContent = item.name; why.textContent = item.reason; text.append(title, document.createElement('br'), why);
      const button = document.createElement('button'); button.textContent = 'Review'; button.dataset.mutate = '';
      button.addEventListener('click', () => { void perform(() => openEditor(undefined, item.file, item)); }); li.append(text, button); fragment.append(li);
    }
    $('review-items').replaceChildren(fragment); $('review-summary').textContent = `${reviewItems.length.toLocaleString()} images were not imported. Your completed imports are retained.`;
    $('review-page').textContent = `${reviewPage + 1} / ${pages}`; $('review-prev').disabled = !reviewPage; $('review-next').disabled = reviewPage + 1 >= pages; controls();
  }
  async function addFiles(files) {
    if (!files.length) return;
    if (files.length > 16384) throw new Error('Select no more than 16,384 images at a time.');
    if (files.length === 1) return openEditor(undefined, files[0]);
    batchFiles = files; $('batch-count').textContent = `${files.length.toLocaleString()} selected images`; $('batch').showModal();
  }
  on('new', 'click', async () => {
    if (!canDiscard()) return; await run('new'); $('library-name').textContent = 'New library'; $('search').value = ''; page = 0;
    reviewItems = []; batchFiles = []; renderReview(); message(''); await refresh();
  });
  on('open', 'click', () => chooseFile('library-file', workspaceChoice())); on('add', 'click', () => chooseFile('add-files', workspaceChoice()));
  on('library-file', 'change', async () => { const file = $('library-file').files[0], owner = takeChoice('library-file'); $('library-file').value = ''; if (file && currentChoice(owner)) await openLibrary(file); });
  on('add-files', 'change', async () => { const files = Array.from($('add-files').files), owner = takeChoice('add-files'); $('add-files').value = ''; if (currentChoice(owner)) await addFiles(files); });
  on('search', 'input', () => { clearTimeout(debounce); page = 0; debounce = setTimeout(() => { void perform(refresh); }, 120); });
  on('previous', 'click', async () => { page--; await refresh(); }); on('next', 'click', async () => { page++; await refresh(); });
  on('cancel', 'click', () => { worker.postMessage({ op: 'cancel', target: active }); $('activity-text').textContent = 'Canceling…'; $('cancel').disabled = true; });
  on('export', 'click', async () => {
    const result = await run('export', { repack: $('compact').checked }); clearDownload('library'); libraryURL = URL.createObjectURL(result.blob);
    $('download').href = libraryURL; $('download').download = result.name; $('library-download').hidden = false;
    message(`Export ready · ${(result.blob.size / 1048576).toFixed(2)} MiB. Choose Download cover.bin to keep it.`);
  });
  on('compact', 'change', () => clearDownload('library'));
  on('download', 'click', () => message('Download requested. Keep the downloaded cover.bin before closing or replacing edited work.'));
  on('choose-image', 'click', () => chooseFile('image-file', modal));
  on('image-file', 'change', async () => { const file = $('image-file').files[0], owner = takeChoice('image-file'); $('image-file').value = ''; if (file && owner && modal === owner) await loadImage(file); });
  on('read-disc', 'click', () => chooseFile('disc-file', modal));
  on('disc-file', 'change', async () => {
    const file = $('disc-file').files[0], owner = takeChoice('disc-file'); $('disc-file').value = ''; if (!file || !owner || modal !== owner) return;
    const result = await run('disc', { token: owner.token, file }); if (modal !== owner) return;
    identity(result.identity); owner.dirty = true; clearDownload('image'); $('editor-error').textContent = '';
  });
  for (const id of ['serial', 'hash']) on(id, 'input', () => { if (modal) modal.dirty = true; clearDownload('image'); });
  on('size', 'change', async () => { if ($('size').value !== 'custom') { const [w, h] = $('size').value.split('x').map(Number); setSize(w, h); await convert(); } });
  for (const id of ['width', 'height']) on(id, 'change', async () => { $('size').value = 'custom'; await convert(); });
  on('fit', 'change', convert);
  on('close-editor', 'click', () => closeEditor());
  on('editor', 'cancel', e => { e.preventDefault(); closeEditor(); });
  on('save-editor', 'click', async () => {
    const owner = modal; if (!owner) return;
    await run('editorSave', { token: owner.token, meta: metadata() });
    if (modal !== owner) { await refresh(); return; }
    if (owner.reviewItem) { reviewItems = reviewItems.filter(item => item !== owner.reviewItem); renderReview(); }
    closeEditor(true); message('Cover applied. Export the library to keep your changes.'); await refresh();
  });
  on('delete', 'click', async () => {
    if (!modal || !confirm('Delete this cover from the library?')) return;
    // The dialog's committed index is kept in the worker; deletion is an edit operation.
    await run('editorDelete', { token: modal.token }); closeEditor(true); await refresh();
  });
  for (const extension of ['bmp', 'png']) on(extension, 'click', async () => {
    const owner = modal; if (!owner) return;
    const result = await run('imageExport', { token: owner.token, meta: metadata(), extension }); if (modal !== owner) return;
    clearDownload('image'); imageURL = URL.createObjectURL(result.blob); $('image-download-link').href = imageURL;
    $('image-download-link').download = result.name; $('image-download-link').textContent = `Download ${extension.toUpperCase()}`; $('image-download').hidden = false;
  });
  on('batch-close', 'click', () => { $('batch').close(); batchFiles = []; });
  on('batch', 'cancel', () => { batchFiles = []; });
  on('batch-start', 'click', async () => {
    const files = batchFiles; batchFiles = []; $('batch').close();
    const [width, height] = $('batch-size').value.split('x').map(Number);
    const result = await run('batch', { files, size: { width, height }, fit: $('batch-fit').value });
    reviewItems = result.skipped.map(item => ({ ...item, file: files[item.index] }));
    for (let i = result.processed; i < files.length; i++) reviewItems.push({ name: files[i].name, reason: 'Not processed — import canceled.', file: files[i] });
    reviewPage = 0; renderReview(); message(`${result.added} covers imported${result.canceled ? ' before cancellation' : ''}.`); await refresh();
  });
  on('review-prev', 'click', () => { reviewPage--; renderReview(); }); on('review-next', 'click', () => { reviewPage++; renderReview(); });
  on('review-dismiss', 'click', () => { reviewItems = []; renderReview(); });
  window.addEventListener('dragover', event => { event.preventDefault(); });
  window.addEventListener('drop', event => { event.preventDefault(); void perform(async () => {
    if (active !== null || !ready || fatal || $('batch').open) return;
    const files = Array.from(event.dataTransfer.files); if (!files.length) return;
    if (modal) { if (files.length !== 1) throw new Error('Drop one image into this cover dialog.'); await loadImage(files[0]); }
    else if (files.length === 1 && /\.bin$/i.test(files[0].name)) await openLibrary(files[0]); else await addFiles(files);
  }); });
  window.addEventListener('beforeunload', event => { if (state.dirty || modal?.dirty) { event.preventDefault(); event.returnValue = ''; } });
  controls();
  try {
    const url = URL.createObjectURL(new Blob([workerSource], { type: 'text/javascript' }));
    try { worker = new Worker(url); } finally { URL.revokeObjectURL(url); }
    worker.onmessage = event => {
      const value = event.data;
      if (value.progress) {
        if (active !== value.id) return;
        $('activity-text').textContent = value.progress.phase === 'import' ? 'Importing images…' : 'Preparing library…';
        if (value.progress.total) { $('progress').max = value.progress.total; $('progress').value = value.progress.done; }
        return;
      }
      const request = pending.get(value.id); if (!request) return; pending.delete(value.id); updateState(value.state);
      if (value.error) { const error = new Error(value.error); error.name = value.name ?? 'Error'; request.reject(error); }
      else request.resolve(value.value);
    };
    const workerFailed = event => {
      event.preventDefault(); fatal = true;
      for (const request of pending.values()) request.reject(new Error('The editor worker stopped. Any prepared download remains available.'));
      pending.clear(); message('The editor worker stopped. Keep any prepared download, then reopen the editor.', true); controls();
    };
    worker.onerror = workerFailed; worker.onmessageerror = workerFailed;
    void perform(async () => {
      const result = await run('init');
      if (!result.imagesSupported) throw new Error('This browser needs Worker, OffscreenCanvas and createImageBitmap support. Open this file in a current supported browser.');
      ready = true; controls(); await refresh();
    });
  } catch (error) { fatal = true; message(`Cannot start the offline editor: ${error.message}`, true); controls(); }
}
