"""Actual embedded launcher + dialogs, isolated fake game and runtime API."""
import json,os,psutil,shutil,subprocess,sys,time,uuid,zipfile
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'scripts'))
from project_paths import inside,write_text
from launcher_ui_smoke import windows,u
import hashlib

def smoke(archive,case):
 root=inside('cache/tests/fallback-'+uuid.uuid4().hex[:8]);root.mkdir(parents=True)
 game=root/'Game Install';game.mkdir()
 with zipfile.ZipFile(inside(archive)) as z:z.extractall(game)
 system=game/'system';system.mkdir();shutil.copyfile(inside('cache/port-v1/Flat Fixture/system/Game.exe'),system/'Game.exe')
 (system/'Default.ini').write_text('[Engine.Engine]\nLanguage=int\n')
 package=game/'HP2VR';data=root/'data';(data/'config').mkdir(parents=True)
 (package/'Options.ini').write_text('[HP2VR]\nLanguage=en\nQuality=balanced\n')
 (data/'config/launcher.ini').write_text('[Launch]\nMode=VR\n')
 if case=='runtime':
  # Deliberately substitute a test API returning "not installed". Do not touch
  # the installed OpenVR registration, SteamVR or any system settings.
  dll=package/'bin/openvr_api.dll';shutil.copyfile(inside('cache/port-v1/missing-runtime.dll'),dll)
  mp=package/'manifest.json';m=json.loads(mp.read_text());m['files']['bin/openvr_api.dll']=hashlib.sha256(dll.read_bytes()).hexdigest()
  m['game_files']={'Game.exe':hashlib.sha256((system/'Game.exe').read_bytes()).hexdigest()};mp.write_text(json.dumps(m))
 expected='SteamVR was not found' if case=='runtime' else 'This game build is not supported'
 prefix='logs/portable-fallback-'+case+'-'+time.strftime('%Y%m%d-%H%M%S')
 command=[str(package/'python/python.exe'),'-B',str(package/'scripts/portable_launcher.py'),'--data-root',str(data)]
 known=set();error_seen=False;flat_seen=False
 with inside(prefix+'.stdout').open('xb') as out,inside(prefix+'.stderr').open('xb') as err:
  process=subprocess.Popen(command,stdout=out,stderr=err,cwd=system,creationflags=subprocess.CREATE_NO_WINDOW)
  known.add(process.pid)
  try:
   deadline=time.monotonic()+30
   while time.monotonic()<deadline:
    assert process.poll() is None,'Launcher stopped early'
    known.update(p.pid for p in psutil.Process(process.pid).children(recursive=True))
    for win in windows(known):
     if not error_seen and win['cls']=='#32770':
      texts='\n'.join(c['text'] for c in win['children']);assert expected in texts,texts
      assert 'Diagnose-HP2VR.cmd' in texts,texts
      error_seen=True;u.PostMessageW(win['hwnd'],0x10,0,0)
     for button in win['children']:
      if button['cls']=='HP2VRModeSwitch' and button['visible'] and button['text']=='Mode: Flat':
       flat_seen=True;u.PostMessageW(win['hwnd'],0x10,0,0)
    if flat_seen:break
    time.sleep(.1)
   assert error_seen and flat_seen,(error_seen,flat_seen)
   assert process.wait(timeout=15)==0
   errors=[json.loads(line) for path in (data/'logs').glob('error-*.jsonl') for line in path.read_text().splitlines()]
   assert any(row.get('event')=='launcher_error' and expected in row.get('reason','') for row in errors),errors
   write_text(prefix+'.json',json.dumps(dict(passed=True,case=case,error_seen=expected,flat_available=True,isolated_fake_game=True,runtime_api_stub=case=='runtime')))
  finally:
   if process.poll() is None:
    for attempt in range(20):
     if process.poll() is not None:break
     known.update(p.pid for p in psutil.Process(process.pid).children(recursive=True))
     for win in windows(known):u.PostMessageW(win['hwnd'],0x10,0,0)
     time.sleep(.1)
 print(prefix+'.json')

if __name__=='__main__':smoke(sys.argv[1],sys.argv[2])
