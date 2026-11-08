/* Delayed native file selections must retain their original workspace/dialog. */
const assert=require('node:assert/strict'),fs=require('node:fs/promises'),path=require('node:path');
const {createHash}=require('node:crypto');
const {pathToFileURL}=require('node:url'),{chromium}=require('./browser/node_modules/playwright');
const html=path.resolve(process.argv[2]),fixtures=path.resolve(process.argv[3]),output=path.resolve(process.argv[4]);
(async()=>{
  const browser=await chromium.launch({channel:'chrome',headless:true}),checks=[];
  try{
    for(const kind of ['image','disc','library','add']){
      const context=await browser.newContext({offline:true});
      await context.addInitScript(()=>{window.calls=[];const Native=window.Worker;
        window.Worker=class extends Native {postMessage(value,...rest){calls.push({op:value.op,token:value.token});super.postMessage(value,...rest);}};});
      const page=await context.newPage();await page.goto(pathToFileURL(html).href);
      const idle=()=>page.waitForFunction(()=>!document.getElementById('open').disabled);await idle();
      const initial=page.waitForEvent('filechooser');await page.locator('#open').click();await(await initial).setFiles(path.join(fixtures,'library.bin'));
      await page.waitForFunction(()=>document.querySelectorAll('#grid .card').length===24);
      if(kind==='image'||kind==='disc'){
        await page.locator('#grid .card').first().click();await page.locator('#editor').waitFor();await idle();
        const chosen=page.waitForEvent('filechooser');await page.locator(kind==='image'?'#choose-image':'#read-disc').click();const chooser=await chosen;
        await page.locator('#close-editor').click();await page.locator('#grid .card').nth(1).click();await idle();
        await page.evaluate(()=>{calls=[];});await chooser.setFiles(path.join(fixtures,kind==='image'?'cover_COLOR~11111111.png':'disc.bin'));
        assert.deepEqual(await page.evaluate(()=>calls),[],`Late ${kind} selection dispatched work to the replacement dialog`);
        assert.equal(await page.locator('#serial').inputValue(),'GAME00000001');
      }else{
        const chosen=page.waitForEvent('filechooser');await page.locator(kind==='library'?'#open':'#add').click();const chooser=await chosen;
        await page.locator('#new').click();await idle();await page.evaluate(()=>{calls=[];});
        await chooser.setFiles(path.join(fixtures,kind==='library'?'library.bin':'cover_COLOR~11111111.png'));
        assert.deepEqual(await page.evaluate(()=>calls),[],`Late ${kind} selection dispatched work to the replacement workspace`);
        assert.match(await page.locator('#library-state').textContent(),/^0 \/ 16,384/);
      }
      checks.push({kind,passed:true});console.log('PASS late '+kind+' selection');await context.close();
    }
  }finally{await fs.writeFile(output,JSON.stringify({browser:browser.version(),html,html_sha256:createHash('sha256').update(await fs.readFile(html)).digest('hex'),checks,passed:checks.length===4&&checks.every(c=>c.passed),hardware:false},null,2)+'\n');await browser.close();}
})().catch(error=>{console.error(error);process.exitCode=1;});
