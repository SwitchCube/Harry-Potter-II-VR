"""Prepare ignored, local build inputs from an owned HP2 installation."""
import argparse
import hashlib
import json
from pathlib import Path

from project_paths import inside, no_links, write_text


def configure(game_directory):
    source = no_links(Path(game_directory).absolute())
    fingerprint = json.loads(inside('config/target-fingerprint.json').read_text())
    pending = []
    for row in fingerprint['files']:
        relative = Path(row['path'])
        original = no_links(source / relative)
        data = original.read_bytes()
        if hashlib.sha256(data).hexdigest() != row['sha256'].lower():
            raise ValueError('Unsupported original: ' + relative.as_posix())
        target = inside(relative)
        if target.exists():
            if target.read_bytes() != data:
                raise ValueError('Existing local input differs: ' + relative.as_posix())
        else:
            pending.append((target, data))
    # Validate all inputs before making any local copies.
    for target, data in pending:
        target.parent.mkdir(parents=True, exist_ok=True)
        with target.open('xb') as handle:
            handle.write(data)
    for directory in ('backups', 'logs', 'cache', 'build', 'dist'):
        inside(directory).mkdir(parents=True, exist_ok=True)
    profile = inside('cache/development-profile')
    profile.mkdir(parents=True, exist_ok=True)
    if not (profile / 'Game.ini').exists():
        write_text(profile / 'Game.ini', '[Engine.Engine]\nLanguage=int\n')
    config = inside('config/project.json')
    if not config.exists():
        write_text(config, json.dumps({
            'schema_version': 1,
            'project_name': 'Harry Potter II VR',
            'game_root': '.',
            'game_executable': 'system/Game.exe',
            'profile_directory': str(profile),
            'source_build_label': 'HPCos_021009_1205-1',
        }, indent=2) + '\n')
    print(f'Checked {len(fingerprint["files"])} original files; local build inputs ready.')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--game-directory', required=True)
    configure(parser.parse_args().game_directory)
