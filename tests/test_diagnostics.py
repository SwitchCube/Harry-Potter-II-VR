import hashlib
import json
import os
from pathlib import Path
import stat
import struct
import subprocess
import sys
import unittest
from unittest.mock import patch
import uuid

sys.path.insert(0, str(Path(__file__).absolute().parent.parent / 'scripts'))
from project_paths import ROOT, inside, no_links, write_text
from inspect_pe import PE, inspect
from backup_original import digest, verify
from restore_original import plan_restore


def minimal_pe(bits):
    data = bytearray(1024)
    data[:2] = b'MZ'
    struct.pack_into('<I', data, 0x3c, 0x80)
    data[0x80:0x84] = b'PE\0\0'
    size = 224 if bits == 32 else 240
    struct.pack_into('<HHIIIHH', data, 0x84, 0x14c if bits == 32 else 0x8664, 1, 0, 0, 0, size, 0)
    opt = 0x98
    struct.pack_into('<H', data, opt, 0x10b if bits == 32 else 0x20b)
    struct.pack_into('<I', data, opt + 60, 0x200)
    struct.pack_into('<I', data, opt + (92 if bits == 32 else 108), 16)
    section = opt + size
    data[section:section + 8] = b'.text\0\0\0'
    struct.pack_into('<IIII', data, section + 8, 0x200, 0x1000, 0x200, 0x200)
    return data


class Boundaries(unittest.TestCase):
    def test_spaces_and_internal_paths(self):
        self.assertEqual(inside('docs/path with spaces.txt'), ROOT / 'docs/path with spaces.txt')

    def test_traversal_and_sibling_prefix_rejected(self):
        for path in ('../outside.txt', 'docs/one.txt:stream', ROOT.parent / (ROOT.name + '-outside') / 'x'):
            with self.assertRaises(ValueError):
                inside(path)

    def test_reparse_point_rejected(self):
        target = ROOT / 'docs'
        original = Path.lstat
        def fake(path, *args, **kwargs):
            result = original(path, *args, **kwargs)
            if path == target:
                class Link:
                    st_file_attributes = stat.FILE_ATTRIBUTE_REPARSE_POINT
                return Link()
            return result
        with patch.object(Path, 'lstat', fake), self.assertRaises(ValueError):
            inside(target / 'outside.txt')


class BinaryParsing(unittest.TestCase):
    def test_x86_and_x64(self):
        for bits in (32, 64):
            pe = PE(minimal_pe(bits))
            self.assertEqual(pe.bits, bits)
            self.assertEqual(pe.offset(0x1080), 0x280)
            self.assertEqual(pe.exports(), [])
            self.assertEqual(pe.imports(), [])

    def test_truncated_input_rejected(self):
        for size in (0, 1, 64, 132, 512, 900):
            with self.assertRaises(ValueError):
                PE(minimal_pe(32)[:size])

    def test_invalid_rva_rejected(self):
        with self.assertRaises(ValueError):
            PE(minimal_pe(32)).offset(0x800000)

    def test_original_reference_is_x86_and_exports_are_real(self):
        expected = json.loads(inside('config/target-fingerprint.json').read_text())
        for row in expected['files']:
            self.assertEqual(digest(inside(row['path'])).lower(), row['sha256'].lower())
        render = inspect(inside('system/Render.dll'))
        self.assertEqual(render['machine'], '0x014c')
        self.assertIn('?DrawWorld@URender@@UAEXPAUFSceneNode@@@Z', [e['name'] for e in render['exports']])


class RestoreSafety(unittest.TestCase):
    def setUp(self):
        self.base = inside('cache/tests/' + uuid.uuid4().hex)
        self.base.mkdir(parents=True)
        self.original = self.base / 'original.bin'
        self.current = self.base / 'current.bin'
        self.original.write_bytes(b'original fixture')
        self.current.write_bytes(b'modified fixture')
        self.manifest = self.base / 'manifest.json'
        self.manifest.write_text(json.dumps({'files':[{
            'origin':'project','relative_path':str(self.current.relative_to(ROOT)),
            'backup_path':'original.bin','sha256':digest(self.original)}]}))
        self.changes = {'files':[{'path':str(self.current.relative_to(ROOT)),
                                  'expected_current_sha256':digest(self.current)}]}

    def test_valid_restore_dry_run_preserves_file(self):
        before = self.current.read_bytes()
        plan = plan_restore(self.manifest, self.changes)
        self.assertEqual(plan[0][0], self.original)
        self.assertEqual(self.current.read_bytes(), before)

    def test_changed_current_file_rejected(self):
        self.current.write_bytes(b'new user progress')
        with self.assertRaises(ValueError):
            plan_restore(self.manifest, self.changes)
        self.assertEqual(self.current.read_bytes(), b'new user progress')

    def test_corrupt_backup_rejected(self):
        self.original.write_bytes(b'corrupt backup')
        with self.assertRaises(ValueError):
            plan_restore(self.manifest, self.changes)

    def test_external_destination_rejected(self):
        self.changes['files'][0]['path'] = str(ROOT.parent / 'outside.bin')
        with self.assertRaises(ValueError):
            plan_restore(self.manifest, self.changes)

    def test_apply_restores_fixture_and_preserves_previous_content(self):
        change_file = self.base / 'changes.json'
        change_file.write_text(json.dumps(self.changes))
        result = subprocess.run([sys.executable, '-B', str(ROOT / 'scripts/restore_original.py'),
                                 '--manifest', str(self.manifest), '--changes', str(change_file), '--apply'],
                                text=True, capture_output=True, check=True)
        report = json.loads(result.stdout.splitlines()[-1])
        self.assertEqual(self.current.read_bytes(), b'original fixture')
        preserved = inside(report['recovery']) / self.current.relative_to(ROOT)
        self.assertEqual(preserved.read_bytes(), b'modified fixture')


if __name__ == '__main__':
    unittest.main(verbosity=2)
