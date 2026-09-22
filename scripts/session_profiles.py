"""Versioned VR sessions: copy prior profiles, publish only clean successful runs."""
import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
import uuid
from project_paths import read_trace_text,ROOT, inside, write_text
from stage_test_profile import stage_profile


def state_file(path):
    p=inside(path)
    if p!=inside('config/vr-session.json') and not p.is_relative_to(inside('cache/tests')):
        raise ValueError('Session state must be config/vr-session.json or a cache/tests fixture')
    if p.exists() and p.stat().st_nlink!=1:
        raise ValueError('Hardlinked session state')
    return p


def profile_path(path):
    p=inside(path)
    if not p.is_relative_to(inside('cache')) or not inside(p/'staging.json').is_file():
        raise ValueError('Session source is not an isolated staged profile')
    return p


def sha(path):
    return hashlib.sha256(inside(path).read_bytes()).hexdigest()


def prepare(destination,resolution='2048x2048',state='config/vr-session.json',fresh=False):
    state=state_file(state)
    config=json.loads(inside('config/project.json').read_text(encoding='utf-8-sig'))
    source=config['profile_directory'];previous=None
    if state.exists() and not fresh:
        previous=json.loads(state.read_text(encoding='utf-8'))
        if previous.get('schema')!=1:raise ValueError('Unknown session state')
        source=profile_path(previous['profile'])
        if previous['has_checkpoint'] and sha(source/'Save/Slot1/Save0.usa')!=previous['checkpoint_sha256']:
            raise ValueError('Previous VR checkpoint changed; preserve and inspect it before resuming')
    dest=stage_profile(source,destination,resolution)
    if previous and previous['has_checkpoint']:
        write_text(dest/'resume-save.flag','Load the verified private VR checkpoint using original LoadGame 0.\n')
    write_text(dest/'session-parent.json',json.dumps(dict(state=str(state.relative_to(ROOT)),previous=previous,fresh=fresh),indent=2)+'\n')
    return dest


def publish(run_path,state='config/vr-session.json'):
    state=state_file(state);run_path=inside(run_path)
    run=json.loads(run_path.read_text(encoding='utf-8-sig'))
    if run['exit_code'] or run['external_profile_changed'] or run['external_profile_added']:
        raise ValueError('Failed or externally modified session is not promoted')
    if state==inside('config/vr-session.json') and (run['mode']!='native-vr' or not run['manual_exit']):
        raise ValueError('Only normal manual VR sessions can replace the user session')
    profile=profile_path(run['profile'])
    trace=[json.loads(x) for x in read_trace_text(run['trace']).splitlines() if x.strip()]
    summaries=[x for x in trace if x.get('event')=='summary']
    if len(summaries)!=1 or summaries[0]['game_exit_code'] or summaries[0]['forced_termination'] or not summaries[0]['profile_redirected']:
        raise ValueError('Unclean original process exit')
    if run.get('resume_requested'):
        native=[json.loads(x) for x in inside(profile/'hp2vr-native.jsonl').read_text().splitlines() if x.strip()]
        if not any(x.get('event')=='original_command' and x.get('command')=='loadgame 0' and x.get('handled') for x in native) or not any(x.get('event')=='level_transition_end' and x.get('loaded') for x in native):
            raise ValueError('Requested checkpoint did not load')
    save=inside(profile/'Save/Slot1/Save0.usa')
    digest=sha(save) if save.is_file() else None
    staging=json.loads(inside(profile/'staging.json').read_text(encoding='utf-8'))
    initial=next((f['staged_sha256'] for f in staging['files'] if f['relative_path'].lower()=='save/slot1/save0.usa'),None)
    checkpoint=bool(digest and save.stat().st_size>=1024 and (digest!=initial or (profile/'resume-save.flag').exists()))
    record=dict(schema=1,date_utc=datetime.now(timezone.utc).isoformat(),profile=profile.relative_to(ROOT).as_posix(),run=run_path.relative_to(ROOT).as_posix(),has_checkpoint=checkpoint,checkpoint_sha256=digest if checkpoint else None)
    if state.exists():
        before=state.read_text(encoding='utf-8')
        write_text('backups/session-state-'+uuid.uuid4().hex+'.json',before)
    temp=inside(state.parent/(state.name+'.'+uuid.uuid4().hex+'.tmp'))
    write_text(temp,json.dumps(record,indent=2)+'\n')
    os.replace(temp,state_file(state))
    return record


def main():
    p=argparse.ArgumentParser();p.add_argument('--run',required=True);p.add_argument('--state',default='config/vr-session.json')
    args=p.parse_args();print(json.dumps(publish(args.run,args.state)))


if __name__=='__main__':main()
