/* Full application capacity, interaction and complete download in offline Chrome. */
const assert=require('node:assert/strict'),fs=require('node:fs/promises'),path=require('node:path');
const {pathToFileURL}=require('node:url'),{chromium}=require('./browser/node_modules/playwright');
const input=path.resolve(process.argv[2]),directory=path.resolve(process.argv[3]);
const html=path.resolve(process.argv[4]||'tools/savetool/web/index.html');
(async()=>{
  await fs.mkdir(directory,{recursive:true});const browser=await chromium.launch({channel:'chrome',headless:true});
  const report={browser:browser.version(),checks:[],errors:[],hardware:false,
    html_sha256:require('node:crypto').createHash('sha256').update(await fs.readFile(html)).digest('hex')};
  try{
    const context=await browser.newContext({offline:true,locale:'en-US',acceptDownloads:true,viewport:{width:1360,height:1000}}),page=await context.newPage();page.setDefaultTimeout(60000);
    page.on('pageerror',error=>report.errors.push(error.message));page.on('request',request=>{if(/^https?:/.test(request.url()))report.errors.push('Unexpected network request');});
    await page.goto(pathToFileURL(html).href);
    await page.waitForFunction(()=>!document.getElementById('open').disabled);
    async function measured(name,action){const result=await action();report.checks.push({name,result});console.log('PASS '+name);}
    await measured('open complete 256 MiB archive in the application',async()=>{
      const chooser=page.waitForEvent('filechooser');await page.locator('#open').click();await(await chooser).setFiles(input);
      await page.waitForFunction(()=>document.querySelectorAll('#volumes .volume').length===24&&!document.getElementById('add-save').disabled);
      assert.match(await page.locator('#archive-state').textContent(),/256.00 MiB/);return {visible_games:await page.locator('.volume').count()};
    });
    await measured('search all 4095 slots, select final save and apply its date',async()=>{
      await page.locator('#volume-search').fill('GAME-4095');await page.waitForFunction(()=>document.querySelectorAll('#volumes .volume').length===1);
      await page.locator('.volume').click();await page.locator('.save-row').click();await page.locator('#date').fill('10146-02-15T04:15');
      await page.locator('#apply').click();await page.waitForFunction(()=>document.getElementById('message').textContent.includes('Save applied'));
      return {final_save:await page.locator('.save-row strong').textContent()};
    });
    await measured('reject an additional game without changing full directory',async()=>{
      page.once('dialog',dialog=>dialog.accept('EXCESS'));await page.locator('#add-slot').click();
      await page.waitForFunction(()=>document.getElementById('message').textContent.includes('directory is full'));return {rejected:true};
    });
    await measured('export and consume complete browser download',async()=>{
      await page.locator('#export').click();await page.locator('#download').waitFor();const event=page.waitForEvent('download');
      await page.locator('#download').click();const item=await event;await item.saveAs(path.join(directory,'full-edited.bin'));
      assert.equal(await item.failure(),null);return {bytes:(await fs.stat(path.join(directory,'full-edited.bin'))).size};
    });
    const left=await fs.open(input,'r'),right=await fs.open(path.join(directory,'full-edited.bin'),'r');
    const a=Buffer.alloc(1048576),b=Buffer.alloc(1048576),dateOffset=4095*65536+128+28;
    try{for(let offset=0;offset<268435456;offset+=a.length){
      const first=await left.read(a,0,a.length,offset),second=await right.read(b,0,b.length,offset);assert.equal(first.bytesRead,second.bytesRead);
      if(dateOffset>=offset&&dateOffset<offset+first.bytesRead)a.writeUInt32BE(0xffffffff,dateOffset-offset);
      assert.deepEqual(a.subarray(0,first.bytesRead),b.subarray(0,second.bytesRead));
    }}finally{await left.close();await right.close();}
    report.all_unedited_bytes_preserved=true;assert.deepEqual(report.errors,[]);report.passed=true;
    await page.screenshot({path:path.join(directory,'full-archive.png')});await context.close();
  }catch(error){report.passed=false;report.failure=error.stack;throw error;}
  finally{
    await fs.writeFile(path.join(directory,'results.json'),JSON.stringify(report,null,2)+'\n');await browser.close();
  }
})().catch(error=>{console.error(error);process.exitCode=1;});
