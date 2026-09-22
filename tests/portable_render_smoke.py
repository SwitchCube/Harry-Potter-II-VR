"""Real original renderer, fresh release profile and relocated packaged binaries."""
import hashlib,json,os,re,subprocess,sys,time
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'scripts'))
from project_paths import inside,write_text,walk_files
from check_native_trace import check
record=json.loads(inside(sys.argv[1]).read_text());profile=inside(record['profile']);build=inside(record['build']);data=build.parent.parent
ini=profile/'Game.ini';text=ini.read_bytes().decode('cp1252')
assert all(value=='32' for value in re.findall(r'(?m)^WindowedColorBits=(\d+)',text))
assert str(profile/'Cache') in text
for key in ('WindowedViewportX','WindowedViewportY'):text=re.sub(r'(?m)^'+key+'=.*',key+'=1536',text)
ini.write_bytes(text.encode('cp1252'))
write_text(profile/'replay-window.flag','Diagnostic: preserve selected resolution during desktop resize.\n')
external=Path(json.loads(inside('config/project.json').read_text())['profile_directory'])
def snapshot():return {str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in walk_files(external)}
before=snapshot();prefix='logs/portable-render-'+time.strftime('%Y%m%d-%H%M%S')
env=os.environ.copy();env['HP2VR_DATA_ROOT']=str(data);env['HP2VR_LANGUAGE']='en';env['TEMP']=env['TMP']=str(data/'cache/tmp')
with inside(prefix+'.jsonl').open('xb') as out,inside(prefix+'.stderr').open('xb') as err:
 result=subprocess.run([str(build/'hp2vr-native-launcher.exe'),record['game'],str(profile),'65','Entryhall_hub.unr','native-replay'],env=env,cwd=Path(record['game']).parent,stdout=out,stderr=err,timeout=90,creationflags=subprocess.CREATE_NO_WINDOW)
after=snapshot()
run=dict(exit_code=result.returncode,profile=str(profile),trace=str(inside(prefix+'.jsonl')),mode='native-replay',window_replay=True,
         render_resolution='1536x1536',external_profile_changed=[p for p,v in before.items() if after.get(p)!=v],external_profile_added=[p for p in after if p not in before],headset_tested=False,portable_fresh_install=True)
write_text(prefix+'.run.json',json.dumps(run,indent=2)+'\n')
evidence=check(prefix+'.run.json');write_text(prefix+'.evidence.json',json.dumps(evidence,indent=2)+'\n')
print(json.dumps(dict(verified=evidence['verified'],profile=str(profile),report=prefix+'.evidence.json',headset_tested=False)))
