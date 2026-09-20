"""HP2VR public launcher. Standard library only; no development dependencies."""
import argparse
import configparser
import ctypes
from ctypes import wintypes
import hashlib
import json
import os
from pathlib import Path
import re
import struct
import subprocess
import sys
import time
import uuid

VERSION = '1.0.2'
LANGUAGE = 'en'
DATA_DIRECTORY = None
REPORT_FAILURES = True

def tr(de, en): return de if LANGUAGE == 'de' else en
def digest(data): return hashlib.sha256(data).hexdigest()

def message(text):
    hint=tr('\n\nBei Problemen: Diagnose-HP2VR.cmd im Spielordner starten. Die erzeugte ZIP-Datei kannst du zur Fehleranalyse weitergeben.',
            '\n\nFor troubleshooting, run Diagnose-HP2VR.cmd in the game folder and share the generated ZIP with the mod maintainer.')
    ctypes.windll.user32.MessageBoxW(None, str(text)+hint, 'Harry Potter II VR '+VERSION, 0x10)

def data_directory(game, override=None):
    identifier=digest(str(game).lower().encode())[:12]
    return Path(override).absolute() if override else known_folder('F1B32785-6FBA-4FCF-9D55-7B8E7F157091')/'HP2VR'/identifier

def record_failure(data, error, stage):
    # Logging is best effort: it must never hide an original error or prevent
    # completed saves from being synchronized. --check remains read-only.
    if data is None or not REPORT_FAILURES: return
    try:
        from project_paths import inside
        logs=inside(data/'logs',root=data);logs.mkdir(parents=True,exist_ok=True)
        path=inside(logs/('error-'+time.strftime('%Y%m%d-%H%M%S')+'-'+uuid.uuid4().hex[:6]+'.jsonl'),root=data)
        with path.open('x',encoding='utf-8') as handle:
            handle.write(json.dumps(dict(event='launcher_error',version=VERSION,stage=stage,
                                         error_type=type(error).__name__,reason=str(error)))+'\n')
    except Exception:
        pass

def native_failure(profile):
    if profile is None: return ''
    try:
        from collect_diagnostics import read_tail, safe_file
        path=safe_file(profile/'hp2vr-native.jsonl',profile)
        if not path.is_file(): return ''
        graphics=None;fatal=None
        for line in read_tail(path)[0].splitlines():
            try: row=json.loads(line)
            except ValueError: continue
            if not isinstance(row,dict): continue
            if row.get('event')=='graphics_error': graphics=row.get('hr')
            if row.get('event')=='fatal': fatal=row
        if not fatal: return ''
        detail=str(fatal.get('stage','native'))+': '+str(fatal.get('reason',''))
        if isinstance(graphics,int): detail+=' (HRESULT 0x%08X)' % (graphics&0xffffffff)
        return detail
    except (OSError,ValueError):
        return ''

def known_folder(identifier):
    value=ctypes.c_wchar_p()
    guid=(ctypes.c_byte*16).from_buffer_copy(uuid.UUID(identifier).bytes_le)
    result=ctypes.windll.shell32.SHGetKnownFolderPath(ctypes.byref(guid), 0, None, ctypes.byref(value))
    if result: raise OSError('Cannot locate Windows user folder: '+str(result))
    try: return Path(value.value)
    finally: ctypes.windll.ole32.CoTaskMemFree(value)

def read_options(path):
    result={'language':'auto','quality':'balanced'}
    parser=configparser.ConfigParser()
    if path.is_file():
        parser.read(path, encoding='utf-8-sig')
        if parser.has_section('HP2VR'): result.update(dict(parser['HP2VR']))
    if result['language'] not in ('auto','de','en'): raise ValueError('Language must be auto, de or en')
    if result['quality'] not in ('performance','balanced','sharp'): raise ValueError('Quality must be performance, balanced or sharp')
    return result

def detect_language(system, profile=None, override='auto'):
    if override!='auto': return override
    for path in ([profile/'Game.ini'] if profile else [])+[system/'Default.ini']:
        if path.is_file():
            found=re.search(r'(?im)^\s*Language\s*=\s*([^\s;]+)',path.read_bytes().decode('cp1252'))
            if found: return 'de' if found[1].lower() in ('ger','de','deu') else 'en'
    return 'en'

def verify_package(package):
    manifest=json.loads((package/'manifest.json').read_text(encoding='utf-8'))
    for name, expected in manifest['files'].items():
        rel=Path(name)
        if rel.is_absolute() or '..' in rel.parts or ':' in name: raise ValueError('Invalid package member')
        p=package/rel
        if not p.is_file() or digest(p.read_bytes())!=expected:
            raise ValueError(tr('Mod-Datei fehlt oder ist beschädigt: ','Missing or damaged mod file: ')+name)
    return manifest

def check_game(system, manifest):
    bad=[]
    for name,expected in manifest['game_files'].items():
        p=system/name
        if not p.is_file() or digest(p.read_bytes())!=expected: bad.append(name)
    return bad

def prepare_image(source, destination, expected_source, expected_runtime):
    data=bytearray(source.read_bytes())
    if digest(data)!=expected_source: raise ValueError('Unsupported original Game.exe')
    if data[:2]!=b'MZ' or len(data)<64: raise ValueError('Invalid PE image')
    pe=struct.unpack_from('<I',data,60)[0]
    if data[pe:pe+4]!=b'PE\0\0' or struct.unpack_from('<H',data,pe+4)[0]!=0x14c: raise ValueError('Expected x86 PE image')
    flags=struct.unpack_from('<H',data,pe+22)[0]
    struct.pack_into('<H',data,pe+22,flags|0x20)
    if digest(data)!=expected_runtime: raise ValueError('Unexpected private runtime image')
    if destination.exists():
        if digest(destination.read_bytes())!=expected_runtime: raise ValueError('Changed private runtime image')
    else:
        with destination.open('xb') as f: f.write(data)

def preflight_vr(dll):
    # These OpenVR queries do not initialize SteamVR or change the active runtime.
    lib=ctypes.CDLL(str(dll))
    lib.VR_IsRuntimeInstalled.restype=ctypes.c_bool
    lib.VR_IsHmdPresent.restype=ctypes.c_bool
    if not lib.VR_IsRuntimeInstalled():
        raise RuntimeError(tr('SteamVR wurde nicht gefunden. Installiere SteamVR über Steam. Flat bleibt verfügbar.',
                              'SteamVR was not found. Install SteamVR through Steam. Flat remains available.'))
    if not lib.VR_IsHmdPresent():
        raise RuntimeError(tr('Kein VR-Headset verfügbar. Starte SteamVR und verbinde dein Headset, dann wähle erneut VR.',
                              'No VR headset is available. Start SteamVR and connect your headset, then select VR again.'))

def ensure_file(source, destination):
    from project_paths import inside
    destination=inside(destination)
    destination.parent.mkdir(parents=True,exist_ok=True)
    data=source.read_bytes()
    if destination.exists():
        if destination.read_bytes()!=data: raise ValueError('Changed installed component: '+destination.name)
    else:
        with destination.open('xb') as f: f.write(data)

def build_relative(manifest):
    # Different native releases get separate verified caches; retain old images
    # and all user state. A script-only update can safely reuse identical code.
    hashes=[manifest['files']['bin/'+name] for name in
            ('hp2vr-native.dll','hp2vr-native-launcher.exe','hp2vr-flat-launcher.exe')]
    hashes.append(manifest['runtime_image_sha256'])
    if any(not isinstance(value,str) or not re.fullmatch('[0-9a-fA-F]{64}',value) for value in hashes):
        raise ValueError('Invalid native build fingerprint')
    return 'build/native-'+digest('\n'.join(value.lower() for value in hashes).encode())[:16]

def setup_data(package, game, profile, data, manifest, options):
    from project_paths import inside, write_text
    from shared_saves import atomic_json
    build=build_relative(manifest)
    for name in ('config','config/vr','cache','cache/tmp','logs',build,'external/openvr/bin/win32'):
        inside(name).mkdir(parents=True,exist_ok=True)
    for name in ('hp2vr-native.dll','hp2vr-native-launcher.exe','hp2vr-flat-launcher.exe'):
        ensure_file(package/'bin'/name, inside(build)/name)
    ensure_file(package/'bin/openvr_api.dll',inside('external/openvr/bin/win32/openvr_api.dll'))
    for source in (package/'defaults/vr').glob('*.json'): ensure_file(source,inside('config/vr')/source.name)
    settings=inside('config/hp2vr.ini')
    if not settings.exists(): write_text(settings,(package/'defaults/hp2vr.ini').read_text(encoding='utf-8'))
    # Options in the distributable are an explicit user override on each start.
    text=settings.read_text(encoding='utf-8');size={'performance':1280,'balanced':1536,'sharp':2048}[options['quality']]
    text=re.sub(r'(?m)^RenderSize=.*\n?', '', text)
    text=text.replace('[VR]','[VR]\nRenderSize='+str(size),1)
    write_text(settings,text,exclusive=False)
    atomic_json('config/project.json',dict(game_executable=str(game/'system/Game.exe'),profile_directory=str(profile),release=VERSION))
    if not inside('config/launcher.ini').exists(): write_text('config/launcher.ini','[Launch]\nMode=Flat\n')

def prepare_profile(game, original):
    from project_paths import inside, write_text
    from menu_sessions import prepare
    # First-run installations need stock defaults even before vanilla was played.
    seed=inside('cache/seed-'+uuid.uuid4().hex[:8]);seed.mkdir()
    for name,default in [('Game.ini','Default.ini'),('User.ini','DefUser.ini')]:
        source=original/name
        if not source.is_file(): source=game/'system'/default
        if not source.is_file(): raise ValueError('Missing stock settings: '+default)
        (seed/name).write_bytes(source.read_bytes())
    # The existing preparation code prefers the last private INIs, then this seed.
    from shared_saves import atomic_json
    project=json.loads(inside('config/project.json').read_text())
    project['profile_directory']=str(seed);atomic_json('config/project.json',project)
    dest=inside('cache/p-'+time.strftime('%Y%m%d-%H%M%S')+'-'+uuid.uuid4().hex[:4])
    try: profile=prepare(dest,shared=False)
    finally:
        project['profile_directory']=str(original);atomic_json('config/project.json',project)
    # stage_profile may copy old saves; the common originals are authoritative.
    save=inside(profile/'Save')
    if any(save.rglob('*.usa')) or any(save.rglob('*.bmp')):
        os.replace(save,inside(profile/'previous-save-copy'));save.mkdir()
    from shared_saves import SharedSaves
    SharedSaves().stage(profile)
    ini=profile/'Game.ini';text=ini.read_bytes().decode('cp1252')
    # Use the installed language even when switching from older private settings.
    lang='ger' if LANGUAGE=='de' else None
    source=original/'Game.ini'
    if not source.exists(): source=game/'system/Default.ini'
    found=re.search(r'(?im)^\s*Language\s*=\s*([^\s;]+)',source.read_bytes().decode('cp1252'))
    if found: lang=found[1]
    if lang: text=re.sub(r'(?im)^Language=[^\r\n]*','Language='+lang,text)
    text=re.sub(r'(?im)^SavePath=[^\r\n]*',lambda _: 'SavePath='+str(profile/'Save/Slot1'),text)
    (profile/'Cache').mkdir(exist_ok=True)
    text=re.sub(r'(?im)^CachePath=[^\r\n]*',lambda _: 'CachePath='+str(profile/'Cache'),text)
    for key in ('WindowedColorBits','FullscreenColorBits'):
        text=re.sub(r'(?im)^'+key+r'=[^\r\n]*',key+'=32',text)
    ini.write_bytes(text.encode('cp1252'))
    return profile

def any_game_running():
    class Entry(ctypes.Structure):
        _fields_=[('size',wintypes.DWORD),('usage',wintypes.DWORD),('pid',wintypes.DWORD),('heap',ctypes.c_size_t),('module',wintypes.DWORD),('threads',wintypes.DWORD),('parent',wintypes.DWORD),('priority',wintypes.LONG),('flags',wintypes.DWORD),('name',wintypes.WCHAR*260)]
    kernel=ctypes.WinDLL('kernel32',use_last_error=True)
    kernel.CreateToolhelp32Snapshot.restype=wintypes.HANDLE
    kernel.Process32FirstW.argtypes=[wintypes.HANDLE,ctypes.POINTER(Entry)]
    kernel.Process32NextW.argtypes=[wintypes.HANDLE,ctypes.POINTER(Entry)]
    kernel.CloseHandle.argtypes=[wintypes.HANDLE]
    handle=kernel.CreateToolhelp32Snapshot(2,0)
    if handle==ctypes.c_void_p(-1).value: raise ctypes.WinError(ctypes.get_last_error())
    try:
        entry=Entry();entry.size=ctypes.sizeof(entry);ok=kernel.Process32FirstW(handle,ctypes.byref(entry))
        while ok:
            if entry.name.lower()=='game.exe': return True
            ok=kernel.Process32NextW(handle,ctypes.byref(entry))
        return False
    finally: kernel.CloseHandle(handle)

def run(package,game,profile,data,manifest,options):
    from project_paths import inside, write_text
    from shared_saves import SharedSaves
    from menu_sessions import activate
    # All installations share a lock for the original save profile, not just a UI.
    kernel=ctypes.WinDLL('kernel32',use_last_error=True);kernel.CreateMutexW.restype=wintypes.HANDLE
    mutex=kernel.CreateMutexW(None,False,'Local\\HP2VR-'+digest(str(profile).lower().encode())[:24])
    if not mutex or ctypes.get_last_error()==183:
        raise RuntimeError(tr('Ein HP2VR-Starter ist bereits geöffnet.','An HP2VR launcher is already open.'))
    try:
        if any_game_running(): raise RuntimeError(tr('Bitte zuerst die laufende Spielsitzung beenden.','Please close the running game first.'))
        setup_data(package,game,profile,data,manifest,options)
        manager=SharedSaves()
        if not manager.state.exists() and not manager.journal.exists(): manager.initialize()
        manager.recover()
        settings=inside('config/launcher.ini');build=inside(build_relative(manifest))
        while True:
            match=re.findall(r'(?im)^Mode=(Flat|VR)\s*$',settings.read_text())
            if len(match)!=1: raise ValueError('Invalid launch mode')
            vr=match[0].lower()=='vr'
            if vr:
                bad=check_game(game/'system',manifest)
                try:
                    if bad: raise RuntimeError(tr('Diese Spielversion wird von VR v1.0 noch nicht unterstützt. Abweichend: ',
                        'This game build is not supported by VR v1.0 yet. Different files: ')+', '.join(bad)+tr('\nDas unveränderte Flat-Spiel bleibt verfügbar.','\nThe unmodified Flat game remains available.'))
                    preflight_vr(inside('external/openvr/bin/win32/openvr_api.dll'))
                except (RuntimeError,OSError) as error:
                    record_failure(data,error,'vr_preflight')
                    message(error);write_text(settings,'[Launch]\nMode=Flat\n',exclusive=False);continue
                prepare_image(game/'system/Game.exe',build/'Game.exe',manifest['game_files']['Game.exe'],manifest['runtime_image_sha256'])
                staged=prepare_profile(game,profile)
                subprocess.run([sys.executable,'-B',str(package/'scripts/stage_vr_bindings.py'),'--profile',str(staged)],check=True,creationflags=subprocess.CREATE_NO_WINDOW)
            stamp=time.strftime('%Y%m%d-%H%M%S')+'-'+uuid.uuid4().hex[:6]
            trace=inside('logs/'+stamp+'.jsonl');errors=inside('logs/'+stamp+'.txt')
            if vr: activate(staged,trace);manager.begin(staged)
            command=[str(build/('hp2vr-native-launcher.exe' if vr else 'hp2vr-flat-launcher.exe')),str(game/'system/Game.exe')]
            if vr: command += [str(staged),'0','frontend','native-menu']
            env=os.environ.copy();env['TEMP']=env['TMP']=str(inside('cache/tmp'))
            with trace.open('xb') as out,errors.open('xb') as err:
                result=subprocess.run(command,cwd=game/'system',env=env,stdout=out,stderr=err,creationflags=subprocess.CREATE_NO_WINDOW)
            if vr: manager.finish(staged)  # Keep complete book saves even after a crash.
            if result.returncode==42:
                events=[json.loads(line) for line in trace.read_text().splitlines() if line.strip()]
                switches=[e for e in events if e.get('event')=='frontend_switch_requested']
                if len(switches)!=1 or switches[0]['vr']==vr: raise RuntimeError('Invalid mode-switch exit')
                continue
            if result.returncode:
                detail=native_failure(staged) if vr else ''
                text=tr('Die Sitzung wurde unterbrochen.','The session was interrupted.')
                if vr: text+=tr(' Vollständige Spielstände wurden abgeglichen.',' Completed saves have been synchronized.')
                text+=tr('\nFehlercode: ','\nExit code: ')+str(result.returncode)
                if detail: text+='\n'+detail
                text+=tr('\nStartprotokoll: ','\nLauncher log: ')+str(trace)
                raise RuntimeError(text)
            return 0
    finally:
        kernel.CloseHandle.argtypes=[wintypes.HANDLE];kernel.CloseHandle(mutex)

def main():
    global LANGUAGE, DATA_DIRECTORY, REPORT_FAILURES
    parser=argparse.ArgumentParser();actions=parser.add_mutually_exclusive_group()
    actions.add_argument('--check',action='store_true');actions.add_argument('--diagnostics',action='store_true')
    parser.add_argument('--data-root');parser.add_argument('--no-open',action='store_true');args=parser.parse_args()
    REPORT_FAILURES=not args.check
    package=Path(__file__).absolute().parent.parent;game=package.parent
    data=data_directory(game,args.data_root);DATA_DIRECTORY=data
    if args.diagnostics:
        from collect_diagnostics import create_report
        create_report(game,data,open_folder=not args.no_open)
        return 0
    if not (game/'system/Game.exe').is_file():
        raise RuntimeError('Bitte das ZIP in den Spielordner mit dem Unterordner System entpacken.\nExtract the ZIP into the game folder containing the System subfolder.')
    profile=known_folder('FDD39AD0-238F-46AF-ADB4-6C85480369C7')/'Harry Potter II'
    options=read_options(package/'Options.ini');LANGUAGE=detect_language(game/'system',profile,options['language'])
    manifest=verify_package(package)
    if len(str(data))>185: raise RuntimeError(tr('Der Datenpfad ist zu lang. Starte mit --data-root und einem kürzeren Pfad.','The data path is too long. Use --data-root with a shorter path.'))
    os.environ['HP2VR_DATA_ROOT']=str(data);os.environ['HP2VR_LANGUAGE']=LANGUAGE
    from project_paths import no_links
    no_links(data);no_links(profile);no_links(game)
    if args.check:
        print(json.dumps(dict(version=VERSION,language=LANGUAGE,quality=options['quality'],game_supported=not check_game(game/'system',manifest),data_directory=str(data),profile=str(profile),package_verified=True)))
        return 0
    return run(package,game,profile,data,manifest,options)

if __name__=='__main__':
    try: sys.exit(main())
    except Exception as error:
        record_failure(DATA_DIRECTORY,error,'launcher')
        print(str(error),file=sys.stderr);message(error);sys.exit(1)
