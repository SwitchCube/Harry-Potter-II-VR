"""Static analysis of the fingerprinted local x86 originals. Never executes them."""
import argparse
from datetime import datetime, timezone
import hashlib
import json
import re
import struct
import sys
from pathlib import Path
from project_paths import ROOT, inside, write_text
from inspect_pe import PE

sys.path.insert(0, str(inside('external/python')))
from capstone import Cs, CS_ARCH_X86, CS_MODE_32

PATTERNS = (
    '?Draw@UGameEngine@@', '?Tick@UGameEngine@@', '?ComputeRenderCoords@FSceneNode@@',
    '?ComputeRenderSize@FSceneNode@@', '?eventPlayerCalcView@APlayerPawn@@',
    '?CreateMasterFrame@URender@@', '?DrawWorld@URender@@', '?OccludeFrame@URender@@',
    '?OccludeBsp@URender@@', '?DrawFrame@URender@@', '?SetSceneNode@UD3DRenderDevice@@',
)


def verify_originals():
    fp = json.loads(inside('config/target-fingerprint.json').read_text(encoding='utf-8-sig'))
    for row in fp['files']:
        path = inside(row['path'])
        if hashlib.sha256(path.read_bytes()).hexdigest().lower() != row['sha256'].lower():
            raise ValueError(f'Unknown original version: {path}')


def follow_jumps(pe, rva):
    visited = []
    for _ in range(8):
        if rva in visited:
            raise ValueError('Jump cycle')
        visited.append(rva)
        off = pe.offset(rva)
        if pe.take(off, 1) != b'\xe9':
            return rva, visited
        rva = rva + 5 + pe.unpack('<i', off + 1)[0]
        pe.offset(rva)
    raise ValueError('Too many export thunks')


def import_map(pe):
    result = {}
    rva, size = pe.directory(1)
    if not rva:
        return result
    for i in range(size // 20):
        original, stamp, forward, name, first = pe.unpack('<IIIII', pe.offset(rva) + i * 20)
        if not name:
            break
        for n in range(100000):
            val = pe.u32(pe.offset(original or first) + n * 4)
            if not val:
                break
            symbol = '#' + str(val & 0xffff) if val & 0x80000000 else pe.string(val + 2)
            result[pe.image_base + first + n * 4] = pe.string(name) + '!' + symbol
    return result


def analyze(path, prefixes=PATTERNS):
    path = inside(path)
    data = path.read_bytes()
    pe = PE(data)
    if pe.machine != 0x14c:
        raise ValueError('Expected x86')
    md = Cs(CS_ARCH_X86, CS_MODE_32)
    labels = import_map(pe)
    starts = set()
    exports = pe.exports()
    for e in exports:
        rva = int(e['rva'], 16)
        labels[pe.image_base + rva] = e['name']
        try:
            target, _ = follow_jumps(pe, rva)
            labels[pe.image_base + target] = e['name']
            starts.add(target)
        except ValueError:
            pass  # Some exports are data, not file-backed executable code.
    functions = []
    text = []
    for e in exports:
        if not any(e['name'].startswith(p) for p in prefixes):
            continue
        thunk = int(e['rva'], 16)
        target, chain = follow_jumps(pe, thunk)
        next_start = min((s for s in starts if s > target), default=target + 12000)
        limit = min(12000, next_start - target)
        offset = pe.offset(target)
        limit = min(limit, len(data) - offset)
        instructions = []
        text.append(f'\n{path.name}!{e["name"]}\nexport RVA {thunk:#x}; implementation RVA {target:#x}; listing window {limit} bytes')
        for instruction in md.disasm(data[offset:offset + limit], pe.image_base + target):
            annotation = []
            for literal in re.findall(r'0x[0-9a-f]+', instruction.op_str):
                address = int(literal, 16)
                if address in labels:
                    annotation.append(labels[address])
            row = dict(rva=instruction.address - pe.image_base, bytes=instruction.bytes.hex(),
                       mnemonic=instruction.mnemonic, operands=instruction.op_str, symbols=annotation)
            instructions.append(row)
            text.append(f'{row["rva"]:08x}  {row["bytes"]:28s} {row["mnemonic"]:8s} {row["operands"]}'
                        + (' ; ' + ', '.join(annotation) if annotation else ''))
        functions.append(dict(export=e['name'], export_rva=thunk, implementation_rva=target,
                              jump_chain=chain, export_bytes=data[pe.offset(thunk):pe.offset(thunk)+5].hex(),
                              implementation_prefix=data[offset:offset+24].hex(), instructions=instructions,
                              listing_limit='next known export implementation or 12000 bytes; not a proved full function boundary'))
    strings = []
    pattern = re.compile(r'(?i)(saveslot|savepath|userini|userdir|ini=|log=|\.unr|nointro|windowed|nosound)')
    for encoding, rx in [('ascii', rb'[\x20-\x7e]{5,}'), ('utf-16-le', rb'(?:[\x20-\x7e]\x00){5,}')]:
        for match in re.finditer(rx, data):
            s = match.group().decode(encoding)
            if pattern.search(s) and len(s) < 240:
                strings.append(dict(file_offset=match.start(), encoding=encoding, text=s))
    return dict(file=str(path.relative_to(ROOT)), sha256=hashlib.sha256(data).hexdigest(),
                preferred_image_base=pe.image_base, functions=functions, relevant_strings=strings), '\n'.join(text)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--output', default='logs/static-' + datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%SZ'))
    args = parser.parse_args()
    verify_originals()
    out = inside(args.output)
    out.mkdir()
    records = []
    for name in ('Game.exe', 'Core.dll', 'Engine.dll', 'Render.dll', 'D3DDrv.dll'):
        record, listing = analyze('system/' + name)
        records.append(record)
        write_text(out / (name + '.asm.txt'), listing + '\n')
        print(name, 'functions:', len(record['functions']))
    write_text(out / 'analysis.json', json.dumps(records, indent=2) + '\n')
    print(out)


if __name__ == '__main__':
    main()
