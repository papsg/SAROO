/* SPDX-License-Identifier: GPL-3.0-only
 * Streaming SS Backup RAM Parser XML transport for SAROO PR #374.
 * Schema reference: hitomi2500/ss-save-parser, extract.cpp at 06922280.
 * Pass the pinned SAX implementation explicitly; no network or runtime import.
 */
import { MAX_SAVE_BYTES, fixedBytes, saveMetadata, unsigned } from './save-fields.mjs';
import { dateFields, dateMinutes } from './save-text.mjs';
const require = (ok, message) => { if (!ok) throw new Error(message); };
const pause = () => new Promise(resolve => setTimeout(resolve, 0));
const space = /^[\x20\t\r\n]*$/;
// XML 1.0 Fifth Edition productions 4/4a/5, including supplementary names.
const nameStart = String.raw`:A-Z_a-z\u00c0-\u00d6\u00d8-\u00f6\u00f8-\u02ff\u0370-\u037d\u037f-\u1fff\u200c-\u200d\u2070-\u218f\u2c00-\u2fef\u3001-\ud7ff\uf900-\ufdcf\ufdf0-\ufffd\u{10000}-\u{effff}`;
const xmlName = new RegExp(`^[${nameStart}][${nameStart}0-9.\\u00b7\\u0300-\\u036f\\u203f-\\u2040-]*$`, 'u');
const invalidXML = /[\u0000-\u0008\u000b\u000c\u000e-\u001f\ufffe\uffff]/u;
function checkCharacters(text) { require(!invalidXML.test(text), 'XML contains a forbidden character.'); }
function number(text, maximum, label) {
  require(typeof text === 'string' && /^\s*\d+\s*$/.test(text), `${label} must be a complete unsigned integer.`);
  return unsigned(Number(text), maximum, label);
}
function unbase64(text, maximum, label) {
  require(text.length <= Math.ceil(maximum / 3) * 4, `${label} exceeds its byte capacity.`);
  let binary; try { binary = atob(text); } catch { throw new Error(`${label} has invalid base64 data.`); }
  require(binary.length <= maximum, `${label} exceeds its byte capacity.`);
  return Uint8Array.from(binary, character => character.charCodeAt(0));
}
function base64(bytes) {
  // Callers use at most 24 KiB; avoid one argument/string allocation per byte.
  return btoa(String.fromCharCode(...bytes));
}
function escapeText(text) { return text.replace(/[&<>\r]/g, char => ({'&':'&amp;','<':'&lt;','>':'&gt;','\r':'&#13;'}[char])); }
function fieldText(bytes) {
  const end = bytes.indexOf(0), text = new TextDecoder('utf-8').decode(end < 0 ? bytes : bytes.subarray(0, end));
  // The human-readable XML fields follow the original writer's UTF-8 display
  // convention. Binary fields retain every byte, including non-text controls.
  return escapeText(text.replace(/[\u0000-\u0008\u000b\u000c\u000e-\u001f\ufffe\uffff]/gu, '\ufffd'));
}

export async function parseXMLSave(file, sax, { signal, onProgress } = {}) {
  require(file instanceof Blob && file.size > 0, 'Choose a nonempty XML file.');
  signal?.throwIfAborted();
  const prefix = new Uint8Array(await file.slice(0, 4).arrayBuffer());
  const little = prefix[0] === 255 && prefix[1] === 254 || prefix[0] === 60 && prefix[1] === 0;
  const big = prefix[0] === 254 && prefix[1] === 255 || prefix[0] === 0 && prefix[1] === 60;
  const encoding = little ? 'utf-16le' : big ? 'utf-16be' : 'utf-8';
  const decoder = new TextDecoder(encoding, { fatal: true });
  // Namespace mode emits every attribute, allowing duplicate expanded names to
  // be rejected here even where the library's non-namespace mode drops them.
  const parser = sax.parser(true, { xmlns: true, strictEntities: true, position: true });
  const groups = new Set(), values = new Map(), stack = [];
  const limits = { counter:64, name:64, comment:64, name_binary:16, comment_binary:16,
    language_code:64, size:64, data:Math.ceil(MAX_SAVE_BYTES / 3) * 4, date:0, time:0 };
  let attributes = new Set(), rootSeen = false, declaration = false, hasNotes = false;
  parser.onerror = error => { throw error; };
  parser.ondoctype = () => { throw new Error('Save XML does not use a document type or custom entities.'); };
  parser.onsgmldeclaration = () => { throw new Error('Unsupported XML declaration.'); };
  parser.onprocessinginstruction = instruction => {
    require(xmlName.test(instruction.name), 'Invalid XML processing-instruction target.');
    if (instruction.name.toLowerCase() !== 'xml') return;
    // TextDecoder consumes an initial BOM; XMLDecl must start at the first '<'.
    require(instruction.name === 'xml' && !rootSeen && !declaration && parser.startTagPosition === 1,
            'Invalid XML declaration position.');
    const match = /^version\s*=\s*(['"])1\.0\1(?:\s+encoding\s*=\s*(['"])([^'"]+)\2)?(?:\s+standalone\s*=\s*(['"])(?:yes|no)\4)?\s*$/.exec(instruction.body);
    require(match, 'Expected an XML 1.0 declaration.');
    if (match[3]) {
      const declared = match[3].toLowerCase().replaceAll('_', '-');
      require(declared === encoding || (declared === 'utf-16' && encoding.startsWith('utf-16')), 'XML encoding declaration disagrees with its bytes.');
    }
    declaration = true;
  };
  parser.onopentagstart = () => { attributes = new Set(); require(stack.length < 64, 'XML nesting exceeds 64 levels.'); };
  parser.onattribute = attribute => {
    checkCharacters(attribute.value);
    const key = `${attribute.uri}\0${attribute.local}`;
    require(!attributes.has(key), 'XML repeats an attribute.'); attributes.add(key);
    require(attributes.size <= 128, 'XML element has too many attributes.');
  };
  parser.onopentag = tag => {
    const parent = stack.at(-1), frame = { name:tag.local, optional:parent?.optional || tag.local === 'optional' };
    if (!stack.length) {
      require(!rootSeen && tag.local === 'Sega_Saturn_Save' && !tag.uri, 'Expected one Sega_Saturn_Save root.'); rootSeen = true;
    } else if (stack.length === 1) {
      require(!tag.uri && ['mandatory','optional'].includes(tag.local) && !groups.has(tag.local), 'Expected one mandatory group and at most one optional group.');
      groups.add(tag.local);
    } else if (!parent.optional) {
      require(stack.length === 2 && !tag.uri && Object.hasOwn(limits, tag.local) && !values.has(tag.local), 'Unexpected, nested or duplicate mandatory field.');
      frame.field = tag.local; frame.text = ''; frame.overflow = false;
      values.set(tag.local, frame);
    } else hasNotes = true;
    if (!frame.optional) for (const attribute of Object.values(tag.attributes)) {
      if (attribute.uri === 'http://www.w3.org/2000/xmlns/') continue;
      const allowed = frame.field === 'date' ? ['year','month','day'] : frame.field === 'time' ? ['hour','minute','second'] : [];
      require(!attribute.uri && allowed.includes(attribute.local), 'Unexpected attribute in save metadata.');
      frame[attribute.local] = attribute.value;
    }
    stack.push(frame);
  };
  const text = value => {
    checkCharacters(value); const frame = stack.at(-1);
    if (frame?.optional) { if (!space.test(value)) hasNotes = true; return; }
    if (!frame?.field || ['date','time'].includes(frame.field)) { require(space.test(value), 'Unexpected text outside a save field.'); return; }
    if (['data','name_binary','comment_binary'].includes(frame.field)) value = value.replace(/[\x20\t\r\n]/g, '');
    if (frame.text.length + value.length > limits[frame.field]) {
      if (frame.field === 'name' || frame.field === 'comment') { frame.overflow = true; return; }
      throw new Error(`XML ${frame.field} exceeds its field capacity.`);
    }
    if (!frame.overflow) frame.text += value;
  };
  parser.ontext = text; parser.oncdata = text;
  // The pinned SAX matcher folds CDATA case, even in strict mode. Inspect its
  // raw opener before SAX clears it; comments/PI/decoded text never enter here.
  parser.onopencdata = () => require(parser.sgmlDecl + parser.c === '[CDATA[', 'Invalid CDATA opening delimiter.');
  parser.onclosetag = () => { stack.pop(); };
  let bracketTail = '', piTargetEnd = false;
  function writeSource(value) {
    if (piTargetEnd && value.length) {
      // A '?' directly after the target must finish '?>'. A PI body instead
      // needs intervening XML whitespace; SAX otherwise silently inserts it.
      require(value[0] === '>', 'Invalid XML processing-instruction target delimiter.');
      piTargetEnd = false;
    }
    let at = 0;
    // SAX skips whitespace after '<' and '</'. Feed the delimiter transitions
    // separately, including across chunks, so only that illegal gap rejects.
    while (at < value.length && (parser.state === sax.STATE.OPEN_WAKA || parser.state === sax.STATE.CLOSE_TAG && !parser.tagName)) {
      require(!space.test(value[at]), 'XML tag name must immediately follow its delimiter.');
      parser.write(value[at++]);
    }
    if (at < value.length) parser.write(value.slice(at));
  }
  function writeChunk(value, final = false) {
    const input = bracketTail ? bracketTail + value : value;
    let start = 0, match;
    const delimiters = /;|\]\]>|<|\?/g;
    while ((match = delimiters.exec(input))) {
      const at = match.index;
      writeSource(input.slice(start, at));
      if (match[0] === '<') {
        require(parser.state !== sax.STATE.ATTRIB_VALUE_QUOTED, 'Literal less-than is forbidden in an XML attribute.');
      } else if (match[0] === ']]>') {
        require(parser.state !== sax.STATE.TEXT, 'CDATA closing delimiter is forbidden in ordinary character data.');
      } else if (match[0] === ';' && (parser.state === sax.STATE.TEXT_ENTITY || parser.state === sax.STATE.ATTRIB_VALUE_ENTITY_Q || parser.state === sax.STATE.ATTRIB_VALUE_ENTITY_U)) {
        // Validate spelling before SAX's case-folding entity lookup. Hex digits
        // may use either case; the 'x' marker and predefined names may not.
        require(/^(?:amp|lt|gt|apos|quot|#(?:[0-9]+|x[0-9a-fA-F]+))$/.test(parser.entity), 'Invalid XML entity spelling.');
      }
      const endsTarget = match[0] === '?' && parser.state === sax.STATE.PROC_INST;
      writeSource(match[0]); if (endsTarget) piTargetEnd = true;
      start = at + match[0].length;
    }
    let end = input.length;
    // Retain at most two brackets to recognize a delimiter across chunk writes.
    if (!final) while (end > start && input[end - 1] === ']' && input.length - end < 2) end--;
    writeSource(input.slice(start, end)); bracketTail = input.slice(end);
  }
  let checkpoint = performance.now(), previousCR = false;
  for (let offset = 0; offset < file.size; offset += 32768) {
    signal?.throwIfAborted();
    const end = Math.min(offset + 32768, file.size);
    let chunk = decoder.decode(await file.slice(offset,end).arrayBuffer(), { stream:end !== file.size });
    signal?.throwIfAborted(); checkCharacters(chunk);
    // XML 1.0 literal line-ending normalization, including chunk crossings.
    if (previousCR && chunk.startsWith('\n')) chunk = chunk.slice(1);
    previousCR = chunk.endsWith('\r'); chunk = chunk.replace(/\r\n?/g, '\n');
    writeChunk(chunk);
    require(parser.attribList.length <= 128, 'XML element has too many attributes.');
    if (performance.now() - checkpoint >= 8) {
      onProgress?.({ phase:'xml', done:end, total:file.size }); await pause(); signal?.throwIfAborted(); checkpoint = performance.now();
    }
  }
  writeChunk('', true); parser.close(); signal?.throwIfAborted();
  require(rootSeen && groups.has('mandatory') && values.has('data'), 'XML needs mandatory save data.');
  const data = unbase64(values.get('data').text, MAX_SAVE_BYTES, 'Save data');
  if (values.has('size')) require(number(values.get('size').text, MAX_SAVE_BYTES, 'Size') === data.length, 'XML size does not match decoded data.');
  function field(binary, readable, width) {
    if (values.has(binary)) return fixedBytes(unbase64(values.get(binary).text, width, binary), width);
    const fallback = values.get(readable);
    require(!fallback?.overflow, `XML ${readable} exceeds its byte capacity.`);
    return fixedBytes(new TextEncoder().encode(fallback?.text ?? ''), width, readable);
  }
  const de = values.get('date'), te = values.get('time'); let date = 0;
  require(de || !te, 'XML time requires a date.');
  if (de) {
    if (te?.second !== undefined) require(number(te.second, 59, 'Second') === 0, 'Saturn save dates store whole minutes; seconds must be zero.');
    date = dateMinutes({year:number(de.year,10146,'Year'),month:number(de.month,12,'Month'),day:number(de.day,31,'Day'),
      hour:te ? number(te.hour,23,'Hour') : 0,minute:te ? number(te.minute,59,'Minute') : 0});
  }
  const metadata = saveMetadata({rawFilename:field('name_binary','name',11),rawComment:field('comment_binary','comment',10),
    language:values.has('language_code') ? number(values.get('language_code').text,255,'Language') : 0,date});
  const counter = values.get('counter')?.text.trim();
  onProgress?.({phase:'xml',done:file.size,total:file.size}); signal?.throwIfAborted();
  return { ...metadata, data, format:'XML', omittedExtraMetadata:hasNotes || !!counter && counter !== '0' };
}

export async function encodeXMLSave(save, {signal,onProgress} = {}) {
  const metadata = saveMetadata(save), incoming = save.data;
  require(incoming instanceof Uint8Array && incoming.length <= MAX_SAVE_BYTES, 'Invalid save payload.');
  signal?.throwIfAborted(); const data = incoming.slice(), date = dateFields(metadata.date);
  const parts = ['<?xml version="1.0" encoding="UTF-8"?>\n<Sega_Saturn_Save>\n  <mandatory>\n    <counter>0</counter>\n' +
    `    <name>${fieldText(metadata.rawFilename)}</name>\n    <name_binary>${base64(metadata.rawFilename)}</name_binary>\n` +
    `    <comment>${fieldText(metadata.rawComment)}</comment>\n    <comment_binary>${base64(metadata.rawComment)}</comment_binary>\n` +
    `    <language_code>${metadata.language}</language_code>\n    <size>${data.length}</size>\n` +
    `    <date year="${date.year}" month="${date.month}" day="${date.day}"/>\n` +
    `    <time hour="${date.hour}" minute="${date.minute}" second="0"/>\n    <data>`];
  let checkpoint=performance.now();
  for (let offset=0;offset<data.length;offset+=24576) {
    signal?.throwIfAborted();parts.push(base64(data.subarray(offset,Math.min(offset+24576,data.length))));
    if (performance.now()-checkpoint>=8) {onProgress?.({phase:'xml-export',done:Math.min(offset+24576,data.length),total:data.length});await pause();signal?.throwIfAborted();checkpoint=performance.now();}
  }
  parts.push('</data>\n  </mandatory>\n  <optional/>\n</Sega_Saturn_Save>\n');
  signal?.throwIfAborted();return new Blob(parts,{type:'application/xml'});
}
