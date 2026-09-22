"""Two-phase restore; apply only with explicitly recorded expected current hashes.

External profile backups can be verified/staged, never overwritten by this tool.
"""
import argparse
from datetime import datetime, timezone
import json
import os
from pathlib import Path
from backup_original import digest, verify
from project_paths import ROOT, inside, write_text


def plan_restore(manifest_path, changes):
    manifest_path = inside(manifest_path)
    verify(manifest_path)
    manifest = json.loads(manifest_path.read_text(encoding='utf-8-sig'))
    rows = []
    for change in changes['files']:
        relative = Path(change['path'])
        dest = inside(relative)
        matches = [r for r in manifest['files'] if r['origin'] == 'project' and Path(r['relative_path']) == relative]
        if len(matches) != 1:
            raise ValueError(f'Kein eindeutiges internes Original: {relative}')
        original = matches[0]
        if not dest.is_file() or dest.stat().st_nlink != 1:
            raise ValueError(f'Kein regulaeres Einzelziel: {dest}')
        if digest(dest) != change['expected_current_sha256']:
            raise ValueError(f'Zwischenzeitlich geaendert: {dest}; Wiederherstellung abgebrochen')
        source = inside(manifest_path.parent / original['backup_path'])
        rows.append((source, dest, original['sha256'], change['expected_current_sha256']))
    if len({r[1] for r in rows}) != len(rows) or not rows:
        raise ValueError('Doppelte oder leere Wiederherstellungsliste')
    return rows


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--manifest', required=True)
    parser.add_argument('--changes', required=True)
    parser.add_argument('--apply', action='store_true')
    args = parser.parse_args()
    changes = json.loads(inside(args.changes).read_text(encoding='utf-8-sig'))
    plan = plan_restore(args.manifest, changes)
    if not args.apply:
        print(json.dumps({'mode':'dry-run','files':[str(r[1].relative_to(ROOT)) for r in plan]}))
        return
    stamp = datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S.%fZ')
    recovery = inside('backups/pre-restore-' + stamp)
    recovery.mkdir()
    events = []
    # Save all current data before writing any original destination.
    for source, dest, original_hash, current_hash in plan:
        saved = inside(recovery / dest.relative_to(ROOT))
        saved.parent.mkdir(parents=True, exist_ok=True)
        data = dest.read_bytes()
        with saved.open('xb') as handle:
            handle.write(data)
        if digest(saved) != current_hash or digest(dest) != current_hash:
            raise ValueError('Datei waehrend Wiederherstellung geaendert')
    for index, (source, dest, original_hash, current_hash) in enumerate(plan):
        # Do not overwrite changes that happened after the initial dry-run check.
        if digest(inside(dest)) != current_hash or dest.stat().st_nlink != 1:
            raise ValueError('Ziel nach Sicherung geaendert; Abbruch')
        staging = inside(dest.parent / (dest.name + '.hp2vr-restore-' + stamp))
        with staging.open('xb') as handle:
            handle.write(source.read_bytes())
        if digest(staging) != original_hash:
            raise ValueError('Restore staging hash mismatch')
        os.replace(staging, inside(dest))
        events.append(dict(path=str(dest.relative_to(ROOT)), before=current_hash, after=digest(dest)))
        write_text(recovery / f'applied-{index:04d}.json', json.dumps(events[-1], indent=2))
    print(json.dumps(dict(mode='restored', recovery=str(recovery.relative_to(ROOT)), files=events)))


if __name__ == '__main__':
    main()
