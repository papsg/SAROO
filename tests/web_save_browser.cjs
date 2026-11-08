/* Offline Chrome workflows against the complete generated save application. */
const assert=require('node:assert/strict');
const fs=require('node:fs/promises'),path=require('node:path'),{pathToFileURL}=require('node:url');
const {chromium}=require('./browser/node_modules/playwright');
const root=path.resolve(process.argv[2]||'tests/build/web-save-ui-tests');
const html=path.resolve(process.argv[3]||'tools/savetool/web/index.html');
const results=[],errors=[],network=[];let browser,context,page,dialogAction;
const exists=async selector=>page.locator(selector).isVisible();
async function pick(button,input){const chooser=page.waitForEvent('filechooser');await page.locator(button).click();await(await chooser).setFiles(path.join(root,input));}
async function fresh(){
  if(context)await context.close();context=await browser.newContext({offline:true,locale:'en-US',acceptDownloads:true,viewport:{width:1360,height:1000}});
  page=await context.newPage();dialogAction=dialog=>dialog.accept();
  page.on('dialog',dialog=>{void dialogAction(dialog);});page.on('pageerror',error=>errors.push(error.message));
  page.on('request',request=>{if(/^https?:/.test(request.url()))network.push(request.url());});
  await page.goto(pathToFileURL(html).href);await page.locator('#open').waitFor();
  await page.waitForFunction(()=>!document.getElementById('open').disabled);
}
async function open(name){await pick('#open',name);await page.waitForFunction(value=>document.getElementById('archive-name').textContent===value,name);}
async function rows(count){await page.waitForFunction(n=>document.querySelectorAll('#saves .save-row').length===n,count);}
async function editor(index=0){await page.locator(`.save-row[data-index="${index}"]`).click();await page.locator('#editor').waitFor();await page.waitForFunction(()=>!document.getElementById('export-save').disabled);}
async function download(link,name){const event=page.waitForEvent('download');await page.locator(link).click();const item=await event;await item.saveAs(path.join(root,name));return fs.readFile(path.join(root,name));}
async function archive(name){await page.locator('#export').click();await page.locator('#download').waitFor();return download('#download',name);}
async function workflow(name,action){if(process.env.SAROO_BROWSER_CASE&&process.env.SAROO_BROWSER_CASE!==name)return;const start=performance.now();try{await fresh();await action();results.push({name,passed:true,ms:performance.now()-start});console.log('PASS '+name);}catch(error){results.push({name,passed:false,error:error.stack});await page.screenshot({path:path.join(root,'failure.png'),fullPage:true}).catch(()=>{});throw error;}}
(async()=>{
  const {SaveArchive}=await import(pathToFileURL(path.resolve(__dirname,'../tools/savetool/web/src/save-codec.mjs')));
  const {parseRawSave}=await import(pathToFileURL(path.resolve(__dirname,'../tools/savetool/web/src/save-fields.mjs')));
  browser=await chromium.launch({channel:'chrome',headless:true});
  try{
    await workflow('new SAROO, game slot, combined save and whole archive export',async()=>{
      await page.locator('#new-saroo').click();await page.waitForFunction(()=>!document.getElementById('add-slot').hidden);
      dialogAction=dialog=>dialog.accept('MK-81000');await page.locator('#add-slot').click();dialogAction=dialog=>dialog.accept();
      await page.waitForFunction(()=>!document.getElementById('add-save').disabled);await page.locator('#add-save').click();
      await page.locator('#filename').fill('MY_SAVE');await page.locator('#comment').fill('Progress');await page.locator('#date').fill('2000-02-29 12:34');
      await page.locator('#apply').click();await rows(1);assert.match(await page.locator('#message').textContent(),/Save applied/);
      const output=await archive('new-saroo.bin'),decoded=await SaveArchive.open(new Blob([output]));
      assert.equal(decoded.volumeCount,1);assert.equal(decoded.stats(1).saves,1);assert.equal(decoded.read(1,0).length,0);
      assert.equal(new TextDecoder().decode(decoded.metadata(1,0).rawFilename).replace(/\0.*$/s,''),'MY_SAVE');
    });
    await workflow('maximum SAROO save uses the public v0.8 slot layout',async()=>{
      await page.locator('#new-saroo').click();await page.waitForFunction(()=>!document.getElementById('add-slot').hidden);
      dialogAction=dialog=>dialog.accept('GAME_MAX');await page.locator('#add-slot').click();dialogAction=dialog=>dialog.accept();
      await page.waitForFunction(()=>!document.getElementById('add-save').disabled);await page.locator('#add-save').click();
      await pick('#import-save','saroo-maximum.SRO');
      await page.waitForFunction(()=>document.getElementById('save-size').textContent==='65,280 payload bytes');
      await page.locator('#apply').click();
      await page.waitForFunction(()=>document.querySelectorAll('#saves .save-row').length===1||document.getElementById('editor-error').textContent);
      assert.equal(await page.locator('#editor-error').textContent(),'');await rows(1);
      const image=await archive('maximum-saroo.bin'),incoming=await fs.readFile(path.join(root,'saroo-maximum.SRO'));
      assert.equal(image.length,131072);assert.equal(image.readUInt32BE(65536+8),65536);assert.equal(image.readUInt16BE(65536+12),128);
      assert.equal(image.readUInt16BE(65536+62),1);assert.equal(image.readUInt32BE(65536+128+12),65280);
      assert.deepEqual(image.subarray(65536+256),incoming.subarray(64));
    });
    await workflow('unmodified archive download preserves every byte',async()=>{
      await open('mems-preserve.bin');await rows(2);assert.deepEqual(await archive('unchanged.bin'),await fs.readFile(path.join(root,'mems-preserve.bin')));
    });
    await workflow('no-op field apply preserves encoded bytes and dirty state',async()=>{
      await open('mems-preserve.bin');await rows(2);await editor();await page.locator('#apply').click();await rows(2);
      await page.waitForFunction(()=>document.getElementById('message').textContent.includes('No save bytes changed'));
      assert.deepEqual(await archive('no-op.bin'),await fs.readFile(path.join(root,'mems-preserve.bin')));
    });
    await workflow('byte editing reaches the end of the full MEMS payload',async()=>{
      await open('mems-490496-0.bin');await rows(1);await editor();await page.locator('#payload').evaluate(item=>{item.open=true;});
      assert.equal(await page.locator('#hex .hex-byte').count(),256);
      await page.locator('#hex .hex-byte').nth(17).click();assert.equal(await page.locator('#byte-offset').inputValue(),'11');
      await page.locator('#byte-offset').fill('77BFF');await page.locator('#byte-value').fill('4D');await page.locator('#byte-apply').click();
      await page.waitForFunction(()=>document.getElementById('hex-label').textContent==='Page 1916 / 1916');
      assert.match(await page.locator('#hex').textContent(),/077BF0/);await page.locator('#apply').click();await rows(1);
      const decoded=await SaveArchive.open(new Blob([await archive('max-edit.bin')]));assert.equal(decoded.read(0,0,490495,1)[0],77);
    });
    await workflow('full archive rejects replacement without half applying metadata',async()=>{
      await open('saroo-full.bin');await rows(2);await editor();await pick('#import-save','incoming.SRO');
      await page.waitForFunction(()=>document.getElementById('save-size').textContent==='1,757 payload bytes');
      await page.locator('#comment').fill('Changed');await page.locator('#apply').click();
      await page.waitForFunction(()=>document.getElementById('editor-error').textContent.includes('Replacement does not fit'));
      assert(await exists('#editor'));await page.locator('#close-editor').click();
      assert.deepEqual(await archive('full-rejected.bin'),await fs.readFile(path.join(root,'saroo-full.bin')));
    });
    await workflow('raw import and all individual export formats remain usable',async()=>{
      await page.locator('#new-mems').click();await page.waitForFunction(()=>!document.getElementById('add-save').disabled);
      await page.locator('#add-save').click();await pick('#import-save','incoming.SRO');
      await page.waitForFunction(()=>document.getElementById('filename').value==='IMPORTED');
      for(const format of ['SRO','BUP','XML','bin']){
        await page.locator('#save-format').selectOption(format);await page.locator('#export-save').click();
        await page.locator('#save-download-link').waitFor();const data=await download('#save-download-link',`individual.${format}`);
        if(format==='SRO'||format==='BUP')assert.equal(parseRawSave(new Uint8Array(data)).data.length,1757);
        else if(format==='bin')assert.equal(data.length,1757);else assert(data.includes(Buffer.from('<size>1757</size>')));
      }
      await page.locator('#apply').click();await rows(1);
    });
    await workflow('XML import preserves raw authority, full date range and supplemental metadata notice',async()=>{
      await page.locator('#new-mems').click();await page.waitForFunction(()=>!document.getElementById('add-save').disabled);
      await page.locator('#add-save').click();await pick('#import-save','xml/case-7-notes.xml');
      await page.waitForFunction(()=>document.getElementById('save-size').textContent==='490,496 payload bytes');
      assert.equal(await page.locator('#date').inputValue(),'10146-02-15T04:15');assert(await exists('#metadata-note'));
      await page.locator('#save-format').selectOption('XML');await page.locator('#export-save').click();
      await page.locator('#save-download-link').waitFor();await download('#save-download-link','roundtrip.xml');
      await page.locator('#apply').click();await rows(1);
    });
    await workflow('invalid XML and overlong text retain existing draft',async()=>{
      await open('mems-preserve.bin');await rows(2);await editor();const initial=await page.locator('#filename').inputValue();
      await pick('#import-save','bad.xml');await page.waitForFunction(()=>document.getElementById('editor-error').textContent.length>0);
      assert.equal(await page.locator('#filename').inputValue(),initial);
      await page.locator('#filename').fill('FILENAME_TOO_LONG');await page.locator('#apply').click();
      await page.waitForFunction(()=>document.getElementById('editor-error').textContent.includes('capacity'));
      await page.locator('#close-editor').click();assert.deepEqual(await archive('invalid-retained.bin'),await fs.readFile(path.join(root,'mems-preserve.bin')));
    });
    await workflow('streaming import can be canceled and the same dialog remains usable',async()=>{
      await open('mems-preserve.bin');await rows(2);await editor();const initial=await page.locator('#filename').inputValue();
      await pick('#import-save','slow.xml');await page.locator('#editor-cancel-operation').click();
      await page.waitForFunction(()=>document.getElementById('editor-activity').hidden);
      assert.equal(await page.locator('#filename').inputValue(),initial);
      await page.locator('#apply').click();await rows(2);await page.waitForFunction(()=>document.getElementById('message').textContent.includes('No save bytes changed'));
    });
    await workflow('read-only Saturn backup permits exports and blocks all write controls',async()=>{
      await open('saturn-145-1.bin');await rows(1);assert(await page.locator('#add-save').isDisabled());await editor();
      for(const selector of ['#filename','#comment','#date','#language'])assert(await page.locator(selector).isDisabled());
      assert(!await exists('#apply'));assert(!await exists('#import-controls'));
      await page.locator('#save-format').selectOption('BUP');await page.locator('#export-save').click();await page.locator('#save-download-link').waitFor();
      assert.equal(parseRawSave(new Uint8Array(await download('#save-download-link','saturn-export.BUP'))).data.length,145);
      await page.locator('#close-editor').click();assert.deepEqual(await archive('saturn-unchanged.bin'),await fs.readFile(path.join(root,'saturn-145-1.bin')));
    });
    await workflow('damaged slot does not hide healthy games or change damaged bytes',async()=>{
      await open('damaged-slot.bin');await page.locator('#slot-error').waitFor();assert(await page.locator('#add-save').isDisabled());
      await page.locator('.volume[data-volume="3"]').click();await page.waitForFunction(()=>!document.getElementById('add-save').disabled);
      assert(!await exists('#slot-error'));assert.deepEqual(await archive('damaged-unchanged.bin'),await fs.readFile(path.join(root,'damaged-slot.bin')));
    });
    await workflow('full directory paging, filtering and literal metadata rendering',async()=>{
      await open('mems-directory-full.bin');await rows(24);await page.locator('#save-search').fill('S0000000447');await rows(1);
      assert.equal(await page.locator('.save-row').getAttribute('data-index'),'447');await editor(447);await page.screenshot({path:path.join(root,'save-dialog.png')});
      await page.locator('#close-editor').click();await page.locator('#save-search').fill('');await rows(24);
      await page.screenshot({path:path.join(root,'save-library.png'),fullPage:true});
      await open('literal.bin');await rows(1);assert.equal(await page.locator('.save-row strong').textContent(),'<save>&');
      assert.equal(await page.locator('.save-row save').count(),0);
    });
    await workflow('dirty archive replacement can be declined and failed open preserves the workspace',async()=>{
      await page.locator('#new-mems').click();await page.waitForFunction(()=>!document.getElementById('add-save').disabled);
      await page.locator('#add-save').click();await page.locator('#apply').click();await rows(1);
      dialogAction=dialog=>dialog.dismiss();await page.locator('#new-saroo').click();await rows(1);
      assert.equal(await page.locator('#archive-name').textContent(),'SS_MEMS.BIN');
      dialogAction=dialog=>dialog.accept();await pick('#open','bad.xml');await page.waitForFunction(()=>document.getElementById('message').classList.contains('error'));
      await rows(1);assert.equal(await page.locator('#archive-name').textContent(),'SS_MEMS.BIN');
    });
    await workflow('late file selection cannot target a replacement dialog',async()=>{
      await open('mems-preserve.bin');await rows(2);await editor();
      const event=page.waitForEvent('filechooser');await page.locator('#import-save').click();const chooser=await event;
      await page.locator('#close-editor').click();await editor(1);
      await chooser.setFiles(path.join(root,'incoming.SRO'));
      assert.equal(await page.locator('#filename').inputValue(),'NEIGHBOR001');
      await page.locator('#apply').click();await page.waitForFunction(()=>document.getElementById('message').textContent.includes('No save bytes changed'));
      assert.deepEqual(await archive('late-selection.bin'),await fs.readFile(path.join(root,'mems-preserve.bin')));
    });
    await workflow('closing during XML import rejects late publication and allows the next dialog',async()=>{
      await open('mems-preserve.bin');await rows(2);await editor();await pick('#import-save','slow.xml');
      await page.locator('#close-editor').click();await page.waitForFunction(()=>!document.getElementById('open').disabled);
      await editor(1);assert.equal(await page.locator('#filename').inputValue(),'NEIGHBOR001');
      await page.locator('#close-editor').click();assert.deepEqual(await archive('closed-import.bin'),await fs.readFile(path.join(root,'mems-preserve.bin')));
    });
    await workflow('worker error preserves a completed archive download',async()=>{
      await open('mems-preserve.bin');await rows(2);await page.locator('#export').click();await page.locator('#download').waitFor();
      await page.workers()[0].evaluate(()=>{setTimeout(()=>{throw new Error('Deliberate fixture worker failure');},0);});
      await page.waitForFunction(()=>document.getElementById('message').textContent.includes('worker stopped'));
      assert(await page.locator('#open').isDisabled());assert(await exists('#download'));
      assert.deepEqual(await download('#download','worker-error.bin'),await fs.readFile(path.join(root,'mems-preserve.bin')));
    });
    await workflow('Japanese entry, raw-byte viewing and mobile controls preserve encoded fields',async()=>{
      await open('mems-preserve.bin');await rows(2);await editor();
      await page.locator('#comment').fill('ソニック');await page.locator('#language').fill('255');
      await page.locator('#encoding').selectOption('bytes');await page.waitForFunction(()=>document.getElementById('comment').value.startsWith('%83'));
      assert.equal(await page.locator('#comment').inputValue(),'%83\\%83j%83b%83N%00%00');
      await page.locator('#encoding').selectOption('shift-jis');await page.waitForFunction(()=>document.getElementById('comment').value==='ソニック');
      await page.setViewportSize({width:390,height:844});await page.locator('#payload').evaluate(item=>{item.open=true;});
      assert.equal(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth),true);
      await page.locator('#apply').scrollIntoViewIfNeeded();await page.screenshot({path:path.join(root,'save-mobile.png')});
      await page.locator('#apply').click();await page.locator('#editor').waitFor({state:'hidden'});
      const decoded=await SaveArchive.open(new Blob([await archive('japanese.bin')]));
      assert.deepEqual([...decoded.metadata(0,0).rawComment],[0x83,0x5c,0x83,0x6a,0x83,0x62,0x83,0x4e,0,0]);assert.equal(decoded.metadata(0,0).language,255);
    });
    assert.deepEqual(errors,[]);assert.deepEqual(network,[]);
  }finally{
    await fs.writeFile(path.join(root,'results.json'),JSON.stringify({browser:browser.version(),html,
      source_sha256:require('node:crypto').createHash('sha256').update(await fs.readFile(html)).digest('hex'),
      results,pageErrors:errors,networkRequests:network},null,2)+'\n');
    if(context)await context.close();await browser.close();
  }
})().catch(error=>{console.error(error);process.exitCode=1;});
