"""Read existing HP2VR diagnostics; never start the game or touch saved games."""
import argparse
import ctypes
import hashlib
import json
import os
from pathlib import Path
import re
import sys
import time
import uuid
import zipfile

MAX_BYTES = 2 * 1024 * 1024


def scrubber(game, data):
    replacements = [(str(data), '<DATA>'), (str(game), '<GAME>')]
    for key, label in [('USERPROFILE', '<USER>'), ('LOCALAPPDATA', '<LOCALAPPDATA>'),
                       ('APPDATA', '<APPDATA>'), ('USERNAME', '<USER>'),
                       ('COMPUTERNAME', '<PC>')]:
        value = os.environ.get(key)
        if value:
            replacements.append((value, label))
    variants = {}
    for value, label in replacements:
        for spelling in (value, value.replace('\\', '/'), value.replace('\\', '\\\\')):
            variants[spelling] = label
    ordered = sorted(variants, key=len, reverse=True)

    def clean(text):
        for value in ordered:
            text = re.sub(re.escape(value), lambda _: variants[value], text, flags=re.I)
        # Also cover paths copied from another Windows account or installation.
        text = re.sub(r'(?i)\b[A-Z]:(?:[\\/]+)[^\r\n"<>|]*', '<PATH>', text)
        text = re.sub(r'\\{2,}[^\s\\/"<>|]+[\\/][^\r\n"<>|]*', '<PATH>', text)
        return text
    return clean


def read_tail(path):
    with path.open('rb') as handle:
        handle.seek(0, 2)
        length = handle.tell()
        handle.seek(max(0, length - MAX_BYTES))
        raw = handle.read(MAX_BYTES)
    text = raw.decode('utf-8-sig', errors='replace')
    if length > MAX_BYTES:
        text = text.partition('\n')[2]  # Omit the incomplete first record.
    return text, length > MAX_BYTES


def safe_file(path, root):
    from project_paths import inside
    path = inside(path, root=root)
    if path.is_file() and path.stat().st_nlink != 1:
        raise ValueError('Linked diagnostic file rejected')
    return path


def machine_info():
    """No host name, serial numbers, network details, registry edits or VR init."""
    result = dict(os='Windows', process_bits=ctypes.sizeof(ctypes.c_void_p) * 8)
    version = sys.getwindowsversion()
    result['windows_version'] = '{}.{}.{}'.format(version.major, version.minor, version.build)
    result['architecture'] = os.environ.get('PROCESSOR_ARCHITEW6432',
                                           os.environ.get('PROCESSOR_ARCHITECTURE', 'unknown'))
    from ctypes import wintypes
    class DisplayDevice(ctypes.Structure):
        _fields_ = [('cb', wintypes.DWORD), ('name', wintypes.WCHAR * 32),
                    ('description', wintypes.WCHAR * 128), ('flags', wintypes.DWORD),
                    ('identifier', wintypes.WCHAR * 128), ('key', wintypes.WCHAR * 128)]
    api = ctypes.windll.user32.EnumDisplayDevicesW
    api.argtypes = [wintypes.LPCWSTR, wintypes.DWORD, ctypes.POINTER(DisplayDevice), wintypes.DWORD]
    api.restype = wintypes.BOOL
    result['graphics_adapters'] = []
    for index in range(16):
        device = DisplayDevice(); device.cb = ctypes.sizeof(device)
        if not api(None, index, ctypes.byref(device), 0):
            break
        result['graphics_adapters'].append(dict(name=device.description, active=bool(device.flags & 1)))
    return result


def installation_info(game, clean):
    """Identify mismatched binaries without bundling any original game content."""
    result = dict(mod_version='unknown', package=None, original_game=None)
    try:
        package = game / 'HP2VR'
        manifest = json.loads(safe_file(package / 'manifest.json', game).read_text(encoding='utf-8'))
        result['mod_version'] = clean(str(manifest.get('version', 'unknown')))
        for key, base, expected in [('package', package, manifest.get('files')),
                                    ('original_game', game / 'system', manifest.get('game_files'))]:
            if not expected:
                continue
            if not isinstance(expected, dict) or len(expected) > 512:
                raise ValueError('Invalid manifest file list')
            checks = []
            for name, digest in expected.items():
                label = clean(str(name))
                try:
                    rel = Path(name)
                    if rel.is_absolute() or '..' in rel.parts or ':' in name:
                        raise ValueError('Invalid manifest path')
                    path = safe_file(base / rel, game)
                    if not path.is_file():
                        checks.append(dict(file=label, status='missing')); continue
                    actual = hashlib.sha256()
                    with path.open('rb') as handle:
                        for block in iter(lambda: handle.read(1024 * 1024), b''):
                            actual.update(block)
                    checks.append(dict(file=label, status='ok' if actual.hexdigest() == digest else 'different',
                                       sha256=actual.hexdigest()))
                except (OSError, ValueError) as error:
                    checks.append(dict(file=label, status='unreadable', reason=clean(str(error))))
            result[key] = dict(passed=all(row['status'] == 'ok' for row in checks), files=checks)
    except (OSError, ValueError, TypeError, AttributeError) as error:
        result['manifest_error'] = clean(str(error))
    return result


def collect(game, data):
    from project_paths import inside, no_links
    game = no_links(game)
    data = no_links(data)
    clean = scrubber(game, data)
    files = {}
    records = []
    sources = []
    logs = inside(data / 'logs', root=data)
    # Only diagnostic files: no profiles, settings, save files or game assets.
    if logs.is_dir():
        traces = [safe_file(p, data) for p in logs.glob('*.jsonl')]
        for trace in sorted(traces, key=lambda p: p.stat().st_mtime, reverse=True)[:12]:
            sources.append(('launcher', trace))
            error = safe_file(trace.with_suffix('.txt'), data)
            if error.is_file():
                sources.append(('stderr', error))
    cache = inside(data / 'cache', root=data)
    if cache.is_dir():
        native = []
        for profile in cache.iterdir():
            if profile.name.startswith(('p-', 'menu-profile-')):
                path = safe_file(profile / 'hp2vr-native.jsonl', data)
                if path.is_file():
                    native.append(path)
        for path in sorted(native, key=lambda p: p.stat().st_mtime, reverse=True)[:6]:
            sources.append(('native', path))
    for index, (kind, source) in enumerate(sources, 1):
        text, truncated = read_tail(source)
        member = '{:02d}-{}.{}'.format(index, kind, 'txt' if kind == 'stderr' else 'jsonl')
        output = []
        relevant = []
        for line in text.splitlines():
            try:
                row = json.loads(line)
            except ValueError:
                output.append(clean(line))
                continue
            # Scrub string values before encoding, including escaped JSON paths.
            def sanitize(value):
                if isinstance(value, str):
                    return clean(value)
                if isinstance(value, dict):
                    return {clean(k): sanitize(v) for k, v in value.items()}
                if isinstance(value, list):
                    return [sanitize(v) for v in value]
                return value
            row = sanitize(row)
            output.append(json.dumps(row, ensure_ascii=False))
            if isinstance(row, dict) and row.get('event') in (
                    'fatal', 'graphics_error', 'summary', 'vanilla_summary', 'render_quality', 'launcher_error',
                    'hud_device', 'hud_target_attempt', 'hud_target_fallback',
                    'pending_image_unloaded', 'profile_image_unloaded_before_engine'):
                relevant.append(row)
        files[member] = ('\n'.join(output) + '\n').encode('utf-8')
        records.append(dict(file=member, kind=kind, modified_utc=time.strftime(
            '%Y-%m-%dT%H:%M:%SZ', time.gmtime(source.stat().st_mtime)),
            tail_only=truncated, findings=relevant))
    installation = installation_info(game, clean)
    try:
        machine = json.loads(clean(json.dumps(machine_info())))
    except Exception as error:
        machine = dict(error=clean(str(error)))
    summary = dict(schema=2, mod_version=installation['mod_version'], installation=installation,
                   machine=machine, game_started=False, saves_accessed=False,
                   native_logs=sum(kind == 'native' for kind, _ in sources), files=records)
    files['summary.json'] = (json.dumps(summary, ensure_ascii=False, indent=2) + '\n').encode('utf-8')
    target_dir = inside(data / 'diagnostics', root=data)
    target_dir.mkdir(parents=True, exist_ok=True)
    target = inside(target_dir / ('HP2VR-diagnostics-' + time.strftime('%Y%m%d-%H%M%S')
                                 + '-' + uuid.uuid4().hex[:6] + '.zip'), root=data)
    with zipfile.ZipFile(target, 'x', zipfile.ZIP_DEFLATED) as archive:
        for name, content in files.items():
            archive.writestr(name, content)
    return target, summary


def create_report(game, data, open_folder=True):
    path, summary = collect(game, data)
    print('Diagnose-ZIP / Diagnostic ZIP:\n' + str(path))
    print('Laufzeitprotokolle / Native logs: ' + str(summary['native_logs']))
    print('Bitte diese ZIP-Datei zur Fehleranalyse senden.\nPlease send this ZIP for troubleshooting.')
    if not summary['native_logs']:
        print('Keine Laufzeitprotokolle gefunden / No native logs found.\n'
              'Bei eigenem Datenordner denselben --data-root wie beim Spiel angeben.\n'
              'For a custom data folder, use the same --data-root as the game launcher.')
    if open_folder:
        try:
            os.startfile(path.parent)
        except OSError:
            print('Ordner bitte manuell oeffnen / Please open the report folder manually.')
    return path


def main():
    import portable_launcher as launcher
    parser = argparse.ArgumentParser()
    parser.add_argument('--data-root')
    parser.add_argument('--no-open', action='store_true')
    args = parser.parse_args()
    game = Path(__file__).absolute().parent.parent.parent
    create_report(game, launcher.data_directory(game, args.data_root), open_folder=not args.no_open)


if __name__ == '__main__':
    try:
        main()
    except Exception as error:
        print(str(error), file=sys.stderr)
        sys.exit(1)
