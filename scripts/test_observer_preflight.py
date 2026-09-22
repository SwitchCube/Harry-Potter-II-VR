"""Negative native tests that must abort before creating any game process."""
import argparse
import hashlib
import json
import subprocess
import uuid
from project_paths import ROOT, inside, write_text


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--build',required=True)
    parser.add_argument('--output',required=True)
    args = parser.parse_args()
    build = json.loads(inside(args.build+'/build.json').read_text(encoding='utf-8-sig'))
    observer = inside(build['executable'])
    if hashlib.sha256(observer.read_bytes()).hexdigest().lower() != build['executable_sha256'].lower():
        raise ValueError('Observer executable changed')
    fixture = inside('cache/tests/native-preflight-'+uuid.uuid4().hex)
    profile = inside(fixture/'cache/profile')
    profile.mkdir(parents=True)
    rows = []
    def refused(name, executable, game, target_profile, duration, map_name, expected, mode='observe'):
        result = subprocess.run([str(executable),str(game),str(target_profile),duration,map_name,mode],
                                capture_output=True,text=True,timeout=10,cwd=ROOT)
        if result.returncode != 10 or '"event":"launched"' in result.stdout or expected not in result.stderr:
            raise ValueError('Native preflight did not refuse '+name+': '+result.stdout+result.stderr)
        rows.append(dict(test=name,status='passed',exit_code=result.returncode,before_game_creation=True))
    game = inside('system/Game.exe')
    refused('profile outside cache',observer,game,ROOT/'docs','5','Entry.unr','Profile must be under project cache')
    refused('unreviewed map',observer,game,profile,'5','Unknown.unr','Unreviewed map argument')
    refused('unbounded duration',observer,game,profile,'999','Entry.unr','Invalid bounded duration')
    refused('pair without warmup',observer,game,profile,'5','Entry.unr','Pair mode requires at least 20 seconds','pair-control')
    refused('unknown camera mode',observer,game,profile,'5','Entry.unr','Unknown camera test mode','invalid')
    refused('different executable name',observer,ROOT/'system/Other.exe',profile,'5','Entry.unr','Game must be system/Game.exe')
    fake_observer = inside(fixture/'build/probe/hp2vr-engine-observer.exe')
    fake_game = inside(fixture/'system/Game.exe')
    fake_observer.parent.mkdir(parents=True)
    fake_game.parent.mkdir(parents=True)
    with fake_observer.open('xb') as handle:
        handle.write(observer.read_bytes())
    with fake_game.open('xb') as handle:
        handle.write(b'Altered target fixture; intentionally not a runnable game')
    refused('unknown game fingerprint',fake_observer,fake_game,profile,'5','Entry.unr','Original fingerprint mismatch')
    report = dict(tests=rows,originals_modified=False,game_processes_started=0,build=args.build)
    write_text(args.output,json.dumps(report,indent=2)+'\n')
    print(json.dumps(report))


if __name__ == '__main__':
    main()
