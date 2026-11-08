/* SPDX-License-Identifier: GPL-3.0-only
 * Pure-codec excerpt from Damián Parrino's PR #374, commit
 * 15a8e388a79ec2598fb285abf8d05e8fef2bee97 (HTML blob 5dcc36c2c4fbe18720e4923d55e9b649d82af84f).
 * https://github.com/tpunix/SAROO/pull/374
 * Only its trailing blank line is omitted. Test-only failing control;
 * never included in the editor. GNU GPLv3 text:
 * tools/savetool/web/LICENSE. UI/DOM code is excluded, original codec unchanged.
 */

// ============================================================================
// SAROO savetool codec — faithful JS port of tools/savetool (C).
// Operates on the whole file buffer in place (mirrors bup_flush()).
// ============================================================================

// ---- byte helpers (big-endian, matching C get_be*/put_be*) ----
const be32 = (u,o)=>((u[o]<<24)|(u[o+1]<<16)|(u[o+2]<<8)|u[o+3])>>>0;
const be16 = (u,o)=>((u[o]<<8)|u[o+1])>>>0;
function wbe32(u,o,v){ u[o]=(v>>>24)&255; u[o+1]=(v>>>16)&255; u[o+2]=(v>>>8)&255; u[o+3]=v&255; }
function wbe16(u,o,v){ u[o]=(v>>>8)&255; u[o+1]=v&255; }
function setBit(u,base,idx,val){ const byte=base+(idx>>3), mask=1<<(idx&7); if(val) u[byte]|=mask; else u[byte]&=~mask; }
function getBit(u,base,idx){ return (u[base+(idx>>3)]>>(idx&7))&1; }
function cstr(u,off,max){ let s=''; for(let i=0;i<max;i++){ const c=u[off+i]; if(c===0) break; s+=String.fromCharCode(c); } return s; }
function writeFixed(u,off,str,len){ for(let i=0;i<len;i++) u[off+i]= i<str.length ? (str.charCodeAt(i)&0xff) : 0; }
// Write a fixed-length field: prefer verbatim raw bytes (faithful round-trip, incl. any
// bytes past the null terminator, exactly like the C tool's memcpy) else the null-padded string.
function writeField(u,off,len,raw,str){ if(raw){ for(let i=0;i<len;i++) u[off+i]=raw[i]||0; } else writeFixed(u,off,str,len); }

function crc32b(data,off,len){
  let crc=0xFFFFFFFF>>>0;
  for(let n=0;n<len;n++){
    crc=(crc^data[off+n])>>>0;
    for(let j=0;j<8;j++) crc=((crc>>>1)^(0xEDB88320 & (-(crc&1)>>>0)))>>>0;
  }
  return (~crc)>>>0;
}

// ---- Saturn BUP date (minutes since 1980-01-01 00:00) ----
const isLeap = y => (y%4===0 && y%100!==0) || y%400===0;
function decodeDate(v){
  if(!v) return null;
  let m=v%60, t=Math.floor(v/60); let h=t%24; let days=Math.floor(t/24);
  let year=1980; for(;;){ const dy=isLeap(year)?366:365; if(days>=dy){ days-=dy; year++; } else break; }
  const md=[31,isLeap(year)?29:28,31,30,31,30,31,31,30,31,30,31]; let mo=0;
  while(days>=md[mo]){ days-=md[mo]; mo++; }
  return {year,month:mo+1,day:days+1,hour:h,min:m};
}
function encodeDate(d){
  let days=0; for(let y=1980;y<d.year;y++) days+=isLeap(y)?366:365;
  const md=[31,isLeap(d.year)?29:28,31,30,31,30,31,31,30,31,30,31];
  for(let m=1;m<d.month;m++) days+=md[m-1];
  days+=d.day-1;
  return (((days*24)+d.hour)*60+d.min)>>>0;
}

// ---- raw save (.SRO SSAVERAW / .BUP Vmem) parse & build ----
function parseRawSave(u){
  const magic = cstr(u,0,8);
  if(u[0]===0x56 && u[1]===0x6d && u[2]===0x65 && u[3]===0x6d){ // "Vmem"
    const dataSize=be32(u,0x2c), date=be32(u,0x28);
    return { fmt:'BUP', filename:cstr(u,0x10,11), comment:cstr(u,0x1c,10),
             rawFilename:u.slice(0x10,0x1b), rawComment:u.slice(0x1c,0x26),
             language:u[0x27], date, dataSize, data:u.slice(0x40,0x40+dataSize) };
  }
  if(magic==='SSAVERAW'){
    const dataSize=be32(u,0x1c), date=be32(u,0x2c);
    const crc=be32(u,0x0c);
    const rec={ fmt:'SRO', filename:cstr(u,0x10,11), comment:cstr(u,0x20,10),
                rawFilename:u.slice(0x10,0x1b), rawComment:u.slice(0x20,0x2a),
                language:u[0x2b], date, dataSize, data:u.slice(0x40,0x40+dataSize), crcOk:true };
    if(crc && crc!==crc32b(u,0x10,dataSize+0x30)) rec.crcOk=false;
    return rec;
  }
  return null;
}

function buildSRO(s){
  const buf=new Uint8Array(0x40+s.dataSize);
  writeFixed(buf,0,"SSAVERAW",12);
  writeField(buf,0x10,11,s.rawFilename,s.filename);
  wbe32(buf,0x1c,s.dataSize);
  writeField(buf,0x20,10,s.rawComment,s.comment);
  buf[0x2b]=s.language&0xff;
  wbe32(buf,0x2c,s.date>>>0);
  buf.set(s.data,0x40);
  wbe32(buf,0x0c,crc32b(buf,0x10,s.dataSize+0x30));
  return buf;
}
function buildBUP(s){
  const buf=new Uint8Array(0x40+s.dataSize);
  writeFixed(buf,0,"Vmem",4);
  writeField(buf,0x10,11,s.rawFilename,s.filename); // dir.filename[12], 11 significant
  writeField(buf,0x1c,10,s.rawComment,s.comment);   // dir.comment[11]
  buf[0x27]=s.language&0xff;
  wbe32(buf,0x28,s.date>>>0);          // dir.date
  wbe32(buf,0x2c,s.dataSize);          // dir.datasize
  wbe32(buf,0x34,s.date>>>0);          // header date
  buf.set(s.data,0x40);
  return buf;
}

// ============================================================================
// Container controller
// ============================================================================
const LANGS=['Japanese','English','French','German','Spanish','Italian'];

function detect(u){
  if(cstr(u,0,16)==="Saroo Save File") return {type:0};
  if(cstr(u,0,16)==="BackUpRam Format") return {type:1,sub:1};
  if(u[1]===0x42&&u[3]===0x61&&u[5]===0x63&&u[7]===0x6b) return {type:1,sub:0}; // interleaved "BackUpRam"
  if(cstr(u,0,8)==="SaroMems") return {type:2};
  return null;
}

class Container {
  constructor(buf){
    this.buf = buf instanceof Uint8Array ? buf : new Uint8Array(buf);
    const d = detect(this.buf);
    if(!d) throw new Error('Unrecognized save file (not a SAROO/Saturn BUP/SaroMems image).');
    this.type = d.type; this.sub = d.sub;
    if(this.type===1){
      // de-interleave to 32KB working memory (read-only format)
      this.mem = new Uint8Array(32768);
      if(this.sub===0){ for(let i=0;i<32768;i++) this.mem[i]=this.buf[i*2+1]; }
      else this.mem.set(this.buf.subarray(0,32768));
    } else {
      this.mem = this.buf; // operate directly on file bytes
    }
  }
  get bytes(){ return this.buf; }

  // ---------- volumes ----------
  volumes(){
    if(this.type===0){
      const v=[]; const b=this.buf;
      for(let i=1;i<4096;i++){ const o=i*16; if(o+16>b.length||be32(b,o)===0) break;
        if((i+1)*0x10000>b.length) break; v.push({id:i,name:cstr(b,o,16)}); }
      return v;
    }
    if(this.type===1) return [{id:0,name:'Saturn Backup RAM'}];
    return [{id:0,name:'SaroMems'}];
  }
  stats(volId){
    if(this.type===0){ const base=volId*0x10000; return {total:512, free:be16(this.buf,base+0x0e)}; }
    if(this.type===2){ return {total:8064, free:be16(this.mem,0x0c)}; }
    return {total:512, free:null};
  }

  // ---------- listing saves ----------
  saves(volId){
    if(this.type===0) return this._sr_list(volId);
    if(this.type===1) return this._ss_list();
    return this._mems_list();
  }

  // ===================== TYPE 0: SAROO save =====================
  _sr_base(volId){ return volId*0x10000; }
  _sr_list(volId){
    const b=this.buf, base=this._sr_base(volId); const out=[];
    if(be32(b,base)!==0x5361726f || be32(b,base+4)!==0x53617665) return out;
    const bs=be16(b,base+0x0c); let block=be16(b,base+0x3e), i=0, guard=0;
    while(block && guard++<512){
      const bp=base+block*bs;
      const dsize=be32(b,bp+0x0c);
      out.push({ id:i, filename:cstr(b,bp,11), comment:cstr(b,bp+0x10,10),
                 language:b[bp+0x1b], date:be32(b,bp+0x1c), dataSize:dsize,
                 blocks:Math.ceil(dsize/bs)+1, _block:block });
      block=be16(b,bp+0x3e); i++;
    }
    return out;
  }
  _sr_read(volId,saveId){
    const b=this.buf, base=this._sr_base(volId), bs=be16(b,base+0x0c);
    let block=be16(b,base+0x3e), i=0;
    while(block){ if(i===saveId) break; block=be16(b,base+block*bs+0x3e); i++; }
    if(!block) return null;
    const bp=base+block*bs, dsize=be32(b,bp+0x0c);
    const data=new Uint8Array(dsize);
    // access_data read via per-save bitmap
    let dp=0, blk=0, bmp=bp+0x40;
    while(dp<dsize){
      blk=this._nextBit(b,bmp,blk,512);
      const src=base+blk*bs, n=Math.min(dsize-dp,bs);
      data.set(b.subarray(src,src+n),dp); dp+=n; blk++;
    }
    return { meta:{...this._sr_list(volId)[saveId], rawFilename:b.slice(bp,bp+11), rawComment:b.slice(bp+0x10,bp+0x1a)}, data };
  }
  _nextBit(u,base,pos,limit){ for(let p=pos;p<limit;p++) if(getBit(u,base,p)) return p; return 0; }
  _sr_freeBlock(base,pos){ // find & claim a free block in global bitmap
    const b=this.buf, bmpBase=base+0x40;
    for(let i=pos;i<512;i++){ if(!getBit(b,bmpBase,i)){ setBit(b,bmpBase,i,1);
      wbe16(b,base+0x0e,be16(b,base+0x0e)-1); return i; } }
    return 0;
  }
  _sr_format(base,gid){ // mirrors sr_bup_format(); gid = 16 bytes from the directory entry
    const b=this.buf; b.fill(0,base,base+0x10000);
    wbe32(b,base,0x5361726f); wbe32(b,base+4,0x53617665);
    wbe32(b,base+8,0x10000); wbe16(b,base+0x0c,0x80); wbe16(b,base+0x0e,512-1);
    for(let i=0;i<16;i++) b[base+0x20+i]=gid[i]||0;
    wbe16(b,base+0x3e,0); b[base+0x40]=0x01;
  }
  _sr_ensureFormat(volId){ // sr_bup_select() formats an unformatted slot before any write
    const b=this.buf, base=volId*0x10000;
    if(be32(b,base)!==0x5361726f || be32(b,base+4)!==0x53617665)
      this._sr_format(base, b.slice(volId*16, volId*16+16));
  }
  _sr_delete(volId,saveId){
    this._sr_ensureFormat(volId);
    const b=this.buf, base=this._sr_base(volId), bs=be16(b,base+0x0c);
    let block=be16(b,base+0x3e), last=0, i=0;
    while(block){ if(i===saveId) break; last=block; block=be16(b,base+block*bs+0x3e); i++; }
    if(!block) throw new Error('save not found');
    const bp=base+block*bs;
    setBit(b,base+0x40,block,0); wbe16(b,base+0x0e,be16(b,base+0x0e)+1);
    let blk=0, bmp=bp+0x40;
    for(;;){ blk=this._nextBit(b,bmp,blk,512); if(!blk) break;
      setBit(b,base+0x40,blk,0); wbe16(b,base+0x0e,be16(b,base+0x0e)+1); blk++; }
    // relink: last(or header)->next
    const lastbp = base + last*bs;
    b[lastbp+0x3e]=b[bp+0x3e]; b[lastbp+0x3f]=b[bp+0x3f];
  }
  _sr_import(volId,save){ // add NEW save (save_id=-1)
    this._sr_ensureFormat(volId);
    const b=this.buf, base=this._sr_base(volId), bs=be16(b,base+0x0c);
    const dsize=save.dataSize, blockNeed=Math.ceil(dsize/bs);
    if((blockNeed+1) > be16(b,base+0x0e)) throw new Error('Not enough free space in this slot.');
    // find last block in linked list
    let block=be16(b,base+0x3e), last=0;
    while(block){ last=block; block=be16(b,base+block*bs+0x3e); }
    // start block
    const hdr=this._sr_freeBlock(base,0), bp=base+hdr*bs;
    b.fill(0,bp,bp+bs);
    writeField(b,bp,11,save.rawFilename,save.filename);
    wbe32(b,bp+0x0c,dsize);
    writeField(b,bp+0x10,10,save.rawComment,save.comment);
    b[bp+0x1b]=save.language&0xff;
    wbe32(b,bp+0x1c,save.date>>>0);
    // data blocks -> per-save bitmap
    let blk=0; for(let k=0;k<blockNeed;k++){ blk=this._sr_freeBlock(base,blk); setBit(b,bp+0x40,blk,1); blk++; }
    // write data
    let dp=0; blk=0;
    while(dp<dsize){ blk=this._nextBit(b,bp+0x40,blk,512); const dst=base+blk*bs, n=Math.min(dsize-dp,bs);
      b.set(save.data.subarray(dp,dp+n),dst); dp+=n; blk++; }
    // link
    wbe16(b, base+last*bs+0x3e, hdr);
  }
  _sr_overwrite(volId,saveId,save){
    this._sr_ensureFormat(volId);
    const b=this.buf, base=this._sr_base(volId), bs=be16(b,base+0x0c);
    let block=be16(b,base+0x3e), i=0;
    while(block){ if(i===saveId) break; block=be16(b,base+block*bs+0x3e); i++; }
    if(!block) throw new Error('save not found');
    const bp=base+block*bs, dsize=be32(b,bp+0x0c);
    if(save.dataSize!==dsize){ // size changed -> delete + re-add, preserving this save's identity
      const keep={ rawFilename:b.slice(bp,bp+11), rawComment:b.slice(bp+0x10,bp+0x1a), language:b[bp+0x1b] };
      this._sr_delete(volId,saveId);
      this._sr_import(volId,{...keep, dataSize:save.dataSize, date:save.date, data:save.data});
      return;
    }
    wbe32(b,bp+0x1c,save.date>>>0);
    let dp=0, blk=0;
    while(dp<dsize){ blk=this._nextBit(b,bp+0x40,blk,512); const dst=base+blk*bs, n=Math.min(dsize-dp,bs);
      b.set(save.data.subarray(dp,dp+n),dst); dp+=n; blk++; }
  }
  _sr_createSlot(gameId){
    const b=this.buf;
    // append-index == current file block count (robust & matches contiguous dirs)
    const idx=Math.floor(b.length/0x10000);
    const nb=new Uint8Array(b.length+0x10000); nb.set(b); this.buf=nb; this.mem=nb;
    writeFixed(nb,idx*16,gameId,16);
    this._sr_format(idx*0x10000, nb.slice(idx*16, idx*16+16));
    return idx;
  }

  // ===================== TYPE 1: Saturn BUP (read-only) =====================
  _ss_list(){
    const m=this.mem, out=[]; let i=0;
    for(let block=2;block<512;block++){ const bp=block*64;
      if(be32(m,bp)===0x80000000){
        const dsize=be32(m,bp+0x1e);
        out.push({ id:i, filename:cstr(m,bp+4,11), comment:cstr(m,bp+0x10,10),
                   language:m[bp+0x0f], date:be32(m,bp+0x1a), dataSize:dsize,
                   blocks:Math.ceil(dsize/62)+1, _block:block, readonly:true });
        i++;
      }
    }
    return out;
  }
  _ss_read(saveId){
    const m=this.mem; let i=0;
    for(let block=2;block<512;block++){ if(be32(m,block*64)!==0x80000000) continue;
      if(i===saveId){ const bp=block*64; return { meta:{...this._ss_list()[saveId], rawFilename:m.slice(bp+4,bp+15), rawComment:m.slice(bp+0x10,bp+0x1a)}, data:this._ss_readData(block) }; }
      i++;
    }
    return null;
  }
  _ss_readData(startBlock){
    const m=this.mem, BS=64; const bp0=startBlock*64; const dsize=be32(m,bp0+0x1e);
    const data=new Uint8Array(dsize);
    // load_blist
    const blist=[startBlock];
    let nbp,dp;
    if(dsize<=0x1e){ nbp=1; dp=0x22; }
    else{
      let bp=bp0; nbp=1; dp=0x22; let idx=1;
      for(;;){ const blk=be16(m,bp+dp); if(blk===0) break; blist[idx++]=blk; dp+=2;
        if(dp===BS){ bp=blist[nbp]*64; nbp++; dp=4; } }
      dp+=2; if(dp===BS){ nbp++; dp=4; }
    }
    // read_data
    let bp=blist[nbp-1]*64; let ds=dsize, out=0;
    while(ds>0){ data[out]=m[bp+dp]; data[out+1]=m[bp+dp+1]; dp+=2; out+=2; ds-=2;
      if(dp===BS){ bp=blist[nbp]*64; nbp++; dp=4; } }
    return data.subarray(0,dsize);
  }

  // ===================== TYPE 2: SaroMems =====================
  _mems_list(){
    const m=this.mem, out=[]; let idx=0;
    for(let i=0;i<448;i++){ const e=1024+i*16; if(m[e]===0) continue;
      const block=be16(m,e+0x0e), bp=block*1024, dsize=be32(m,bp+0x0c);
      out.push({ id:idx, filename:cstr(m,bp,11), comment:cstr(m,bp+0x10,10),
                 language:m[bp+0x1b], date:be32(m,bp+0x1c), dataSize:dsize,
                 blocks:(dsize>960?Math.ceil(dsize/1024):0)+1, _entry:i, _block:block });
      idx++;
    }
    return out;
  }
  _mems_read(saveId){
    const m=this.mem; let idx=0;
    for(let i=0;i<448;i++){ const e=1024+i*16; if(m[e]===0) continue;
      if(idx===saveId){ const block=be16(m,e+0x0e), bp=block*1024; return { meta:{...this._mems_list()[saveId], rawFilename:m.slice(bp,bp+11), rawComment:m.slice(bp+0x10,bp+0x1a)}, data:this._mems_readData(block) }; }
      idx++;
    }
    return null;
  }
  _mems_readData(block){
    const m=this.mem, BS=1024, bp=block*1024, dsize=be32(m,bp+0x0c);
    const data=new Uint8Array(dsize);
    if(dsize<=BS-64){ data.set(m.subarray(bp+0x40,bp+0x40+dsize)); return data; }
    let dp=0, bmp=bp+0x40;
    while(dp<dsize){ const blk=be16(m,bmp); bmp+=2; const src=blk*1024, n=Math.min(dsize-dp,BS);
      data.set(m.subarray(src,src+n),dp); dp+=n; }
    return data;
  }
  _mems_free(pos){ const m=this.mem; for(let i=pos;i<8064;i++){ if(!getBit(m,0x10,i)){ setBit(m,0x10,i,1);
    wbe16(m,0x0c,be16(m,0x0c)-1); return i; } } return 0; }
  _mems_import(save){
    const m=this.mem, BS=1024, dsize=save.dataSize;
    let last=-1; for(let i=0;i<448;i++){ if(m[1024+i*16]===0){ last=i; break; } }
    if(last<0) throw new Error('Directory full (448 saves max).');
    let blockNeed = dsize<=(BS-64) ? 0 : Math.ceil(dsize/BS);
    if(blockNeed>0 && (blockNeed+1)>be16(m,0x0c)) throw new Error('Not enough free space.');
    const hdr=this._mems_free(0), bp=hdr*1024;
    m.fill(0,bp,bp+1024);
    writeField(m,bp,11,save.rawFilename,save.filename); wbe32(m,bp+0x0c,dsize);
    writeField(m,bp+0x10,10,save.rawComment,save.comment); m[bp+0x1b]=save.language&0xff; wbe32(m,bp+0x1c,save.date>>>0);
    let blk=0,k=0; for(k=0;k<blockNeed;k++){ blk=this._mems_free(blk); wbe16(m,bp+0x40+k*2,blk); blk++; }
    wbe16(m,bp+0x40+k*2,0);
    // write data
    if(dsize<=BS-64){ m.set(save.data.subarray(0,dsize),bp+0x40); }
    else { let dp=0,bmp=bp+0x40; while(dp<dsize){ const b2=be16(m,bmp); bmp+=2; const n=Math.min(dsize-dp,BS);
      m.set(save.data.subarray(dp,dp+n),b2*1024); dp+=n; } }
    // directory
    writeField(m,1024+last*16,11,save.rawFilename,save.filename); wbe16(m,1024+last*16+0x0e,hdr);
  }
  _mems_delete(saveId){
    const m=this.mem; let idx=0, last=-1, block=0;
    for(let i=0;i<448;i++){ const e=1024+i*16; if(m[e]===0) continue;
      if(idx===saveId){ block=be16(m,e+0x0e); last=i; break; } idx++; }
    if(!block) throw new Error('save not found');
    const bp=block*1024;
    setBit(m,0x10,block,0); wbe16(m,0x0c,be16(m,0x0c)+1);
    const dsize=be32(m,bp+0x0c);
    if(dsize>960){ let bmp=bp+0x40; for(;;){ const blk=be16(m,bmp); if(blk===0) break;
      setBit(m,0x10,blk,0); wbe16(m,0x0c,be16(m,0x0c)+1); bmp+=2; } }
    m.fill(0,1024+last*16,1024+last*16+16);
  }
  _mems_overwrite(saveId,save){
    const m=this.mem; let idx=0, block=0;
    for(let i=0;i<448;i++){ const e=1024+i*16; if(m[e]===0) continue;
      if(idx===saveId){ block=be16(m,e+0x0e); break; } idx++; }
    if(!block) throw new Error('save not found');
    const bp=block*1024, dsize=be32(m,bp+0x0c);
    if(save.dataSize!==dsize){
      const keep={ rawFilename:m.slice(bp,bp+11), rawComment:m.slice(bp+0x10,bp+0x1a), language:m[bp+0x1b] };
      this._mems_delete(saveId);
      this._mems_import({...keep, dataSize:save.dataSize, date:save.date, data:save.data});
      return;
    }
    wbe32(m,bp+0x1c,save.date>>>0);
    if(dsize<=1024-64){ m.set(save.data.subarray(0,dsize),bp+0x40); }
    else { let dp=0,bmp=bp+0x40; while(dp<dsize){ const b2=be16(m,bmp); bmp+=2; const n=Math.min(dsize-dp,1024);
      m.set(save.data.subarray(dp,dp+n),b2*1024); dp+=n; } }
  }

  // ---------- unified public ops ----------
  read(volId,saveId){ return this.type===0?this._sr_read(volId,saveId):this.type===1?this._ss_read(saveId):this._mems_read(saveId); }
  importNew(volId,save){ if(this.type===0)this._sr_import(volId,save); else if(this.type===2)this._mems_import(save); else throw new Error('Import not supported for Saturn Backup RAM.'); }
  overwrite(volId,saveId,save){ if(this.type===0)this._sr_overwrite(volId,saveId,save); else if(this.type===2)this._mems_overwrite(saveId,save); else throw new Error('Not supported.'); }
  remove(volId,saveId){ if(this.type===0)this._sr_delete(volId,saveId); else if(this.type===2)this._mems_delete(saveId); else throw new Error('Delete not supported for Saturn Backup RAM.'); }
  createSlot(gameId){ if(this.type!==0) throw new Error('Slots only exist in SAROO save files.'); return this._sr_createSlot(gameId); }

  // Edit a save's metadata (name/comment/language) in place. Data is untouched.
  editMeta(volId,saveId,meta){
    if(this.type===1) throw new Error('Saturn Backup RAM is read-only.');
    const list=this.saves(volId), s=list[saveId];
    if(!s) throw new Error('save not found');
    const m=this.mem;
    const bp = this.type===0 ? volId*0x10000 + s._block*be16(this.buf,volId*0x10000+0x0c) : s._block*1024;
    // filename/comment may be raw byte arrays (e.g. Shift-JIS encoded) or plain strings.
    const isRaw = v => v instanceof Uint8Array || Array.isArray(v);
    if(meta.filename!==undefined){ if(isRaw(meta.filename)) writeField(m,bp,11,meta.filename); else writeFixed(m,bp,meta.filename,11); }
    if(meta.comment!==undefined){ if(isRaw(meta.comment)) writeField(m,bp+0x10,10,meta.comment); else writeFixed(m,bp+0x10,meta.comment,10); }
    if(meta.language!==undefined) m[bp+0x1b]=meta.language&0xff;
    if(this.type===2 && meta.filename!==undefined){ const o=1024+s._entry*16; if(isRaw(meta.filename)) writeField(m,o,11,meta.filename); else writeFixed(m,o,meta.filename,11); }
  }

  // Overwrite a save's data payload in place (same length). Metadata is untouched.
  writeData(volId,saveId,bytes){
    if(this.type===1) throw new Error('Saturn Backup RAM is read-only.');
    if(this.type===0) return this._sr_writeData(volId,saveId,bytes);
    return this._mems_writeData(saveId,bytes);
  }
  _sr_writeData(volId,saveId,bytes){
    const b=this.buf, base=volId*0x10000, bs=be16(b,base+0x0c);
    let block=be16(b,base+0x3e), i=0;
    while(block){ if(i===saveId) break; block=be16(b,base+block*bs+0x3e); i++; }
    if(!block) throw new Error('save not found');
    const bp=base+block*bs, dsize=be32(b,bp+0x0c);
    if(bytes.length!==dsize) throw new Error('data size mismatch');
    let dp=0, blk=0;
    while(dp<dsize){ blk=this._nextBit(b,bp+0x40,blk,512); const dst=base+blk*bs, n=Math.min(dsize-dp,bs);
      b.set(bytes.subarray(dp,dp+n),dst); dp+=n; blk++; }
  }
  _mems_writeData(saveId,bytes){
    const m=this.mem; let idx=0, block=0;
    for(let i=0;i<448;i++){ const e=1024+i*16; if(m[e]===0) continue;
      if(idx===saveId){ block=be16(m,e+0x0e); break; } idx++; }
    if(!block) throw new Error('save not found');
    const bp=block*1024, dsize=be32(m,bp+0x0c);
    if(bytes.length!==dsize) throw new Error('data size mismatch');
    if(dsize<=1024-64){ m.set(bytes.subarray(0,dsize),bp+0x40); }
    else { let dp=0,bmp=bp+0x40; while(dp<dsize){ const b2=be16(m,bmp); bmp+=2; const n=Math.min(dsize-dp,1024);
      m.set(bytes.subarray(dp,dp+n),b2*1024); dp+=n; } }
  }
}
