/* Offline rendered-card ownership regressions against the real DOM and worker.
 * Delay only delivery of completed list replies; calculations stay unchanged.
 * Usage: node tests/web_cover_render.cjs COVER_HTML OUTPUT
 */
const assert=require('node:assert/strict'), fs=require('node:fs/promises'), path=require('node:path');
const {pathToFileURL}=require('node:url'), {createHash}=require('node:crypto');
const {chromium}=require('./browser/node_modules/playwright');
const [html,output]=process.argv.slice(2).map(value=>path.resolve(value));
function library(prefix) {
  const bytes=Buffer.alloc(524288+3*1025);
  for(let i=0;i<3;i++) {
    const at=i*32; bytes.write(prefix+i,at,'ascii'); bytes.writeUInt32LE(i+1,at+12);
    bytes.writeUInt32LE(524288+i*1025,at+16); bytes.writeUInt16LE(1,at+20); bytes.writeUInt16LE(1,at+22);
    bytes.fill(20+i,524288+i*1025,524288+(i+1)*1025);
  }
  return bytes;
}
(async()=>{
  await fs.mkdir(output,{recursive:true});
  for(const prefix of ['OLD','NEW']) await fs.writeFile(path.join(output,prefix+'.bin'),library(prefix));
  const browser=await chromium.launch({channel:process.env.SAROO_BROWSER_CHANNEL||'chrome',headless:true});
  const report={browser:browser.version(),html_sha256:createHash('sha256').update(await fs.readFile(html)).digest('hex'),
                cases:[],errors:[],requests:[],hardware:false};
  try {
    for(const kind of ['workspace','revision']) {
      const context=await browser.newContext({offline:true,locale:'en-US',viewport:{width:1200,height:950}});
      try {
        await context.addInitScript(()=>{
          window.audit={hold:false,held:[],operations:[]}; const Native=window.Worker;
          window.Worker=class extends Native {
            constructor(...args) {
              super(...args); this.operations=new Map();
              super.addEventListener('message',event=>{
                if(window.audit.hold&&this.operations.get(event.data.id)==='list'&&!event.data.progress) {
                  window.audit.held.push({owner:this,data:event.data}); event.stopImmediatePropagation();
                }
              });
            }
            postMessage(data,...args) {
              const {id,op,stamp,index}=data; window.audit.operations.push({id,op,stamp,index});
              this.operations.set(id,op); return super.postMessage(data,...args);
            }
          };
          window.audit.release=()=>{
            window.audit.hold=false;
            for(const row of window.audit.held.splice(0)) row.owner.dispatchEvent(new MessageEvent('message',{data:row.data}));
          };
        });
        const page=await context.newPage(); page.setDefaultTimeout(10000);
        page.on('pageerror',error=>report.errors.push(error.message));
        page.on('request',request=>{if(/^https?:/.test(request.url()))report.requests.push(request.url());});
        const idle=()=>page.waitForFunction(()=>!document.getElementById('open').disabled);
        const pick=async name=>{
          const event=page.waitForEvent('filechooser'); await page.locator('#open').click();
          await(await event).setFiles(path.join(output,name+'.bin'));
        };
        await page.goto(pathToFileURL(html).href); await idle(); await pick('OLD');
        await page.waitForFunction(()=>document.querySelector('#grid .serial')?.textContent==='OLD0');
        if(kind==='revision') {
          await page.locator('#grid .card').first().click(); await page.locator('#editor').waitFor({state:'visible'}); await idle();
          await page.evaluate(()=>{audit.hold=true;}); page.once('dialog',dialog=>dialog.accept());
          await page.locator('#delete').click(); await page.locator('#editor').waitFor({state:'hidden'});
        } else { await page.evaluate(()=>{audit.hold=true;}); await pick('NEW'); }
        await page.waitForFunction(()=>audit.held.length>0); await idle();
        const stale=page.locator('#grid .card').nth(kind==='revision'?1:0);
        const before={visible:await stale.innerText(),disabled:await stale.isDisabled()};
        const count=()=>page.evaluate(()=>audit.operations.filter(row=>row.op==='editorStart').length);
        const prior=await count(); if(!before.disabled)await stale.click(); await idle();
        const opened=await page.locator('#editor').evaluate(dialog=>dialog.open), dispatched=await count()-prior;
        const wrongSelection=opened?await page.locator('#serial').inputValue():null;
        await page.screenshot({path:path.join(output,kind+'-pending.png')});
        if(opened)await page.locator('#close-editor').click(); await page.evaluate(()=>audit.release());
        const expected=kind==='revision'?'OLD1':'NEW0';
        await page.waitForFunction(expected=>document.querySelector('#grid .serial')?.textContent===expected,expected);
        await page.locator('#grid .card').first().click(); await page.locator('#editor').waitFor({state:'visible'}); await idle();
        const current=await page.locator('#serial').inputValue();
        report.cases.push({kind,before,wrongSelection,dispatched,current,expected,passed:!opened&&dispatched===0&&current===expected});
        console.log('CHECK',kind,JSON.stringify(report.cases.at(-1)));
      } finally {await context.close();}
    }
    assert.deepEqual(report.errors,[]); assert.deepEqual(report.requests,[]);
    report.passed=report.cases.length===2&&report.cases.every(item=>item.passed); assert.equal(report.passed,true);
  } catch(error) {report.passed=false; report.failure=error.stack; throw error;}
  finally {await fs.writeFile(path.join(output,'results.json'),JSON.stringify(report,null,2)+'\n');await browser.close();}
})().catch(error=>{console.error(error);process.exitCode=1;});
