"""Extracted full/update packages, real embedded x86 Python, no game launch."""
import hashlib
import json
import shutil
import subprocess
import sys
import uuid
import zipfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'scripts'))
from project_paths import inside, write_text
from package_release import private_scan


def smoke(full, update):
    root = inside('cache/tests/integrated-diagnostics-' + uuid.uuid4().hex[:8])
    game = root / 'Spiel Ã„'; game.mkdir(parents=True)
    with zipfile.ZipFile(inside('dist/HP2VR-1.0.zip')) as archive:
        archive.extractall(game)
        original_bins = {n: archive.read(n) for n in archive.namelist() if n.startswith('HP2VR/bin/')}
    package = game / 'HP2VR'; data = root / 'Eigene Daten'
    settings = b'[HP2VR]\nLanguage=en\nQuality=sharp\n'
    (package / 'Options.ini').write_bytes(settings)
    (data / 'cache/p-fixture/Save/Slot1').mkdir(parents=True)
    save = data / 'cache/p-fixture/Save/Slot1/Save0.usa'; save.write_bytes(b'private-save-sentinel')
    with zipfile.ZipFile(inside(update)) as archive:
        assert 'HP2VR/Options.ini' not in archive.namelist()
        archive.extractall(game)
    assert (package / 'Options.ini').read_bytes() == settings
    with zipfile.ZipFile(inside(full)) as archive:
        for name in archive.namelist():
            if name != 'HP2VR/Options.ini': assert (game / name).read_bytes() == archive.read(name), name
    native_unchanged = all((game / name).read_bytes() == original for name, original in original_bins.items())
    manifest = json.loads((package / 'manifest.json').read_text())
    (game / 'system').mkdir()
    for name in manifest['game_files']:
        shutil.copyfile(inside('system') / name, game / 'system' / name)
    (game / 'system/Default.ini').write_text('[Engine.Engine]\nLanguage=int\n')
    runtime = package / 'python/python.exe'
    def run(script, *args):
        result = subprocess.run([str(runtime), '-B', str(package / 'scripts' / script), *map(str, args)],
                                capture_output=True, text=True, creationflags=subprocess.CREATE_NO_WINDOW)
        assert result.returncode == 0, result.stderr
        return result
    checked = json.loads(run('portable_launcher.py', '--check', '--data-root', data).stdout)
    assert checked['version'] == manifest['version'] and checked['game_supported'] and checked['quality'] == 'sharp'
    assert not (data / 'config').exists(), '--check wrote user state'
    (data / 'logs').mkdir()
    (data / 'logs/session.jsonl').write_text('{"event":"summary","game_exit_code":137}\n')
    (data / 'cache/p-fixture/hp2vr-native.jsonl').write_text(
        '{"event":"graphics_error","hr":-123}\n'
        '{"event":"fatal","stage":"hud_begin","reason":"synthetic fixture only"}\n')
    run('collect_diagnostics.py', '--data-root', data, '--no-open')
    reports = list((data / 'diagnostics').glob('*.zip')); assert len(reports) == 1
    with zipfile.ZipFile(reports[0]) as archive:
        summary = json.loads(archive.read('summary.json'))
        assert summary['native_logs'] == 1 and summary['installation']['package']['passed']
        assert summary['installation']['original_game']['passed']
        assert summary['machine']['process_bits'] == 32 and 'windows_version' in summary['machine']
        for name in archive.namelist():
            content = archive.read(name); private_scan(name, content)
            assert b'private-save-sentinel' not in content
    # A broken payload must still be diagnosable before any successful launch.
    (package / 'bin/hp2vr-native.dll').write_bytes(b'deliberately damaged test copy')
    fresh = root / 'Fresh Data'
    run('portable_launcher.py', '--diagnostics', '--data-root', fresh, '--no-open')
    with zipfile.ZipFile(next((fresh / 'diagnostics').glob('*.zip'))) as archive:
        summary = json.loads(archive.read('summary.json'))
        checks = summary['installation']['package']['files']
        assert any(r['file'] == 'bin/hp2vr-native.dll' and r['status'] == 'different' for r in checks)
        assert summary['native_logs'] == 0
    assert not (fresh / 'config').exists() and not (fresh / 'cache').exists()
    assert save.read_bytes() == b'private-save-sentinel'
    evidence = dict(passed=True, full=full, update=update, embedded_python_bits=32,
                    update_preserved_options=True, original_supported=True,
                    native_binaries_unchanged=native_unchanged, both_diagnostic_entries=True,
                    damaged_package_reported=True, missing_native_explicit=True,
                    saved_games_unchanged=True, privacy_scan='passed', game_started=False)
    write_text('logs/portable-integrated-diagnostics-smoke.json', json.dumps(evidence, indent=2) + '\n', exclusive=False)
    print(json.dumps(evidence))


if __name__ == '__main__':
    smoke(sys.argv[1], sys.argv[2])
