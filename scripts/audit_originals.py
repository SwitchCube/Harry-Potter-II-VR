"""Read-only comparison of originals and existing saves with recorded references."""
import argparse
from datetime import datetime, timezone
import json
from pathlib import Path
from backup_original import digest, verify
from project_paths import inside, no_links, walk_files, write_text


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--manifest',required=True)
    parser.add_argument('--output',required=True)
    args = parser.parse_args()
    manifest_path = inside(args.manifest)
    verify(manifest_path)
    manifest = json.loads(manifest_path.read_text(encoding='utf-8-sig'))
    reference = json.loads(inside('config/target-fingerprint.json').read_text(encoding='utf-8-sig'))
    rows = []
    for row in reference['files']:
        current = digest(inside(row['path']))
        rows.append(dict(kind='fingerprint',path=row['path'],unchanged=current.lower()==row['sha256'].lower(),sha256=current))
    for row in manifest['files']:
        if row['origin'] == 'project' or row['relative_path'].lower().startswith('save/'):
            path = inside(row['source_path']) if row['origin'] == 'project' else no_links(row['source_path'])
            current = digest(path)
            rows.append(dict(kind=row['origin'],path=row['relative_path'],unchanged=current==row['sha256'],sha256=current))
    config = json.loads(inside('config/project.json').read_text(encoding='utf-8-sig'))
    profile = no_links(config['profile_directory'])
    previous_names = {r['relative_path'] for r in manifest['files'] if r['origin']=='external-profile'}
    added_saves = [p.relative_to(profile).as_posix() for p in walk_files(profile/'Save')
                   if p.relative_to(profile).as_posix() not in previous_names]
    report = dict(date_utc=datetime.now(timezone.utc).isoformat(),manifest=args.manifest,
                  files=rows,added_external_saves=added_saves,
                  external_running_marker_present=(profile/'Running.ini').exists(),
                  all_compared_files_unchanged=all(r['unchanged'] for r in rows),
                  scope='Fingerprinted binaries/package, backed-up internal files and original save files; external INI/log changes documented separately')
    write_text(args.output,json.dumps(report,indent=2)+'\n')
    print(json.dumps({k:v for k,v in report.items() if k!='files'}))
    if not report['all_compared_files_unchanged'] or added_saves:
        raise SystemExit(1)


if __name__ == '__main__':
    main()
