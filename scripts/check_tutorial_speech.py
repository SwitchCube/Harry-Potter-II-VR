"""Local ASR plausibility check; this does not certify voice similarity."""
import json
import argparse
import os
import re
from project_paths import inside


def words(text):
    return re.findall(r'[a-zäöüß]+',text.lower())


def distance(a,b):
    row=list(range(len(b)+1))
    for i,x in enumerate(a,1):
        new=[i]
        for j,y in enumerate(b,1):new.append(min(row[j]+1,new[-1]+1,row[j-1]+(x!=y)))
        row=new
    return row[-1]


def main():
    parser=argparse.ArgumentParser();parser.add_argument('--language',choices=('de','en'),default='de');parser.add_argument('--id',action='append');args=parser.parse_args()
    for key,path in {'HF_HOME':'cache/huggingface','TORCH_HOME':'cache/torch','NUMBA_CACHE_DIR':'cache/numba','TEMP':'cache/tmp','TMP':'cache/tmp'}.items():os.environ[key]=str(inside(path))
    os.environ['HF_HUB_OFFLINE']='1';os.environ['HF_HUB_DISABLE_TELEMETRY']='1'
    import torch
    import soundfile as sf
    import librosa
    from transformers import pipeline
    torch.set_num_threads(4)
    torch.cuda.set_per_process_memory_fraction(.20,0)
    recognizer=pipeline('automatic-speech-recognition',model=os.environ.get('HP2VR_ASR_MODEL',str(inside('tools/tutorial-asr-model'))),device=0,torch_dtype=torch.float16)
    results=[]
    for meta in sorted(inside('assets/tutorial/'+args.language).glob('*.json')):
        source=json.loads(meta.read_text(encoding='utf-8'))
        if args.id and source['id'] not in args.id:continue
        audio,sr=sf.read(str(meta.with_suffix('.wav')),dtype='float32')
        if sr!=16000:audio=librosa.resample(audio,orig_sr=sr,target_sr=16000)
        transcript=recognizer({'raw':audio,'sampling_rate':16000},generate_kwargs={'language':'english' if args.language=='en' else 'german','task':'transcribe'},return_timestamps=False)['text']
        target=words(source['text']);heard=words(transcript);wer=distance(target,heard)/max(1,len(target))
        result=dict(id=source['id'],variant=source['variant'],text=source['text'],transcript=transcript,word_error_rate=wer,needs_review=wer>.22 or bool(re.search(r'maustaste|pfeiltasten|tastatur',transcript,re.I)))
        results.append(result);print(json.dumps(result,ensure_ascii=True),flush=True)
    suffix=('-en' if args.language=='en' else '')+('-selected' if args.id else '')
    inside('logs/tutorial-speech-asr'+suffix+'.json').write_text(json.dumps(results,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')


if __name__=='__main__':main()
