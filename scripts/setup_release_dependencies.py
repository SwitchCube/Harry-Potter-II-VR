"""Fetch pinned OpenVR and Python runtime inputs; verify every downloaded byte."""
import hashlib
import json
import urllib.request

from project_paths import inside


def fetch(row, path_key):
    target = inside(row[path_key])
    if target.exists():
        data = target.read_bytes()
    else:
        if not row['url'].startswith('https://'):
            raise ValueError('HTTPS required')
        with urllib.request.urlopen(row['url'], timeout=60) as response:
            data = response.read()
    if hashlib.sha256(data).hexdigest() != row['sha256'].lower():
        raise ValueError('Dependency checksum mismatch: ' + row[path_key])
    if not target.exists():
        target.parent.mkdir(parents=True, exist_ok=True)
        with target.open('xb') as handle:
            handle.write(data)


def main():
    openvr = json.loads(inside('config/research-sources.lock.json').read_text())
    for row in openvr['files']:
        if row['repository'] != 'ValveSoftware/openvr':
            raise ValueError('Unexpected OpenVR dependency')
        prefix = 'https://raw.githubusercontent.com/' + row['repository'] + '/' + row['commit'] + '/'
        if not row['url'].startswith(prefix):
            raise ValueError('Unpinned OpenVR dependency')
        fetch(row, 'local_path')
    runtime = json.loads(inside('config/release-dependencies.lock.json').read_text())
    for row in runtime['files']:
        fetch(row, 'path')
    print('OpenVR and embedded Python release dependencies verified.')


if __name__ == '__main__':
    main()
