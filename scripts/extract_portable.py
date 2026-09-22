"""Resume extraction of the reviewed Zig archive; all destinations remain local."""
import hashlib
from pathlib import PurePosixPath
import zipfile
from project_paths import ROOT, inside

ARCHIVE_SHA256 = '3a0ed1e8799a2f8ce2a6e6290a9ff22e6906f8227865911fb7ddedc3cc14cb0c'


def main():
    archive = inside('cache/zig-x86_64-windows-0.15.2.zip')
    with archive.open('rb') as handle:
        if hashlib.file_digest(handle, 'sha256').hexdigest() != ARCHIVE_SHA256:
            raise ValueError('Archive SHA256 mismatch')
    destination = inside('tools/zig-x86_64-windows-0.15.2')
    with zipfile.ZipFile(archive) as z:
        for item in z.infolist():
            rel = PurePosixPath(item.filename)
            if rel.is_absolute() or '..' in rel.parts or any(':' in p for p in rel.parts):
                raise ValueError('Invalid archive path')
            if (item.external_attr >> 16) & 0xf000 == 0xa000:
                raise ValueError('Archive symlink rejected')
            out = inside(ROOT / 'tools' / str(rel))
            if not out.is_relative_to(destination):
                raise ValueError('Unexpected archive prefix')
            if item.is_dir():
                out.mkdir(parents=True, exist_ok=True)
                continue
            data = z.read(item)  # Includes CRC validation.
            if out.exists():
                if out.read_bytes() != data:
                    raise ValueError(f'Existing partial or modified file: {out}')
                continue
            out.parent.mkdir(parents=True, exist_ok=True)
            with out.open('xb') as handle:
                handle.write(data)
    print(f'Verified toolchain extracted: {destination}')


if __name__ == '__main__':
    main()
