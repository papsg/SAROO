/* SPDX-License-Identifier: GPL-3.0-only
 * Single-owner save workspace and dialog controller for SAROO PR #374.
 */
import { SaveArchive } from './save-codec.mjs';
import { SaveText, dateText, parseDateText } from './save-text.mjs';
import { parseXMLSave, encodeXMLSave } from './save-xml.mjs';
import { parseRawSave, encodeRawSave, saveFilename, textBytes, byteText, unsigned, MAX_SAVE_BYTES } from './save-fields.mjs';
const require = (ok, message) => { if (!ok) throw new Error(message); };
const PAGE_SIZE = 24, HEX_PAGE = 256;
const abort = () => new DOMException('Operation canceled.', 'AbortError');
const pause = () => new Promise(resolve => setTimeout(resolve, 0));

export function createSaveWorker(post, sax) {
  let archive = null, epoch = 0, name = '', busy = null, editor = null, volumeSearch = null;
  const text = new SaveText();
  const state = () => ({ epoch, revision:archive?.revision ?? 0, loaded:!!archive, type:archive?.type,
    volumeCount:archive?.volumeCount ?? 0, bytes:archive?.bytes ?? 0, dirty:archive?.dirty ?? false,
    readOnly:archive?.readOnly ?? false, name });
  const checkStamp = stamp => {require(stamp && stamp.epoch===epoch && stamp.revision===(archive?.revision??0), 'The workspace changed; refresh before applying this operation.');};
  const reply = (id,result) => post({id,result,state:state()});
  const progress = (id,value) => post({id,progress:value});
  function checkEditor(message) {
    checkStamp(message.stamp);
    require(editor && editor.token===message.token, 'This save dialog is no longer active.');return editor;
  }
  function writable() {require(archive && !archive.readOnly,'This archive is read-only.');}
  function closeEditor() {editor=null;}
  function display(meta,encoding='shift-jis') {
    const filename=text.display(meta.rawFilename,encoding),comment=text.display(meta.rawComment,encoding);
    return {filename:filename.text,comment:comment.text,filenameValid:filename.valid,commentValid:comment.valid,
      language:meta.language,date:dateText(meta.date),rawFilename:byteText(meta.rawFilename),rawComment:byteText(meta.rawComment)};
  }
  function editorState(owner) {
    return {index:owner.index,volume:owner.volume,size:owner.data?.length??owner.size,
      readOnly:archive.readOnly,fields:display(owner.meta,owner.encoding),encoding:owner.encoding,
      omittedExtraMetadata:owner.omittedExtraMetadata??false};
  }
  async function fields(owner,value,signal) {
    require(['shift-jis','utf-8','bytes'].includes(value.encoding),'Choose a supported field encoding.');
    const rawFilename=await text.edit(owner.meta.rawFilename,value.filename,value.encoding,11,{signal});
    const rawComment=await text.edit(owner.meta.rawComment,value.comment,value.encoding,10,{signal});
    const language=unsigned(value.language,255,'Language'),date=parseDateText(value.date);
    require(rawFilename[0],'A save needs a nonempty filename.');
    signal.throwIfAborted();require(editor===owner,'This save dialog is no longer active.');
    owner.meta={rawFilename,rawComment,language,date};owner.encoding=value.encoding;
  }
  function matchingName(a,b) {
    for(let i=0;i<11;i++){if(a[i]!==b[i])return false;if(!a[i])return true;}return true;
  }
  function checkNewIdentity(owner) {
    const original=owner.index===null?null:archive.metadata(owner.volume,owner.index).rawFilename;
    if(original && matchingName(original,owner.meta.rawFilename))return;
    const count=archive.stats(owner.volume).saves;
    for(let i=0;i<count;i++)if(i!==owner.index &&
      matchingName(archive.metadata(owner.volume,i).rawFilename,owner.meta.rawFilename))
      throw new Error('That save name already exists. Open the existing save to replace it.');
  }
  async function lists(message) {
    checkStamp(message.stamp);require(archive,'Open an archive first.');
    const query=String(message.query??'').toLowerCase(),current=archive,startEpoch=epoch;
    let indices,count,volume=message.volume;
    if(message.op==='volumes') {
      count=current.volumeCount;
      if(query) {
        if(!volumeSearch || volumeSearch.archive!==current || volumeSearch.count!==count || volumeSearch.query!==query) {
          const matches=new Uint16Array(count);let used=0,checkpoint=performance.now();
          for(let i=0;i<count;i++) {
            if(text.display(current.volume(i).rawName).text.toLowerCase().includes(query))matches[used++]=i;
            if(performance.now()-checkpoint>=8){await pause();checkStamp(message.stamp);checkpoint=performance.now();}
          }
          require(archive===current && epoch===startEpoch,'The archive changed during search.');
          volumeSearch={archive:current,count,query,matches:matches.subarray(0,used)};
        }
        indices=volumeSearch.matches;count=indices.length;
      }
    } else {
      count=current.stats(volume).saves;
      if(query) {
        const matches=[];
        for(let i=0;i<count;i++) {
          const meta=current.metadata(volume,i),labels=display(meta);
          if((labels.filename+'\n'+labels.comment).toLowerCase().includes(query))matches.push(i);
        }
        indices=matches;count=matches.length;
      }
    }
    const page=Math.max(0,Math.min(Math.floor(Number(message.page)||0),Math.max(0,Math.ceil(count/PAGE_SIZE)-1))),items=[];
    for(let i=page*PAGE_SIZE;i<Math.min(count,(page+1)*PAGE_SIZE);i++) {
      const index=indices?indices[i]:i;
      if(message.op==='volumes') {const item=current.volume(index);items.push({index,id:item.id,name:text.display(item.rawName).text});}
      else {const meta=current.metadata(volume,index);items.push({index,size:meta.size,blocks:meta.blocks,...display(meta)});}
    }
    return {page,total:count,pageSize:PAGE_SIZE,items,stats:message.op==='saves'?current.stats(volume):null};
  }
  async function incoming(file,signal,onProgress) {
    require(file instanceof Blob,'Choose a save file.');
    const prefix=new Uint8Array(await file.slice(0,8).arrayBuffer());signal.throwIfAborted();
    const signature=String.fromCharCode(...prefix);
    if(signature.startsWith('SSAVERAW') || signature.startsWith('Vmem')) {
      require(file.size<=MAX_SAVE_BYTES+64,'Raw save exceeds the 1 MiB payload limit.');
      const bytes=new Uint8Array(await file.arrayBuffer());signal.throwIfAborted();return parseRawSave(bytes);
    }
    return parseXMLSave(file,sax,{signal,onProgress});
  }
  async function dispatch(message,signal) {
    const options={signal,onProgress:value=>progress(message.id,value)};
    if(message.op==='init')return {pageSize:PAGE_SIZE,hexPage:HEX_PAGE,shiftJISSupported:text.shiftJISSupported};
    checkStamp(message.stamp);
    switch(message.op) {
      case 'new': {
        const candidate=SaveArchive.create(message.type);closeEditor();archive=candidate;epoch++;volumeSearch=null;
        name=message.type==='saroo'?'SS_SAVE.BIN':'SS_MEMS.BIN';return {};
      }
      case 'open': {
        const candidate=await SaveArchive.open(message.file,options);signal.throwIfAborted();
        closeEditor();archive=candidate;epoch++;volumeSearch=null;name=message.file.name||'archive.bin';return {};
      }
      case 'slot':writable();return archive.createSlot(textBytes(message.gameId,16));
      case 'export':require(archive,'Open an archive first.');return {blob:await archive.export(options),name};
      case 'editorStart': {
        require(archive,'Open an archive first.');archive.stats(message.volume);
        const index=message.index??null;if(index===null)writable();
        const meta=index===null ? {rawFilename:new TextEncoder().encode('NEWSAVE'),rawComment:new Uint8Array(10),language:1,date:0}
          : archive.metadata(message.volume,index);
        editor={token:message.token,volume:message.volume,index,meta,encoding:'shift-jis',size:meta.size??0,data:index===null?new Uint8Array():null};
        return editorState(editor);
      }
      case 'editorFields': {
        writable();const owner=checkEditor(message);await fields(owner,message.fields,signal);return editorState(owner);
      }
      case 'editorEncoding': {
        const owner=checkEditor(message);require(['shift-jis','utf-8','bytes'].includes(message.encoding),'Unknown encoding.');
        owner.encoding=message.encoding;return editorState(owner);
      }
      case 'editorRange': {
        const owner=checkEditor(message),size=owner.data?.length??owner.size;
        const page=unsigned(message.page,Math.max(0,Math.ceil(size/HEX_PAGE)-1),'Hex page'),offset=page*HEX_PAGE,length=Math.min(HEX_PAGE,Math.max(0,size-offset));
        const bytes=owner.data?owner.data.slice(offset,offset+length):archive.read(owner.volume,owner.index,offset,length);
        return {page,offset,size,bytes,pageSize:HEX_PAGE};
      }
      case 'editorByte': {
        writable();const owner=checkEditor(message),size=owner.data?.length??owner.size;
        const offset=unsigned(message.offset,size-1,'Byte offset'),value=unsigned(message.value,255,'Byte value');
        if(!owner.data)owner.data=archive.read(owner.volume,owner.index);
        owner.data[offset]=value;return {offset,value};
      }
      case 'editorImport': {
        writable();const owner=checkEditor(message);
        const save=await incoming(message.file,signal,options.onProgress);signal.throwIfAborted();checkEditor(message);
        require(save.data.length<=(archive.type==='saroo'?65280:490496),'Imported payload exceeds this archive format’s per-save capacity.');
        const meta=owner.index===null || message.useMetadata ? save : {...owner.meta,date:save.date};
        owner.meta={rawFilename:meta.rawFilename.slice(),rawComment:meta.rawComment.slice(),language:meta.language,date:meta.date};
        owner.data=save.data;owner.omittedExtraMetadata=!!save.omittedExtraMetadata;return editorState(owner);
      }
      case 'editorCommit': {
        writable();const owner=checkEditor(message);if(message.fields)await fields(owner,message.fields,signal);
        checkEditor(message);checkNewIdentity(owner);
        const changes={...owner.meta};if(owner.data)changes.data=owner.data;
        const result=archive.update(owner.volume,owner.index,changes);closeEditor();return result;
      }
      case 'editorDelete': {
        writable();const owner=checkEditor(message);require(owner.index!==null,'This save has not been added yet.');
        const result=archive.remove(owner.volume,owner.index);closeEditor();return result;
      }
      case 'saveExport': {
        const owner=checkEditor(message);if(message.fields){writable();await fields(owner,message.fields,signal);}
        const save={...owner.meta,data:owner.data??archive.read(owner.volume,owner.index)};
        const extension=message.format;let blob;
        if(extension==='XML')blob=await encodeXMLSave(save,options);
        else if(extension==='bin')blob=new Blob([save.data],{type:'application/octet-stream'});
        else blob=new Blob([encodeRawSave(save,extension)],{type:'application/octet-stream'});
        signal.throwIfAborted();checkEditor(message);return {blob,name:saveFilename(save.rawFilename,extension)};
      }
      default:throw new Error('Unknown save operation.');
    }
  }
  return async message=>{
    if(message.op==='cancel'){if(busy?.id===message.target)busy.controller.abort(abort());return;}
    if(message.op==='editorClose') {
      if(editor?.token===message.token){if(busy?.token===message.token)busy.controller.abort(abort());closeEditor();}return;
    }
    if(message.op==='volumes'||message.op==='saves') {
      try{reply(message.id,await lists(message));}catch(error){post({id:message.id,error:error.message,name:error.name,state:state()});}return;
    }
    if(busy){post({id:message.id,error:'An operation is already running.',state:state()});return;}
    const job={id:message.id,token:message.token,controller:new AbortController()};busy=job;
    try{reply(message.id,await dispatch(message,job.controller.signal));}
    catch(error){post({id:message.id,error:error.message,name:error.name,state:state()});}
    finally{if(busy===job)busy=null;}
  };
}

export function bootSaveWorker(sax) {
  const receive=createSaveWorker(value=>self.postMessage(value),sax);
  self.onmessage=event=>{void receive(event.data);};
}
