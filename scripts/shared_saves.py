"""One set of stock save slots for Flat and VR; settings remain mode-specific.

The user explicitly requested writes to the original Save directory on 20 Sep.
No external INI, DLL or other directory is writable through this module.
Slot replacement is journaled, and every displaced directory is retained.
"""
import argparse,hashlib,json,os,shutil,uuid
from pathlib import Path
from project_paths import ROOT,inside,no_links,walk_files,write_text

SLOTS=tuple('Slot'+str(n) for n in range(1,7))
STATE='config/shared-saves.json'
JOURNAL='cache/shared-saves-pending.json'

def digest(data):return hashlib.sha256(data).hexdigest()
def same_volume(a,b):return os.path.splitdrive(str(a))[0].lower()==os.path.splitdrive(str(b))[0].lower()
def atomic_json(path,data):
    path=inside(path)
    if path.exists() and path.stat().st_nlink!=1:raise ValueError('Hardlinked shared-save state')
    temp=inside(str(path)+'.'+uuid.uuid4().hex+'.tmp')
    write_text(temp,json.dumps(data,indent=2)+'\n');os.replace(temp,path)

def snapshot(directory):
    """Ignore unfinished Save.tmp; keep each complete slot as one unit."""
    result={}
    for p in walk_files(directory):
        if p.suffix.lower() not in ('.usa','.bmp'):continue
        key=p.relative_to(directory).as_posix();data=p.read_bytes()
        if p.name.lower()=='save0.usa' and (len(data)<64 or data[:4]!=bytes.fromhex('c1832a9e')):
            raise ValueError('Invalid original checkpoint: '+str(p))
        result[key]=digest(data)
    return result

def all_slots(root):return {slot:snapshot(no_links(root/slot)) for slot in SLOTS}

def copy_snapshot(source,dest,expected):
    dest=no_links(dest);dest.mkdir(parents=True,exist_ok=True)
    for name,sha in expected.items():
        rel=Path(name)
        if rel.is_absolute() or '..' in rel.parts or ':' in name:raise ValueError('Unsafe save member')
        data=no_links(source/rel).read_bytes()
        if digest(data)!=sha:raise ValueError('Save changed while copying: '+str(source/rel))
        output=no_links(dest/rel);output.parent.mkdir(parents=True,exist_ok=True)
        with output.open('xb') as f:f.write(data);f.flush();os.fsync(f.fileno())
    if snapshot(dest)!=expected:raise ValueError('Save copy verification failed')

class SharedSaves:
    def __init__(self,state=STATE,journal=JOURNAL,external=None):
        self.state=inside(state);self.journal=inside(journal)
        configured=no_links(json.loads(inside('config/project.json').read_text(encoding='utf-8-sig'))['profile_directory'])/'Save'
        self.external=no_links(external or configured)
        # The sole production exception is precisely the configured original Save.
        if self.external!=configured and not self.external.is_relative_to(inside('cache/tests')):
            raise ValueError('Shared saves may only write original Save or a test fixture')
    def read(self):
        state=json.loads(self.state.read_text())
        if state['schema']!=1 or no_links(state['save_root'])!=self.external:raise ValueError('Unexpected shared-save root')
        return state
    def new_state(self):return dict(schema=1,save_root=str(self.external),active_vr=None)
    def recover_transaction(self):
        if not self.journal.exists():return
        job=json.loads(self.journal.read_text());folder=inside(job['folder'])
        if job['save_root']!=str(self.external) or not folder.is_relative_to(inside('backups/shared-saves')):raise ValueError('Untrusted save transaction')
        # Directory renames must stay on one volume. For installations on another
        # drive, stage next to Save/SlotN; retain the journal and verified backup
        # in the mod's data directory. Never fall back to a destructive copy.
        exchange=folder
        if job.get('external_exchange'):
            if len(folder.name)!=32 or any(c not in '0123456789abcdef' for c in folder.name):raise ValueError('Invalid exchange id')
            exchange=no_links(self.external/'.hp2vr-transactions'/folder.name)
        for slot,entry in job['slots'].items():
            if slot not in SLOTS:raise ValueError('Invalid slot')
            target=no_links(self.external/slot);desired=inside(folder/'desired'/slot);retired=no_links(exchange/'retired'/slot)
            if snapshot(desired)!=entry['desired']:raise ValueError('Changed transaction data')
            current=snapshot(target)
            if target.exists() and current==entry['desired']:continue
            if target.exists():
                if retired.exists() or current!=entry['before']:raise ValueError('Concurrent save change; both copies retained')
                retired.parent.mkdir(parents=True,exist_ok=True)
                # Move the complete old slot into the project backup. No deletion.
                os.replace(target,retired)
            elif entry['existed'] and not retired.exists():raise ValueError('Original slot disappeared outside transaction')
            # Keep the desired backup intact as well as the displaced original.
            staging=no_links(exchange/('install-'+slot))
            if staging.exists():
                try:staged_ok=snapshot(staging)==entry['desired']
                except ValueError:staged_ok=False
                if not staged_ok:
                    # An interrupted copy is archived; rebuild from verified desired.
                    os.replace(staging,no_links(exchange/('partial-'+slot+'-'+uuid.uuid4().hex)))
                    copy_snapshot(desired,staging,entry['desired'])
            else:copy_snapshot(desired,staging,entry['desired'])
            self.external.mkdir(parents=True,exist_ok=True);os.replace(staging,target)
            if snapshot(target)!=entry['desired']:raise ValueError('Installed save mismatch')
        atomic_json(self.state,job['next_state'])
        # Preserve the completed journal rather than deleting it.
        os.replace(self.journal,inside(folder/'completed.json'))
    def commit(self,changes,next_state):
        self.recover_transaction()
        if not changes:atomic_json(self.state,next_state);return
        folder=inside('backups/shared-saves/'+uuid.uuid4().hex);folder.mkdir(parents=True)
        slots={}
        for slot,source in changes.items():
            if slot not in SLOTS:raise ValueError('Invalid slot')
            target=no_links(self.external/slot);source=no_links(source)
            wanted=snapshot(source);before=snapshot(target)
            copy_snapshot(source,inside(folder/'desired'/slot),wanted)
            # All original contents, including unfinished files, are retained by
            # the directory move. Before mutation also copy the completed saves.
            copy_snapshot(target,inside(folder/'before'/slot),before)
            slots[slot]=dict(desired=wanted,before=before,existed=target.exists())
        atomic_json(self.journal,dict(save_root=str(self.external),folder=str(folder.relative_to(ROOT)),slots=slots,next_state=next_state,external_exchange=not same_volume(self.external,folder)))
        self.recover_transaction()
    def initialize(self,vr=None):
        self.recover_transaction()
        if self.state.exists():return self.read()
        state=self.new_state();changes={}
        if vr is None:
            # Fresh public install: existing vanilla slots are authoritative.
            all_slots(self.external);self.commit({},state);return state
        vr=inside(vr)
        # Initial migration: the user's confirmed current VR continuation is
        # newer than the original Flat slots. Preserve Flat-only slots as-is.
        for slot in SLOTS:
            source=vr/'Save'/slot
            if (source/'Save0.usa').is_file() and snapshot(source)!=snapshot(self.external/slot):changes[slot]=source
        state['initial_vr_profile']=str(vr.relative_to(ROOT));state['initial_slots']=list(changes)
        self.commit(changes,state);return state
    def stage(self,profile):
        profile=inside(profile)
        if not profile.is_relative_to(inside('cache')) or not (profile/'frontend.flag').is_file():raise ValueError('Not a staged menu profile')
        for slot in SLOTS:
            target=inside(profile/'Save'/slot)
            if snapshot(target):raise ValueError('Staging requires empty private save slots')
            copy_snapshot(self.external/slot,target,snapshot(self.external/slot))
    def begin(self,profile):
        self.recover();state=self.read();profile=inside(profile)
        if not profile.is_relative_to(inside('cache')) or not (profile/'frontend.flag').is_file():raise ValueError('Not a staged menu profile')
        baseline=all_slots(self.external)
        if all_slots(profile/'Save')!=baseline:raise ValueError('VR save staging differs from common saves')
        state['active_vr']=dict(profile=str(profile.relative_to(ROOT)),baseline=baseline)
        atomic_json(self.state,state)
    def finish(self,profile=None):
        self.recover_transaction();state=self.read();active=state.get('active_vr')
        if not active:return dict(synced_slots=[])
        source=inside(active['profile'])
        if profile is not None and inside(profile)!=source:raise ValueError('An older session cannot replace newer progress')
        if not source.is_relative_to(inside('cache')) or not (source/'frontend.flag').is_file():raise ValueError('Untrusted VR source')
        changes={};external=all_slots(self.external);current=all_slots(source/'Save')
        for slot in SLOTS:
            baseline=active['baseline'][slot]
            if current[slot]==baseline or current[slot]==external[slot]:continue
            if external[slot]!=baseline:raise ValueError('Both modes changed '+slot+' independently; both versions retained')
            changes[slot]=source/'Save'/slot
        state['active_vr']=None;state['last_vr_profile']=active['profile'];state['last_synced_slots']=list(changes)
        self.commit(changes,state);return dict(synced_slots=list(changes))
    def recover(self):
        self.recover_transaction()
        if self.state.exists() and self.read().get('active_vr'):return self.finish()
        return dict(synced_slots=[])

def main():
    p=argparse.ArgumentParser();group=p.add_mutually_exclusive_group(required=True)
    group.add_argument('--initialize',action='store_true');group.add_argument('--recover',action='store_true');group.add_argument('--begin');group.add_argument('--finish');a=p.parse_args()
    manager=SharedSaves()
    if a.initialize or a.recover:
        if not manager.state.exists() and not manager.journal.exists():
            pointer=inside('config/menu-session.json')
            old=json.loads(pointer.read_text()) if pointer.exists() else {}
            result=manager.initialize(old.get('profile'))
        else:result=manager.recover()
    elif a.begin:manager.begin(a.begin);result=dict(active_vr=a.begin)
    else:result=manager.finish(a.finish)
    print(json.dumps(result))
if __name__=='__main__':main()
