import ctypes as c,json,psutil,sys,time,subprocess,os,hashlib
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/"scripts"))
from project_paths import inside,write_text,walk_files,ROOT
from ctypes import wintypes as w
u=c.WinDLL('user32',use_last_error=True)
CB=c.WINFUNCTYPE(w.BOOL,w.HWND,w.LPARAM)
u.EnumWindows.argtypes=[CB,w.LPARAM];u.EnumChildWindows.argtypes=[w.HWND,CB,w.LPARAM]
u.GetWindowThreadProcessId.argtypes=[w.HWND,c.POINTER(w.DWORD)]
u.GetClassNameW.argtypes=[w.HWND,w.LPWSTR,c.c_int]
u.IsWindowVisible.argtypes=[w.HWND]
u.GetDlgCtrlID.argtypes=[w.HWND]
u.GetWindowTextW.argtypes=[w.HWND,w.LPWSTR,c.c_int]
u.SendMessageTimeoutW.argtypes=[w.HWND,w.UINT,w.WPARAM,w.LPARAM,w.UINT,w.UINT,c.POINTER(c.c_size_t)]
u.PostMessageW.argtypes=[w.HWND,w.UINT,w.WPARAM,w.LPARAM]
u.GetWindow.argtypes=[w.HWND,w.UINT];u.GetWindow.restype=w.HWND
u.GetWindowRect.argtypes=[w.HWND,c.POINTER(w.RECT)]
u.SetForegroundWindow.argtypes=[w.HWND]
u.RedrawWindow.argtypes=[w.HWND,c.c_void_p,c.c_void_p,w.UINT]
def text(hwnd):
 b=c.create_unicode_buffer(1024);r=c.c_size_t()
 u.SendMessageTimeoutW(hwnd,13,1024,c.addressof(b),2,300,c.byref(r));return b.value
def info(hwnd):
 pid=w.DWORD();u.GetWindowThreadProcessId(hwnd,c.byref(pid));cl=c.create_unicode_buffer(256);u.GetClassNameW(hwnd,cl,256)
 return dict(hwnd=hwnd,pid=pid.value,cls=cl.value,text=text(hwnd),id=u.GetDlgCtrlID(hwnd),visible=bool(u.IsWindowVisible(hwnd)))
def windows(pids):
 found=[];popups=[]
 @CB
 def top(hwnd,_):
  pid=w.DWORD();u.GetWindowThreadProcessId(hwnd,c.byref(pid))
  if pid.value not in pids:
   cls=c.create_unicode_buffer(256);u.GetClassNameW(hwnd,cls,256)
   if cls.value!='HP2VRModeSwitch':return True
  r=info(hwnd)
  if r['cls']=='HP2VRModeSwitch':r['owner']=u.GetWindow(hwnd,4);popups.append(r)
  if r['pid'] in pids and r['visible']:
   r['children']=[]
   @CB
   def child(h,_):r['children'].append(info(h));return True
   u.EnumChildWindows(hwnd,child,0);found.append(r)
  return True
 u.EnumWindows(top,0)
 for popup in popups:
  for root in found:
   if popup['owner']==root['hwnd']:root['children'].append(popup)
 return found

def smoke(build):
 build=inside(build);assert not [p for p in psutil.process_iter(['name']) if p.info['name'].lower()=='game.exe']
 assert 'Mode=VR' in inside('config/launcher.ini').read_text()
 stamp=time.strftime('%Y%m%d-%H%M%S');prefix='logs/launcher-ui-smoke-'+stamp
 state=inside('config/menu-session.json').read_bytes()
 cfg=json.loads(inside('config/project.json').read_text(encoding='utf-8-sig'))
 saves={p:hashlib.sha256(p.read_bytes()).hexdigest() for p in walk_files(Path(cfg['profile_directory'])/'Save')}
 env=os.environ.copy();env['PSModulePath']=str(Path(env['WINDIR'])/'System32/WindowsPowerShell/v1.0/Modules')
 observations=[];known=set();report={'build':str(build.relative_to(ROOT)),'passed':False}
 with inside(prefix+'.stdout.txt').open('xb') as out,inside(prefix+'.stderr.txt').open('xb') as err:
  start=subprocess.Popen(['powershell.exe','-NoProfile','-ExecutionPolicy','Bypass','-File',str(inside('scripts/Start-Spiel.ps1')),'-BuildDirectory',str(build),'-Diagnostic'],cwd=ROOT,env=env,stdout=out,stderr=err,creationflags=subprocess.CREATE_NO_WINDOW)
  try:
   for mode in ('VR','Flat','VR'):
    deadline=time.monotonic()+60
    while time.monotonic()<deadline:
     assert start.poll() is None,'Starter stopped before menu'
     family=psutil.Process(start.pid).children(recursive=True);known.update(p.pid for p in family)
     games=[p for p in family if p.name().lower()=='game.exe']
     view=windows({p.pid for p in games});match=[(v,z) for v in view for z in v['children'] if z['visible'] and z['cls']=='HP2VRModeSwitch' and z['text']=='Modus: '+mode]
     if len(match)==1:break
     time.sleep(.15)
    else:raise AssertionError('Menu timeout: '+mode)
    win,button=match[0];game=psutil.Process(win['pid'])
    expected=build/'Game.exe' if mode=='VR' else inside('system/Game.exe')
    assert game.exe().lower()==str(expected).lower()
    assert len(game.cmdline())==1,'Unexpected game command-line overrides'
    modules=[m.path for m in game.memory_maps() if m.path.lower().endswith(('.exe','.dll'))]
    if mode=='Flat':assert not any('hp2vr' in x.lower() or 'openvr' in x.lower() for x in modules)
    if mode=='Flat':
     from PIL import ImageGrab
     u.SetForegroundWindow(win['hwnd']);time.sleep(.5)
     rect=w.RECT();assert u.GetWindowRect(button['hwnd'],c.byref(rect))
     for repaint in range(2):
      if repaint:u.RedrawWindow(win['hwnd'],None,None,0x85);time.sleep(1.0)
      picture=ImageGrab.grab(bbox=(rect.left,rect.top,rect.right,rect.bottom)).convert('RGB')
      pixels=list(picture.getdata());fraction=sum(pixel==(31,36,60) for pixel in pixels)/len(pixels)
      picture.save(inside(prefix+'-flat-visible-'+str(repaint)+'.png'))
      assert fraction>.5,'Flat switch is clickable but not visibly painted: '+str(fraction)
     button['visible_pixel_fraction']=fraction
    observations.append(dict(mode=mode,pid=game.pid,exe=game.exe(),cmd=game.cmdline(),modules=modules,window=win))
    if len(observations)<3:
     assert u.PostMessageW(button['hwnd'],0x202,0,0)
     game.wait(timeout=15)
    else:assert u.PostMessageW(win['hwnd'],0x10,0,0)
   assert start.wait(timeout=25)==0
   assert inside('config/menu-session.json').read_bytes()==state,'Diagnostic changed VR continuation'
   assert all(hashlib.sha256(p.read_bytes()).hexdigest()==h for p,h in saves.items()),'Original save altered'
   report.update(passed=True,original_saves_unchanged=len(saves),vr_continuation_unchanged=True)
  finally:
   report['observations']=observations;write_text(prefix+'.json',json.dumps(report,ensure_ascii=False,indent=2)+'\n')
   if start.poll() is None:
    # Close only this test's known windows, never another user's session.
    for win in windows(known):u.PostMessageW(win['hwnd'],0x10,0,0)
   print(json.dumps(dict(report=prefix+'.json',passed=report['passed'])))
if __name__=='__main__':smoke(sys.argv[1])
