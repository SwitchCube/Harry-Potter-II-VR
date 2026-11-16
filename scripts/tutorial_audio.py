"""Offline HP2 tutorial speech. Independent v79 sound serializer, verified against HP2 Engine.dll and HPSounds.u."""
import argparse,hashlib,json,os,struct,subprocess,sys,uuid,wave,math,array
from pathlib import Path
from project_paths import ROOT,inside
from unreal_package import Package,Reader
LANGUAGE=os.environ.get('HP2VR_TUTORIAL_LANGUAGE','de')
if LANGUAGE not in ('de','en'):raise ValueError('Unsupported language')
PACKAGE='HP2VRTutorial'+('EN' if LANGUAGE=='en' else '')
FOLDER='assets/tutorial/'+LANGUAGE

def catalogue():return json.loads(inside('config/tutorials-'+LANGUAGE+'.json').read_text(encoding='utf-8'))

def compact(value):
 negative=value<0;value=abs(value);first=value&63;value>>=6;out=bytearray([first|(128 if negative else 0)|(64 if value else 0)])
 while value:
  byte=value&127;value>>=7;out.append(byte|(128 if value else 0))
 return bytes(out)

def decode_reference(raw,samples):
 # EA XA v1 mono, 15-byte blocks / 28 samples. Codec reference:
 # https://github.com/vgmstream/vgmstream/blob/master/src/coding/ea_xa_decoder.c
 # Used only to read local voice references, never a runtime dependency.
 if len(raw)%15 or samples>len(raw)//15*28:raise ValueError('Unsupported HP2 XA extent')
 coefficients=((0,0),(240,0),(460,-208),(392,-220));out=[];last=prior=0
 for offset in range(0,len(raw),15):
  header=raw[offset];predictor=header>>4;shift=(header&15)+8
  if predictor>=4:raise ValueError('Unsupported XA predictor')
  a,b=coefficients[predictor]
  for byte in raw[offset+1:offset+15]:
   for nibble in (byte>>4,byte&15):
    signed=nibble if nibble<8 else nibble-16
    sample=max(-32768,min(32767,(((signed<<28)>>shift)+a*last+b*prior+128)>>8))
    out.append(sample);prior,last=last,sample
 return struct.pack('<%dh'%samples,*out[:samples])

def prepare_references():
 config=catalogue();original=Package(inside('sounds/AllDialog.GER_uax'))
 texts=dict(line.split('=',1) for line in inside('system/HPDialog.ger').read_text(encoding='cp1252').splitlines() if '=' in line)
 texts={k.lower():v.strip() for k,v in texts.items()}
 folder=inside('cache/tutorial-voices/reference');folder.mkdir(parents=True,exist_ok=True)
 for line in config['lines']:
  key=line['id'];row=next(r for r in original.exports if r['name'].lower()==key.lower());r=Reader(original.data,row['offset'])
  assert original.name(r.compact())=='None' and original.name(r.compact())=='XA'
  flags,seconds,samples,bits,channels,rate=r.unpack('<If4I');assert bits==16 and channels==1 and rate==22050
  end=r.unpack('<I')[0];raw=r.take(r.compact());assert r.pos==end
  with wave.open(str(folder/(key+'.wav')),'wb') as wav:wav.setparams((1,2,rate,0,'NONE','not compressed'));wav.writeframes(decode_reference(raw,samples))
  text=texts[key.lower()];text=text.split(']',1)[-1].strip()
  (folder/(key+'.txt')).write_text(text,encoding='utf-8')

def generate(limit=0):
    # All runtime caches, including dependencies' temporary files, stay local.
    for key,relative in {'HF_HOME':'cache/huggingface','TORCH_HOME':'cache/torch','NUMBA_CACHE_DIR':'cache/numba','TEMP':'cache/tmp','TMP':'cache/tmp','MPLCONFIGDIR':'cache/matplotlib'}.items():
        path=inside(relative);path.mkdir(parents=True,exist_ok=True);os.environ[key]=str(path)
    os.environ['HF_HUB_DISABLE_TELEMETRY']='1'
    os.environ['PYTHONDONTWRITEBYTECODE']='1'
    import torch
    import soundfile as sf
    import numpy as np
    from qwen_tts import Qwen3TTSModel
    prepare_references()
    config=catalogue(); model_dir=Path(os.environ.get('HP2VR_VOICE_MODEL',inside('tools/tutorial-voice-model')))
    if not (model_dir/'config.json').is_file():raise ValueError('Local voice model is missing')
    torch.set_num_threads(6)
    model=Qwen3TTSModel.from_pretrained(str(model_dir),device_map='cuda:0',dtype=torch.bfloat16,attn_implementation='sdpa')
    out=inside(FOLDER);out.mkdir(parents=True,exist_ok=True)
    produced=0
    for index,line in enumerate(config['lines']):
        reference=inside('cache/tutorial-voices/reference')/line['id']
        pending=[]
        for variant,labels in config['labels'].items():
            text=line['text'].format(**labels);filename=f'{line["id"]}_{variant}.wav';target=out/filename
            meta=target.with_suffix('.json')
            if target.exists() and meta.exists():
                previous=json.loads(meta.read_text(encoding='utf-8'))
                if previous.get('text')==text and hashlib.sha256(target.read_bytes()).hexdigest()==previous.get('sha256'):continue
            pending.append((variant,text,filename,target,meta))
        if not pending:continue
        if limit:pending=pending[:limit-produced]
        prompt=model.create_voice_clone_prompt(ref_audio=str(reference.with_suffix('.wav')),ref_text=reference.with_suffix('.txt').read_text(encoding='utf-8'),x_vector_only_mode=False)
        torch.manual_seed(20260926+index)
        print(json.dumps(dict(generating=line['id'],variants=len(pending))),flush=True)
        with torch.inference_mode():
            wavs,sr=model.generate_voice_clone(text=[p[1] for p in pending],language=['English' if LANGUAGE=='en' else 'German']*len(pending),voice_clone_prompt=prompt,max_new_tokens=1000)
        for (variant,text,filename,target,meta),generated in zip(pending,wavs,strict=True):
            audio=np.asarray(generated,dtype=np.float32)
            if not np.isfinite(audio).all() or not 1<len(audio)/sr<65:raise ValueError('Invalid generated tutorial audio '+filename)
            # Keep safe headroom; PCM is read by the original audio engine.
            reference_audio,_=sf.read(str(reference.with_suffix('.wav')),dtype='float32')
            rms=float(np.sqrt(np.mean(audio*audio)));reference_rms=float(np.sqrt(np.mean(reference_audio*reference_audio)))
            peak=float(np.max(np.abs(audio)));audio*=min(2,reference_rms/max(.001,rms),.89/max(.001,peak))
            sf.write(str(target),audio,sr,subtype='PCM_16')
            record=dict(id=line['id'],variant=variant,text=text,seconds=len(audio)/sr,model=config['voice_model'],synthetic=True,local_reference=True,sha256=hashlib.sha256(target.read_bytes()).hexdigest())
            meta.write_text(json.dumps(record,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
            print(json.dumps(record,ensure_ascii=True),flush=True);produced+=1
            if limit and produced>=limit:return


def lip_envelope(path):
 # HP2 GetLipsyncValue interpolates unsigned amplitude bytes at 20 ms intervals.
 with wave.open(str(path),'rb') as wav:
  rate=wav.getframerate();pcm=array.array('h',wav.readframes(wav.getnframes()))
  if sys.byteorder!='little':pcm.byteswap()
 result=bytearray()
 for i in range(math.ceil(len(pcm)*50/rate)):
  block=pcm[i*rate//50:(i+1)*rate//50]
  rms=math.sqrt(sum(v*v for v in block)/max(1,len(block)))/32768
  result.append(round(min(1,rms/.18)*255))
 return bytes(result)+b'\0\0' # interpolation guard / closed mouth after speech

def pack():
 config=catalogue();folder=inside(FOLDER);out=bytearray(64);names=['None','wav','Package','Class','Engine','Core','Sound'];rows=[]
 for line in config['lines']:
  symbol=line['id']+'_default';path=folder/(symbol+'.wav');data=path.read_bytes()
  with wave.open(str(path),'rb') as wav:
   assert wav.getnchannels()==1 and wav.getsampwidth()==2;seconds=wav.getnframes()/wav.getframerate()
  rows.append(dict(line=line,symbol=symbol,data=data,seconds=seconds,lip=lip_envelope(path)));names.append(symbol)
 for name in names:out+=compact(len(name)+1)+name.encode('ascii')+b'\0'+struct.pack('<I',0x70010)
 for row in rows:
  row['offset']=len(out);data=row.pop('data')
  # HP2 v79: RIFF PCM plus its own 50 Hz lip envelope (flag 0x80).
  # Both TLazyArray end positions are absolute file offsets.
  prefix=b'\0\1'+struct.pack('<If4I',0x80,row['seconds'],0,0,0,0)
  lazy=compact(len(data));audio_end=row['offset']+len(prefix)+4+len(lazy)+len(data)
  lip=row['lip'];lip_count=compact(len(lip));lip_end=audio_end+4+len(lip_count)+len(lip)
  row['size']=lip_end-row['offset']
  out+=prefix+struct.pack('<I',audio_end)+lazy+data+struct.pack('<I',lip_end)+lip_count+lip

 io=len(out);out+=compact(5)+compact(3)+struct.pack('<i',-2)+compact(6);out+=compact(5)+compact(2)+struct.pack('<i',0)+compact(4)
 eo=len(out)
 for i,row in enumerate(rows):out+=compact(-1)+compact(0)+struct.pack('<i',0)+compact(7+i)+struct.pack('<I',0xf0004)+compact(row['size'])+compact(row['offset'])
 struct.pack_into('<9I',out,0,0x9e2a83c1,79,1,len(names),64,len(rows),eo,2,io)
 out[36:52]=uuid.UUID('4d5145a1-6d7d-46da-9e67-e5d9dfc2baaa' if LANGUAGE=='de' else '85ab7347-08ac-4bb0-b356-b9308a141eae').bytes_le
 struct.pack_into('<3I',out,52,1,len(rows),len(names))
 destination=inside('assets/tutorial/'+PACKAGE+'.uax');destination.write_bytes(out)
 def w(text):return 'L'+json.dumps(text,ensure_ascii=True)
 h=['#pragma once','// Generated synthetic speech, independent HP2 v79 assets.']
 if LANGUAGE=='de':h+=['struct Hp2TutorialLine {const wchar_t* id; const wchar_t* text; const wchar_t* sound; float seconds; const char* actions[2]; const char* group;};']
 h+=['static const Hp2TutorialLine tutorialLines'+('En' if LANGUAGE=='en' else '')+'[]={']
 for row in rows:
  line=row['line'];actions=line['actions']+['']*(2-len(line['actions']))
  h+=[' {%s,%s,%s,%.6ff,{%s,%s},%s},'%(w(line['id']),w('[Normal]'+line['text']),w(PACKAGE+'.'+row['symbol']),row['seconds'],json.dumps(actions[0]),json.dumps(actions[1]),json.dumps(line['group']))]
 h+=['};'];inside('src/vr_tutorial_catalogue'+('_en' if LANGUAGE=='en' else '')+'.h').write_text('\n'.join(h)+'\n',encoding='utf-8')
 print(json.dumps(dict(package=destination.relative_to(ROOT).as_posix(),sounds=len(rows),bytes=len(out))))

def verify_assets():
 package=Package(inside('assets/tutorial/'+PACKAGE+'.uax'));config=catalogue();assert len(package.exports)==len(config['lines'])
 for line,row in zip(config['lines'],package.exports):
  path=inside(FOLDER)/(line['id']+'_default.wav');record=json.loads(path.with_suffix('.json').read_text(encoding='utf-8'))
  assert record['synthetic'] and record['text']==line['text'] and hashlib.sha256(path.read_bytes()).hexdigest()==record['sha256']
  r=Reader(package.data,row['offset']);assert package.name(r.compact())=='None' and package.name(r.compact())=='wav'
  flags,seconds,*metadata=r.unpack('<If4I');assert flags==0x80 and not any(metadata) and abs(seconds-record['seconds'])<.001
  end=r.unpack('<I')[0];raw=r.take(r.compact());assert raw==path.read_bytes() and r.pos==end
  end=r.unpack('<I')[0];lip=r.take(r.compact());assert lip==lip_envelope(path) and len(lip)>=math.ceil(seconds*50)+1 and r.pos==end==row['offset']+row['size']
 return len(package.exports)

if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('--generate',action='store_true');p.add_argument('--pack',action='store_true');p.add_argument('--verify',action='store_true');p.add_argument('--references',action='store_true');p.add_argument('--limit',type=int,default=0);a=p.parse_args()
 if a.references:prepare_references()
 if a.generate:generate(a.limit)
 if a.pack:pack()
 if a.verify:print(verify_assets())
