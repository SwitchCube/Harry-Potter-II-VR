import sys,time,json,os,subprocess,psutil,hashlib
from pathlib import Path
sys.path[:0]=[str(Path(__file__).resolve().parents[1]/'scripts'),str(Path(__file__).resolve().parent)]
import launcher_ui_smoke as ui
from project_paths import inside,write_text,ROOT
from shared_saves import SharedSaves,all_slots
from PIL import ImageGrab
build=sys.argv[1]
assert 'Mode=VR' in inside('config/launcher.ini').read_text(), 'Select VR before this menu test'
m=SharedSaves();before=all_slots(m.external);assert m.read()['active_vr'] is None
assert not [p for p in psutil.process_iter(['name']) if p.info['name'].lower()=='game.exe']
prefix='logs/shared-game-smoke-'+time.strftime('%Y%m%d-%H%M%S');record={'passed':False};known=set()
env=os.environ.copy();env['PSModulePath']=str(Path(env['WINDIR'])/'System32/WindowsPowerShell/v1.0/Modules')
def find(start,predicate,seconds=55):
 deadline=time.monotonic()+seconds
 while time.monotonic()<deadline:
  assert start.poll() is None,'Supervisor stopped'
  family=psutil.Process(start.pid).children(recursive=True);known.update(p.pid for p in family)
  # Original Game.exe launches a child and exits: psutil.children alone loses
  # that still-running child. Follow previously observed parent IDs as well.
  games=[]
  for p in psutil.process_iter(['pid','ppid','name','create_time']):
   if p.info['name'].lower()=='game.exe' and p.info['create_time']>=started_at-1 and (p.pid in known or p.ppid() in known):
    assert p.exe().lower() in (str(inside('system/Game.exe')).lower(),str(inside(build)/'Game.exe').lower())
    games.append(p);known.add(p.pid)
  view=ui.windows({p.pid for p in games})
  result=predicate(view)
  if result:return result
  time.sleep(.1)
 raise AssertionError('Window timeout')
def mode(view,name):
 for root in view:
  for b in root['children']:
   if b['cls']=='HP2VRModeSwitch' and b['visible'] and b['text']=='Modus: '+name:return root,b
with inside(prefix+'.stdout.txt').open('xb') as out,inside(prefix+'.stderr.txt').open('xb') as err:
 started_at=time.time()
 start=subprocess.Popen(['powershell.exe','-NoProfile','-ExecutionPolicy','Bypass','-File',str(inside('scripts/Start-Spiel.ps1')),'-BuildDirectory',build],cwd=ROOT,env=env,stdout=out,stderr=err,creationflags=subprocess.CREATE_NO_WINDOW)
 try:
  root,button=find(start,lambda view:mode(view,'VR'));record['active_at_vr_menu']=m.read();assert m.read()['active_vr'] is not None
  assert ui.u.PostMessageW(button['hwnd'],0x202,0,0)
  root,button=find(start,lambda view:mode(view,'Flat'));assert m.read()['active_vr'] is None
  rect=ui.w.RECT();ui.u.GetWindowRect(root['hwnd'],ui.c.byref(rect));ui.u.SetForegroundWindow(root['hwnd']);time.sleep(.5)
  ImageGrab.grab(bbox=(rect.left,rect.top,rect.right,rect.bottom)).save(inside(prefix+'-menu.png'))
  buttons=[b for b in root['children'] if b['visible'] and b['text']=='Spiel laden'];assert len(buttons)==1
  assert ui.u.PostMessageW(buttons[0]['hwnd'],0xf5,0,0)
  selection=find(start,lambda view:next((x for x in view if x['text']=='Spiel laden'),None))
  slots=[b for b in selection['children'] if b['visible'] and b['cls']=='Button'];record['slots']=slots
  for n in (1,2,4):assert any(b['text']==str(n)+' - Verwendet' for b in slots)
  button=next(b for b in slots if b['text']=='4 - Verwendet');assert ui.u.PostMessageW(button['hwnd'],0xf5,0,0)
  game=find(start,lambda view:next((x for x in view if x['cls']=='GameUnrealWWindowsViewportWindow'),None))
  process=psutil.Process(game['pid']);assert process.exe().lower()==str(inside('system/Game.exe')).lower()
  assert '-SAVESLOT=4' in process.cmdline();time.sleep(6)
  mods=[x.path for x in process.memory_maps() if x.path.lower().endswith(('.dll','.exe'))]
  assert not any('hp2vr' in x.lower() or 'openvr' in x.lower() for x in mods)
  ui.u.GetWindowRect(game['hwnd'],ui.c.byref(rect));ImageGrab.grab(bbox=(rect.left,rect.top,rect.right,rect.bottom)).save(inside(prefix+'-game.png'))
  record.update(game_pid=process.pid,exe=process.exe(),command=process.cmdline(),modules=mods)
  assert ui.u.PostMessageW(game['hwnd'],0x10,0,0);assert start.wait(timeout=25)==0
  assert all_slots(m.external)==before,'Original game changed checkpoint during load-only test'
  assert m.read()['active_vr'] is None
  profile=inside(json.loads(inside('config/menu-session.json').read_text())['profile'])
  assert all_slots(profile/'Save')==before
  record.update(passed=True,shared_checkpoint_files_unchanged=True,vr_profile=str(profile.relative_to(ROOT)),vr_begin_finish_verified=True)
 finally:
  if start.poll() is None:
   for win in ui.windows(known):ui.u.PostMessageW(win['hwnd'],0x10,0,0)
  write_text(prefix+'.json',json.dumps(record,ensure_ascii=False,indent=2));print(json.dumps(dict(report=prefix+'.json',passed=record['passed'])))
