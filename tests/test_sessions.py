import json
import sys
import unittest
import uuid
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'scripts'))
from project_paths import inside,write_text
from stage_test_profile import stage_profile
from session_profiles import publish,prepare


class SessionProfiles(unittest.TestCase):
    def setUp(self):
        self.base=inside('cache/tests/sessions-'+uuid.uuid4().hex)
        self.source=self.base/'source';(self.source/'Save/Slot1').mkdir(parents=True)
        write_text(self.source/'Game.ini','SavePath='+str(self.source)+'\\Save\nStartupFullscreen=True\n')
        (self.source/'Save/Slot1/Save0.usa').write_bytes(b'original'+b'a'*1500)
        self.profile=stage_profile(self.source,self.base/'first')
        self.trace=self.base/'trace.jsonl'
        write_text(self.trace,json.dumps(dict(event='summary',game_exit_code=0,forced_termination=False,profile_redirected=True)))
        self.run=self.base/'run.json';self.state=self.base/'session.json'
        self.record=dict(exit_code=0,external_profile_changed=[],external_profile_added=[],mode='native-replay',manual_exit=False,profile=str(self.profile),trace=str(self.trace))
        write_text(self.run,json.dumps(self.record))

    def save(self):
        (self.profile/'Save/Slot1/Save0.usa').write_bytes(b'new checkpoint'+b'b'*1600)

    def test_windows_powershell_utf16_trace_preserves_checkpoint(self):
        text=self.trace.read_text(encoding='utf-8')
        self.trace.write_bytes(text.encode('utf-16'))
        self.save()
        self.assertTrue(publish(self.run,self.state)['has_checkpoint'])
        dest=prepare(self.base/'next',state=self.state)
        self.assertTrue((dest/'resume-save.flag').is_file())

    def test_unmodified_import_is_not_a_vr_checkpoint(self):
        self.assertFalse(publish(self.run,self.state)['has_checkpoint'])
        dest=prepare(self.base/'next',state=self.state)
        self.assertFalse((dest/'resume-save.flag').exists())

    def test_checkpoint_copied_and_previous_session_preserved(self):
        self.save();self.assertTrue(publish(self.run,self.state)['has_checkpoint'])
        before=(self.profile/'Save/Slot1/Save0.usa').read_bytes()
        dest=prepare(self.base/'next',state=self.state)
        self.assertTrue((dest/'resume-save.flag').is_file())
        self.assertEqual((dest/'Save/Slot1/Save0.usa').read_bytes(),before)
        (dest/'Save/Slot1/Save0.usa').write_bytes(b'changed next session')
        self.assertEqual((self.profile/'Save/Slot1/Save0.usa').read_bytes(),before)
        self.assertIn('original',(self.source/'Save/Slot1/Save0.usa').read_text())
        self.assertIn(str(dest),(dest/'Game.ini').read_text())

    def test_failed_run_does_not_replace_known_checkpoint(self):
        self.save();publish(self.run,self.state);before=self.state.read_bytes()
        self.record['exit_code']=6;write_text(self.run,json.dumps(self.record),exclusive=False)
        with self.assertRaises(ValueError):publish(self.run,self.state)
        self.assertEqual(self.state.read_bytes(),before)

    def test_changed_checkpoint_is_refused_before_staging(self):
        self.save();publish(self.run,self.state)
        (self.profile/'Save/Slot1/Save0.usa').write_bytes(b'corrupt')
        with self.assertRaises(ValueError):prepare(self.base/'next',state=self.state)
        self.assertFalse((self.base/'next').exists())

    def test_bad_state_path_and_test_promotion_refused(self):
        with self.assertRaises(ValueError):publish(self.run,'config/project.json')
        with self.assertRaises(ValueError):publish(self.run)


if __name__=='__main__':unittest.main()
