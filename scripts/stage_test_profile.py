"""Stage a small independent profile for the instrumented original game."""
import argparse
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import re
from project_paths import ROOT, inside, no_links, walk_files, write_text


def stage_profile(source, destination, resolution='800x600', include_saves=True):
    if resolution not in ('800x600', '1280x960', '1280x1280', '1536x1536', '2048x2048'):
        raise ValueError('Unreviewed test resolution')
    width, height = resolution.split('x')
    dest = inside(destination)
    if dest.exists():
        raise ValueError('Profile destination already exists')
    if len(str(dest)) > 235:
        raise ValueError('Test profile path too long for legacy game buffer')
    source = no_links(source)
    dest.mkdir(parents=True)
    rows = []
    selected = [p for p in walk_files(source) if p.name.lower() != 'running.ini'
                and ((p.suffix.lower() == '.ini' and not p.is_relative_to(source / 'Save'))
                     or (include_saves and p.is_relative_to(source / 'Save')))]
    for path in selected:
        relative = path.relative_to(source)
        data = path.read_bytes()
        original_hash = hashlib.sha256(data).hexdigest()
        if path.suffix.lower() == '.ini':
            # The original INIs in this target are 8-bit; retain all other bytes.
            if data.startswith((b'\xff\xfe', b'\xfe\xff')):
                raise ValueError('Unexpected UTF16 INI; inspect encoding before staging')
            text = data.decode('cp1252')
            text = text.replace(str(source), str(dest)).replace(str(source).replace('\\', '/'), str(dest))
            replacements = {
                'StartupFullscreen': 'False', 'UseFullscreen': 'False',
                'WindowedViewportX': width, 'WindowedViewportY': height,
                'WindowedRenderDevice': 'D3DDrv.D3DRenderDevice',
                'Reconfig': '0', 'CaptureMouse': 'False',
            }
            for key, value in replacements.items():
                text = re.sub(r'(?m)^' + key + r'=[^\r\n]*', key + '=' + value, text)
            data = text.encode('cp1252')
        output = inside(dest / relative)
        output.parent.mkdir(parents=True, exist_ok=True)
        with output.open('xb') as f:
            f.write(data)
        rows.append(dict(relative_path=relative.as_posix(), source_sha256=original_hash,
                         staged_sha256=hashlib.sha256(data).hexdigest(), bytes=len(data)))
    (dest / 'Save' / 'Slot1').mkdir(parents=True, exist_ok=True)
    write_text(dest / 'staging.json', json.dumps(dict(date_utc=datetime.now(timezone.utc).isoformat(),
               purpose='Isolated test profile; original profile remains read-only', files=rows), indent=2))
    return dest


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--output', required=True)
    parser.add_argument('--resolution', choices=('800x600','1280x960','1280x1280','1536x1536','2048x2048'), default='800x600')
    parser.add_argument('--session-state')
    parser.add_argument('--fresh-session', action='store_true')
    args = parser.parse_args()
    config = json.loads(inside('config/project.json').read_text(encoding='utf-8-sig'))
    if args.session_state:
        from session_profiles import prepare
        print(prepare(args.output,args.resolution,args.session_state,args.fresh_session))
    else:
        print(stage_profile(config['profile_directory'], args.output, args.resolution))


if __name__ == '__main__':
    main()
