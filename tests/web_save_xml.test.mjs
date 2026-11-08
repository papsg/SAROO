import assert from 'node:assert/strict';
import { test } from 'node:test';
import { readFile, writeFile } from 'node:fs/promises';
import { createRequire } from 'node:module';
import { createHash } from 'node:crypto';
import { parseXMLSave, encodeXMLSave } from '../tools/savetool/web/src/save-xml.mjs';
import { SaveArchive } from '../tools/savetool/web/src/save-codec.mjs';
import { fixedBytes } from '../tools/savetool/web/src/save-fields.mjs';
const sax = createRequire(import.meta.url)(process.env.SAROO_SAVE_SAX);
const root=process.env.SAROO_SAVE_XML_FIXTURES;
const manifest=JSON.parse(await readFile(`${root}/fixtures.json`,'utf8'));
const load=name=>readFile(`${root}/${name}`).then(bytes=>new Blob([bytes]));
const hash=bytes=>createHash('sha256').update(bytes).digest('hex');
const parse=text=>parseXMLSave(new Blob([text]),sax);
const simple='<Sega_Saturn_Save><mandatory><name>A</name><comment>B</comment><language_code>1</language_code><size>3</size><date year="2000" month="2" day="29"/><time hour="12" minute="34" second="0"/><data>AQID</data></mandatory><optional/></Sega_Saturn_Save>';

test('quoted attributes reject literal less-than and retain escaped comparisons',async t=>{
  const encode=(text,kind)=>{
    if(kind==='utf8')return new Blob([text]);
    const bytes=Buffer.from(text,'utf16le');if(kind==='utf16be')bytes.swap16();
    return new Blob([kind==='utf16be'?Uint8Array.of(254,255):Uint8Array.of(255,254),bytes]);
  };
  const invalid=[],valid=[];
  for(const quote of ['"',"'"]){
    invalid.push([`optional ${quote}`,simple.replace('<optional/>',`<optional value=${quote}A<B${quote}/>`)],
      [`note ${quote}`,simple.replace('<optional/>',`<optional><note value=${quote}A<B${quote}/></optional>`)]);
    for(const ref of ['&lt;','&#60;','&#x3c;'])valid.push([`reference ${ref} ${quote}`,
      simple.replace('<optional/>',`<optional><note value=${quote}A${ref}B${quote}/></optional>`)]);
  }
  valid.push(['comment','<!--A<B-->'+simple],['PI','<?note A<B?>'+simple],
    ['CDATA',simple.replace('<name>A</name>','<name><![CDATA[A<B]]></name>')],
    ['literal greater-than',simple.replace('<optional/>','<optional value="A>B"/>')]);
  await writeFile(`${root}/xml-attribute-syntax.json`,JSON.stringify([
    ...invalid.map(([name,text])=>({name,text,valid:false})),
    ...valid.map(([name,text])=>({name,text,valid:true}))]));
  for(const kind of ['utf8','utf16le','utf16be']){
    for(const [name,text] of invalid)await t.test(`reject ${kind} ${name}`,async()=>{
      await assert.rejects(parseXMLSave(encode(text,kind),sax),/less-than|attribute/);
    });
    for(const [name,text] of valid)await t.test(`accept ${kind} ${name}`,async()=>{
      assert.deepEqual([...(await parseXMLSave(encode(text,kind),sax)).data],[1,2,3]);
    });
    const prefix='<Sega_Saturn_Save><mandatory><name>A</name><data>AQID</data></mandatory><optional><note value="';
    const suffix='B"/></optional></Sega_Saturn_Save>',unit=kind==='utf8'?1:2,bom=kind==='utf8'?0:2;
    for(const delta of [-unit,0])await t.test(`reject ${kind} literal at boundary ${delta}`,async()=>{
      const padding=(32768+delta-bom)/unit-prefix.length;
      await assert.rejects(parseXMLSave(encode(prefix+'x'.repeat(padding)+'<'+suffix,kind),sax),/less-than|attribute/);
    });
    await t.test(`accept ${kind} escaped less-than split in entity`,async()=>{
      const padding=(32768-bom)/unit-prefix.length-2;
      assert.deepEqual([...(await parseXMLSave(encode(prefix+'x'.repeat(padding)+'&lt;'+suffix,kind),sax)).data],[1,2,3]);
    });
  }
});

test('processing-instruction targets follow the complete XML 1.0 Name grammar',async t=>{
  const encode=(text,kind)=>{
    if(kind==='utf8')return new Blob([text]);
    const bytes=Buffer.from(text,'utf16le');if(kind==='utf16be')bytes.swap16();
    return new Blob([kind==='utf16be'?Uint8Array.of(254,255):Uint8Array.of(255,254),bytes]);
  };
  const invalid=['','1note','.note','-note','\u00b7note','\u0300note','\u203fnote',
    'note/','note=','\u00d7','\u00f7','\u037e','\u2000','\u206f','\u3000','\u{f0000}'];
  const common=['note','_','a:b',':','note1.-_','é','Ω','あ','a\u00b7','a\u0300'];
  // Fifth Edition additions are normative controls: local Expat still uses an
  // older Name table and cannot independently qualify these valid spellings.
  const fifth=['\u200c','\u200d','\u2070','\u218f','\u2c00','\u2fef','\u3001','\ud7ff',
    '\uf900','\ufdcf','\ufdf0','\ufffd','\u{10000}','\u{effff}','a\u203f','a\u2040'];
  const source=name=>`<?${name} x?>`+simple;
  await writeFile(`${root}/xml-pi-syntax.json`,JSON.stringify([
    ...invalid.map(name=>({name,text:source(name),valid:false})),
    ...common.map(name=>({name,text:source(name),valid:true})),
    {name:'target missing whitespace separator',text:'<?note?body?>'+simple,valid:false},
    {name:'body with question mark',text:'<?note ?body?>'+simple,valid:true}]));
  for(const kind of ['utf8','utf16le','utf16be']){
    for(const name of invalid)await t.test(`reject ${kind} target ${JSON.stringify(name)}`,async()=>{
      await assert.rejects(parseXMLSave(encode(source(name),kind),sax),/processing-instruction target/);
    });
    for(const name of [...common,...fifth])await t.test(`accept ${kind} target ${JSON.stringify(name)}`,async()=>{
      assert.deepEqual([...(await parseXMLSave(encode(source(name),kind),sax)).data],[1,2,3]);
    });
    for(const [name,token,valid] of [['target missing separator','<?note?body?>',false],
      ['separated body','<?note ?body?>',true],['no body','<?note?>',true]]){
      await t.test(`${valid?'accept':'reject'} ${kind} ${name}`,async()=>{
        const input=encode(simple.replace('<optional/>',`<optional>${token}</optional>`),kind);
        if(valid)assert.deepEqual([...(await parseXMLSave(input,sax)).data],[1,2,3]);
        else await assert.rejects(parseXMLSave(input,sax),/processing-instruction target/);
      });
    }
    const unit=kind==='utf8'?1:2,bom=kind==='utf8'?0:2;
    for(const [name,token,split,valid] of [['close','<?note?>',7*unit,true],
      ['bad close','<?note?body?>',7*unit,false],['supplementary name','<?\u{10000}note?>',kind==='utf8'?3:6,true]]){
      await t.test(`${valid?'accept':'reject'} ${kind} chunk-split ${name}`,async()=>{
        const input=encode(' '.repeat((32768-bom-split)/unit)+token+simple,kind);
        if(valid)assert.deepEqual([...(await parseXMLSave(input,sax)).data],[1,2,3]);
        else await assert.rejects(parseXMLSave(input,sax),/processing-instruction target/);
      });
    }
  }
});

test('tag names immediately follow their delimiters while legal tag whitespace is retained',async t=>{
  const encode=(text,kind)=>{
    if(kind==='utf8')return new Blob([text]);
    const bytes=Buffer.from(text,'utf16le');if(kind==='utf16be')bytes.swap16();
    return new Blob([kind==='utf16be'?Uint8Array.of(254,255):Uint8Array.of(255,254),bytes]);
  };
  const invalid=[['root space',simple.replace('<Sega','< Sega')],
    ['mandatory tab',simple.replace('<mandatory>','<\tmandatory>')],
    ['closing name space',simple.replace('</name>','</ name>')],
    ['closing mandatory LF',simple.replace('</mandatory>','</\nmandatory>')],
    ['empty tag space',simple.replace('<optional/>','< optional/>')],
    ['space before slash',simple.replace('</name>','< /name>')],
    ['optional CRLF',simple.replace('<optional/>','<optional><\r\nnote/></optional>')]];
  const valid=[['root trailing whitespace',simple.replace('<Sega_Saturn_Save>','<Sega_Saturn_Save \t\n>')],
    ['end tag trailing whitespace',simple.replace('</name>','</name \t>')],
    ['attribute whitespace',simple.replace('<optional/>',`<optional ><note a = "x" b = 'y' /></optional>`) ],
    ['comment','<!--< name> </ name>-->'+simple],['PI','<?note < name> </ name>?>'+simple],
    ['CDATA',simple.replace('<name>A</name>','<name><![CDATA[< name>]]></name>')]];
  await writeFile(`${root}/xml-tag-syntax.json`,JSON.stringify([
    ...invalid.map(([name,text])=>({name,text,valid:false})),
    ...valid.map(([name,text])=>({name,text,valid:true}))]));
  for(const kind of ['utf8','utf16le','utf16be']){
    for(const [name,text] of invalid)await t.test(`reject ${kind} ${name}`,async()=>{
      await assert.rejects(parseXMLSave(encode(text,kind),sax),/delimiter|tag name/);
    });
    for(const [name,text] of valid)await t.test(`accept ${kind} ${name}`,async()=>{
      assert.deepEqual([...(await parseXMLSave(encode(text,kind),sax)).data],[1,2,3]);
    });
    const prefix='<Sega_Saturn_Save><mandatory><name>A</name><data>AQID</data></mandatory><optional>';
    const unit=kind==='utf8'?1:2,bom=kind==='utf8'?0:2;
    for(const [name,token,opening,preceding,valid] of [
      ['opening','< note/>','',1,false],['opening','<note />','',1,true],
      ['closing','</ note>','<note>',2,false],['closing','</note \t>','<note>',2,true]]){
      await t.test(`${valid?'accept':'reject'} ${kind} chunk-split ${name}`,async()=>{
        const padding=(32768-bom)/unit-prefix.length-opening.length-preceding;
        const input=encode(prefix+' '.repeat(padding)+opening+token+'</optional></Sega_Saturn_Save>',kind);
        if(valid)assert.deepEqual([...(await parseXMLSave(input,sax)).data],[1,2,3]);
        else await assert.rejects(parseXMLSave(input,sax),/delimiter|tag name/);
      });
    }
  }
});

test('XML entities and CDATA opening delimiters retain exact case',async t=>{
  const encode=(text,kind)=>{
    if(kind==='utf8')return new Blob([text]);
    const bytes=Buffer.from(text,'utf16le');if(kind==='utf16be')bytes.swap16();
    return new Blob([kind==='utf16be'?Uint8Array.of(254,255):Uint8Array.of(255,254),bytes]);
  };
  const field=value=>simple.replace('<name>A</name>',`<name>${value}</name>`);
  const optional=value=>simple.replace('<optional/>',`<optional>${value}</optional>`);
  const invalid=[...['AMP','LT','GT','APOS','QUOT','#X41','#X4a','unknown'].map(entity=>
    [`entity ${entity}`,field(`&${entity};`)]),
    ['attribute entity',simple.replace('<optional/>','<optional note="&AMP;"/>')],
    ['optional entity',optional('&AMP;')],
    ...['cdata','CData','cDaTa'].map(word=>[`delimiter ${word}`,field(`<![${word}[A]]>`)])];
  const valid=[['predefined',field('&amp;&lt;&gt;&apos;&quot;'),'&<>\'"'],
    ['decimal and hex digits',field('&#65;&#x41;&#x4a;&#x4A;&#00065;'),'AAJJA'],
    ['CDATA',field('<![CDATA[&AMP;&#X41;]]>'),'&AMP;&#X41;'],
    ['attribute references',simple.replace('<optional/>','<optional note="&amp;&#x4A;"/>'),'A'],
    ['comment','<!--&AMP;&#X41;<![cdata[-->'+simple,'A'],
    ['PI','<?note &AMP;&#X41;<![cdata[?>'+simple,'A']];
  await writeFile(`${root}/xml-case-syntax.json`,JSON.stringify([
    ...invalid.map(([name,text])=>({name,text,valid:false})),
    ...valid.map(([name,text])=>({name,text,valid:true}))]));
  for(const kind of ['utf8','utf16le','utf16be']){
    for(const [name,text] of invalid)await t.test(`reject ${kind} ${name}`,async()=>{
      await assert.rejects(parseXMLSave(encode(text,kind),sax),/entity|CDATA/);
    });
    for(const [name,text,expected] of valid)await t.test(`accept ${kind} ${name}`,async()=>{
      const save=await parseXMLSave(encode(text,kind),sax);
      assert.equal(new TextDecoder().decode(save.rawFilename).replace(/\0.*$/s,''),expected);
      assert.deepEqual([...save.data],[1,2,3]);
    });
    const prefix='<Sega_Saturn_Save><mandatory><name>A</name><data>AQID</data></mandatory><optional>';
    const unit=kind==='utf8'?1:2,bom=kind==='utf8'?0:2;
    for(const [name,token,preceding,valid] of [
      ['bad entity','&AMP;',2,false],['valid entity','&amp;',2,true],
      ['bad hex marker','&#X41;',3,false],['valid hex marker','&#x4A;',3,true],
      ['bad CDATA','<![cdata[A]]>',6,false],['valid CDATA','<![CDATA[A]]>',6,true]]){
      await t.test(`${valid?'accept':'reject'} ${kind} chunk-split ${name}`,async()=>{
        const padding=(32768-bom)/unit-prefix.length-preceding;
        const input=encode(prefix+' '.repeat(padding)+token+'</optional></Sega_Saturn_Save>',kind);
        if(valid)assert.deepEqual([...(await parseXMLSave(input,sax)).data],[1,2,3]);
        else await assert.rejects(parseXMLSave(input,sax),/entity|CDATA/);
      });
    }
  }
});

test('CDATA closing delimiter is lexical: reject ordinary text and retain legal spellings',async t=>{
  const encode=(text,kind)=>{
    if(kind==='utf8')return new Blob([text]);
    const bytes=Buffer.from(text,'utf16le');if(kind==='utf16be')bytes.swap16();
    return new Blob([kind==='utf16be'?Uint8Array.of(254,255):Uint8Array.of(255,254),bytes]);
  };
  for(const kind of ['utf8','utf16le','utf16be']){
    await t.test(`reject ${kind} literal in ordinary name text`,async()=>{
      await assert.rejects(parseXMLSave(encode(simple.replace('<name>A</name>','<name>A]]>B</name>'),kind),sax),/CDATA|character data/);
    });
    const forms=[['escaped','<name>A]]&gt;B</name>'],['character reference','<name>A]]&#62;B</name>'],
                 ['split CDATA','<name><![CDATA[A]]]]><![CDATA[>B]]></name>']];
    for(const [name,field] of forms)await t.test(`accept ${kind} ${name}`,async()=>{
      const save=await parseXMLSave(encode(simple.replace('<name>A</name>',field),kind),sax);
      assert.equal(new TextDecoder().decode(save.rawFilename).replace(/\0.*$/s,''),'A]]>B');
    });
    for(const [name,text] of [['comment','<!--]]>-->'+simple],['PI','<?note ]]>?>'+simple],
                            ['quoted attribute',simple.replace('<optional/>','<optional note="]]>"/>')]]){
      await t.test(`accept ${kind} delimiter in ${name}`,async()=>{
        assert.deepEqual([...(await parseXMLSave(encode(text,kind),sax)).data],[1,2,3]);
      });
    }
    const prefix='<Sega_Saturn_Save><mandatory><name>A</name><data>AQID</data></mandatory><optional><note>';
    const unit=kind==='utf8'?1:2,bom=kind==='utf8'?0:2;
    for(const preceding of [1,2])await t.test(`reject ${kind} delimiter split after ${preceding} brackets`,async()=>{
      const at=32768-unit*preceding,padding=(at-bom)/unit-prefix.length;
      const text=prefix+'x'.repeat(padding)+']]></note></optional></Sega_Saturn_Save>';
      await assert.rejects(parseXMLSave(encode(text,kind),sax),/CDATA|character data/);
    });
  }
});

test('XML declaration must precede all prolog content across encodings and chunks',async t=>{
  const declaration='<?xml version="1.0"?>';
  const prefixes=[['whitespace',' \n'],['comment','<!--prefix-->'],['PI','<?note prefix?>'],
                  ['chunk-crossing comment','<!--'+'x'.repeat(33000)+'-->']];
  const encode=(text,kind)=>{
    if(kind==='utf8')return new Blob([text]);
    if(kind==='utf8-bom')return new Blob([Uint8Array.of(239,187,191),text]);
    const bytes=Buffer.from(text,'utf16le');if(kind==='utf16be')bytes.swap16();
    return new Blob([kind==='utf16be'?Uint8Array.of(254,255):Uint8Array.of(255,254),bytes]);
  };
  for(const kind of ['utf8','utf8-bom','utf16le','utf16be']){
    for(const [name,prefix] of prefixes)await t.test(`reject ${kind} declaration after ${name}`,async()=>{
      await assert.rejects(parseXMLSave(encode(prefix+declaration+simple,kind),sax),/declaration position/);
    });
    for(const [name,text] of [['leading declaration',declaration+'\n<!--allowed--><?note allowed?>'+simple],
                             ['legal Misc without declaration',' \n<!--allowed--><?note allowed?>'+simple]]){
      await t.test(`accept ${kind} ${name}`,async()=>{
        const save=await parseXMLSave(encode(text,kind),sax);
        assert.deepEqual([...save.data],[1,2,3]);assert.equal(save.rawFilename[0],65);
      });
    }
  }
  for(const big of [false,true])await t.test(`accept UTF16 ${big?'BE':'LE'} declaration without BOM`,async()=>{
    const bytes=Buffer.from(declaration+simple,'utf16le');if(big)bytes.swap16();
    assert.deepEqual([...(await parseXMLSave(new Blob([bytes]),sax)).data],[1,2,3]);
  });
});

test('independent UTF-8/UTF-16 XML and all base64 boundaries preserve authoritative bytes',async()=>{
  for(const item of manifest){
    const save=await parseXMLSave(await load(item.file),sax);
    assert.equal(hash(save.data),item.payload_sha256,item.file);assert.equal(save.data.length,item.size);
    assert.deepEqual([...save.rawFilename],item.name);assert.deepEqual([...save.rawComment],item.comment);
    assert.equal(save.date,item.date);assert.equal(save.language,item.language);
    const output=await encodeXMLSave(save);await writeFile(`${root}/${item.file}.out.xml`,new Uint8Array(await output.arrayBuffer()));
    const again=await parseXMLSave(output,sax);assert.deepEqual(again,save);
  }
});
test('text-only fields retain whitespace/entities and enforce encoded byte widths',async()=>{
  const save=await parse(simple.replace('<name>A</name>','<name> A &amp; B </name>'));
  assert.deepEqual(save.rawFilename,fixedBytes(new TextEncoder().encode(' A & B '),11));
  const comment=await parse(simple.replace('<comment>B</comment>','<comment><![CDATA[A<B]]></comment>'));
  assert.deepEqual(comment.rawComment,fixedBytes(new TextEncoder().encode('A<B'),10));
  await assert.rejects(parse(simple.replace('<name>A</name>','<name>あいうえお</name>')),/capacity|bytes/);
  await assert.rejects(parse(simple.replace('<comment>B</comment>','<comment>éééééé</comment>')),/capacity|bytes/);
});
test('malformed numeric, binary, calendar, root and repeated fields fail before archive mutation',async()=>{
  const archive=SaveArchive.create('mems');archive.update(0,null,{rawFilename:new TextEncoder().encode('ORIGINAL'),data:Uint8Array.of(9)});
  const original=new Uint8Array(await(await archive.export()).arrayBuffer()),revision=archive.revision;
  const cases=[
    simple.replace('<size>3','<size>3junk'),simple.replace('<size>3','<size>4'),simple.replace('<size>3','<size>-1'),
    simple.replace('AQID','AQI!'),simple.replace('AQID','A'),simple.replace('AQID','AQID='),
    simple.replace('year="2000"','year="2100"'),simple.replace('second="0"','second="1"'),
    simple.replace('year="2000"','year="2000" year="2001"'),simple.replace('<language_code>1','<language_code>256'),
    simple.replace('<name>A</name>','<name>A</name><name>B</name>'),
    simple.replace('<data>AQID</data>','<data><data>AQID</data></data>'),
    simple.replace('<name>A</name>','<name_binary>QUFBQUFBQUFBQUFB</name_binary>'),
    `<wrapper>${simple}</wrapper>`,simple+simple,
    simple.replace('<data>AQID</data>',''),simple.replace('<name>A</name>','<name>&#0;</name>'),
    '<?xml version="1.0" encoding="UTF-16"?>'+simple,
    '<!DOCTYPE Sega_Saturn_Save>'+simple,
  ];
  for(const input of cases){
    await assert.rejects(async()=>{const save=await parse(input);archive.update(0,0,save);});
    assert.equal(archive.revision,revision);assert.deepEqual(new Uint8Array(await(await archive.export()).arrayBuffer()),original);
  }
});
test('optional XML fields and counter are reported as unstorable supplemental metadata',async()=>{
  assert.equal((await parse(simple)).omittedExtraMetadata,false);
  assert.equal((await parse(simple.replace('<optional/>','<optional><author name="Artist">Note</author></optional>'))).omittedExtraMetadata,true);
  assert.equal((await parse(simple.replace('<name>A</name>','<counter>7</counter><name>A</name>'))).omittedExtraMetadata,true);
});
test('streaming whitespace crosses chunks without reducing payload capacity and cancellation preserves the archive',async()=>{
  const text=simple.replace('AQID',' '.repeat(32760)+'A\r\nQ\tI D'+' '.repeat(100000));
  assert.deepEqual((await parse(text)).data,Uint8Array.of(1,2,3));
  const controller=new AbortController();controller.abort();await assert.rejects(parseXMLSave(new Blob([text]),sax,{signal:controller.signal}));
  const later=new AbortController();
  await assert.rejects(parseXMLSave(new Blob([simple.replace('AQID',' '.repeat(8*1048576)+'AQID')]),sax,{signal:later.signal,onProgress:()=>later.abort()}));
});
test('XML export owns its data across asynchronous progress callbacks',async()=>{
  const item=await parseXMLSave(await load(manifest[8].file),sax),expected=hash(item.data);
  const real=globalThis.performance;let ticks=0,callbacks=0,blob;
  try {
    // Force cooperative checkpoints deterministically, independent of host speed.
    globalThis.performance={now:()=>++ticks*9};
    blob=await encodeXMLSave(item,{onProgress:()=>{callbacks++;item.data.fill(77);}});
  } finally {globalThis.performance=real;}
  assert(callbacks>1);const output=await parseXMLSave(blob,sax);assert.equal(hash(output.data),expected);
});
