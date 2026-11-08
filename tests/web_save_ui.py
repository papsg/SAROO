#!/usr/bin/env python3
"""Generate local browser-workflow inputs; all archives are synthetic, never SD copies."""
import argparse
import hashlib
import json
from pathlib import Path
import sys
from save_fixtures import fixtures
from test_web_save_xml import fixtures as xml_fixtures
from save_fixtures import raw_save, mems_fixture
from save_fixtures import slot_fixture, UNIT


def prepare(root):
    root.mkdir(parents=True,exist_ok=True);fixtures(root)
    xml=root/'xml';xml.mkdir(exist_ok=True);xml_fixtures(xml)
    text=(xml/'case-7.xml').read_text()
    (xml/'case-7-notes.xml').write_text(text.replace('<optional />','<optional><author>Fixture author</author></optional>'))
    (root/'incoming.SRO').write_bytes(raw_save(b'IMPORTED',bytes(range(251))*7))
    (root/'saroo-maximum.SRO').write_bytes(raw_save(b'MAXIMUM0001',bytes((i*7+27)%251 for i in range(65280))))
    (root/'bad.xml').write_text('<Sega_Saturn_Save><mandatory><data>Incomplete')
    image,_=mems_fixture([(0,b'<save>&',b'original')]);(root/'literal.bin').write_bytes(image)
    original=(root/'saroo-preserve.bin').read_bytes();damaged=bytearray(original);damaged[65536]=0
    (root/'damaged-slot.bin').write_bytes(damaged)
    (root/'slow.xml').write_text('<Sega_Saturn_Save><mandatory><name>IMPORTED</name><data>'+' '*16777216+'AQID</data></mandatory></Sega_Saturn_Save>')
    first,_=slot_fixture([(b'FIRST',b'first')],gameids=(b'GAME_A',b'GAME_B'))
    second,_=slot_fixture([(b'SECOND',b'second')],gameids=(b'GAME_B',))
    first[2*UNIT:3*UNIT]=second[UNIT:2*UNIT]
    (root/'two-games.bin').write_bytes(first)
    return root


def prepare_capacity(root):
    """Complete v0.8 archive; bounded independent fixture-encoder workspace."""
    root.mkdir(parents=True,exist_ok=True)
    ids=[f'GAME-{i:04}'.encode().ljust(16,b'\0') for i in range(1,4096)]
    directory=bytearray(65536);directory[:16]=b'Saroo Save File\0'
    for index,gameid in enumerate(ids,1):directory[index*16:index*16+16]=gameid
    digest=hashlib.sha256()
    with (root/'full-saroo.bin').open('wb') as output:
        output.write(directory);digest.update(directory)
        for index,gameid in enumerate(ids,1):
            records=[(b'LASTSAVE001',b'Final slot payload.')] if index==4095 else []
            encoded=slot_fixture(records,[gameid])[0][65536:]
            output.write(encoded);digest.update(encoded)
    (root/'fixture.json').write_text(json.dumps({'bytes':268435456,'sha256':digest.hexdigest(),
                                                'last_slot':4095,'hardware':False},indent=2)+'\n')
    return root


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('directory',type=Path)
    parser.add_argument('--capacity',action='store_true',help='Generate the complete 256 MiB archive instead of ordinary UI fixtures')
    args=parser.parse_args();print(prepare_capacity(args.directory) if args.capacity else prepare(args.directory))
