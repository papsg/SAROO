"""Bounded 512-byte exFAT media, adapted from paps-g SSR-163 / f3714b9e."""
from pathlib import Path
import hashlib
import struct

def rolling(data,bits):
 total=0;mask=(1<<bits)-1
 for value in data:total=((total>>1)|((total&1)<<(bits-1)))+value;total&=mask
 return total

def inspect(path):
 data=path.read_bytes();u16=lambda o:struct.unpack_from('<H',data,o)[0];u32=lambda o:struct.unpack_from('<I',data,o)[0];u64=lambda o:struct.unpack_from('<Q',data,o)[0]
 assert data[3:11]==b'EXFAT   ' and u16(510)==0xaa55
 ss=1<<data[108];cs=1<<data[109];assert ss==512 and data[109]<=16 and data[110]==1
 sectors=u64(72);fat=u32(80);fatsectors=u32(84);heap=u32(88);count=u32(92);root=u32(96)
 assert sectors*ss==len(data) and fat>=24 and fat+fatsectors<=heap and heap+count*cs<=sectors and fatsectors*ss>=(count+2)*4
 for region in [0,12]:
  block=data[region*ss:(region+11)*ss];value=rolling(bytes(b for i,b in enumerate(block) if i not in [106,107,112]),32);assert all(u32((region+11)*ss+i)==value for i in range(0,ss,4))
 def chain(first):
  result=[];seen=set();c=first
  while c!=0xffffffff:
   assert 2<=c<count+2 and c not in seen
   seen.add(c);result.append(c);c=u32(fat*ss+c*4)
  return result
 def contents(clusters):return b''.join(data[(heap+(c-2)*cs)*ss:(heap+(c-1)*cs)*ss] for c in clusters)
 rc=chain(root);directory=contents(rc);entries=[];position=0;bitmap=table=None
 while position<len(directory):
  raw=directory[position:position+32];typ=raw[0]
  if typ==0:break
  if typ==0x81:
   assert bitmap is None;bitmap=raw
  elif typ==0x82:
   assert table is None;table=raw
  elif typ==0x85:
   secondary=raw[1];assert 2<=secondary<=18;block=directory[position:position+(secondary+1)*32];assert len(block)==(secondary+1)*32;assert rolling(block[:2]+block[4:],16)==struct.unpack_from('<H',raw,2)[0];stream=block[32:64];assert stream[0]==0xc0
   length=stream[3];name=[]
   for o in range(64,len(block),32):
    assert block[o]==0xc1;name+=list(struct.unpack_from('<15H',block,o+2))
   name=name[:length];assert len(name)==length and length>0
   first=struct.unpack_from('<I',stream,20)[0];size=struct.unpack_from('<Q',stream,24)[0];valid=struct.unpack_from('<Q',stream,8)[0];assert 0<=valid<=size
   if stream[1]&2:
    assert size>0 and first>=2
    clusters=list(range(first,first+(size+cs*ss-1)//(cs*ss)))
   else:clusters=chain(first) if first else []
   assert len(clusters)*cs*ss>=size and all(2<=c<count+2 for c in clusters)
   payload=contents(clusters)[:size];text=struct.pack('<'+'H'*len(name),*name).decode('utf-16le')
   entries.append({'name':text,'units':name,'clusters':clusters,'bytes':size,'valid_bytes':valid,'stored_hash':struct.unpack_from('<H',stream,4)[0],'payload_sha256':hashlib.sha256(payload).hexdigest(),'entry_set_checksum_valid':True})
   position+=secondary*32
  else:assert not typ&0x80 or typ==0x83
  position+=32
 assert bitmap is not None and table is not None
 bc=chain(struct.unpack_from('<I',bitmap,20)[0]);bitmap_size=struct.unpack_from('<Q',bitmap,24)[0];bits=contents(bc)[:bitmap_size];assert len(bits)*8>=count
 tc=chain(struct.unpack_from('<I',table,20)[0]);table_size=struct.unpack_from('<Q',table,24)[0];encoded=contents(tc)[:table_size];assert len(encoded)==table_size and table_size%2==0 and rolling(encoded,32)==struct.unpack_from('<I',table,4)[0]
 mappings=[];cursor=0
 while len(mappings)<65536:
  assert cursor+2<=len(encoded);value=struct.unpack_from('<H',encoded,cursor)[0];cursor+=2
  if value==0xffff and len(mappings)!=0xffff:
   assert cursor+2<=len(encoded);skip=struct.unpack_from('<H',encoded,cursor)[0];cursor+=2;assert skip and len(mappings)+skip<=65536;mappings+=list(range(len(mappings),len(mappings)+skip))
  else:mappings.append(value)
 assert cursor==len(encoded) and len(mappings)==65536
 assert mappings[:128]==[c-32 if 97<=c<=122 else c for c in range(128)]
 owners={}
 for role,clusters in [('root',rc),('bitmap',bc),('upcase',tc)]+[(x['name'],x['clusters']) for x in entries]:
  for c in clusters:assert c not in owners;owners[c]=role
 allocated={i+2 for i in range(count) if bits[i//8]&(1<<(i%8))};assert allocated==set(owners)
 groups={};bad=[]
 for f in entries:
  canonical=[mappings[c] for c in f['units']];folded=struct.pack('<'+'H'*len(canonical),*canonical);required=rolling(folded,16);f['required_hash']=required;f['hash_valid']=required==f['stored_hash'];groups.setdefault(tuple(canonical),[]).append(f['name'])
  if not f['hash_valid']:bad.append(f['name'])
 return {'path':str(path),'sha256':hashlib.sha256(data).hexdigest(),'bytes':len(data),'boot_checksums_valid':True,'geometry_and_full_allocation_valid':True,'no_crosslinks_or_orphans':True,'upcase_mappings':len(mappings),'upcase_bytes':len(encoded),'mandatory_ascii_valid':True,'mapping_0250':mappings[0x250],'mapping_2c6f':mappings[0x2c6f],'files':entries,'invalid_name_hashes':bad,'case_equivalent_groups':[x for x in groups.values() if len(x)>1]}

class Volume:
 def __init__(self,path):
  self.path=path;self.data=bytearray(path.read_bytes());self.ss=512;self.cs=1<<self.data[109];self.fat=self.u32(80);self.heap=self.u32(88);self.count=self.u32(92);self.root=self.u32(96);root=self.chain(self.root);self.root_clusters=root;self.entries=[]
  for c in root:
   for p in range(self.at(c),self.at(c)+self.cs*512,32):
    if not self.data[p]:self.end=p;break
    self.entries.append(p)
   else:continue
   break
  self.bitmap=next(p for p in self.entries if self.data[p]==0x81);self.table=next(p for p in self.entries if self.data[p]==0x82);self.bitmap_clusters=self.chain(self.u32(self.bitmap+20));self.table_clusters=self.chain(self.u32(self.table+20));self.length=self.u64(self.table+24);self.encoded=b''.join(self.data[self.at(c):self.at(c)+self.cs*512] for c in self.table_clusters)[:self.length]
 def u32(self,p):return struct.unpack_from('<I',self.data,p)[0]
 def u64(self,p):return struct.unpack_from('<Q',self.data,p)[0]
 def put32(self,p,v):struct.pack_into('<I',self.data,p,v)
 def put64(self,p,v):struct.pack_into('<Q',self.data,p,v)
 def at(self,c):return (self.heap+(c-2)*self.cs)*512
 def chain(self,c):
  result=[]
  while c!=0xffffffff:
   assert 2<=c<self.count+2 and c not in result;result.append(c);c=self.u32(self.fat*512+c*4)
  return result
 def allocated(self,c,value=None):
  bit=c-2;logical=bit//8;cluster=self.bitmap_clusters[logical//(self.cs*512)];pos=self.at(cluster)+logical%(self.cs*512);mask=1<<(bit%8)
  if value is None:return bool(self.data[pos]&mask)
  if value:self.data[pos]|=mask
  else:self.data[pos]&=255^mask
 def maps(self):
  words=struct.unpack('<'+'H'*(len(self.encoded)//2),self.encoded);out=[];i=0
  while i<len(words):
   word=words[i];i+=1
   if word==65535 and len(out)!=65535:
    run=words[i];i+=1;assert run and len(out)+run<=65536;out.extend(range(len(out),len(out)+run))
   else:out.append(word)
  assert len(out)==65536;return out
 def replace_table(self,encoded,fragmented=False):
  for c in self.table_clusters:self.allocated(c,False);self.put32(self.fat*512+c*4,0)
  needed=(len(encoded)+self.cs*512-1)//(self.cs*512);step=2 if fragmented else 1;clusters=list(range(1000,1000+needed*step,step));assert clusters[-1]<self.count+2 and all(not self.allocated(c) for c in clusters)
  for i,c in enumerate(clusters):
   self.allocated(c,True);self.put32(self.fat*512+c*4,clusters[i+1] if i+1<len(clusters) else 0xffffffff);piece=encoded[i*self.cs*512:(i+1)*self.cs*512];self.data[self.at(c):self.at(c)+self.cs*512]=piece.ljust(self.cs*512,b'\0')
  self.put32(self.table+20,clusters[0]);self.put64(self.table+24,len(encoded));self.put32(self.table+4,rolling(encoded,32));self.table_clusters=clusters;self.encoded=encoded;self.length=len(encoded)
 def move_root_table(self):
  c=3000;assert not self.allocated(c);self.allocated(c,True);self.put32(self.fat*512+self.root_clusters[-1]*4,c);self.put32(self.fat*512+c*4,0xffffffff);entry=bytes(self.data[self.table:self.table+32]);self.data[self.table]=2
  for p in range(self.end,self.at(self.root_clusters[-1])+self.cs*512,32):self.data[p]=5
  self.data[self.at(c):self.at(c)+self.cs*512]=bytes(self.cs*512);self.data[self.at(c):self.at(c)+32]=entry;self.table=self.at(c)
 def save(self):self.path.write_bytes(self.data)
