"""Exercise relocated release launchers; never use the real save directory."""
import ctypes,json,os,subprocess,sys,time
from pathlib import Path
import psutil
from PIL import ImageGrab
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'scripts'))
from project_paths import inside,write_text
from launcher_ui_smoke import windows,u,w

def exercise(record,language,flat=False):
 data=Path(record['build']).parent.parent;env=os.environ.copy()
 env['HP2VR_DATA_ROOT']=str(data);env['HP2VR_LANGUAGE']=language
 build=Path(record['build']);profile=Path(record['profile'])
 game=Path(record['game']);name='Flat' if flat else 'VR'
 cmd=[str(build/('hp2vr-flat-launcher.exe' if flat else 'hp2vr-native-launcher.exe')),str(game)]
 if not flat:cmd += [str(profile),'0','frontend','native-menu']
 stamp=time.strftime('%Y%m%d-%H%M%S')+'-'+language+'-'+name;prefix=inside('logs/portable-native-'+stamp)
 known=set();observed=None;passed=False
 with Path(str(prefix)+'.jsonl').open('xb') as out,Path(str(prefix)+'.stderr').open('xb') as err:
  process=subprocess.Popen(cmd,env=env,cwd=game.parent,stdout=out,stderr=err,creationflags=subprocess.CREATE_NO_WINDOW)
  try:
   deadline=time.monotonic()+40
   while time.monotonic()<deadline:
    if process.poll() is not None:raise AssertionError('Launcher exited early: '+Path(str(prefix)+'.stderr').read_text())
    family=psutil.Process(process.pid).children(recursive=True);known.update(x.pid for x in family)
    matches=[(v,b) for v in windows(known) for b in v['children'] if b['cls']=='HP2VRModeSwitch' and b['visible']]
    if len(matches)==1:break
    time.sleep(.15)
   else:raise AssertionError('No visible mode toggle')
   win,button=matches[0]
   assert button['text']==('Mode: ' if language=='en' else 'Modus: ')+name,button
   child=psutil.Process(win['pid']);expected=game if flat else build/'Game.exe'
   assert child.exe().lower()==str(expected).lower()
   assert len(child.cmdline())==1
   if flat:assert not any('hp2vr' in Path(m.path).name.lower() or 'openvr' in Path(m.path).name.lower() for m in child.memory_maps())
   u.SetForegroundWindow(win['hwnd']);time.sleep(.4)
   rect=w.RECT();assert u.GetWindowRect(button['hwnd'],ctypes.byref(rect))
   fractions=[]
   for repaint in range(2):
    if repaint:u.RedrawWindow(win['hwnd'],None,None,0x85);time.sleep(.4)
    picture=ImageGrab.grab(bbox=(rect.left,rect.top,rect.right,rect.bottom)).convert('RGB')
    color=(31,36,60) if flat else (56,36,108)
    fraction=sum(pixel==color for pixel in picture.getdata())/(picture.width*picture.height)
    assert fraction>.5,(language,name,fraction)
    fractions.append(fraction)
   picture.save(str(prefix)+'.png')
   observed=dict(language=language,mode=name,window=win,toggle=button,painted_fractions=fractions,command=child.cmdline())
   if language=='en':
    assert u.PostMessageW(button['hwnd'],0x202,0,0);expected_exit=42
   else:assert u.PostMessageW(win['hwnd'],0x10,0,0);expected_exit=0
   assert process.wait(timeout=20)==expected_exit;passed=True
  finally:
   if process.poll() is None:
    for win in windows(known):u.PostMessageW(win['hwnd'],0x10,0,0)
   write_text(str(prefix)+'.json',json.dumps(dict(passed=passed,observation=observed),indent=2)+'\n')
 print(json.dumps(dict(report=str(prefix)+'.json',mode=name,language=language)))

if __name__=='__main__':
 record=json.loads(inside(sys.argv[1]).read_text());exercise(record,sys.argv[2],len(sys.argv)>3 and sys.argv[3]=='flat')
