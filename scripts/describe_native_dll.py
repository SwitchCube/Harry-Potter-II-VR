import argparse,hashlib
from inspect_pe import PE
from project_paths import inside,write_text
p=argparse.ArgumentParser();p.add_argument('--dll',required=True);p.add_argument('--output',required=True);a=p.parse_args()
data=inside(a.dll).read_bytes();pe=PE(data)
entry=next(e for e in pe.exports() if e['name'].lstrip('_').split('@')[0]=='HP2VR_Initialize')
write_text(a.output,'#pragma once\nstatic const char* kNativeDllSha="'+hashlib.sha256(data).hexdigest()+'";\nstatic const DWORD kNativeInitRva='+entry['rva']+';\n')
print('Native initialization export verified:',entry['name'],entry['rva'])
