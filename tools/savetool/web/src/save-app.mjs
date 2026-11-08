/* SPDX-License-Identifier: GPL-3.0-only
 * Offline save editor adapted from the feature proposed in SAROO PR #374.
 * The worker owns archive bytes; this page owns selections and disposable views.
 */
export function bootSaveApp(workerSource) {
  const $=id=>document.getElementById(id),pending=new Map();
  let worker,state={epoch:0,revision:0,loaded:false,dirty:false,readOnly:false},ready=false,fatal=false;
  let sequence=0,active=null,modal=null,modalSequence=0,selected=null,slotReady=false;
  let volumePage=0,savePage=0,volumePages=1,savePages=1,volumeGeneration=0,saveGeneration=0;
  let volumeOwner=null,saveOwner=null;
  let volumeTimer=null,saveTimer=null,archiveURL=null,saveURL=null,archivePicker=null,savePicker=null;
  const stamp=()=>({epoch:state.epoch,revision:state.revision});
  const same=value=>value?.epoch===state.epoch&&value.revision===state.revision;
  const notice=(value,error=false)=>{$('message').textContent=value;$('message').hidden=!value;$('message').classList.toggle('error',error);};
  const node=(tag,text,className)=>{const item=document.createElement(tag);if(text!==undefined)item.textContent=text;if(className)item.className=className;return item;};
  function clearDownload(kind) {
    if(kind==='archive'){if(archiveURL)URL.revokeObjectURL(archiveURL);archiveURL=null;$('archive-download').hidden=true;}
    else{if(saveURL)URL.revokeObjectURL(saveURL);saveURL=null;$('save-download').hidden=true;}
  }
  function controls() {
    const blocked=!ready||fatal||active!==null;
    for(const item of document.querySelectorAll('[data-work]'))item.disabled=blocked;
    for(const item of $('volumes').querySelectorAll('.volume'))item.disabled||=!same(volumeOwner);
    for(const item of $('saves').querySelectorAll('.save-row'))item.disabled||=!slotReady||!same(saveOwner)||saveOwner.volume!==selected;
    for(const item of document.querySelectorAll('[data-write]'))item.disabled=blocked||state.readOnly;
    $('export').disabled=blocked||!state.loaded;
    $('add-slot').hidden=!state.loaded||state.type!=='saroo';
    $('add-save').disabled=blocked||!slotReady||state.readOnly;
    $('volume-previous').disabled=blocked||!volumePage;$('volume-next').disabled=blocked||volumePage+1>=volumePages;
    $('save-previous').disabled=blocked||!savePage;$('save-next').disabled=blocked||savePage+1>=savePages;
    $('activity').hidden=active===null||!!modal;$('editor-activity').hidden=active===null;
    $('cancel').disabled=active===null;$('editor-cancel-operation').disabled=active===null;
    if(modal){
      const pages=Math.max(1,Math.ceil(modal.size/256));
      $('hex-previous').disabled=blocked||!modal.hexPage;$('hex-next').disabled=blocked||modal.hexPage+1>=pages;
      $('byte-apply').disabled=blocked||state.readOnly||!modal.size;
    }
  }
  function updateState(value) {
    if(!value||value.epoch<state.epoch||value.epoch===state.epoch&&value.revision<state.revision)return;
    if(!same(value))clearDownload('archive');state=value;
    $('archive-name').textContent=state.name||'No archive open';
    $('archive-state').textContent=state.loaded?`${state.type==='saroo'?'SAROO game saves':state.type==='mems'?'SAROO memory card':'Saturn backup · read-only'} · ${(state.bytes/1048576).toFixed(2)} MiB · ${state.dirty?'Edited · export to keep changes':'No changes'}`:'Open your backup, or start a new archive.';
    $('workspace').hidden=!state.loaded;$('welcome').hidden=state.loaded;controls();
  }
  function rpc(op,data={}) {
    const id=++sequence,promise=new Promise((resolve,reject)=>pending.set(id,{resolve,reject}));
    try{worker.postMessage({op,id,stamp:stamp(),...data});}
    catch(error){pending.get(id).reject(error);pending.delete(id);}return {id,promise};
  }
  async function run(op,data={}) {
    if(active!==null)throw new Error('Wait for the current operation or cancel it.');
    if(fatal||!ready&&op!=='init')throw new Error('Reopen the editor to continue.');
    const request=rpc(op,data);active=request.id;
    for(const id of ['activity-text','editor-activity-text'])$(id).textContent='Working…';
    for(const id of ['progress','editor-progress'])$(id).removeAttribute('value');controls();
    try{return await request.promise;}finally{if(active===request.id)active=null;controls();}
  }
  async function perform(action) {
    const owner=modal;
    try{await action();}catch(error){
      if(error.name==='AbortError'||owner&&modal!==owner)return;
      if(modal)$('editor-error').textContent=error.message;else notice(error.message,true);
    }
  }
  const on=(id,event,action)=>$(id).addEventListener(event,e=>{void perform(()=>action(e));});
  function discardArchive(){return !state.dirty||confirm('Replace this edited archive? Export it first if you need its changes.');}
  async function refreshVolumes() {
    const generation=++volumeGeneration,owner=stamp();
    const result=await rpc('volumes',{page:volumePage,query:$('volume-search').value}).promise;
    if(generation!==volumeGeneration||!same(owner))return;
    volumePage=result.page;volumePages=Math.max(1,Math.ceil(result.total/result.pageSize));
    const fragment=document.createDocumentFragment();
    for(const item of result.items){
      const button=node('button',state.type==='saroo'?item.name:state.type==='mems'?'Memory card':'Saturn backup');
      button.className='volume';button.dataset.work='';button.dataset.volume=String(item.id);
      button.classList.toggle('selected',item.id===selected);button.setAttribute('aria-pressed',String(item.id===selected));
      button.addEventListener('click',()=>{if(!same(owner))return;void perform(async()=>{selected=item.id;slotReady=false;controls();savePage=0;$('save-search').value='';await refreshVolumes();await refreshSaves();});});
      fragment.append(button);
    }
    volumeOwner=owner;$('volumes').replaceChildren(fragment);$('volume-empty').hidden=result.total!==0;
    $('volume-empty').textContent=$('volume-search').value?'No matching games.':'Add a game to start saving.';
    $('volume-label').textContent=`${volumePage+1} / ${volumePages} · ${result.total.toLocaleString()}`;
    $('volume-pages').hidden=volumePages<=1;controls();
  }
  async function refreshSaves() {
    const generation=++saveGeneration,owner=stamp(),volume=selected;slotReady=false;controls();
    $('saves').replaceChildren();$('slot-error').hidden=true;$('save-empty').hidden=false;
    if(volume===null){$('save-empty').textContent='Choose a game to see its saves.';$('save-pages').hidden=true;$('slot-state').textContent='';return;}
    try{
      const result=await rpc('saves',{volume,page:savePage,query:$('save-search').value}).promise;
      if(generation!==saveGeneration||volume!==selected||!same(owner))return;
      savePage=result.page;savePages=Math.max(1,Math.ceil(result.total/result.pageSize));slotReady=true;
      const fragment=document.createDocumentFragment();
      for(const item of result.items){
        const button=node('button');button.className='save-row';button.dataset.work='';button.dataset.index=String(item.index);
        const labels=node('span');labels.append(node('strong',item.filename),node('small',item.comment||'No comment'));
        const detail=node('span',`${item.size.toLocaleString()} bytes`,'save-detail');detail.append(node('small',item.date?.replace('T',' ')||'Date unset'));
        button.append(labels,detail);button.setAttribute('aria-label',`Open save ${item.filename}`);
        button.addEventListener('click',()=>{if(slotReady&&same(owner)&&volume===selected)void perform(()=>openEditor(item.index));});fragment.append(button);
      }
      saveOwner={...owner,volume};$('saves').replaceChildren(fragment);$('save-empty').hidden=result.total!==0;
      $('save-empty').textContent=$('save-search').value?'No matching saves.':'No saves here yet. Add or import one.';
      $('save-label').textContent=`${savePage+1} / ${savePages} · ${result.total.toLocaleString()} saves`;
      $('save-pages').hidden=savePages<=1;
      const stats=result.stats;$('slot-state').textContent=`${stats.saves} saves · ${stats.free.toLocaleString()} / ${stats.total.toLocaleString()} blocks free`;
      controls();
    }catch(error){
      if(generation!==saveGeneration||volume!==selected||!same(owner))return;
      $('save-empty').hidden=true;$('save-pages').hidden=true;$('slot-state').textContent='';
      $('slot-error').textContent=`This game slot could not be opened: ${error.message} Its bytes have been preserved. You can select another game or export the archive.`;
      $('slot-error').hidden=false;controls();
    }
  }
  async function changedWorkspace() {
    selected=null;volumePage=savePage=0;slotReady=false;volumeGeneration++;saveGeneration++;
    $('volume-search').value='';$('save-search').value='';archivePicker=savePicker=null;notice('');
    if(state.volumeCount){const result=await rpc('volumes',{page:0}).promise;selected=result.items[0]?.id??null;}
    await refreshVolumes();await refreshSaves();
  }
  async function openArchive(file) {
    if(modal||!discardArchive())return;
    await run('open',{file});await changedWorkspace();
  }
  function markDraft() {if(modal){modal.dirty=true;clearDownload('save');$('editor-error').textContent='';}}
  function metadata() {
    if(!/^\d+$/.test($('language').value))throw new Error('Language must be a number from 0 to 255.');
    return {filename:$('filename').value,comment:$('comment').value,encoding:modal.encoding,
      language:Number($('language').value),date:$('date').value};
  }
  function showDraft(result) {
    modal.size=result.size;modal.encoding=result.encoding;
    for(const id of ['filename','comment','language','date'])$(id).value=result.fields[id];
    $('encoding').value=result.encoding;$('raw-fields').textContent=`Name bytes: ${result.fields.rawFilename}\nComment bytes: ${result.fields.rawComment}`;
    $('encoding-note').textContent=result.fields.filenameValid&&result.fields.commentValid?'Unchanged text keeps its original bytes.':
      'Some bytes do not decode in this encoding. Their reversible byte notation is shown; leave it unchanged to preserve it, or select Raw bytes to edit it.';
    $('metadata-note').hidden=!result.omittedExtraMetadata;
    $('save-size').textContent=`${result.size.toLocaleString()} payload bytes`;
    modal.hexPage=Math.min(modal.hexPage,Math.max(0,Math.ceil(result.size/256)-1));controls();
  }
  async function openEditor(index=null,file=null) {
    if(modal||!slotReady)return;
    const token=++modalSequence,volume=selected,result=await run('editorStart',{token,volume,index});
    modal={token,volume,index,dirty:false,encoding:result.encoding,size:result.size,hexPage:0,rangeGeneration:0};
    $('editor-title').textContent=state.readOnly?'View save':index===null?'Add save':'Edit save';
    $('delete').hidden=index===null||state.readOnly;$('apply').hidden=state.readOnly;$('import-controls').hidden=state.readOnly;
    $('editor-error').textContent='';$('use-metadata').checked=false;$('byte-offset').value='0';$('byte-value').value='00';
    showDraft(result);$('editor').showModal();await refreshHex();if(file)await importSave(file);
  }
  function closeEditor(force=false) {
    if(!modal||!force&&modal.dirty&&!confirm('Discard the changes in this save dialog?'))return;
    worker.postMessage({op:'editorClose',token:modal.token});modal=null;savePicker=null;
    $('editor').close();$('hex').replaceChildren();clearDownload('save');controls();
  }
  async function flushFields(owner) {
    const result=await run('editorFields',{token:owner.token,fields:metadata()});
    if(modal!==owner)return false;showDraft(result);return true;
  }
  async function importSave(file) {
    const owner=modal;if(!owner||state.readOnly)return;
    if(!await flushFields(owner))return;
    const result=await run('editorImport',{token:owner.token,file,useMetadata:$('use-metadata').checked});
    if(modal!==owner)return;showDraft(result);markDraft();await refreshHex();
  }
  async function refreshHex() {
    const owner=modal;if(!owner)return;const generation=++owner.rangeGeneration;
    const result=await run('editorRange',{token:owner.token,page:owner.hexPage});
    if(modal!==owner||generation!==owner.rangeGeneration)return;
    owner.hexPage=result.page;const fragment=document.createDocumentFragment();
    for(let i=0;i<result.bytes.length;i+=16){const row=result.bytes.subarray(i,i+16);
      fragment.append(`${(result.offset+i).toString(16).toUpperCase().padStart(6,'0')}  `);
      for(let j=0;j<16;j++){
        if(j<row.length){const value=row[j].toString(16).toUpperCase().padStart(2,'0'),button=node('button',value,'hex-byte');
          button.dataset.offset=String(result.offset+i+j);button.dataset.work='';button.tabIndex=-1;
          button.setAttribute('aria-label',`Byte ${(result.offset+i+j).toString(16).toUpperCase()}: ${value}`);fragment.append(button);
        }else fragment.append('  ');
        if(j<15)fragment.append(' ');
      }
      fragment.append(`  ${Array.from(row,b=>b>=32&&b<=126?String.fromCharCode(b):'.').join('')}\n`);
    }
    if(!result.bytes.length)fragment.append('Empty payload');$('hex').replaceChildren(fragment);
    $('hex-label').textContent=`Page ${result.page+1} / ${Math.max(1,Math.ceil(result.size/256))}`;controls();
  }
  const cancel=()=>{if(active!==null){worker.postMessage({op:'cancel',target:active});for(const id of ['activity-text','editor-activity-text'])$(id).textContent='Canceling…';}};
  on('cancel','click',cancel);on('editor-cancel-operation','click',cancel);
  on('open','click',()=>{archivePicker=stamp();$('archive-file').click();});
  on('archive-file','change',async()=>{
    const file=$('archive-file').files[0],owner=archivePicker;$('archive-file').value='';archivePicker=null;
    if(file&&same(owner)&&!modal)await openArchive(file);
  });
  for(const [id,type] of [['new-saroo','saroo'],['new-mems','mems']])on(id,'click',async()=>{
    if(!discardArchive())return;await run('new',{type});await changedWorkspace();
  });
  on('add-slot','click',async()=>{
    const gameId=prompt('Game ID (up to 16 bytes, normally the disc serial):');if(gameId===null)return;
    const result=await run('slot',{gameId});selected=result.id;savePage=0;$('volume-search').value='';
    await refreshVolumes();await refreshSaves();notice(result.created?'Game added. Export the archive to keep it.':'That game already exists.');
  });
  on('export','click',async()=>{
    const result=await run('export');clearDownload('archive');archiveURL=URL.createObjectURL(result.blob);
    $('download').href=archiveURL;$('download').download=result.name;$('download').textContent=`Download ${result.name}`;
    $('archive-download').hidden=false;notice('Archive export ready. Download it to keep this version.');
  });
  on('download','click',()=>notice('Download requested. Keep the downloaded archive before closing edited work.'));
  on('volume-search','input',()=>{clearTimeout(volumeTimer);volumePage=0;volumeTimer=setTimeout(()=>{if(state.loaded)void perform(refreshVolumes);},120);});
  on('save-search','input',()=>{clearTimeout(saveTimer);savePage=0;saveTimer=setTimeout(()=>{if(state.loaded)void perform(refreshSaves);},120);});
  on('volume-previous','click',async()=>{volumePage--;await refreshVolumes();});on('volume-next','click',async()=>{volumePage++;await refreshVolumes();});
  on('save-previous','click',async()=>{savePage--;await refreshSaves();});on('save-next','click',async()=>{savePage++;await refreshSaves();});
  on('add-save','click',()=>openEditor());
  for(const id of ['filename','comment','language','date'])on(id,'input',markDraft);
  on('encoding','change',async()=>{
    const owner=modal;if(!owner)return;const encoding=$('encoding').value;
    try{
      if(!state.readOnly&&!await flushFields(owner))return;
      const result=await run('editorEncoding',{token:owner.token,encoding});if(modal===owner)showDraft(result);
    }catch(error){if(modal===owner)$('encoding').value=owner.encoding;throw error;}
  });
  on('import-save','click',()=>{savePicker=modal;$('save-file').click();});
  on('save-file','change',async()=>{const file=$('save-file').files[0],owner=savePicker;$('save-file').value='';savePicker=null;if(file&&owner&&modal===owner)await importSave(file);});
  on('close-editor','click',()=>closeEditor());on('editor','cancel',event=>{event.preventDefault();closeEditor();});
  on('apply','click',async()=>{
    const owner=modal;if(!owner)return;
    const result=await run('editorCommit',{token:owner.token,fields:metadata()});
    if(modal===owner)closeEditor(true);await refreshVolumes();await refreshSaves();notice(result.changed?'Save applied. Export the archive to keep it.':'No save bytes changed.');
  });
  on('delete','click',async()=>{
    const owner=modal;if(!owner||!confirm('Delete this save from the archive?'))return;
    await run('editorDelete',{token:owner.token});if(modal===owner)closeEditor(true);await refreshVolumes();await refreshSaves();notice('Save deleted. Export the archive to keep this version.');
  });
  on('export-save','click',async()=>{
    const owner=modal;if(!owner)return;
    const result=await run('saveExport',{token:owner.token,format:$('save-format').value,...(state.readOnly?{}:{fields:metadata()})});
    if(modal!==owner)return;clearDownload('save');saveURL=URL.createObjectURL(result.blob);
    $('save-download-link').href=saveURL;$('save-download-link').download=result.name;$('save-download-link').textContent=`Download ${result.name}`;$('save-download').hidden=false;
  });
  on('hex-previous','click',async()=>{modal.hexPage--;await refreshHex();});on('hex-next','click',async()=>{modal.hexPage++;await refreshHex();});
  on('hex','click',event=>{
    const button=event.target.closest('.hex-byte');if(!button||button.disabled||!modal)return;
    $('byte-offset').value=Number(button.dataset.offset).toString(16).toUpperCase();$('byte-value').value=button.textContent;
    if(!state.readOnly){$('byte-value').focus();$('byte-value').select();}
  });
  function byteOffset(){
    const value=$('byte-offset').value;if(!/^[0-9a-f]{1,6}$/i.test(value))throw new Error('Enter a hexadecimal byte offset.');
    const offset=parseInt(value,16);if(!modal.size||offset>=modal.size)throw new Error('Offset lies outside this save.');return offset;
  }
  on('hex-go','click',async()=>{modal.hexPage=Math.floor(byteOffset()/256);await refreshHex();});
  on('byte-apply','click',async()=>{
    const owner=modal,offset=byteOffset(),value=$('byte-value').value;
    if(!/^[0-9a-f]{2}$/i.test(value))throw new Error('Enter one byte as two hexadecimal digits.');
    await run('editorByte',{token:owner.token,offset,value:parseInt(value,16)});if(modal!==owner)return;
    markDraft();owner.hexPage=Math.floor(offset/256);await refreshHex();
  });
  window.addEventListener('dragover',event=>event.preventDefault());
  window.addEventListener('drop',event=>{event.preventDefault();void perform(async()=>{
    if(!ready||fatal||active!==null)return;const files=Array.from(event.dataTransfer.files);
    if(files.length!==1)throw new Error('Drop one archive or save file at a time.');
    if(modal)await importSave(files[0]);else await openArchive(files[0]);
  });});
  window.addEventListener('beforeunload',event=>{if(state.dirty||modal?.dirty){event.preventDefault();event.returnValue='';}});
  controls();
  try{
    const url=URL.createObjectURL(new Blob([workerSource],{type:'text/javascript'}));
    try{worker=new Worker(url);}finally{URL.revokeObjectURL(url);}
    worker.onmessage=event=>{
      const value=event.data;
      if(value.progress){
        if(active!==value.id)return;
        const label=value.progress.phase==='xml'?'Reading XML…':value.progress.phase==='xml-export'?'Preparing XML…':'Preparing archive…';
        for(const id of ['activity-text','editor-activity-text'])$(id).textContent=label;
        if(value.progress.total)for(const id of ['progress','editor-progress']){$(id).max=value.progress.total;$(id).value=value.progress.done;}return;
      }
      const request=pending.get(value.id);if(!request)return;pending.delete(value.id);updateState(value.state);
      if(value.error)request.reject(Object.assign(new Error(value.error),{name:value.name??'Error'}));else request.resolve(value.result);
    };
    const failed=event=>{
      event.preventDefault();fatal=true;
      for(const request of pending.values())request.reject(new Error('The editor worker stopped. Any prepared download remains available.'));
      pending.clear();notice('The editor worker stopped. Keep any prepared download, then reopen the editor.',true);controls();
    };
    worker.onerror=failed;worker.onmessageerror=failed;
    void perform(async()=>{await run('init');ready=true;updateState(state);controls();});
  }catch(error){fatal=true;notice(error.message,true);controls();}
}
