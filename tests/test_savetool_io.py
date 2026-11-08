"""Persistence failure and host-path checks using only generated disposable data."""
import hashlib
import json
import os
from pathlib import Path
import stat
import subprocess
import sys
import tempfile
import unittest

from save_fixtures import mems_fixture, raw_save, slot_fixture, saturn_fixture, disk_bytes
from savetool_support import NativeSaveTest, BUILD, ROOT
import shlex

NAME = b'SAVE_A00001'
OLD = b'Original payload' * 17
NEW = b'Replacement payload' * 29


def archive_bytes(kind, name=NAME):
    if kind == 'mems': return mems_fixture([(0, name, OLD)])[0]
    if kind == 'slot': return slot_fixture([(name, OLD)])[0]
    return disk_bytes(saturn_fixture([(name, OLD)])[0], kind == 'odd')


def encoded(name):
    allowed = b'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789 ._-'
    return ''.join(chr(c) if c in allowed else '%%%02X' % c for c in name.split(b'\0')[0])


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest() if path.exists() else None


class SaveIOTests(NativeSaveTest):
    @classmethod
    def setUpClass(cls):
        super().setUpClass()
        cls.fault_build = tempfile.TemporaryDirectory(prefix="io-faults-", dir=BUILD)
        cls.addClassCleanup(cls.fault_build.cleanup)
        cls.fault_binary = Path(cls.fault_build.name) / "sst"
        common = Path(cls.fault_build.name) / "main.o"
        flags = [*shlex.split(os.environ.get("CC", "cc")), "-std=gnu11", "-Wall", "-O1", "-g",
                 "-fno-strict-aliasing", "-fsanitize=address,undefined", "-fno-omit-frame-pointer"]
        commands = [[*flags, "-include", str(ROOT / "tests/savetool_io_faults.h"),
                     "-c", str(cls.source / "main.c"), "-o", str(common)],
                    [*flags, "-I", str(cls.source), str(common),
                     *[str(cls.source / name) for name in ("sr_bup.c", "ss_bup.c", "sr_mems.c")],
                     str(ROOT / "tests/savetool_io_faults.c"), "-o", str(cls.fault_binary)]]
        with (BUILD / "SaveIOTests.fault.compile.log").open("wb") as log:
            for command in commands:
                result = subprocess.run(command, capture_output=True, timeout=60)
                log.write(result.stdout + result.stderr)
                result.check_returncode()

    def invoke(self, directory, args, fault=None, archive='archive.bin'):
        env = {'PATH': os.defpath, 'UBSAN_OPTIONS': 'halt_on_error=1', 'ASAN_OPTIONS': 'detect_leaks=0'}
        if fault is not None: env['SAROO_TEST_IO_FAULT'] = fault
        result = subprocess.run([str(self.fault_binary if fault is not None else self.binary),
                                 archive, *map(str, args)], cwd=directory, capture_output=True,
                                timeout=3, env=env)
        self.assertNotIn(b'AddressSanitizer', result.stderr)
        self.assertNotIn(b'runtime error:', result.stderr)
        return result

    def setup_operation(self, directory, kind, operation, existing=True):
        archive = directory / 'archive.bin'
        archive.write_bytes(archive_bytes(kind))
        (directory / 'incoming.SRO').write_bytes(raw_save(NAME, NEW))
        args = ['-t', 1] if kind == 'slot' else []
        if operation == 'overwrite': args += ['-s', 0, '-i', 'incoming.SRO']
        elif operation == 'import': args += ['-i', 'incoming.SRO']
        elif operation == 'delete': args += ['-s', 0, '-d']
        elif operation == 'create': args = ['-c', 'NEWGAME']
        else:
            args += ['-s' if operation == 'SRO' else '-x', 0]
            target = directory / (NAME.decode() + '.' + operation)
            if existing: target.write_bytes(b'Existing export must remain intact')
            return args, target
        return args, archive

    def operations(self):
        for kind in ('mems', 'slot', 'plain', 'odd'):
            for op in ('SRO', 'BUP'): yield kind, op
            if kind in ('mems', 'slot'):
                for op in ('overwrite', 'import', 'delete'): yield kind, op
            if kind == 'slot': yield kind, 'create'

    def native_metadata(self, flags_to_check, protected):
        results = []
        try:
            cases = [('mems', 'delete'), ('slot', 'delete'),
                     ('mems', 'SRO'), ('slot', 'BUP'),
                     ('plain', 'SRO'), ('odd', 'BUP')]
            for kind, op in cases:
                for flags in flags_to_check:
                    with self.subTest(kind=kind, op=op, flags=flags), self.case() as temp:
                        directory = Path(temp)
                        args, target = self.setup_operation(directory, kind, op)
                        original = target.read_bytes()
                        archive = directory / 'archive.bin'
                        source = archive.read_bytes()
                        target.chmod(0o640)
                        subprocess.run(['/usr/bin/xattr', '-w', 'com.saroo.spec-test',
                                        'preserved metadata', str(target)], check=True,
                                       capture_output=True, timeout=5)
                        before = target.stat()
                        try:
                            os.chflags(target, flags)
                            result = self.invoke(directory, args)
                            after = target.stat()
                            temporary = [dict(name=p.name, bytes=p.stat().st_size, flags=p.stat().st_flags)
                                         for p in directory.glob('.saroo-save-*')]
                            results.append(dict(kind=kind, operation=op, flags=flags,
                                protected=protected, returncode=result.returncode,
                                stdout=result.stdout.decode(errors='replace'),
                                stderr=result.stderr.decode(errors='replace'), temporary=temporary,
                                original_preserved=target.read_bytes()==original))
                            self.assertEqual(result.returncode==0, not protected, result.stdout + result.stderr)
                            self.assertEqual(after.st_flags, flags)
                            self.assertEqual((after.st_uid, after.st_gid, stat.S_IMODE(after.st_mode)),
                                             (before.st_uid, before.st_gid, stat.S_IMODE(before.st_mode)))
                            attribute = subprocess.check_output(['/usr/bin/xattr', '-p', '-x',
                                'com.saroo.spec-test', str(target)], timeout=5)
                            self.assertEqual(bytes.fromhex(attribute.decode()), b'preserved metadata')
                            if protected:
                                self.assertEqual(target.read_bytes(), original)
                                self.assertNotIn(b'Updated archive:', result.stdout)
                                self.assertNotIn(b'Export Save_', result.stdout)
                            elif op == 'delete':
                                self.assertNotEqual(target.read_bytes(), original)
                                self.assertEqual(target.stat().st_size, len(original))
                                listing = self.invoke(directory, ['-t', 1] if kind == 'slot' else [])
                                self.assertEqual(listing.returncode, 0)
                                self.assertNotIn(NAME, listing.stdout)
                            else:
                                self.assertEqual(target.read_bytes()[64:], OLD)
                            if op != 'delete': self.assertEqual(archive.read_bytes(), source)
                            self.assertEqual(temporary, [])
                        finally:
                            # Clear flags only on fixture-owned disposable files after recording.
                            for path in directory.iterdir():
                                if path.stat().st_flags: os.chflags(path, 0)
        finally:
            name = 'protected' if protected else 'ordinary'
            (BUILD / ('metadata-' + name + '-results.json')).write_text(
                json.dumps(results, indent=2) + '\n')

    @unittest.skipUnless(sys.platform == 'darwin', 'Native macOS file flags required')
    def test_protected_native_metadata_refuses_without_abandoned_staging(self):
        self.native_metadata((stat.UF_IMMUTABLE, stat.UF_APPEND,
                              stat.UF_IMMUTABLE | stat.UF_APPEND), True)

    @unittest.skipUnless(sys.platform == 'darwin', 'Native macOS metadata required')
    def test_ordinary_native_metadata_survives_complete_publication(self):
        self.native_metadata((0, stat.UF_NODUMP), False)

    def test_precommit_failures_preserve_complete_destination(self):
        faults = ['create', 'partial', 'zero', 'sync', 'close', 'rename']
        if sys.platform == 'darwin': faults.append('metadata')
        for kind, op in self.operations():
            for fault in faults:
                with self.subTest(kind=kind, op=op, fault=fault), self.case() as temp:
                    directory = Path(temp)
                    args, target = self.setup_operation(directory, kind, op)
                    before = digest(target)
                    result = self.invoke(directory, args, fault)
                    self.assertEqual(digest(target), before, 'Failed write changed existing data')
                    self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
                    self.assertNotIn(b'Updated archive:', result.stdout)
                    self.assertNotIn(b'Export Save_', result.stdout)
                    self.assertNotIn(b'Import ', result.stdout)
                    self.assertNotIn(b'Delete Save_', result.stdout)
                    self.assertEqual(list(directory.glob('.saroo-save-*')), [])

    def test_failed_new_exports_do_not_publish_partial_files(self):
        for kind in ('mems', 'slot', 'plain', 'odd'):
            for op in ('SRO', 'BUP'):
                for fault in ('partial', 'sync', 'close', 'rename'):
                    with self.subTest(kind=kind, op=op, fault=fault), self.case() as temp:
                        directory = Path(temp)
                        args, target = self.setup_operation(directory, kind, op, existing=False)
                        before = digest(directory / 'archive.bin')
                        result = self.invoke(directory, args, fault)
                        self.assertFalse(target.exists())
                        self.assertNotEqual(result.returncode, 0)
                        self.assertEqual(digest(directory / 'archive.bin'), before)
                        self.assertEqual(list(directory.glob('.saroo-save-*')), [])

    def test_normal_short_and_interrupted_writes_complete(self):
        for kind, op in self.operations():
            with self.case() as temp:
                directory = Path(temp)
                args, target = self.setup_operation(directory, kind, op)
                result = self.invoke(directory, args)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                expected = digest(target)
                if op in ('SRO', 'BUP'): self.assertEqual(target.read_bytes()[64:], OLD)
                for fault in ('short', 'interrupt'):
                    with self.subTest(kind=kind, op=op, fault=fault):
                        args, target = self.setup_operation(directory, kind, op)
                        result = self.invoke(directory, args, fault)
                        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                        self.assertEqual(digest(target), expected)
                        self.assertEqual(list(directory.glob('.saroo-save-*')), [])

    def test_directory_sync_failure_reports_completed_replacement(self):
        for kind, op in self.operations():
            with self.subTest(kind=kind, op=op), self.case() as temp:
                directory = Path(temp)
                args, target = self.setup_operation(directory, kind, op)
                self.assertEqual(self.invoke(directory, args).returncode, 0)
                expected = digest(target)
                args, target = self.setup_operation(directory, kind, op)
                result = self.invoke(directory, args, 'directory')
                self.assertNotEqual(result.returncode, 0)
                self.assertIn(b'Replacement completed; directory synchronization failed', result.stderr)
                self.assertEqual(digest(target), expected)
                self.assertNotIn(b'Updated archive:', result.stdout)
                self.assertNotIn(b'Export Save_', result.stdout)
                self.assertEqual(list(directory.glob('.saroo-save-*')), [])

    def test_derived_names_are_local_and_byte_preserving(self):
        names = [b'../ESCAPE', b'A//B', b'A/B', b'A%2FB', b'A\\B',
                 b'\x01\x1b\xff\x80:*?<>|%', b'Normal Name', b'..']
        for kind in ('mems', 'slot', 'plain', 'odd'):
            for name in names:
                with self.subTest(kind=kind, name=name), self.case() as temp:
                    directory = Path(temp) / 'work'
                    directory.mkdir()
                    archive = directory / 'archive.bin'
                    archive.write_bytes(archive_bytes(kind, name))
                    before = digest(archive)
                    args = ['-t', 1] if kind == 'slot' else []
                    for suffix, flag in (('SRO', '-s'), ('BUP', '-x')):
                        result = self.invoke(directory, args + [flag, 0])
                        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                        export = directory / (encoded(name) + '.' + suffix)
                        self.assertEqual(export.read_bytes()[64:], OLD)
                        self.assertEqual(export.read_bytes()[16:27], name.ljust(11, b'\0'))
                    self.assertEqual(digest(archive), before)
                    if kind in ('mems', 'slot'):
                        (directory / (encoded(name) + '.bin')).write_bytes(raw_save(name, NEW))
                        result = self.invoke(directory, args + ['-s', 0, '-i'])
                        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                        self.assertEqual(self.invoke(directory, args + ['-s', 0]).returncode, 0)
                        self.assertEqual((directory / (encoded(name) + '.SRO')).read_bytes()[64:], NEW)
                    self.assertEqual(list(Path(temp).iterdir()), [directory])

    def test_derived_output_aliases_and_special_files_are_rejected(self):
        for kind in ('mems', 'slot', 'plain', 'odd'):
            for alias in ('symlink', 'hardlink', 'directory', 'fifo', 'same-name'):
                with self.subTest(kind=kind, alias=alias), self.case() as temp:
                    directory = Path(temp)
                    archive = directory / ('SAVE_A00001.SRO' if alias == 'same-name' else 'archive.bin')
                    archive.write_bytes(archive_bytes(kind))
                    before = digest(archive)
                    target = directory / 'SAVE_A00001.SRO'
                    other = directory / 'other.bin'
                    other.write_bytes(b'Unrelated data')
                    if alias == 'symlink': target.symlink_to(other)
                    elif alias == 'hardlink': os.link(archive, target)
                    elif alias == 'directory': target.mkdir()
                    elif alias == 'fifo': os.mkfifo(target)
                    args = ['-t', 1] if kind == 'slot' else []
                    result = self.invoke(directory, args + ['-s', 0], archive=archive.name)
                    self.assertNotEqual(result.returncode, 0)
                    self.assertEqual(digest(archive), before)
                    self.assertEqual(other.read_bytes(), b'Unrelated data')
                    if alias == 'symlink': self.assertTrue(target.is_symlink())
                    if alias == 'fifo': self.assertTrue(stat.S_ISFIFO(target.lstat().st_mode))

    def test_inferred_inputs_reject_links_and_fifos_but_explicit_paths_work(self):
        for kind in ('mems', 'slot'):
            for form in ('symlink', 'fifo', 'explicit'):
                with self.subTest(kind=kind, form=form), self.case() as temp:
                    directory = Path(temp)
                    archive = directory / 'archive.bin'
                    archive.write_bytes(archive_bytes(kind))
                    before = digest(archive)
                    sub = directory / 'sub'
                    sub.mkdir()
                    incoming = sub / 'incoming.SRO'
                    incoming.write_bytes(raw_save(NAME, NEW))
                    target = directory / 'SAVE_A00001.bin'
                    if form == 'fifo': os.mkfifo(target)
                    else: target.symlink_to(incoming)
                    args = (['-t', 1] if kind == 'slot' else []) + ['-s', 0, '-i']
                    if form == 'explicit': args += ['SAVE_A00001.bin']
                    result = self.invoke(directory, args)
                    if form == 'explicit':
                        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                        self.assertNotEqual(digest(archive), before)
                    else:
                        self.assertNotEqual(result.returncode, 0)
                        self.assertEqual(digest(archive), before)
                    self.assertEqual(incoming.read_bytes(), raw_save(NAME, NEW))

    def test_archive_symlink_and_destination_metadata_survive(self):
        for kind in ('mems', 'slot'):
            for operation in ('overwrite', 'SRO'):
                with self.subTest(kind=kind, op=operation), self.case() as temp:
                    directory = Path(temp)
                    args, target = self.setup_operation(directory, kind, operation)
                    archive = directory / 'archive.bin'
                    actual = directory / ('a' * 240)
                    archive.rename(actual)
                    archive.symlink_to(actual)
                    if operation == 'overwrite': target = actual
                    target.chmod(0o640)
                    if sys.platform == 'darwin':
                        subprocess.run(['/usr/bin/xattr', '-w', 'com.saroo.test', 'Preserve me', str(target)], check=True)
                        subprocess.run(['/bin/chmod', '+a', 'everyone allow read', str(target)], check=True)
                        acl_before = subprocess.check_output(['/bin/ls', '-lde', str(target)]).splitlines()[1:]
                    before = digest(target)
                    owner = (target.stat().st_uid, target.stat().st_gid)
                    result = self.invoke(directory, args)
                    self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                    self.assertTrue(archive.is_symlink())
                    self.assertNotEqual(digest(target), before)
                    self.assertEqual(stat.S_IMODE(target.stat().st_mode), 0o640)
                    self.assertEqual((target.stat().st_uid, target.stat().st_gid), owner)
                    if sys.platform == 'darwin':
                        self.assertEqual(subprocess.check_output(['/usr/bin/xattr', '-p', 'com.saroo.test', str(target)]).strip(), b'Preserve me')
                        self.assertEqual(subprocess.check_output(['/bin/ls', '-lde', str(target)]).splitlines()[1:], acl_before)
