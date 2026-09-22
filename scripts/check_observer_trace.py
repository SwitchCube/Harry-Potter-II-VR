"""Validate actual run evidence; reject absent, reordered or mismatched camera data."""
import argparse
import json
import math
from project_paths import inside, write_text


def check_trace(events, shift):
    def vector(event):
        values = event['position']
        if len(values) != 3 or not all(math.isfinite(v) for v in values):
            raise ValueError('Invalid camera position')
        return values
    summary = next(e for e in events if e['event'] == 'summary')
    access = next(e for e in events if e['event'] == 'camera_access_summary')
    if not summary['profile_redirected'] or summary['game_exit_code'] != 0 or summary['forced_termination']:
        raise ValueError('Game did not finish cleanly with redirected profile')
    calls = summary['camera_calls']
    if calls <= 0 or summary['tick_calls'] != calls or summary['draw_world_calls'] != calls:
        raise ValueError('Unexpected simulation/camera/draw call totals')
    if access['shift_x'] != shift or access['camera_writes'] != (calls if shift else 0):
        raise ValueError('Unexpected camera write count')
    if access['draw_origin_verified'] != calls or access['master_occlusion_origin_verified'] != calls:
        raise ValueError('Not all master cameras reached verified visibility')
    samples, pending, tested = 0, None, set()
    for event in events:
        if event['event'] == 'camera':
            vector(event)
            if pending and tested != {'DrawWorld', 'OccludeFrame'}:
                raise ValueError('Incomplete sampled frame')
            pending, tested = event, set()
        elif event['event'] == 'camera_input_verified':
            if not all(math.isfinite(event[k]) for k in ('applied_x','original_x')):
                raise ValueError('Non-finite camera input')
            if pending is None or event['frame'] != pending['frame'] or not event['caller_stack_copy']:
                raise ValueError('Camera has no verified caller stack copy')
            if abs(event['applied_x'] - event['original_x'] - shift) > 0.001:
                raise ValueError('Wrong sampled camera offset')
            if abs(pending['position'][0] - event['applied_x']) > 0.001:
                raise ValueError('Camera sample differs from verified input')
            samples += 1
        elif event['event'] == 'camera_at_render_verified':
            if not pending or event['frame'] != pending['frame'] or event['tick'] != pending['tick']:
                raise ValueError('Render evidence has a different frame/tick')
            if any(abs(a-b) > 0.001 for a,b in zip(vector(event),vector(pending))):
                raise ValueError('Camera did not reach render/visibility')
            if event['label'] in tested or (event['label'] == 'OccludeFrame' and 'DrawWorld' not in tested):
                raise ValueError('Unexpected render evidence order')
            tested.add(event['label'])
    if samples == 0 or tested != {'DrawWorld', 'OccludeFrame'}:
        raise ValueError('Missing complete sampled camera evidence')
    return dict(status='passed', sampled_cameras=samples, total_cameras=calls,
                shift_world_x_units=shift, master_draws=access['draw_origin_verified'],
                master_occlusions=access['master_occlusion_origin_verified'], stereo=False,
                limits='Single-view camera access only; not a performance or headset test')


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--run', required=True)
    parser.add_argument('--output', required=True)
    args = parser.parse_args()
    run = json.loads(inside(args.run).read_text(encoding='utf-8-sig'))
    if run['observer_exit_code'] or run['external_profile_changed'] or run['external_profile_added']:
        raise ValueError('Run failed or changed the external profile')
    events = [json.loads(s) for s in inside(run['trace']).read_text(encoding='utf-8-sig').splitlines()]
    result = check_trace(events, 8 if run['camera_mode'] == 'shift-x8' else 0)
    result['run'] = str(inside(args.run))
    write_text(args.output, json.dumps(result, indent=2)+'\n')
    print(json.dumps(result))


if __name__ == '__main__':
    main()
