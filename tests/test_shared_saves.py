import json,sys,unittest,uuid,os
from pathlib import Path
from unittest.mock import patch
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'scripts'))
from project_paths import inside,write_text
from shared_saves import SharedSaves,snapshot

def checkpoint(label):return bytes.fromhex('c1832a9e')+label.encode()*100
class CommonSaves(unittest.TestCase):
 def setUp(self):
  self.root=inside('cache/tests/shared-'+uuid.uuid4().hex);self.root.mkdir(parents=True)
  self.flat=self.root/'original/Save';self.vr=self.root/'vr';self.vr.mkdir()
  write_text(self.vr/'frontend.flag','fixture')
  self.manager=SharedSaves(self.root/'state.json',self.root/'journal.json',self.flat)
 def put(self,root,slot,label,name='Save0.usa'):
  path=root/slot/name;path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(checkpoint(label));return path
 def initialize(self):
  self.put(self.flat,'Slot1','flat-old');self.put(self.vr/'Save','Slot1','vr-current')
  self.manager.initialize(self.vr)
 def begin(self):
  p=self.root/('played-'+uuid.uuid4().hex);p.mkdir();write_text(p/'frontend.flag','fixture')
  self.manager.stage(p);self.manager.begin(p);return p
 def test_initial_merge_preserves_flat_only_slots_and_all_level_files(self):
  self.put(self.flat,'Slot2','flat-only');self.put(self.vr/'Save','Slot1','level','Grounds_pa.usa')
  self.initialize()
  self.assertEqual(snapshot(self.flat/'Slot1'),snapshot(self.vr/'Save/Slot1'))
  self.assertEqual((self.flat/'Slot2/Save0.usa').read_bytes(),checkpoint('flat-only'))
  self.assertEqual(self.manager.read()['initial_slots'],['Slot1'])
 def test_vr_to_flat_and_flat_to_vr(self):
  self.initialize();p=self.begin();self.put(p/'Save','Slot1','new-vr')
  self.manager.finish(p);self.assertEqual((self.flat/'Slot1/Save0.usa').read_bytes(),checkpoint('new-vr'))
  self.put(self.flat,'Slot1','new-flat');q=self.begin()
  self.assertEqual((q/'Save/Slot1/Save0.usa').read_bytes(),checkpoint('new-flat'))
 def test_crash_recovery_keeps_completed_book_and_level(self):
  self.initialize();p=self.begin();self.put(p/'Save','Slot1','book-after-crash')
  self.put(p/'Save','Slot1','changed-level','Entryhall_pa.usa');self.manager.recover()
  self.assertEqual(snapshot(self.flat/'Slot1'),snapshot(p/'Save/Slot1'))
  self.assertIsNone(self.manager.read()['active_vr'])
 def test_unfinished_tmp_is_not_published(self):
  self.initialize();p=self.begin();before=snapshot(self.flat/'Slot1')
  (p/'Save/Slot1/Save.tmp').write_bytes(b'partial save')
  self.assertEqual(self.manager.finish(p)['synced_slots'],[])
  self.assertEqual(snapshot(self.flat/'Slot1'),before);self.assertFalse((self.flat/'Slot1/Save.tmp').exists())
 def test_new_game_removes_stale_level_files_as_whole_slot(self):
  self.put(self.vr/'Save','Slot1','old-level','Grounds_pa.usa');self.initialize();p=self.begin()
  (p/'Save/Slot1/Grounds_pa.usa').unlink();self.put(p/'Save','Slot1','new-game')
  self.manager.finish(p);self.assertFalse((self.flat/'Slot1/Grounds_pa.usa').exists())
 def test_unchanged_vr_does_not_roll_back_newer_flat_save(self):
  self.initialize();p=self.begin();self.put(self.flat,'Slot1','flat-newer');self.manager.finish(p)
  self.assertEqual((self.flat/'Slot1/Save0.usa').read_bytes(),checkpoint('flat-newer'))
 def test_conflicting_changes_keep_both_versions(self):
  self.initialize();p=self.begin();self.put(self.flat,'Slot1','flat-change');self.put(p/'Save','Slot1','vr-change')
  with self.assertRaisesRegex(ValueError,'independently'):self.manager.finish(p)
  self.assertEqual((self.flat/'Slot1/Save0.usa').read_bytes(),checkpoint('flat-change'))
  self.assertEqual((p/'Save/Slot1/Save0.usa').read_bytes(),checkpoint('vr-change'))
 def test_old_session_cannot_publish_over_current(self):
  self.initialize();p=self.begin()
  with self.assertRaisesRegex(ValueError,'older session'):self.manager.finish(self.vr)
  self.assertIsNotNone(self.manager.read()['active_vr'])
 def test_directory_exchange_recovers_after_interruption(self):
  self.initialize();p=self.begin();self.put(p/'Save','Slot1','new-complete')
  original=os.replace
  def fail(source,target):
   if Path(source).name=='install-Slot1':raise OSError('simulated interruption after old slot archived')
   return original(source,target)
  with patch('shared_saves.os.replace',side_effect=fail):
   with self.assertRaisesRegex(OSError,'simulated'):self.manager.finish(p)
  self.assertTrue(self.manager.journal.exists());job=json.loads(self.manager.journal.read_text())
  retired=inside(job['folder'])/'retired/Slot1/Save0.usa'
  self.assertEqual(retired.read_bytes(),checkpoint('vr-current'))
  self.manager.recover();self.assertFalse(self.manager.journal.exists())
  self.assertEqual((self.flat/'Slot1/Save0.usa').read_bytes(),checkpoint('new-complete'))
 def test_recovery_rebuilds_a_truncated_staging_copy(self):
  self.initialize();p=self.begin();self.put(p/'Save','Slot1','complete-after-interruption')
  original=os.replace
  def fail(source,target):
   if Path(source).name=='install-Slot1':raise OSError('interrupted')
   return original(source,target)
  with patch('shared_saves.os.replace',side_effect=fail):
   with self.assertRaises(OSError):self.manager.finish(p)
  job=json.loads(self.manager.journal.read_text());folder=inside(job['folder'])
  (folder/'install-Slot1/Save0.usa').write_bytes(b'partial header')
  self.manager.recover()
  self.assertEqual((self.flat/'Slot1/Save0.usa').read_bytes(),checkpoint('complete-after-interruption'))
 def test_corrupt_completed_checkpoint_does_not_overwrite_good_save(self):
  self.initialize();p=self.begin();old=(self.flat/'Slot1/Save0.usa').read_bytes()
  (p/'Save/Slot1/Save0.usa').write_bytes(b'broken')
  with self.assertRaisesRegex(ValueError,'Invalid original checkpoint'):self.manager.finish(p)
  self.assertEqual((self.flat/'Slot1/Save0.usa').read_bytes(),old)
 def test_other_external_paths_rejected(self):
  with self.assertRaises(ValueError):SharedSaves(external='C:/Users/Public/OtherSave')
 def test_diagnostic_read_only_staging_leaves_both_states_unchanged(self):
  self.initialize();before=self.manager.state.read_bytes();files=snapshot(self.flat/'Slot1')
  p=self.root/'diagnostic';p.mkdir();write_text(p/'frontend.flag','fixture');self.manager.stage(p)
  self.assertEqual(self.manager.state.read_bytes(),before);self.assertEqual(snapshot(self.flat/'Slot1'),files)
if __name__=='__main__':unittest.main()
