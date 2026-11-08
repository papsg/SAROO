/* Offline save-list ownership regressions against real DOM and worker replies.
 * Only delivery of already computed list replies is delayed. No app/worker
 * calculation, record, stamp or selected index is substituted by the observer.
 * Usage: node tests/web_save_render.cjs SAVE_HTML FIXTURES OUTPUT
 */
const assert = require('node:assert/strict');
const fs = require('node:fs/promises');
const path = require('node:path');
const { pathToFileURL } = require('node:url');
const { chromium } = require('./browser/node_modules/playwright');
const [saveHTML, fixtures, output] = process.argv.slice(2).map(value => path.resolve(value));

(async () => {
  await fs.mkdir(output, { recursive: true });
  const browser = await chromium.launch({ channel: process.env.SAROO_BROWSER_CHANNEL || 'chrome', headless: true });
  const report = { browser: browser.version(), cases: [], errors: [], requests: [], hardware: false };
  try {
    for (const kind of ['save-game', 'save-edit-switch']) {
      const context = await browser.newContext({ offline: true, locale: 'en-US', viewport: { width: 1200, height: 950 } });
      try {
        await context.addInitScript(() => {
          window.audit = { hold: null, held: [], operations: [] };
          const Native = window.Worker;
          window.Worker = class extends Native {
            constructor(...args) {
              super(...args); this.operations = new Map();
              super.addEventListener('message', event => {
                if (window.audit.hold && this.operations.get(event.data.id) === window.audit.hold && !event.data.progress) {
                  window.audit.held.push({ owner: this, data: event.data }); event.stopImmediatePropagation();
                }
              });
            }
            postMessage(data, ...args) {
              const { id, op, stamp, volume, index } = data;
              window.audit.operations.push({ id, op, stamp, volume, index });
              this.operations.set(id, op); return super.postMessage(data, ...args);
            }
          };
          window.audit.release = () => {
            window.audit.hold = null;
            for (const row of window.audit.held.splice(0)) row.owner.dispatchEvent(new MessageEvent('message', { data: row.data }));
          };
        });
        const page = await context.newPage(); page.setDefaultTimeout(10000);
        page.on('pageerror', error => report.errors.push(error.message));
        page.on('request', request => { if (/^https?:/.test(request.url())) report.requests.push(request.url()); });
        const card = '.save-row', label = '#saves strong', field = '#filename';
        const idle = () => page.waitForFunction(() => !document.getElementById('open').disabled);
        const pick = async file => {
          const choosing = page.waitForEvent('filechooser'); await page.locator('#open').click();
          await (await choosing).setFiles(file);
        };
        await page.goto(pathToFileURL(saveHTML).href); await idle();
        await pick(path.join(fixtures, 'two-games.bin'));
        await page.waitForFunction(({ label, text }) => document.querySelector(label)?.textContent === text,
                                   { label, text: 'FIRST' });
        if (kind === 'save-edit-switch') {
          await page.locator(card).first().click(); await page.locator('#editor').waitFor({ state: 'visible' }); await idle();
          await page.locator('#comment').fill('Edited'); await page.locator('#apply').click();
          await page.locator('#editor').waitFor({ state: 'hidden' });
          await page.waitForFunction(() => document.querySelector('#saves strong')?.textContent === 'FIRST' && !document.querySelector('.save-row').disabled);
          const disabled = await page.locator('[data-volume="2"]').isDisabled();
          if (!disabled) {
            await page.locator('[data-volume="2"]').click();
            await page.waitForFunction(() => document.querySelector('#saves strong')?.textContent === 'SECOND');
          }
          const current = await page.locator(label).first().textContent();
          const result = { kind, gameDisabledAfterEdit: disabled, current, expected: 'SECOND', passed: !disabled && current === 'SECOND' };
          report.cases.push(result); console.log(JSON.stringify(result)); continue;
        }
        await page.evaluate(() => { window.audit.hold = 'volumes'; });
        await page.locator('[data-volume="2"]').click();
        await page.waitForFunction(() => window.audit.held.length > 0); await idle();
        const stale = page.locator(card).first();
        const before = { visible: await stale.innerText(), disabled: await stale.isDisabled() };
        const requestsBefore = await page.evaluate(() => window.audit.operations.filter(row => row.op === 'editorStart').length);
        if (!before.disabled) await stale.click();
        await idle();
        const opened = await page.locator('#editor').evaluate(dialog => dialog.open);
        const wrongSelection = opened ? await page.locator(field).inputValue() : null;
        const requestsAfter = await page.evaluate(() => window.audit.operations.filter(row => row.op === 'editorStart').length);
        await page.screenshot({ path: path.join(output, kind + '-pending.png') });
        if (opened) await page.locator('#close-editor').click();
        await page.evaluate(() => window.audit.release());
        const fresh = 'SECOND';
        await page.waitForFunction(({ label, text }) => document.querySelector(label)?.textContent === text, { label, text: fresh });
        await page.locator(card).first().click(); await page.locator('#editor').waitFor({ state: 'visible' }); await idle();
        const current = await page.locator(field).inputValue();
        const result = { kind, before, wrongSelection, staleRequestCount: requestsAfter - requestsBefore,
                         current, expected: fresh, passed: !opened && requestsAfter === requestsBefore && current === fresh };
        report.cases.push(result); console.log(JSON.stringify(result));
        report[kind + '-operations'] = await page.evaluate(() => window.audit.operations);
      } finally { await context.close(); }
    }
    assert.deepEqual(report.errors, []); assert.deepEqual(report.requests, []);
    report.passed = report.cases.length === 2 && report.cases.every(item => item.passed);
    if (!report.passed) process.exitCode = 2;
  } finally {
    await fs.writeFile(path.join(output, 'results.json'), JSON.stringify(report, null, 2) + '\n');
    await browser.close();
  }
})().catch(error => { console.error(error); process.exitCode = 1; });
