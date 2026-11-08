import assert from 'node:assert/strict';
import { test } from 'node:test';
import { readFile } from 'node:fs/promises';
import { createRequire } from 'node:module';
import { createSaveWorker } from '../tools/savetool/web/src/save-worker.mjs';
import { SaveArchive } from '../tools/savetool/web/src/save-codec.mjs';
import { encodeRawSave, parseRawSave, fixedBytes } from '../tools/savetool/web/src/save-fields.mjs';
import { parseXMLSave } from '../tools/savetool/web/src/save-xml.mjs';
const sax=createRequire(import.meta.url)(process.env.SAROO_SAVE_SAX);
const fixture=async name=>new File([await readFile(`${process.env.SAROO_SAVE_WORKER_FIXTURES}/${name}`)],name);
const bytes=async blob=>new Uint8Array(await blob.arrayBuffer());
const fields=label=>({filename:label,comment:'Comment',language:255,date:'2000-02-29T12:34',encoding:'shift-jis'});
const raw=(length=3)=>new File([encodeRawSave({rawFilename:fixedBytes(new TextEncoder().encode('INCOMING'),11),
  rawComment:fixedBytes(new TextEncoder().encode('New'),10),language:2,date:1234,data:new Uint8Array(length).fill(19)},'SRO')],'incoming.SRO');
function harness() {
  let state={epoch:0,revision:0},sequence=0,hook=()=>{};const pending=new Map();
  const receive=createSaveWorker(value=>{
    const copy=structuredClone(value);hook(copy);
    if(copy.progress)return;
    state=copy.state;const done=pending.get(copy.id);pending.delete(copy.id);done?.(copy);
  },sax);
  function send(op,data={}) {
    const id=++sequence,promise=new Promise(resolve=>pending.set(id,resolve));
    void receive({op,id,stamp:{...state},...data});return {id,promise};
  }
  async function call(op,data={}) {
    const response=await send(op,data).promise;
    if(response.error)throw Object.assign(new Error(response.error),{name:response.name??'Error'});
    return response.result;
  }
  return {send,call,get state(){return state;},set hook(value){hook=value;},
    close:token=>receive({op:'editorClose',token}),cancel:target=>receive({op:'cancel',target})};
}

test('combined edits, cancel, whole exports and all four individual formats preserve native fields',async()=>{
  const h=harness();await h.call('open',{file:await fixture('mems-preserve.bin')});
  const original=await bytes((await h.call('export')).blob);
  let draft=await h.call('editorStart',{volume:0,index:0,token:1});
  assert.equal(draft.size,129);await h.call('editorByte',{token:1,offset:128,value:77});await h.close(1);
  assert.deepEqual(await bytes((await h.call('export')).blob),original);assert.equal(h.state.dirty,false);
  draft=await h.call('editorStart',{volume:0,index:0,token:2});
  const unchanged={...draft.fields,encoding:draft.encoding};
  for(const format of ['SRO','BUP','XML','bin']) {
    const output=await h.call('saveExport',{token:2,format,fields:unchanged});
    const input=format==='XML'?await parseXMLSave(output.blob,sax):format==='bin'?null:parseRawSave(await bytes(output.blob));
    if(input){assert.equal(input.rawFilename[2],80);assert.equal(input.rawComment[6],255);assert.equal(input.data.length,129);}
    else assert.equal(output.blob.size,129);
  }
  await h.call('editorByte',{token:2,offset:128,value:77});
  const result=await h.call('editorCommit',{token:2,fields:{...unchanged,date:'2000-02-29T12:34'}});
  assert(result.changed);const reopened=await SaveArchive.open((await h.call('export')).blob);
  assert.equal(reopened.read(0,0)[128],77);assert.equal(reopened.metadata(0,0).rawFilename[2],80);
  assert.equal(reopened.metadata(0,0).rawComment[6],255);assert.equal(reopened.stats(0).saves,2);
  assert.equal(new TextDecoder().decode(reopened.read(0,1)),'neighbor'.repeat(19));
});

test('failed capacity replacement applies neither metadata nor payload and leaves a recoverable draft',async()=>{
  for(const type of ['saroo','mems']){
    const h=harness();await h.call('open',{file:await fixture(`${type}-full.bin`)});
    const volume=type==='saroo'?1:0,original=await bytes((await h.call('export')).blob),revision=h.state.revision;
    const draft=await h.call('editorStart',{volume,index:0,token:1});
    await h.call('editorImport',{token:1,file:raw(2049)});
    await assert.rejects(h.call('editorCommit',{token:1,fields:{...draft.fields,encoding:draft.encoding,date:'2000-02-29T12:34'}}),/Replacement does not fit/);
    assert.equal(h.state.revision,revision);assert.deepEqual(await bytes((await h.call('export')).blob),original);
    assert.equal((await h.call('editorRange',{token:1,page:8})).bytes[0],19);
    await h.close(1);assert.deepEqual(await bytes((await h.call('export')).blob),original);
  }
});

test('failed file/field edits retain draft contents; unchanged duplicate encodings and padding survive',async()=>{
  const h=harness();await h.call('new',{type:'mems'});await h.call('editorStart',{volume:0,token:1});
  await h.call('editorImport',{token:1,file:raw()});
  await assert.rejects(h.call('editorFields',{token:1,fields:fields('THIS_NAME_DOES_NOT_FIT')}),/capacity|bytes/);
  await assert.rejects(h.call('editorImport',{token:1,file:new File(['invalid'],'bad.xml')}));
  assert.equal((await h.call('editorRange',{token:1,page:0})).bytes[0],19);
  await h.call('editorCommit',{token:1});
  const archive=await SaveArchive.open((await h.call('export')).blob);
  assert.equal(new TextDecoder().decode(archive.metadata(0,0).rawFilename).replace(/\0.*$/s,''),'INCOMING');
  assert.equal(archive.metadata(0,0).date,1234);
});

test('new/imported identities cannot silently replace an existing save',async()=>{
  const h=harness();await h.call('new',{type:'mems'});
  for(let token=1;token<=2;token++) {
    await h.call('editorStart',{volume:0,token});await h.call('editorImport',{token,file:raw()});
    if(token===1)await h.call('editorCommit',{token});
    else await assert.rejects(h.call('editorCommit',{token}),/already exists/);
  }
  assert.equal((await h.call('saves',{volume:0})).total,1);await h.close(2);
  let draft=await h.call('editorStart',{volume:0,index:0,token:3});
  await h.call('editorFields',{token:3,fields:fields('KEEP')});
  draft=await h.call('editorImport',{token:3,file:raw(17)});assert.equal(draft.fields.filename,'KEEP');assert.equal(draft.fields.language,255);
  draft=await h.call('editorImport',{token:3,file:raw(17),useMetadata:true});assert.equal(draft.fields.filename,'INCOMING');assert.equal(draft.fields.language,2);
});

test('all native capacities are pageable, including bytes past the former 32 KiB truncation',async()=>{
  for(const [type,count,volume] of [['saroo',511,1],['mems',448,0],['saturn',510,0]]){
    const h=harness();await h.call('open',{file:await fixture(`${type}-directory-full.bin`)});
    const last=await h.call('saves',{volume,page:1000});assert.equal(last.total,count);
    assert.equal(last.items.at(-1).index,count-1);assert(last.items.length<=24);
  }
  const h=harness();await h.call('open',{file:await fixture('mems-490496-0.bin')});
  await h.call('editorStart',{volume:0,index:0,token:1});
  const last=await h.call('editorRange',{token:1,page:1915});assert.equal(last.offset,490240);assert.equal(last.bytes.length,256);
  await h.call('editorByte',{token:1,offset:490495,value:77});await h.call('editorCommit',{token:1});
  const reopened=await SaveArchive.open((await h.call('export')).blob);assert.equal(reopened.read(0,0,490495,1)[0],77);
});

test('a damaged SAROO slot leaves other volumes usable without formatting the damaged one',async()=>{
  const file=await fixture('saroo-preserve.bin'),image=await bytes(file);image[65536]=0;
  const h=harness();await h.call('open',{file:new File([image],'damaged.bin')});
  assert.equal((await h.call('volumes')).total,2);
  await assert.rejects(h.call('saves',{volume:1}),/signature|format|SaroSave/i);
  assert.equal((await h.call('saves',{volume:3})).total,0);
  assert.deepEqual(await bytes((await h.call('export')).blob),image);
});

test('physical Saturn archives remain export-only at every mutating worker entry',async()=>{
  const h=harness();await h.call('open',{file:await fixture('saturn-145-1.bin')});
  assert.equal(h.state.readOnly,true);await h.call('editorStart',{volume:0,index:0,token:1});
  for(const [op,data] of [['slot',{gameId:'GAME'}],['editorFields',{fields:fields('A')}],['editorByte',{offset:0,value:0}],
    ['editorImport',{file:raw()}],['editorCommit',{}],['editorDelete',{}],['editorStart',{volume:0,index:null}]])
    await assert.rejects(h.call(op,{token:1,...data}),/read-only/);
  assert((await h.call('saveExport',{token:1,format:'SRO'})).blob.size>64);
});

test('canceled open retains old workspace and cannot overlap another mutation',async()=>{
  const h=harness();await h.call('new',{type:'mems'});let release;
  const gate=new Promise(resolve=>{release=resolve;});
  class SlowFile extends File {async arrayBuffer(){await gate;return super.arrayBuffer();}}
  const state={...h.state},request=h.send('open',{file:new SlowFile([await fixture('saroo-empty.bin')],'slow.bin')});
  await assert.rejects(h.call('new',{type:'saroo'}),/already running/);h.cancel(request.id);release();
  assert.equal((await request.promise).name,'AbortError');assert.deepEqual(h.state,state);
  await h.call('new',{type:'saroo'});assert.equal(h.state.epoch,state.epoch+1);
  await assert.rejects(h.call('slot',{stamp:state,gameId:'STALE'}),/workspace changed/);
});

test('closed dialog cancels streaming import without publishing into the next save',async()=>{
  const h=harness();await h.call('new',{type:'mems'});await h.call('editorStart',{volume:0,token:1});
  await h.call('editorImport',{token:1,file:raw(3)});
  const input=new File(['<Sega_Saturn_Save><mandatory><name>OTHER</name><data>',' '.repeat(4*1048576),
    'AQID</data></mandatory></Sega_Saturn_Save>'],'slow.xml');
  let closed=false;h.hook=value=>{if(value.progress&&!closed){closed=true;void h.close(1);}};
  await assert.rejects(h.call('editorImport',{token:1,file:input}),{name:'AbortError'});assert(closed);
  h.hook=()=>{};const draft=await h.call('editorStart',{volume:0,token:2});assert.equal(draft.fields.filename,'NEWSAVE');assert.equal(draft.size,0);
  await assert.rejects(h.call('editorCommit',{token:1}),/no longer active/);assert.equal((await h.call('saves',{volume:0})).total,0);
});
