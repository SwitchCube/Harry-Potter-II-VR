"""Project boundary helpers. No writes through junctions/symlinks or hardlinks."""
from pathlib import Path
import os
import stat

ROOT = Path(os.environ.get('HP2VR_DATA_ROOT', Path(__file__).absolute().parent.parent)).absolute()


def no_links(path):
    path = Path(os.path.abspath(path))
    for item in (*reversed(path.parents), path):
        if os.path.lexists(item):
            info = item.lstat()
            if info.st_file_attributes & stat.FILE_ATTRIBUTE_REPARSE_POINT:
                raise ValueError(f"Verzeichnisverknuepfung nicht erlaubt: {item}")
    return path


def inside(path, root=ROOT):
    root = no_links(root)
    path = Path(path)
    if not path.is_absolute():
        path = root / path
    path = no_links(path)
    if any(':' in part for part in path.parts[1:]):
        raise ValueError(f"Alternate data stream not allowed: {path}")
    if not path.is_relative_to(root):
        raise ValueError(f"Pfad ausserhalb der Projektwurzel: {path}")
    return path


def write_text(path, text, *, exclusive=True):
    path = inside(path)
    if path.exists() and path.stat().st_nlink != 1:
        raise ValueError(f"Hardlink nicht erlaubt: {path}")
    inside(path.parent).mkdir(parents=True, exist_ok=True)
    with path.open("x" if exclusive else "w", encoding="utf-8", newline="\n") as handle:
        handle.write(text)
    return path


def walk_files(directory):
    directory = no_links(directory)
    if not directory.is_dir():
        return
    for base, dirs, files in os.walk(directory, followlinks=False):
        for name in dirs[:]:
            child = Path(base) / name
            no_links(child)  # Fail closed, do not silently omit saves.
        for name in files:
            yield no_links(Path(base) / name)


def read_trace_text(path):
    """Native stdout redirected by Windows PowerShell 5 is UTF-16; pwsh is UTF-8."""
    data = inside(path).read_bytes()
    encoding = 'utf-16' if data.startswith((b'\xff\xfe', b'\xfe\xff')) else 'utf-8-sig'
    return data.decode(encoding)
