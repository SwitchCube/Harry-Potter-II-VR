"""Keep every stock save slot in a private, versioned menu-session profile."""
import argparse,json,os,uuid
from project_paths import read_trace_text,inside,write_text,ROOT
from stage_test_profile import stage_profile
STATE='config/menu-session.json'
def prepare(output,shared=False):
    state=inside(STATE);source=json.loads(inside('config/project.json').read_text(encoding='utf-8-sig'))['profile_directory']
    if state.exists():
        old=json.loads(state.read_text(encoding='utf-8'));source=inside(old['profile'])
        if not source.is_relative_to(inside('cache')) or not (source/'staging.json').is_file():raise ValueError('Untrusted session profile')
    elif inside('config/vr-session.json').exists():
        old=json.loads(inside('config/vr-session.json').read_text(encoding='utf-8'));candidate=inside(old['profile'])
        if candidate.is_relative_to(inside('cache')) and (candidate/'staging.json').is_file():source=candidate
    profile=stage_profile(source,output,'1280x960',include_saves=not shared);write_text(profile/'frontend.flag','Normal original front-end, no automatic loadgame override.\n')
    if shared:
        from shared_saves import SharedSaves
        SharedSaves().stage(profile)
    return profile

def activate(profile, trace):
    """Retain a real session from launch, including completed stock saves after a crash.

    The original game writes Save.tmp and then renames it to Save0.usa. Waiting
    for a clean process exit to retain the profile discarded these completed
    checkpoints. Activation keeps the previous profile and pointer as backups.
    Diagnostic sessions must never call this function.
    """
    profile=inside(profile);trace=inside(trace)
    if not profile.is_relative_to(inside('cache')) or not inside(profile/'frontend.flag').is_file() or not inside(profile/'staging.json').is_file():
        raise ValueError('Not a staged menu session')
    state=inside(STATE);previous=None
    if state.exists():
        previous=state.read_text(encoding='utf-8')
        write_text('backups/menu-session-'+uuid.uuid4().hex+'.json',previous)
    record=dict(schema=1,profile=profile.relative_to(ROOT).as_posix(),trace=trace.relative_to(ROOT).as_posix(),session_status='started')
    if previous:record['previous_profile']=json.loads(previous)['profile']
    temporary=inside(STATE+'.'+uuid.uuid4().hex+'.tmp');write_text(temporary,json.dumps(record,indent=2)+'\n')
    if state.exists() and state.stat().st_nlink!=1:raise ValueError('Hardlinked session pointer')
    os.replace(temporary,inside(state))
    return dict(activated=True,**record)

def publish(profile,trace):
    profile=inside(profile);trace=inside(trace)
    if not profile.is_relative_to(inside('cache')) or not (profile/'frontend.flag').is_file():raise ValueError('Not a staged menu session')
    events=[json.loads(x) for x in read_trace_text(trace).splitlines() if x.strip()]
    summaries=[e for e in events if e.get('event')=='summary']
    if len(summaries)!=1 or not summaries[0]['profile_redirected'] or summaries[0]['game_exit_code'] or summaries[0]['forced_termination']:raise ValueError('Unclean menu session')
    # A visit to the menu without launching a game must not replace a played session.
    if not (profile/'hp2vr-native.jsonl').is_file():return {'published':False,'reason':'menu_only'}
    state=inside(STATE)
    if state.exists() and inside(json.loads(state.read_text(encoding='utf-8'))['profile'])!=profile:
        return {'published':False,'reason':'superseded_session'}
    if state.exists():write_text('backups/menu-session-'+uuid.uuid4().hex+'.json',state.read_text(encoding='utf-8'))
    record=dict(schema=1,profile=profile.relative_to(ROOT).as_posix(),trace=trace.relative_to(ROOT).as_posix())
    temporary=inside(STATE+'.'+uuid.uuid4().hex+'.tmp');write_text(temporary,json.dumps(record,indent=2)+'\n');inside(state)
    if state.exists() and state.stat().st_nlink!=1:raise ValueError('Hardlinked session pointer')
    os.replace(temporary,state);return dict(published=True,**record)
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--prepare');p.add_argument('--publish');p.add_argument('--activate');p.add_argument('--trace');p.add_argument('--shared',action='store_true');a=p.parse_args()
    print(prepare(a.prepare,a.shared) if a.prepare else json.dumps(activate(a.activate,a.trace) if a.activate else publish(a.publish,a.trace)))
