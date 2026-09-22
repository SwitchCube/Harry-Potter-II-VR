"""Build an allowlisted, relocation-safe release, without game assets or user data."""
import argparse,ctypes,hashlib,io,json,os,re,zipfile
from pathlib import Path
from project_paths import ROOT,inside,write_text

def sha(data):return hashlib.sha256(data).hexdigest()
def private_scan(name,data):
    tokens=[str(ROOT),str(Path.home()),Path.home().name]
    # Derive legacy Windows short paths locally; never embed an account name.
    for path in (ROOT, Path.home()):
        buffer=ctypes.create_unicode_buffer(32768)
        length=ctypes.windll.kernel32.GetShortPathNameW(str(path),buffer,len(buffer))
        if 0 < length < len(buffer):
            tokens.extend((buffer.value,Path(buffer.value).name))
    tokens += [os.environ[n] for n in ('USERNAME','COMPUTERNAME') if os.environ.get(n)]
    for token in tokens:
        for encoding in ('utf-8','utf-16-le'):
            if token.lower().encode(encoding) in data.lower():raise ValueError('Personal data in '+name)
    if name.endswith('.zip'):
        with zipfile.ZipFile(io.BytesIO(data)) as archive:
            for member in archive.namelist():private_scan(name+'/'+member,archive.read(member))

def package(build,output):
    build=inside(build);output=inside(output);files={}
    def add(name,source):files[name]=inside(source).read_bytes()
    for name in ('hp2vr-native.dll','hp2vr-native-launcher.exe','hp2vr-flat-launcher.exe'):add('HP2VR/bin/'+name,build/name)
    add('HP2VR/bin/openvr_api.dll','external/openvr/bin/win32/openvr_api.dll')
    for name in ('project_paths.py','shared_saves.py','menu_sessions.py','stage_test_profile.py','stage_vr_bindings.py'):add('HP2VR/scripts/'+name,'scripts/'+name)
    for name in ('portable_launcher.py','collect_diagnostics.py'):add('HP2VR/scripts/'+name,'release/'+name)
    for name in ('Options.ini','README-DE.txt','README-EN.txt','README-DIAGNOSE.txt','CONTROLLERS.txt','THIRD-PARTY.txt'):add('HP2VR/'+name,'release/'+name)
    add('Start-HP2VR.cmd','release/Start-HP2VR.cmd')
    add('Diagnose-HP2VR.cmd','release/Diagnose-HP2VR.cmd')
    add('README-HP2VR.txt','release/README-HP2VR.txt')
    add('HP2VR/licenses/OpenVR.txt','external/openvr/LICENSE')
    add('HP2VR/licenses/MinHook.txt','external/minhook/LICENSE.txt')
    for name,path in [('Zig.txt','LICENSE'),('LLVM-libcxx.txt','lib/libcxx/LICENSE.TXT'),('LLVM-libcxxabi.txt','lib/libcxxabi/LICENSE.TXT'),('LLVM-libunwind.txt','lib/libunwind/LICENSE.TXT'),('MinGW.txt','lib/libc/mingw/COPYING')]:
        add('HP2VR/licenses/'+name,'tools/zig-x86_64-windows-0.15.2/'+path)
    # Only calibrated public defaults, never controller remaps or presentation
    # settings copied out of an individual's live profile.
    defaults='''[VR]
UnitsPerMetre=50
EyeHeightOffset=-5
SnapTurnDegrees=30
WandLengthMetres=0.28
DominantHand=right
StickDeadzone=0.20
MovementSpeed=1.0
TurnMode=snap
SmoothTurnDegreesPerSecond=90
WandOffsetRightMetres=0
WandOffsetUpMetres=0
WandOffsetForwardMetres=0
WandPitchDegrees=-15.563
WandYawDegrees=0
WandRollDegrees=0
HapticStrength=0.45
HudEnabled=1
HudWidthMetres=1.35
RenderSize=1536
AutoQuality=1
'''
    files['HP2VR/defaults/hp2vr.ini']=defaults.encode()
    manifests=['actions.json','actions_left.json']
    bindings=set()
    for name in manifests:
        add('HP2VR/defaults/vr/'+name,'config/vr/'+name)
        bindings.update(b['binding_url'] for b in json.loads(files['HP2VR/defaults/vr/'+name])['default_bindings'])
    for name in sorted(bindings):add('HP2VR/defaults/vr/'+name,'config/vr/'+name)
    runtime=inside('external/python-runtime/python-3.13.13-embed-win32.zip')
    if sha(runtime.read_bytes())!='f93abb82d239bdeafa72b2537859f82dc8fa543ec4c4d7eed1bafa3caf374e02':raise ValueError('Changed Python runtime archive')
    with zipfile.ZipFile(runtime) as z:
        for name in ('python.exe','python313.dll','vcruntime140.dll','LICENSE.txt','_ctypes.pyd','libffi-8.dll','python313.zip'):
            files['HP2VR/python/'+name]=z.read(name)
    files['HP2VR/python/python313._pth']=b'python313.zip\n.\n../scripts\n'
    # Enforce supported engine ABI, not irrelevant optional debug DLLs or local
    # resource translations. Every original module used by a hook is included.
    required={'game.exe','core.dll','engine.dll','render.dll','d3ddrv.dll','windrv.dll','alaudio.dll','hgame.u'}
    fingerprint=json.loads(inside('config/target-fingerprint.json').read_text())
    originals={Path(row['path'].replace('\\','/')).name:row['sha256'].lower() for row in fingerprint['files'] if Path(row['path'].replace('\\','/')).name.lower() in required}
    if len(originals)!=len(required):raise ValueError('Incomplete original ABI fingerprint')
    version=re.search(r"(?m)^VERSION = '([0-9]+\.[0-9]+(?:\.[0-9]+)?)'$",files['HP2VR/scripts/portable_launcher.py'].decode('utf-8'))
    if not version:raise ValueError('Missing release version')
    manifest=dict(version=version[1],game_build='HPCos_021009_1205-1',game_files=originals,
                  runtime_image_sha256=sha((build/'Game.exe').read_bytes()),
                  files={n.removeprefix('HP2VR/'):sha(d) for n,d in sorted(files.items()) if n.startswith('HP2VR/') and n!='HP2VR/Options.ini'})
    files['HP2VR/manifest.json']=(json.dumps(manifest,indent=2)+'\n').encode()
    for name,data in files.items():
        private_scan(name,data)
        if Path(name).name.lower()=='game.exe' or Path(name).suffix.lower() in ('.usa','.unr','.u','.utx','.uax','.umx','.bmp','.pdb'):
            raise ValueError('Forbidden release asset: '+name)
    output.parent.mkdir(parents=True,exist_ok=True)
    with zipfile.ZipFile(output,'x',zipfile.ZIP_DEFLATED,compresslevel=9) as z:
        for name,data in sorted(files.items()):
            info=zipfile.ZipInfo(name,date_time=(2026,9,20,0,0,0));info.compress_type=zipfile.ZIP_DEFLATED;z.writestr(info,data)
    summary=dict(zip=output.relative_to(ROOT).as_posix(),sha256=sha(output.read_bytes()),bytes=output.stat().st_size,files=len(files),personal_data_scan='passed',original_game_files_included=False)
    write_text(str(output)+'.json',json.dumps(summary,indent=2)+'\n')
    write_text(str(output)+'.sha256',summary['sha256']+'  '+output.name+'\n')
    print(json.dumps(summary));return output

def update_package(previous,current,output):
    """Only changed distributable files; preserve the user's public Options.ini."""
    previous=inside(previous);current=inside(current);output=inside(output)
    with zipfile.ZipFile(previous) as old,zipfile.ZipFile(current) as new:
        if old.testzip() or new.testzip():raise ValueError('Corrupt release ZIP')
        old_names=set(old.namelist());new_names=set(new.namelist())
        if old_names-new_names:raise ValueError('Update would require removing obsolete files')
        changed={name:new.read(name) for name in sorted(new_names) if name!='HP2VR/Options.ini'
                 and (name not in old_names or new.read(name)!=old.read(name))}
        before=json.loads(old.read('HP2VR/manifest.json'))['version']
        after=json.loads(new.read('HP2VR/manifest.json'))['version']
    for name,data in changed.items():private_scan(name,data)
    with zipfile.ZipFile(output,'x',zipfile.ZIP_DEFLATED,compresslevel=9) as archive:
        for name,data in changed.items():archive.writestr(name,data)
    summary=dict(zip=output.relative_to(ROOT).as_posix(),from_version=before,to_version=after,
                 sha256=sha(output.read_bytes()),bytes=output.stat().st_size,files=len(changed),preserves_options=True)
    write_text(str(output)+'.json',json.dumps(summary,indent=2)+'\n')
    write_text(str(output)+'.sha256',summary['sha256']+'  '+output.name+'\n')
    print(json.dumps(summary));return output

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--build',required=True);p.add_argument('--output',required=True)
    p.add_argument('--previous');a=p.parse_args();current=package(a.build,a.output)
    if a.previous:update_package(a.previous,current,current.with_name(current.stem+'-Update.zip'))
