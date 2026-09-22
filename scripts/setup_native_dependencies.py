"""Fetch only the pinned and hashed native dependency files into the project."""
import hashlib,json,urllib.request
from project_paths import inside
lock=json.loads(inside('config/native-vr-dependencies.lock.json').read_text())
for row in lock['files']:
 p=inside(row['path'])
 if p.exists():
  if hashlib.sha256(p.read_bytes()).hexdigest()!=row['sha256']:raise ValueError('Existing dependency differs: '+row['path'])
  continue
 if not row['url'].startswith('https://raw.githubusercontent.com/'+row['repository']+'/'+row['commit']+'/'):raise ValueError('Unpinned URL')
 data=urllib.request.urlopen(row['url'],timeout=30).read()
 if hashlib.sha256(data).hexdigest()!=row['sha256']:raise ValueError('Downloaded dependency differs')
 inside(p.parent).mkdir(parents=True,exist_ok=True)
 with p.open('xb') as f:f.write(data)
print('Native dependency files verified')
