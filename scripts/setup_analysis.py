"""Fetch/verify the pinned analysis wheel and extract only project-local files."""
import hashlib
import json
from pathlib import PurePosixPath
import urllib.request
import zipfile
from project_paths import inside


def main():
    lock = json.loads(inside('config/analysis-toolchain.lock.json').read_text())
    archive = inside(lock['wheel'])
    if not archive.exists():
        with urllib.request.urlopen('https://pypi.org/pypi/capstone/'+lock['version']+'/json',timeout=30) as response:
            metadata = json.load(response)
        entry = next(e for e in metadata['urls'] if e['filename'] == archive.name)
        if entry['digests']['sha256'] != lock['wheel_sha256']:
            raise ValueError('PyPI wheel does not match pinned SHA256')
        with urllib.request.urlopen(entry['url'],timeout=30) as response:
            data = response.read()
        if len(data) != lock['wheel_bytes'] or hashlib.sha256(data).hexdigest() != lock['wheel_sha256']:
            raise ValueError('Downloaded wheel failed verification')
        archive.parent.mkdir(parents=True,exist_ok=True)
        with archive.open('xb') as handle:
            handle.write(data)
    if hashlib.sha256(archive.read_bytes()).hexdigest() != lock['wheel_sha256']:
        raise ValueError('Cached analysis wheel changed')
    destination = inside(lock['install_directory'])
    with zipfile.ZipFile(archive) as wheel:
        for item in wheel.infolist():
            rel = PurePosixPath(item.filename)
            if rel.is_absolute() or '..' in rel.parts or any(':' in part for part in rel.parts):
                raise ValueError('Invalid wheel path')
            if rel.parts[0] not in ('capstone','capstone-'+lock['version']+'.dist-info'):
                raise ValueError('Unexpected wheel content')
            if (item.external_attr >> 16) & 0xf000 == 0xa000:
                raise ValueError('Wheel symlink rejected')
            # pip rewrites RECORD at install time; it is not executable package content.
            if rel.name == 'RECORD' and rel.parts[0].endswith('.dist-info'):
                continue
            output = inside(destination / str(rel))
            if item.is_dir():
                output.mkdir(parents=True,exist_ok=True)
                continue
            data = wheel.read(item)
            if output.exists():
                if output.read_bytes() != data:
                    raise ValueError('Installed analysis file differs from pinned wheel: '+str(rel))
                continue
            output.parent.mkdir(parents=True,exist_ok=True)
            with output.open('xb') as handle:
                handle.write(data)
    print('Capstone '+lock['version']+' verified against pinned wheel; project-local only.')


if __name__ == '__main__':
    main()
