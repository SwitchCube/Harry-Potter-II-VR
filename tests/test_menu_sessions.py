import json,sys,unittest,uuid
from pathlib import Path
from unittest.mock import patch
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'scripts'))
import menu_sessions
from stage_test_profile import stage_profile
from project_paths import inside,write_text

class MenuContinuation(unittest.TestCase):
 def setUp(self):
  self.root=inside('cache/tests/menu-retain-'+uuid.uuid4().hex)
  source=self.root/'source';(source/'Save/Slot1').mkdir(parents=True)
  write_text(source/'Game.ini','SavePath='+str(source)+'\\Save\\Slot1\n')
  (source/'Save/Slot1/Save0.usa').write_bytes(b'previous checkpoint'*100)
  (source/'Save/Slot1/Ch1Rictusempra_pa.usa').write_bytes(b'persistent challenge state')
  self.profile=stage_profile(source,self.root/'played')
  write_text(self.profile/'frontend.flag','Original frontend')
  self.state=self.root/'state.json'
  self.trace=self.root/'trace.jsonl'
  self.patch=patch.object(menu_sessions,'STATE',str(self.state));self.patch.start();self.addCleanup(self.patch.stop)

 def test_crash_keeps_completed_book_and_persistent_level_state(self):
  menu_sessions.activate(self.profile,self.trace)
  (self.profile/'Save/Slot1/Save0.usa').write_bytes(b'completed original book checkpoint'*100)
  (self.profile/'Save/Slot1/Ch1Rictusempra_pa.usa').write_bytes(b'challenge completed')
  write_text(self.trace,json.dumps(dict(event='summary',profile_redirected=True,game_exit_code=127,forced_termination=False)))
  with self.assertRaises(ValueError):menu_sessions.publish(self.profile,self.trace)
  next_profile=menu_sessions.prepare(self.root/'next')
  self.assertEqual((next_profile/'Save/Slot1/Save0.usa').read_bytes(),(self.profile/'Save/Slot1/Save0.usa').read_bytes())
  self.assertEqual((next_profile/'Save/Slot1/Ch1Rictusempra_pa.usa').read_bytes(),b'challenge completed')

 def test_unfinished_temporary_save_does_not_replace_previous_checkpoint(self):
  original=(self.profile/'Save/Slot1/Save0.usa').read_bytes()
  menu_sessions.activate(self.profile,self.trace)
  (self.profile/'Save/Slot1/Save.tmp').write_bytes(b'incomplete')
  following=menu_sessions.prepare(self.root/'following')
  self.assertEqual((following/'Save/Slot1/Save0.usa').read_bytes(),original)

 def test_second_activation_preserves_previous_profile(self):
  menu_sessions.activate(self.profile,self.trace)
  following=menu_sessions.prepare(self.root/'following')
  record=menu_sessions.activate(following,self.root/'second.jsonl')
  self.assertEqual(record['previous_profile'],self.profile.relative_to(inside('.')).as_posix())
  self.assertTrue((self.profile/'Save/Slot1/Save0.usa').exists())

 def test_old_menu_exit_does_not_roll_back_newer_session(self):
  menu_sessions.activate(self.profile,self.trace)
  following=menu_sessions.prepare(self.root/'following')
  menu_sessions.activate(following,self.root/'second.jsonl')
  write_text(self.profile/'hp2vr-native.jsonl','{}')
  write_text(self.trace,json.dumps(dict(event='summary',profile_redirected=True,game_exit_code=0,forced_termination=False)))
  result=menu_sessions.publish(self.profile,self.trace)
  self.assertEqual(result['reason'],'superseded_session')
  self.assertEqual(json.loads(self.state.read_text())['profile'],following.relative_to(inside('.')).as_posix())

 def test_non_frontend_fixture_is_not_activated(self):
  invalid=stage_profile(self.profile,self.root/'diagnostic')
  with self.assertRaises(ValueError):menu_sessions.activate(invalid,self.trace)
  self.assertFalse(self.state.exists())

if __name__=='__main__':unittest.main()
