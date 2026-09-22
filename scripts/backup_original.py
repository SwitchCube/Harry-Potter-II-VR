"""Verified snapshots inside backups; original game/profile are read-only inputs."""
import argparse
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
from project_paths import ROOT, inside, no_links, walk_files, write_text


def digest(path):
    with path.open('rb') as f:
        return hashlib.file_digest(f, 'sha256').hexdigest()


def snapshot(reason):
    config = json.loads(inside('config/project.json').read_text(encoding='utf-8-sig'))
    stamp = datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S.%fZ')
    target = inside('backups/' + stamp)
    target.mkdir()
    system = inside(Path(config['game_executable']).parent)
    files = [(p, 'project', p.relative_to(ROOT)) for p in system.iterdir()
             if p.suffix.lower() in ('.ini', '.dll', '.exe')]
    profile = no_links(config['profile_directory'])
    files += [(p, 'external-profile', p.relative_to(profile)) for p in walk_files(profile)]
    files += [(p, 'project', p.relative_to(ROOT)) for p in walk_files(inside('Save'))]
    rows = []
    for source, origin, relative in files:
        no_links(source)
        before = digest(source)
        dest = inside(target / 'files' / origin / relative)
        dest.parent.mkdir(parents=True, exist_ok=True)
        with source.open('rb') as src, dest.open('xb') as out:
            while chunk := src.read(1024 * 1024):
                out.write(chunk)
        if before != digest(dest) or before != digest(source):
            raise RuntimeError(f'Quelle beim Sichern veraendert: {source}')
        rows.append(dict(origin=origin, relative_path=relative.as_posix(), source_path=str(source),
                         backup_path=dest.relative_to(target).as_posix(), sha256=before, bytes=dest.stat().st_size))
    manifest = dict(schema_version=1, created_utc=stamp, reason=reason, files=rows,
                    external_restore_policy='Only stage under project root; never overwrite external profile automatically')
    write_text(target / 'manifest.json', json.dumps(manifest, indent=2) + '\n')
    print(json.dumps(dict(backup=str(target.relative_to(ROOT)), files=len(rows), verified=True)))
    return target


def verify(manifest_path):
    path = inside(manifest_path)
    manifest = json.loads(path.read_text(encoding='utf-8-sig'))
    for row in manifest['files']:
        saved = inside(path.parent / row['backup_path'])
        if not saved.is_relative_to(path.parent):
            raise ValueError('Backup path traversal')
        if digest(saved) != row['sha256']:
            raise ValueError(f'Beschaedigte Sicherung: {saved}')
    print(json.dumps(dict(backup=str(path.parent.relative_to(ROOT)), files=len(manifest['files']), verified=True)))


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--reason', default='Before unchanged flat reference run')
    parser.add_argument('--verify')
    args = parser.parse_args()
    if args.verify:
        verify(args.verify)
    else:
        snapshot(args.reason)
