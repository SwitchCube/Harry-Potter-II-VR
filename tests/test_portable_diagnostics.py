import json
import os
from pathlib import Path
import sys
import unittest
import uuid
from unittest.mock import patch
import zipfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'scripts'))
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'release'))
from project_paths import inside
import collect_diagnostics as diagnostic
import portable_launcher as launcher


class Diagnostics(unittest.TestCase):
    def setUp(self):
        self.root = inside('cache/tests/diagnostic-' + uuid.uuid4().hex[:10])
        self.game = self.root / 'Game Install'
        self.data = self.root / 'Data'
        (self.game / 'HP2VR').mkdir(parents=True)
        (self.data / 'logs').mkdir(parents=True)
        (self.game / 'HP2VR/manifest.json').write_text('{"version":"1.0"}')

    def read_report(self):
        path, summary = diagnostic.collect(self.game, self.data)
        with zipfile.ZipFile(path) as archive:
            files = {name: archive.read(name).decode('utf-8') for name in archive.namelist()}
        return files, summary

    def test_runtime_reason_and_hresult_included_without_saves_or_settings(self):
        profile = self.data / 'cache/p-fixture'
        (profile / 'Save/Slot1').mkdir(parents=True)
        (profile / 'Save/Slot1/Save0.usa').write_bytes(b'save-private-sentinel')
        (profile / 'Game.ini').write_text('settings-private-sentinel')
        (profile / 'hp2vr-native.jsonl').write_text(
            '{"event":"graphics_error","hr":-123}\n'
            '{"event":"fatal","stage":"hud_begin","reason":"fixture target failed"}\n')
        (self.data / 'logs/session.jsonl').write_text(
            '{"event":"summary","game_exit_code":137}\n')
        files, summary = self.read_report()
        all_text = '\n'.join(files.values())
        self.assertEqual(summary['native_logs'], 1)
        self.assertIn('fixture target failed', all_text)
        self.assertIn('"hr": -123', all_text)
        self.assertIn('"game_exit_code": 137', all_text)
        self.assertNotIn('private-sentinel', all_text)
        self.assertEqual((profile / 'Save/Slot1/Save0.usa').read_bytes(), b'save-private-sentinel')
        self.assertFalse(summary['game_started'])

    def test_paths_names_and_escaped_json_are_redacted(self):
        rows = [dict(event='fixture', path=str(self.game / 'system/Game.exe'),
                     profile=str(self.data / 'cache/p-fixture'),
                     other=r'C:\Users\Different Person\saved.log',
                     account='Fixture Person', machine='Fixture-PC')]
        (self.data / 'logs/session.jsonl').write_text('\n'.join(json.dumps(row) for row in rows))
        with patch.dict(os.environ, {'USERNAME': 'Fixture Person', 'COMPUTERNAME': 'Fixture-PC'}):
            files, _ = self.read_report()
        text = '\n'.join(files.values())
        for value in ('Fixture Person', 'Different Person', 'Fixture-PC', str(self.game), str(self.data)):
            self.assertNotIn(value, text)
        row = json.loads(files['01-launcher.jsonl'])
        self.assertEqual(row['account'], '<USER>')
        self.assertTrue(row['path'].startswith('<GAME>'))
        self.assertEqual(row['other'], '<PATH>')

    def test_missing_native_logs_are_explicit_and_source_unchanged(self):
        path = self.data / 'logs/session.jsonl'
        original = b'{"event":"summary","game_exit_code":137}\n'
        path.write_bytes(original)
        files, summary = self.read_report()
        self.assertEqual(summary['native_logs'], 0)
        self.assertEqual(path.read_bytes(), original)
        self.assertEqual(len(files), 2)

    def test_tail_and_partial_json_keep_the_last_failure(self):
        path = self.data / 'logs/session.jsonl'
        path.write_text('discard this long prefix\n' * 100 + '{"event":"fatal","reason":"last failure"}\npartial')
        with patch.object(diagnostic, 'MAX_BYTES', 100):
            files, summary = self.read_report()
        self.assertTrue(summary['files'][0]['tail_only'])
        self.assertIn('last failure', files['01-launcher.jsonl'])
        self.assertIn('partial', files['01-launcher.jsonl'])

    def test_hardlinked_diagnostic_is_rejected(self):
        source = self.root / 'private.txt'
        source.write_text('private data')
        os.link(source, self.data / 'logs/session.jsonl')
        with self.assertRaisesRegex(ValueError, 'Linked diagnostic'):
            self.read_report()

    def test_preflight_errors_are_available_without_any_game_run(self):
        launcher.record_failure(self.data, RuntimeError('SteamVR fixture missing'), 'vr_preflight')
        files, summary = self.read_report()
        self.assertIn('SteamVR fixture missing', '\n'.join(files.values()))
        self.assertEqual(summary['files'][0]['findings'][0]['stage'], 'vr_preflight')

    def test_logging_failure_never_replaces_original_and_check_stays_read_only(self):
        with patch.object(Path, 'mkdir', side_effect=PermissionError('fixture')):
            launcher.record_failure(self.data, RuntimeError('original'), 'launcher')
        with patch.object(launcher, 'REPORT_FAILURES', False):
            launcher.record_failure(self.data, RuntimeError('check failure'), 'launcher')
        self.assertEqual(list((self.data / 'logs').iterdir()), [])

    def test_native_failure_is_shown_with_graphics_code(self):
        profile = self.data / 'cache/p-fixture'; profile.mkdir(parents=True)
        (profile / 'hp2vr-native.jsonl').write_text(
            '{"event":"graphics_error","hr":-123}\n'
            '{"event":"fatal","stage":"hud_begin","reason":"fixture target failed"}\npartial')
        self.assertEqual(launcher.native_failure(profile),
                         'hud_begin: fixture target failed (HRESULT 0xFFFFFF85)')
        self.assertEqual(launcher.native_failure(None), '')

    def test_missing_and_different_components_are_identified(self):
        package = self.game / 'HP2VR'
        (package / 'changed.dll').write_bytes(b'changed')
        (package / 'manifest.json').write_text(json.dumps(dict(version='1.0.1', files={
            'changed.dll': '0' * 64, 'missing.dll': '1' * 64}, game_files={'Game.exe': '2' * 64})))
        _, summary = self.read_report()
        installation = summary['installation']
        self.assertFalse(installation['package']['passed'])
        self.assertEqual([r['status'] for r in installation['package']['files']], ['different', 'missing'])
        self.assertFalse(installation['original_game']['passed'])

    def test_diagnostics_work_before_first_start_with_broken_manifest(self):
        fresh = self.root / 'Fresh Data'
        (self.game / 'HP2VR/manifest.json').write_text('{broken')
        report, summary = diagnostic.collect(self.game, fresh)
        self.assertTrue(report.is_file())
        self.assertEqual(summary['native_logs'], 0)
        self.assertIn('manifest_error', summary['installation'])
        self.assertFalse((fresh / 'config').exists())
        self.assertFalse((fresh / 'cache').exists())

    def test_unc_paths_and_failure_dialog_hint(self):
        clean = diagnostic.scrubber(self.game, self.data)
        self.assertEqual(clean(r'File: \\PrivateServer\PersonalShare\user.log'), 'File: <PATH>')
        for language in ('de', 'en'):
            with patch.object(launcher, 'LANGUAGE', language), patch.object(launcher.ctypes.windll.user32, 'MessageBoxW') as box:
                launcher.message('original failure')
                self.assertIn('original failure', box.call_args.args[1])
                self.assertIn('Diagnose-HP2VR.cmd', box.call_args.args[1])


if __name__ == '__main__':
    unittest.main()
