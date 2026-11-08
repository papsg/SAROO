"""Independent XML transport corpus, decoded using Python's standard XML parser."""
import base64
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
import sys
import xml.etree.ElementTree as ET
import xml.parsers.expat as expat

from test_web_save_text import calendar
ROOT=Path(__file__).resolve().parents[1]
def prepare_sax():
    return ROOT / 'tools/savetool/web/vendor/sax/lib/sax.js'


def fixtures(directory):
    manifest=[]
    cases=[(n,'utf-8') for n in [0,1,2,3,24575,24576,24577,490496,1048576]]
    cases += [(513,'utf-16le'),(514,'utf-16be')]
    for index,(size,encoding) in enumerate(cases):
        name=b'\x82\xa0\0PAD12345';comment=b'\x01CTRL\0pad!';date=0xffffffff if index&1 else 0
        data=bytes((i*19+7)%251 for i in range(size));language=255 if index&1 else 0
        root=ET.Element('Sega_Saturn_Save');mandatory=ET.SubElement(root,'mandatory');ET.SubElement(root,'optional')
        values={'counter':'0','name':'human description '*10,'name_binary':base64.b64encode(name).decode(),
                'comment':'alternate displayed label','comment_binary':base64.b64encode(comment).decode(),
                'language_code':str(language),'size':str(size)}
        for key,value in values.items():ET.SubElement(mandatory,key).text=value
        fields=calendar(date)
        ET.SubElement(mandatory,'date',{key:str(fields[key]) for key in ['year','month','day']})
        ET.SubElement(mandatory,'time',{'hour':str(fields['hour']),'minute':str(fields['minute']),'second':'0'})
        ET.SubElement(mandatory,'data').text=base64.b64encode(data).decode()
        text=ET.tostring(root,encoding='unicode')
        text=f'<?xml version="1.0" encoding="{encoding}"?>\n'+text
        encoded=text.encode(encoding)
        if encoding=='utf-16le':encoded=b'\xff\xfe'+encoded
        elif encoding=='utf-16be':encoded=b'\xfe\xff'+encoded
        filename=f'case-{index}.xml';(directory/filename).write_bytes(encoded)
        manifest.append(dict(file=filename,size=size,name=list(name),comment=list(comment),date=date,language=language,payload_sha256=hashlib.sha256(data).hexdigest()))
    (directory/'fixtures.json').write_text(json.dumps(manifest,indent=2)+'\n')
    return manifest


class BrowserSaveXMLTests(unittest.TestCase):
    def test_xml_transport(self):
        node=shutil.which('node')
        if not node:self.skipTest('Node is required for XML checks')
        dependency=os.environ.get('SAROO_SAVE_SAX') or str(prepare_sax())
        with tempfile.TemporaryDirectory(prefix='saroo-save-xml-') as temporary:
            directory=Path(temporary);manifest=fixtures(directory)
            result=subprocess.run([node,'--test','--test-reporter=tap',str(ROOT/'tests/web_save_xml.test.mjs')],cwd=ROOT,
                env={**os.environ,'SAROO_SAVE_XML_FIXTURES':str(directory),'SAROO_SAVE_SAX':dependency},capture_output=True,text=True,timeout=120)
            evidence=ROOT/'tests/build/web-save-xml-tests';evidence.mkdir(parents=True,exist_ok=True)
            (evidence/'node.log').write_text(result.stdout+result.stderr)
            self.assertEqual(result.returncode,0,result.stdout+result.stderr)
            for corpus in ['xml-case-syntax.json','xml-tag-syntax.json','xml-attribute-syntax.json']:
                for item in json.loads((directory/corpus).read_text()):
                    try:ET.fromstring(item['text']);valid=True
                    except ET.ParseError:valid=False
                    self.assertEqual(valid,item['valid'],item['name'])
            for item in json.loads((directory/'xml-pi-syntax.json').read_text()):
                # XML Name permits colons in PI targets. Disable namespace
                # processing for this independent XML (not NCName) check.
                try:expat.ParserCreate().Parse(item['text'],True);valid=True
                except expat.ExpatError:valid=False
                self.assertEqual(valid,item['valid'],item['name'])
            for item in manifest:
                output=directory/(item['file']+'.out.xml');doc=ET.parse(output).getroot();self.assertEqual(doc.tag,'Sega_Saturn_Save')
                mandatory=doc.find('mandatory')
                self.assertEqual(base64.b64decode(mandatory.findtext('name_binary')),bytes(item['name']))
                self.assertEqual(base64.b64decode(mandatory.findtext('comment_binary')),bytes(item['comment']))
                self.assertEqual(hashlib.sha256(base64.b64decode(mandatory.findtext('data'))).hexdigest(),item['payload_sha256'])
                self.assertEqual(int(mandatory.findtext('language_code')),item['language'])
                self.assertEqual(int(mandatory.findtext('size')),item['size'])
                fields=calendar(item['date'])
                for name in ['year','month','day']:self.assertEqual(int(mandatory.find('date').get(name)),fields[name])
                for name in ['hour','minute']:self.assertEqual(int(mandatory.find('time').get(name)),fields[name])
            (evidence/'fixtures.json').write_text(json.dumps(manifest,indent=2)+'\n')


if __name__=='__main__':unittest.main()
