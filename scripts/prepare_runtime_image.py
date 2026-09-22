"""Make a fingerprinted private Game.exe with only the PE LAA flag changed."""
import argparse,hashlib,json,struct
from project_paths import inside,write_text
from inspect_pe import PE

def prepare(build):
    build=inside(build)
    if build.parent!=inside('build'):raise ValueError('Expected a versioned build directory')
    source=inside('system/Game.exe').read_bytes()
    expected=json.loads(inside('config/target-fingerprint.json').read_text(encoding='utf-8-sig'))
    digest=next(r['sha256'] for r in expected['files'] if r['path'].replace('\\','/').lower()=='system/game.exe')
    if hashlib.sha256(source).hexdigest()!=digest:raise ValueError('Unknown original Game.exe')
    pe=PE(source)
    if pe.machine!=0x14c or pe.bits!=32:raise ValueError('Expected original x86 image')
    offset=pe.u32(0x3c)+22;before=pe.u16(offset)
    image=bytearray(source);struct.pack_into('<H',image,offset,before|0x20)
    changed=[i for i,(a,b) in enumerate(zip(source,image)) if a!=b]
    if changed!=[offset] or image[offset]^source[offset]!=0x20:raise ValueError('Unexpected PE change')
    target=inside(build/'Game.exe')
    with target.open('xb') as f:f.write(image)
    sha=hashlib.sha256(image).hexdigest()
    write_text(build/'runtime-image.json',json.dumps(dict(source='system/Game.exe',original_sha256=digest,sha256=sha,changed_offsets=changed,flag='IMAGE_FILE_LARGE_ADDRESS_AWARE',references=['https://learn.microsoft.com/en-us/windows/win32/memory/memory-limits-for-windows-releases']),indent=2)+'\n')
    write_text(build/'runtime_image_target.h','#pragma once\nstatic const char* kRuntimeImageSha="'+sha+'";\n')
    return target
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--build',required=True);print(prepare(p.parse_args().build))
