/* SPDX-License-Identifier: GPL-3.0-only
 * Pure-codec excerpt from Damián Parrino's PR #374, commit
 * 15a8e388a79ec2598fb285abf8d05e8fef2bee97 (HTML blob 69150a384ff25ddd8b4d00f219fc929aabbe682d).
 * https://github.com/tpunix/SAROO/pull/374
 * Only trailing blank lines are omitted. Test-only failing control; never
 * included in the editor. GPLv3 text: ../../tools/covertool/web/LICENSE
 */
// ============================ CODEC (verified against tools/covertool) ============================
const MAX_COVERS = 0x4000, IDX_SIZE = 32, IDX_BYTES = MAX_COVERS*IDX_SIZE, PAL_BYTES = 256*4;
const MAX_W = 128, MAX_H = 192;

function adler32(data){ let a=1,b=0; for(let i=0;i<data.length;i++){ a=(a+data[i])%65521; b=(b+a)%65521; } return ((b<<16)|a)>>>0; }

function parseCoverBin(buf){
  const u8 = buf instanceof Uint8Array ? buf : new Uint8Array(buf);
  const dv = new DataView(u8.buffer, u8.byteOffset, u8.byteLength);
  const covers=[];
  for(let i=0;i<MAX_COVERS;i++){
    const base=i*IDX_SIZE;
    if(base+IDX_SIZE>u8.length) break;
    if(u8[base]===0) break;
    let serial=''; for(let k=0;k<12;k++){ const c=u8[base+k]; if(c===0) break; serial+=String.fromCharCode(c); }
    const hash=dv.getUint32(base+12,true), off=dv.getUint32(base+16,true);
    const width=dv.getUint16(base+20,true), height=dv.getUint16(base+22,true);
    if(off+PAL_BYTES+width*height>u8.length) throw new Error(`Cover "${serial}" points past end of file — not a valid cover.bin?`);
    const palette=new Uint32Array(256);
    for(let p=0;p<256;p++){ const o=off+p*4; palette[p]=((u8[o+2]<<16)|(u8[o+1]<<8)|u8[o])>>>0; }
    const start=off+PAL_BYTES;
    covers.push({ serial, hash, width, height, palette, indices:u8.slice(start,start+width*height) });
  }
  return covers;
}

function buildCoverBin(covers){
  const chunks=[]; let offset=IDX_BYTES; const dedup=new Map(); const offsets=new Array(covers.length);
  covers.forEach((c,i)=>{
    const h=adler32(c.indices);
    if(dedup.has(h)){ offsets[i]=dedup.get(h); return; }
    const off=offset; dedup.set(h,off); offsets[i]=off;
    const pal=new Uint8Array(PAL_BYTES);
    for(let p=0;p<256;p++){ const v=c.palette[p]>>>0; pal[p*4]=v&255; pal[p*4+1]=(v>>8)&255; pal[p*4+2]=(v>>16)&255; pal[p*4+3]=0; }
    chunks.push(pal,c.indices); offset+=PAL_BYTES+c.indices.length;
  });
  const out=new Uint8Array(offset); const dv=new DataView(out.buffer);
  covers.forEach((c,i)=>{
    const base=i*IDX_SIZE; const serial=String(c.serial).slice(0,11);
    for(let k=0;k<serial.length;k++) out[base+k]=serial.charCodeAt(k)&0xff;
    dv.setUint32(base+12,c.hash>>>0,true); dv.setUint32(base+16,offsets[i]>>>0,true);
    dv.setUint16(base+20,c.width,true); dv.setUint16(base+22,c.height,true);
  });
  let o=IDX_BYTES; for(const ch of chunks){ out.set(ch,o); o+=ch.length; }
  return out;
}

function decodeBMP8(buf){
  const u8=buf instanceof Uint8Array?buf:new Uint8Array(buf);
  const dv=new DataView(u8.buffer,u8.byteOffset,u8.byteLength);
  if(u8.length<54||dv.getUint16(0,true)!==0x4d42) return null;
  const dataOffset=dv.getUint32(0x0a,true), infoSize=dv.getUint32(0x0e,true);
  const width=dv.getInt32(0x12,true); let height=dv.getInt32(0x16,true);
  const bpp=dv.getUint16(0x1c,true), compression=dv.getUint32(0x1e,true);
  if(bpp!==8||compression!==0||width<=0) return null;
  const topDown=height<0; height=Math.abs(height);
  const palOff=14+infoSize, palCount=Math.max(0,Math.min(256,Math.floor((dataOffset-palOff)/4)));
  const palette=new Uint32Array(256);
  for(let p=0;p<palCount;p++){ const o=palOff+p*4; palette[p]=((u8[o+2]<<16)|(u8[o+1]<<8)|u8[o])>>>0; }
  const rowSize=(width+3)&~3; const indices=new Uint8Array(width*height);
  for(let y=0;y<height;y++){ const fr=topDown?y:(height-1-y); const s=dataOffset+fr*rowSize; indices.set(u8.subarray(s,s+width),y*width); }
  return { width, height, palette, indices };
}

function encodeBMP8(cover){
  const {width,height,palette,indices}=cover;
  const rowSize=(width+3)&~3, imgSize=rowSize*height, dataOffset=14+40+PAL_BYTES, fileSize=dataOffset+imgSize;
  const out=new Uint8Array(fileSize), dv=new DataView(out.buffer);
  dv.setUint16(0,0x4d42,true); dv.setUint32(2,fileSize,true); dv.setUint32(0x0a,dataOffset,true);
  dv.setUint32(0x0e,40,true); dv.setInt32(0x12,width,true); dv.setInt32(0x16,height,true);
  dv.setUint16(0x1a,1,true); dv.setUint16(0x1c,8,true); dv.setUint32(0x1e,0,true);
  dv.setUint32(0x22,imgSize,true); dv.setUint32(0x26,0x0b13,true); dv.setUint32(0x2a,0x0b13,true); dv.setUint32(0x2e,256,true);
  for(let p=0;p<256;p++){ const v=palette[p]>>>0; const o=0x36+p*4; out[o]=v&255; out[o+1]=(v>>8)&255; out[o+2]=(v>>16)&255; out[o+3]=0; }
  for(let y=0;y<height;y++){ const sy=height-1-y; out.set(indices.subarray(sy*width,sy*width+width),dataOffset+y*rowSize); }
  return out;
}

function quantize(rgba,width,height,maxColors=256){
  const n=width*height, hist=new Map(), px=new Uint32Array(n);
  for(let i=0;i<n;i++){
    let r=rgba[i*4],g=rgba[i*4+1],b=rgba[i*4+2]; const a=rgba[i*4+3];
    if(a<255){ const af=a/255, ia=1-af; r=Math.round(r*af+255*ia); g=Math.round(g*af+255*ia); b=Math.round(b*af+255*ia); }
    const key=((r<<16)|(g<<8)|b)>>>0; px[i]=key; hist.set(key,(hist.get(key)||0)+1);
  }
  const colors=[]; for(const [key,count] of hist) colors.push({r:(key>>16)&255,g:(key>>8)&255,b:key&255,count,key});
  const palette=new Uint32Array(256), map=new Map();
  if(colors.length<=maxColors){ colors.forEach((c,i)=>{ palette[i]=c.key; map.set(c.key,i); }); }
  else {
    const range=(box)=>{ let a=255,b=0,c=255,d=0,e=255,f=0; for(const x of box){ if(x.r<a)a=x.r; if(x.r>b)b=x.r; if(x.g<c)c=x.g; if(x.g>d)d=x.g; if(x.b<e)e=x.b; if(x.b>f)f=x.b; } return {rr:b-a,gr:d-c,br:f-e}; };
    const boxes=[colors];
    while(boxes.length<maxColors){
      let bi=-1,best=-1;
      for(let i=0;i<boxes.length;i++){ if(boxes[i].length<2) continue; const rg=range(boxes[i]); const mr=Math.max(rg.rr,rg.gr,rg.br); let pop=0; for(const c of boxes[i]) pop+=c.count; const s=mr*pop; if(s>best){ best=s; bi=i; } }
      if(bi<0) break;
      const box=boxes[bi], rg=range(box); let ch='r';
      if(rg.gr>=rg.rr&&rg.gr>=rg.br) ch='g'; else if(rg.br>=rg.rr&&rg.br>=rg.gr) ch='b';
      box.sort((a,b)=>a[ch]-b[ch]);
      let total=0; for(const c of box) total+=c.count; let acc=0,split=0;
      for(let i=0;i<box.length;i++){ acc+=box[i].count; if(acc*2>=total){ split=i+1; break; } }
      if(split<=0) split=1; if(split>=box.length) split=box.length-1;
      boxes.splice(bi,1,box.slice(0,split),box.slice(split));
    }
    boxes.forEach((box,idx)=>{ let sr=0,sg=0,sb=0,sc=0; for(const c of box){ sr+=c.r*c.count; sg+=c.g*c.count; sb+=c.b*c.count; sc+=c.count; } palette[idx]=((Math.round(sr/sc)<<16)|(Math.round(sg/sc)<<8)|Math.round(sb/sc))>>>0; for(const c of box) map.set(c.key,idx); });
  }
  const indices=new Uint8Array(n); for(let i=0;i<n;i++) indices[i]=map.get(px[i]);
  return { width, height, palette, indices };
}

function parseFilename(name){
  const m=/cover_(.+)~([0-9A-Fa-f]{1,8})\.(bmp|png)$/i.exec(name);
  if(!m) return null; return { serial:m[1].slice(0,11), hash:parseInt(m[2],16)>>>0 };
}

function discHash(u8){
  const sig="SEGA SEGASATURN "; const match=(o)=>{ for(let k=0;k<16;k++) if(u8[o+k]!==sig.charCodeAt(k)) return false; return true; };
  let ip=0; if(!match(0)) ip=0x10; if(!match(ip)) return null;
  let gid=''; for(let k=0;k<16;k++){ const c=u8[ip+0x20+k]; if(c===0) break; gid+=String.fromCharCode(c); }
  const vi=gid.lastIndexOf('V'); if(vi>=0) gid=gid.slice(0,vi);
  const si=gid.indexOf(' '); if(si>=0) gid=gid.slice(0,si);
  const hash=adler32(u8.subarray(ip+0x30,ip+0x30+64));
  return { serial:gid.trim(), hash };
}
