"""Generate version-specific observer descriptors from reviewed original exports."""
import argparse
import hashlib
import json
from pathlib import Path
from analyze_original import verify_originals, follow_jumps
from inspect_pe import PE
from project_paths import inside, write_text

TARGETS = [
    ('Tick', 'Engine.dll', '?Tick@UGameEngine@@UAEXM@Z'),
    ('ComputeRenderCoords', 'Engine.dll', '?ComputeRenderCoords@FSceneNode@@QAEXAAVFVector@@AAVFRotator@@@Z'),
    ('DrawWorld', 'Render.dll', '?DrawWorld@URender@@UAEXPAUFSceneNode@@@Z'),
    ('OccludeFrame', 'Render.dll', '?OccludeFrame@URender@@QAEXPAUFSceneNode@@@Z'),
]


def describe(module, symbol):
    data = inside('system/' + module).read_bytes()
    pe = PE(data)
    export = next(e for e in pe.exports() if e['name'] == symbol)
    rva = int(export['rva'], 16)
    target, chain = follow_jumps(pe, rva)
    if len(chain) != 2 or pe.take(pe.offset(target), 5) != bytes.fromhex('558bec6aff'):
        raise ValueError('Reviewed thunk/prolog no longer matches')
    return pe, dict(module=module, export=symbol, export_rva=rva, implementation_rva=target,
                    thunk_bytes=pe.take(pe.offset(rva), 5).hex(), prolog_bytes='558bec6aff',
                    sha256=hashlib.sha256(data).hexdigest())


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--check', action='store_true', help='Verify generated descriptors without writing')
    parser.add_argument('--refresh', action='store_true', help='Replace only the two generated project descriptors')
    args = parser.parse_args()
    verify_originals()
    rows = []
    for label, module, symbol in TARGETS:
        _, row = describe(module, symbol)
        row['label'] = label
        rows.append(row)
    pe, profile = describe('Core.dll', '?appUserDir@@YAPBGXZ')
    off = pe.offset(profile['implementation_rva'])
    if pe.take(off + 0x1b, 3) != bytes.fromhex('66833d') or pe.take(off + 0x93, 1) != b'\xb8':
        raise ValueError('Unreviewed appUserDir cache logic')
    cache_va = pe.u32(off + 0x1e)
    if pe.u32(off + 0x94) != cache_va:
        raise ValueError('appUserDir does not return the tested cache buffer')
    profile.update(cache_rva=cache_va - pe.image_base, maximum_characters=240,
                   evidence='cmp cached UTF16 first character then branch to return same cache address; observed original only')
    fp = json.loads(inside('config/target-fingerprint.json').read_text(encoding='utf-8-sig'))
    modules = {Path(r['path']).name: r['sha256'].lower() for r in fp['files']}
    record = dict(schema_version=1, mode='hardware-debug-register-observer', targets=rows, profile=profile,
                  game_sha256=modules['Game.exe'], no_disk_patching=True)
    render = PE(inside('system/Render.dll').read_bytes())
    engine = PE(inside('system/Engine.dll').read_bytes())
    # Reviewed CreateMasterFrame passes its by-value FVector/FRotator stack arguments.
    if render.take(render.offset(0x1fec2), 10) != bytes.fromhex('8d4d188d550c51528bce'):
        raise ValueError('Unreviewed master-frame camera arguments')
    if render.take(render.offset(0x1fecc), 2) != b'\xff\x15':
        raise ValueError('Unreviewed camera call')
    if engine.take(engine.offset(0x8ac51), 3) != bytes.fromhex('8d4334'):
        raise ValueError('Unreviewed frame coordinate destination')
    record['camera_access'] = dict(master_coords_return_rva=0x1fed2,
        position_caller_ebp_offset=12, rotation_caller_ebp_offset=24,
        frame_coords_origin_offset=0x34, test_shift_world_x_units=8,
        stereo=False, evidence='Reviewed Render 0x1fec2..0x1fed2; Engine 0x8ac2a..0x8ac62')
    def blob(value):
        return '{' + ','.join('0x'+value[i:i+2] for i in range(0,len(value),2)) + '}'
    lines = ['// Generated from fingerprinted local originals by prepare_observer.py; no guessed offsets.',
             '#pragma once', 'struct Target { const char* label; const wchar_t* module; unsigned long exportRva, entryRva; unsigned char thunk[5]; };',
             'static const Target kTargets[] = {']
    for row in rows:
        lines.append(' {"%s", L"%s", 0x%x, 0x%x, %s},' % (row['label'], row['module'], row['export_rva'], row['implementation_rva'], blob(row['thunk_bytes'])))
    lines += ['};', 'static const unsigned char kProlog[5] = {0x55,0x8b,0xec,0x6a,0xff};',
              'static const unsigned long kUserDirEntryRva = 0x%x;' % profile['implementation_rva'],
              'static const unsigned long kUserDirCacheRva = 0x%x;' % profile['cache_rva'],
              'static const unsigned long kMasterCoordsReturnRva = 0x1fed2;',
              'static const unsigned long kPositionCallerEbpOffset = 12;',
              'static const unsigned long kRotationCallerEbpOffset = 24;',
              'static const unsigned long kFrameCoordsOriginOffset = 0x34;',
              'struct ModuleHash { const wchar_t* name; const char* sha; };',
              'static const ModuleHash kHashes[] = {']
    for name in ('Game.exe', 'Core.dll', 'Engine.dll', 'Render.dll', 'D3DDrv.dll'):
        lines.append(' {L"%s", "%s"},' % (name, modules[name]))
    lines += ['};']
    outputs = {'config/observer-targets.json': json.dumps(record, indent=2) + '\n',
               'src/observed_target.h': '\n'.join(lines) + '\n'}
    for path, text in outputs.items():
        if args.check:
            if inside(path).read_text(encoding='utf-8') != text:
                raise ValueError('Stale generated descriptor: ' + path)
        else:
            write_text(path, text, exclusive=not args.refresh)
    print('Observer descriptors checked.' if args.check else 'Observer descriptors generated from reviewed originals.')


if __name__ == '__main__':
    main()
