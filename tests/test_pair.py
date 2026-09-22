import struct
import subprocess
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).absolute().parent.parent/'scripts'))
from check_pair_trace import check_pair, compare_bitmaps, read_bitmap
from project_paths import ROOT


class PairEvidence(unittest.TestCase):
    @staticmethod
    def fixture():
        events = [dict(event='pair_begin', tick=5, frame_a=100, separation_units=8,
                       position_a=[0,0,0], position_b=[8,0,0])]
        script = dict(event='pair_script_dispatch', secondary=False, object=400, function=500,
                      name='Update', wrapper_caller=600, parameter_bytes=4, return_offset=65535)
        events.append(script)
        for stage in range(1,16):
            events.append(dict(event='pair_call', stage=stage, tick=5))
            if stage in (3,10):
                events.append(dict(event='pair_image', file='view-a.bmp' if stage==3 else 'view-b.bmp', tick=5))
            if stage==7:
                events.append(dict(event='pair_second_draw', frame_b=200, tick=5, independent_frame=True))
            if stage==8:
                events += [dict(script, secondary=True), dict(event='pair_second_visibility_verified', frame_b=200, tick=5, position=[8,0,0])]
        events += [dict(event='pair_complete', tick=5, extra_ticks=0, master_depth_restored=1,
                        player_pose_unchanged=True, primary_script_calls=1, secondary_script_calls_suppressed=1,
                        secondary_update_calls_suppressed=1, secondary_optional_callbacks_skipped=1),
                   dict(event='summary', profile_redirected=True, game_exit_code=0, forced_termination=False,
                        tick_calls=6, camera_calls=7, draw_world_calls=7),
                   dict(event='camera_access_summary', draw_origin_verified=7, master_occlusion_origin_verified=7)]
        return events

    def test_recorded_sequence_shape_and_later_normal_ticks(self):
        self.assertEqual(check_pair(self.fixture(),8)['extra_ticks'],0)

    def test_extra_tick_or_reused_frame_rejected(self):
        for field,value in [('tick',6),('frame_b',100)]:
            events=self.fixture()
            next(e for e in events if e['event']=='pair_second_draw')[field]=value
            with self.assertRaises(ValueError): check_pair(events,8)

    def test_unseen_or_nonvoid_script_rejected(self):
        for field,value in [('object',401),('return_offset',0),('name','Cast')]:
            events=self.fixture()
            next(e for e in events if e['event']=='pair_script_dispatch' and e['secondary'])[field]=value
            with self.assertRaises(ValueError): check_pair(events,8)

    def test_missing_visibility_or_native_call_rejected(self):
        for name in ('pair_second_visibility_verified','pair_call'):
            events=self.fixture()
            events.remove(next(e for e in events if e['event']==name))
            with self.assertRaises(ValueError): check_pair(events,8)

    def test_wrong_or_nonfinite_camera_rejected(self):
        for position in ([9,0,0],[float('nan'),0,0],[]):
            events=self.fixture()
            next(e for e in events if e['event']=='pair_second_visibility_verified')['position']=position
            with self.assertRaises(ValueError): check_pair(events,8)

    def test_pair_descriptors_match_originals(self):
        result=subprocess.run([sys.executable,'-B',str(ROOT/'scripts/prepare_pair_probe.py'),'--check'],capture_output=True,text=True)
        self.assertEqual(result.returncode,0,result.stderr)


class BitmapEvidence(unittest.TestCase):
    @staticmethod
    def bitmap():
        pixels=bytes(range(256))*48
        return (struct.pack('<2sIHHI',b'BM',54+len(pixels),0,0,54)+
                struct.pack('<IiiHHIIiiII',40,64,-64,1,24,0,len(pixels),0,0,0,0)+pixels)

    def test_pixel_counts_and_exact_control(self):
        a=self.bitmap(); b=bytearray(a); b[54]^=255
        self.assertTrue(compare_bitmaps(a,a)['identical'])
        self.assertEqual(compare_bitmaps(a,b)['changed_pixels'],1)

    def test_truncation_bad_dimensions_or_compression_rejected(self):
        a=self.bitmap()
        cases=[a[:-1],a+b'x']
        for offset,value in ((22,64),(30,1),(18,0)):
            b=bytearray(a); struct.pack_into('<i',b,offset,value); cases.append(b)
        for data in cases:
            with self.assertRaises(ValueError): read_bitmap(data)


if __name__=='__main__': unittest.main(verbosity=2)
