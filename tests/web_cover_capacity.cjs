/* Complete offline application check against a synthetic 400.5 MiB library.
 * Usage: node tests/web_cover_capacity.cjs INPUT OUTPUT [COVER_HTML]
 * Compare every byte except the deliberately changed final serial field.
 */
const assert = require('node:assert/strict');
const fs = require('node:fs');
const fsp = require('node:fs/promises');
const path = require('node:path');
const crypto = require('node:crypto');
const { pathToFileURL } = require('node:url');
const { chromium } = require('./browser/node_modules/playwright');
const input = path.resolve(process.argv[2]), output = path.resolve(process.argv[3]);
const html = path.resolve(process.argv[4] || path.join(__dirname, '../tools/covertool/web/index.html'));
async function digest(file) {
  const hash = crypto.createHash('sha256');
  for await (const chunk of fs.createReadStream(file)) hash.update(chunk);
  return hash.digest('hex');
}
(async () => {
  assert.equal((await fsp.stat(input)).size, 419954688);
  await fsp.mkdir(output, { recursive: true });
  const report = { input: { bytes: 419954688, sha256: await digest(input) },
                   html_sha256: await digest(html), checks: [], errors: [], requests: [], hardware: false };
  const browser = await chromium.launch({ channel: process.env.SAROO_BROWSER_CHANNEL || 'chrome', headless: true });
  try {
    report.browser = browser.version();
    const context = await browser.newContext({ offline: true, acceptDownloads: true, locale: 'en-US',
                                              viewport: { width: 1200, height: 950 } });
    const page = await context.newPage(); page.setDefaultTimeout(60000);
    page.on('pageerror', error => report.errors.push(error.message));
    page.on('request', request => { if (/^https?:/.test(request.url())) report.requests.push(request.url()); });
    const idle = () => page.waitForFunction(() => !document.getElementById('open').disabled);
    const pick = async () => {
      const choice = page.waitForEvent('filechooser'); await page.locator('#open').click();
      await (await choice).setFiles(input);
    };
    async function check(name, action) {
      await action(); report.checks.push({ name, passed: true }); console.log('PASS', name);
    }
    await page.goto(pathToFileURL(html).href); await idle();
    await check('canceled complete-file open preserves the empty workspace', async () => {
      await pick(); await page.locator('#activity').waitFor({ state: 'visible' });
      await page.locator('#cancel').click(); await idle();
      assert.match(await page.locator('#library-state').innerText(), /^0 \/ 16,384/);
    });
    await check('open all 16,384 unique maximum-size covers with only 24 visible canvases', async () => {
      await pick(); await idle(); await page.waitForFunction(() => document.querySelectorAll('#grid .card').length === 24);
      assert.match(await page.locator('#library-state').innerText(), /^16,384 \/ 16,384/);
      assert.equal(await page.locator('#grid canvas').count(), 24);
    });
    const handle = await fsp.open(input, 'r'), header = Buffer.alloc(32);
    try { await handle.read(header, 0, 32, 16383*32); } finally { await handle.close(); }
    const lastSerial = header.subarray(0, 12).toString('ascii').replace(/\0.*$/, '');
    await check('find and edit the final record then return to the first page', async () => {
      await page.locator('#search').fill(lastSerial);
      await page.waitForFunction(() => document.querySelectorAll('#grid .card').length === 1);
      assert.equal(await page.locator('#grid .serial').innerText(), lastSerial);
      await page.locator('#grid .card').click(); await page.locator('#serial').fill('FINAL-ENTRY');
      await page.locator('#save-editor').click(); await page.locator('#editor').waitFor({ state: 'hidden' }); await idle();
      await page.locator('#search').fill(''); await page.waitForFunction(() => document.querySelectorAll('#grid .card').length === 24);
    });
    await check('download the complete edited library', async () => {
      await page.locator('#export').click(); await idle(); await page.locator('#library-download').waitFor({ state: 'visible' });
      const event = page.waitForEvent('download'); await page.locator('#download').click();
      const item = await event; await item.saveAs(path.join(output, 'full-edited.bin'));
      assert.equal(await item.failure(), null);
    });
    const edited = path.join(output, 'full-edited.bin');
    report.output = { bytes: (await fsp.stat(edited)).size, sha256: await digest(edited) };
    assert.equal(report.output.bytes, report.input.bytes);
    const a = await fsp.open(input, 'r'), b = await fsp.open(edited, 'r');
    const left = Buffer.alloc(1048576), right = Buffer.alloc(1048576);
    try {
      for (let offset=0;offset<report.input.bytes;offset+=left.length) {
        const ar=await a.read(left,0,left.length,offset), br=await b.read(right,0,right.length,offset);
        assert.equal(ar.bytesRead, br.bytesRead);
        if (!offset) {
          const position=16383*32; left.fill(0,position,position+12); left.write('FINAL-ENTRY',position,'ascii');
        }
        assert.deepEqual(left.subarray(0,ar.bytesRead), right.subarray(0,br.bytesRead));
      }
    } finally { await a.close(); await b.close(); }
    report.all_unedited_bytes_preserved = true;
    assert.deepEqual(report.errors, []); assert.deepEqual(report.requests, []); report.passed = true;
    await page.screenshot({ path: path.join(output, 'full-library.png') });
  } catch (error) { report.passed = false; report.failure = error.stack; throw error; }
  finally {
    await fsp.writeFile(path.join(output, 'results.json'), JSON.stringify(report, null, 2)+'\n'); await browser.close();
  }
})().catch(error => { console.error(error); process.exitCode = 1; });
