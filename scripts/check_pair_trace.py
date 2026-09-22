"""Check the bounded two-view experiment, including actual BMP pixels.

Passing trace checks is not proof that every native animation/render cache is frozen.
Control-image differences are reported without hiding or accepting them as VR-ready.
"""
import argparse
import hashlib
import json
import math
import struct
from project_paths import inside, write_text


def read_bitmap(data):
    if len(data) < 54 or data[:2] != b'BM':
        raise ValueError('Not a complete BMP')
    size, _, _, offset = struct.unpack_from('<IHHI', data, 2)
    header, width, height, planes, bits, compression, image_size = struct.unpack_from('<IiiHHII', data, 14)
    if (size != len(data) or offset != 54 or header != 40 or planes != 1 or bits != 24
            or compression != 0 or not 64 <= width <= 2048 or not -2048 <= height <= -64):
        raise ValueError('Unexpected diagnostic BMP format')
    stride = (width * 3 + 3) & ~3
    if image_size != stride * -height or len(data) != 54 + image_size:
        raise ValueError('Truncated or oversized BMP pixels')
    pixels = b''.join(data[54+y*stride:54+y*stride+width*3] for y in range(-height))
    return width, -height, pixels


def compare_bitmaps(a, b):
    width, height, pa = read_bitmap(a)
    wb, hb, pb = read_bitmap(b)
    if (width, height) != (wb, hb):
        raise ValueError('Different view dimensions')
    if min(len(set(pa)), len(set(pb))) < 16:
        raise ValueError('Capture lacks useful scene content')
    changed = sum(pa[i:i+3] != pb[i:i+3] for i in range(0, len(pa), 3))
    return dict(width=width, height=height, identical=pa == pb,
                changed_pixels=changed, changed_pixel_fraction=changed/(width*height),
                mean_absolute_channel_difference=sum(abs(x-y) for x,y in zip(pa,pb))/len(pa),
                image_a_sha256=hashlib.sha256(a).hexdigest(), image_b_sha256=hashlib.sha256(b).hexdigest())


def check_pair(events, separation):
    def only(name):
        rows = [e for e in events if e['event'] == name]
        if len(rows) != 1:
            raise ValueError('Missing or duplicate event: '+name)
        return rows[0]
    begin, second, visible, done = [only(n) for n in (
        'pair_begin', 'pair_second_draw', 'pair_second_visibility_verified', 'pair_complete')]
    summary, access = only('summary'), only('camera_access_summary')
    if not summary['profile_redirected'] or summary['game_exit_code'] or summary['forced_termination']:
        raise ValueError('Unclean or unisolated game exit')
    if (summary['camera_calls'] != summary['tick_calls']+1 or
            summary['draw_world_calls'] != summary['camera_calls'] or
            access['draw_origin_verified'] != summary['camera_calls'] or
            access['master_occlusion_origin_verified'] != summary['camera_calls']):
        raise ValueError('Not exactly one extra verified world view')
    tick = begin['tick']
    if any(e.get('tick', tick) != tick for e in events if e['event'].startswith('pair_')):
        raise ValueError('Pair crossed a simulation tick')
    if (begin['frame_a'] == second['frame_b'] or not second['independent_frame'] or
            visible['frame_b'] != second['frame_b']):
        raise ValueError('Second frame/visibility is not independent')
    vectors = [begin['position_a'], begin['position_b'], visible['position']]
    if any(len(v) != 3 or not all(math.isfinite(n) for n in v) for v in vectors):
        raise ValueError('Invalid camera vectors')
    distance = math.dist(vectors[0], vectors[1])
    if (begin['separation_units'] != separation or abs(distance-separation) > 0.002
            or math.dist(vectors[1], vectors[2]) > 0.001):
        raise ValueError('Wrong camera separation or visibility camera')
    if done['extra_ticks'] or done['master_depth_restored'] != 1 or not done['player_pose_unchanged']:
        raise ValueError('Tick, master lifetime or player pose changed')
    if done['secondary_optional_callbacks_skipped'] != 1:
        raise ValueError('Optional outer callback was not skipped once')
    calls = [e for e in events if e['event'] == 'pair_call']
    if [e['stage'] for e in calls] != list(range(1, 16)):
        raise ValueError('Incomplete/duplicate native call sequence')
    images = [e for e in events if e['event'] == 'pair_image']
    if [e['file'] for e in images] != ['view-a.bmp', 'view-b.bmp']:
        raise ValueError('Two distinct image captures missing')
    positions = [events.index(e) for e in (begin, calls[0], images[0], second, visible, images[1], done, summary)]
    if positions != sorted(set(positions)):
        raise ValueError('Pair evidence out of order')
    primary, secondary = [], []
    for event in events:
        if event['event'] == 'pair_script_dispatch':
            (secondary if event['secondary'] else primary).append(event)
    seen = {(e['object'], e['function'], e['name'], e['wrapper_caller']) for e in primary}
    for event in secondary:
        key = (event['object'], event['function'], event['name'], event['wrapper_caller'])
        if (key not in seen or event['name'] not in ('Update', 'RenderOverlays')
                or event['parameter_bytes'] != 4 or event['return_offset'] != 65535):
            raise ValueError('Unreviewed or newly visible secondary script event')
    if (done['primary_script_calls'] != len(primary) or
            done['secondary_script_calls_suppressed'] != len(secondary) or
            done['secondary_update_calls_suppressed'] != sum(e['name']=='Update' for e in secondary)):
        raise ValueError('Script dispatch counts disagree')
    return dict(trace_verified=True, tick=tick, frame_a=begin['frame_a'], frame_b=second['frame_b'],
                separation_units=distance, secondary_script_calls_suppressed=len(secondary),
                extra_ticks=0, player_pose_unchanged=True, total_ticks=summary['tick_calls'])


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--run', required=True)
    parser.add_argument('--output', required=True)
    args = parser.parse_args()
    run = json.loads(inside(args.run).read_text(encoding='utf-8-sig'))
    if run['observer_exit_code'] or run['external_profile_changed'] or run['external_profile_added']:
        raise ValueError('Failed run or external profile changes')
    if run['camera_mode'] not in ('pair-control', 'pair-capture'):
        raise ValueError('Not a pair experiment')
    events = [json.loads(line) for line in inside(run['trace']).read_text(encoding='utf-8-sig').splitlines()]
    separation = 0 if run['camera_mode'] == 'pair-control' else 8
    report = check_pair(events, separation)
    profile = inside(run['profile'])
    report['pixels'] = compare_bitmaps(inside(profile/'view-a.bmp').read_bytes(), inside(profile/'view-b.bmp').read_bytes())
    if separation and report['pixels']['identical']:
        raise ValueError('Offset camera produced no pixel change')
    for image in (e for e in events if e['event'] == 'pair_image'):
        if (image['width'],image['height']) != (report['pixels']['width'],report['pixels']['height']):
            raise ValueError('Pixel dimensions differ from trace')
    report.update(run=args.run, mode=run['camera_mode'], map=run['map'], headset_test=False,
                  state='two_view_probe_verified', complete_simulation_freeze_proven=False,
                  limitation='Same Tick and unchanged player pose; native render/animation caches not exhaustively frozen. No VR projection or texture submission.')
    write_text(args.output, json.dumps(report, indent=2)+'\n')
    print(json.dumps(report))


if __name__ == '__main__':
    main()
