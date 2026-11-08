import assert from 'node:assert/strict';
import { test } from 'node:test';
import { pathToFileURL } from 'node:url';
import { readFile } from 'node:fs/promises';
const { SaveText, dateFields, dateMinutes, dateText, parseDateText } = await import(process.env.SAROO_SAVE_TEXT
  ? pathToFileURL(process.env.SAROO_SAVE_TEXT) : '../tools/savetool/web/src/save-text.mjs');
import { fixedBytes, byteText } from '../tools/savetool/web/src/save-fields.mjs';
const raw = text => new TextEncoder().encode(text);

test('calendar conversion matches independent Gregorian dates across the full u32 range', async () => {
  const cases = JSON.parse(await readFile(process.env.SAROO_SAVE_DATE_FIXTURE, 'utf8'));
  for (const [minutes, date] of cases) {
    assert.deepEqual(dateFields(minutes), date); assert.equal(dateMinutes(date), minutes);
    assert.equal(parseDateText(dateText(minutes)), minutes);
  }
  for (const date of [
    {year:2100,month:2,day:29,hour:0,minute:0}, {year:2026,month:4,day:31,hour:0,minute:0},
    {year:1979,month:12,day:31,hour:23,minute:59}, {year:1980,month:0,day:1,hour:0,minute:0},
    {year:1980,month:1,day:1,hour:24,minute:0}, {year:1980,month:1,day:1,hour:0,minute:60},
    {year:10146,month:12,day:31,hour:23,minute:59},
  ]) assert.throws(() => dateMinutes(date));
  for (const value of [-1, NaN, 4294967296, 1.5]) assert.throws(() => dateFields(value));
  for (const text of ['2026-1-01 00:00','2026-01-01T00:00:01','2026-01-01T00:00Z','2026-01-01T00:00 junk']) assert.throws(() => parseDateText(text));
});

test('complete decoder inverse matches first-valid-byte policy for every supported two-byte character', async () => {
  const codec = new SaveText(), decoder = new TextDecoder('shift-jis', {fatal:true}), inverse = new Map();
  for (let first = 0x81; first <= 0xfc; first++) {
    if (first === 0xa0) first = 0xe0;
    for (let second = 0x40; second <= 0xfc; second++) {
      if (second === 0x7f) continue;
      let text; try { text = decoder.decode(Uint8Array.of(first,second)); } catch { continue; }
      assert.equal(text.length,1); if (!inverse.has(text)) inverse.set(text, Uint8Array.of(first,second));
    }
  }
  for (const [text,bytes] of inverse) assert.deepEqual(await codec.encode(text,2),bytes,`${text} ${byteText(bytes)}`);
  assert.equal(codec.reverseBytes,45120);
  for (let point=0;point<=0x80;point++) assert.deepEqual(await codec.encode(String.fromCharCode(point),1),Uint8Array.of(point));
  for (let point=0xff61;point<=0xff9f;point++) assert.deepEqual(await codec.encode(String.fromCharCode(point),1),Uint8Array.of(point-0xff61+0xa1));
});

test('Japanese entry observes byte capacity and rejects unsupported text without dropping characters', async () => {
  const codec = new SaveText();
  assert.deepEqual(await codec.encode('ソニック', 11), Uint8Array.of(0x83,0x5c,0x83,0x6a,0x83,0x62,0x83,0x4e));
  assert.deepEqual(await codec.encode('あいうえおA',11), Uint8Array.of(0x82,0xa0,0x82,0xa2,0x82,0xa4,0x82,0xa6,0x82,0xa8,65));
  for (const text of ['あいうえおか','A😀','A\ud800','—']) await assert.rejects(codec.encode(text,11));
  assert.deepEqual(await codec.encode('A',11),Uint8Array.of(65));
});

test('unchanged fields retain padding and alternative encodings; deliberate changes replace only that field', async () => {
  const codec = new SaveText(), original = Uint8Array.of(0xfa,0x5c,0,80,65,68,255,1,2,3,4);
  const display = codec.display(original); assert.equal(display.valid,true); assert.equal(display.text,'纊');
  assert.deepEqual(await codec.edit(original,display.text,'shift-jis',11),original); assert.equal(codec.reverseBytes,0);
  assert.deepEqual(await codec.edit(original,'NEW','shift-jis',11),fixedBytes(raw('NEW'),11));
  const invalid=Uint8Array.of(0x82,0,80,65,68);
  const view=codec.display(invalid);assert.equal(view.valid,false);
  assert.deepEqual(await codec.edit(invalid,view.text,'shift-jis',11),fixedBytes(invalid,11));
  assert.deepEqual(await codec.edit(invalid,'Renamed','utf-8',11),fixedBytes(raw('Renamed'),11));
  await assert.rejects(codec.edit(original,'x\0y','shift-jis',11));
  const changed=await codec.edit(original,'A%00PAD','bytes',11);assert.deepEqual(changed,fixedBytes(Uint8Array.of(65,0,80,65,68),11));
  await assert.rejects(codec.edit(original,'é'.repeat(6),'utf-8',11));
});

test('cancellation rejects work and leaves any previously prepared table reusable', async () => {
  const codec=new SaveText(), controller=new AbortController();controller.abort();
  await assert.rejects(codec.encode('あ',11,{signal:controller.signal}));assert.equal(codec.reverseBytes,0);
  await assert.rejects(codec.edit(raw('A'),'A','shift-jis',11,{signal:controller.signal}));
  assert.deepEqual(await codec.encode('あ',2),Uint8Array.of(0x82,0xa0));
  await assert.rejects(codec.encode('い',2,{signal:controller.signal}));assert.equal(codec.reverseBytes,45120);
});
