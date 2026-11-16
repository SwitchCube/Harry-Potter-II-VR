"""Private original-engine replays for the local VR settings/tutorial revision."""
import argparse
import hashlib
import json
import os
import subprocess
import sys
import uuid
from datetime import datetime
from pathlib import Path
from project_paths import ROOT, inside, walk_files
from stage_test_profile import stage_profile


def snapshot(path):
    return {str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in walk_files(path)}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--build', required=True)
    parser.add_argument('--fixture', choices=('settings', 'tutorial', 'startup'), required=True)
    parser.add_argument('--resolution', choices=('1280x1280', '1536x1536', '2048x2048'), default='1280x1280')
    args = parser.parse_args()
    build = json.loads(inside(args.build).joinpath('build.json').read_text(encoding='utf-8-sig'))
    config = json.loads(inside('config/project.json').read_text(encoding='utf-8-sig'))
    source = Path(config['profile_directory'])
    before = snapshot(source)
    stamp = datetime.now().strftime('%Y%m%d-%H%M%S') + '-' + uuid.uuid4().hex[:6]
    profile = inside('cache/vr-' + args.fixture + '-' + stamp)
    stage_profile(source, profile, args.resolution, include_saves=False)
    subprocess.run([sys.executable, "-B", "scripts/stage_vr_bindings.py", "--profile", str(profile)], cwd=ROOT, check=True)
    (profile / 'config').mkdir()
    (profile / ('replay-preferences-startup.flag' if args.fixture == 'startup' else 'replay-vr-' + args.fixture + '.flag')).write_text('Private engine integration replay\n')
    # Stable starting preferences, independent of the user's current choices.
    (profile / 'config/vr-preferences.ini').write_text('[VR]\nLanguage=2\nRenderSize=1024\n' if args.fixture == 'startup' else '[VR]\nLanguage=1\nRenderSize=2048\n')
    (profile / 'config/vr-presentation.ini').write_text('[Presentation]\nSpatialScenes=1\n')
    env = os.environ.copy()
    env.update(TEMP=str(inside('cache/tmp')), TMP=str(inside('cache/tmp')), HP2VR_LANGUAGE='de')
    stdout = inside('logs/vr-' + args.fixture + '-' + stamp + '.stdout.jsonl')
    stderr = stdout.with_suffix('.stderr.txt')
    with stdout.open('wb') as out, stderr.open('wb') as err:
        result = subprocess.run([build['launcher'], str(inside(config['game_executable'])), str(profile),
                                 ('25' if args.fixture == 'startup' else '50'), 'Entryhall_hub.unr', 'native-input-replay'],
                                cwd=ROOT, env=env, stdout=out, stderr=err, timeout=100)
    events = [json.loads(line) for line in (profile / 'hp2vr-native.jsonl').read_text().splitlines() if line.startswith('{')]
    completed = [e for e in events if e.get('event') == 'hp2_' + args.fixture + '_test_complete']
    if args.fixture == 'startup':
        size = {(e['width'], e['height']) for e in events if e.get('event') == 'gpu_texture_verified'}
        applied = [e for e in events if e.get('event') == 'original_command' and e.get('command') == 'setres 1024x1024w' and e.get('handled')]
        completed = [dict(next_launch_render_size=1024, engine_command=applied, gpu_sizes=sorted(size))] if applied and (1024, 1024) in size and any(e.get('event') == 'mod_preferences_loaded' and e['english'] and e['language_choice'] == 2 for e in events) else []
    report = dict(fixture=args.fixture, build=args.build, resolution=args.resolution, profile=str(profile.relative_to(ROOT)),
                  exit_code=result.returncode, completed=completed, real_profile_unchanged=before == snapshot(source),
                  fatal=[e for e in events if e.get('event') == 'fatal'], headset_tested=False)
    report['passed'] = result.returncode == 0 and bool(completed) and report['real_profile_unchanged'] and not report['fatal']
    path = stdout.with_suffix('.report.json')
    path.write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report, indent=2), flush=True)
    if not report['passed']:
        raise SystemExit(1)


if __name__ == '__main__':
    main()
