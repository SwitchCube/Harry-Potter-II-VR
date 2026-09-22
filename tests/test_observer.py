import copy
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import sys
import unittest
import uuid
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).absolute().parent.parent / 'scripts'))
from project_paths import ROOT, inside
from inspect_pe import PE
from analyze_original import follow_jumps, verify_originals
from stage_test_profile import stage_profile
from check_observer_trace import check_trace
from test_diagnostics import minimal_pe


class ObserverAnalysis(unittest.TestCase):
    def test_export_jump_and_cycle(self):
        data = minimal_pe(32)
        data[0x200:0x205] = b'\xe9' + struct.pack('<i', 11)
        self.assertEqual(follow_jumps(PE(data),0x1000), (0x1010,[0x1000,0x1010]))
        data[0x210:0x215] = b'\xe9' + struct.pack('<i', -21)
        with self.assertRaisesRegex(ValueError,'cycle'):
            follow_jumps(PE(data),0x1000)

    def test_altered_fingerprint_aborts(self):
        original = Path.read_bytes
        def altered(path):
            data = original(path)
            return data + b'changed fixture' if path.name.lower() == 'game.exe' else data
        with patch.object(Path, 'read_bytes', altered), self.assertRaisesRegex(ValueError,'Unknown original'):
            verify_originals()

    def test_generated_descriptor_matches_reviewed_originals(self):
        result = subprocess.run([sys.executable,'-B',str(ROOT/'scripts/prepare_observer.py'),'--check'],
                                capture_output=True,text=True)
        self.assertEqual(result.returncode,0,result.stderr)


class ProfileStaging(unittest.TestCase):
    def setUp(self):
        self.base = inside('cache/tests/observer-'+uuid.uuid4().hex)
        self.source = self.base/'source profile'
        (self.source/'Save/Slot1').mkdir(parents=True)
        (self.source/'Game.ini').write_bytes(('SavePath='+str(self.source)+'\\Save\r\nStartupFullscreen=True\r\n').encode('cp1252'))
        (self.source/'Running.ini').write_bytes(b'unclean exit marker')
        (self.source/'Game.log').write_bytes(b'not copied')
        (self.source/'Save/Slot1/save.usa').write_bytes(b'original save fixture')
        self.before = {p: p.read_bytes() for p in self.source.rglob('*') if p.is_file()}

    def test_isolated_profile_and_save_preserved(self):
        dest = stage_profile(self.source,self.base/'staged profile')
        text = (dest/'Game.ini').read_bytes().decode('cp1252')
        self.assertIn(str(dest)+'\\Save',text)
        self.assertIn('StartupFullscreen=False',text)
        self.assertFalse((dest/'Running.ini').exists())
        self.assertFalse((dest/'Game.log').exists())
        self.assertEqual((dest/'Save/Slot1/save.usa').read_bytes(),b'original save fixture')
        for path,data in self.before.items():
            self.assertEqual(path.read_bytes(),data)
        (dest/'Save/Slot1/save.usa').write_bytes(b'test progress')
        self.assertEqual((self.source/'Save/Slot1/save.usa').read_bytes(),b'original save fixture')

    def test_existing_destination_and_external_destination_refused(self):
        with self.assertRaises(ValueError):
            stage_profile(self.source,self.source)
        with self.assertRaises(ValueError):
            stage_profile(self.source,ROOT.parent/'invalid-profile-fixture')
        self.assertFalse((ROOT.parent/'invalid-profile-fixture').exists())


class RecordedEvidence(unittest.TestCase):
    @staticmethod
    def events(shift):
        position = [10+shift,20,30]
        return [
            dict(event='camera',frame=100,tick=1,position=position),
            dict(event='camera_input_verified',frame=100,caller_stack_copy=True,original_x=10,applied_x=10+shift),
            dict(event='camera_at_render_verified',label='DrawWorld',frame=100,tick=1,position=position),
            dict(event='camera_at_render_verified',label='OccludeFrame',frame=100,tick=1,position=position),
            dict(event='summary',profile_redirected=True,game_exit_code=0,forced_termination=False,
                 camera_calls=1,tick_calls=1,draw_world_calls=1),
            dict(event='camera_access_summary',shift_x=shift,camera_writes=1 if shift else 0,
                 draw_origin_verified=1,master_occlusion_origin_verified=1)]

    def test_both_modes_accept_consistent_evidence(self):
        for shift in (0,8):
            self.assertEqual(check_trace(self.events(shift),shift)['total_cameras'],1)

    def test_missing_occlusion_rejected(self):
        events = self.events(8)
        del events[3]
        with self.assertRaises(ValueError):
            check_trace(events,8)

    def test_changed_origin_or_second_tick_rejected(self):
        for key,value in [('position',[100,20,30]),('tick',2)]:
            events = copy.deepcopy(self.events(8))
            events[3][key] = value
            with self.assertRaises(ValueError):
                check_trace(events,8)

    def test_missing_axis_or_nonfinite_position_rejected(self):
        for position in ([],[10,20],[float('nan'),20,30],[float('inf'),20,30]):
            events = copy.deepcopy(self.events(8))
            events[3]['position'] = position
            with self.assertRaises(ValueError):
                check_trace(events,8)


if __name__ == '__main__':
    unittest.main(verbosity=2)
