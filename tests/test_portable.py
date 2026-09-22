import ctypes,json,os,sys,tempfile,unittest,uuid
from pathlib import Path
from unittest.mock import patch
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'scripts'))
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'release'))
from project_paths import inside,write_text
import portable_launcher as portable
from shared_saves import SharedSaves,snapshot

class PortableRelease(unittest.TestCase):
 def setUp(self):
  self.root=inside('cache/tests/portable-'+uuid.uuid4().hex[:10]);self.root.mkdir(parents=True)
  self.system=self.root/'System';self.system.mkdir()
  self.profile=self.root/'Profile';self.profile.mkdir()
 def test_language_from_install_and_profile_override(self):
  (self.system/'Default.ini').write_bytes(b'[Engine.Engine]\r\nLanguage=ger\r\n')
  self.assertEqual(portable.detect_language(self.system,self.profile),'de')
  (self.profile/'Game.ini').write_bytes(b'[Engine.Engine]\r\nLanguage=int\r\n')
  self.assertEqual(portable.detect_language(self.system,self.profile),'en')
  self.assertEqual(portable.detect_language(self.system,self.profile,'de'),'de')
  (self.profile/'Game.ini').write_bytes(b'Language=fra\n')
  self.assertEqual(portable.detect_language(self.system,self.profile),'en')
 def test_missing_runtime_and_disconnected_headset_are_different_errors(self):
  portable.LANGUAGE='en'
  with patch('portable_launcher.ctypes.CDLL') as load:
   load.return_value.VR_IsRuntimeInstalled.return_value=False
   with self.assertRaisesRegex(RuntimeError,'SteamVR was not found'):portable.preflight_vr('fixture.dll')
   load.return_value.VR_IsHmdPresent.assert_not_called()
   load.return_value.VR_IsRuntimeInstalled.return_value=True;load.return_value.VR_IsHmdPresent.return_value=False
   with self.assertRaisesRegex(RuntimeError,'No VR headset'):portable.preflight_vr('fixture.dll')
   load.return_value.VR_IsHmdPresent.return_value=True;portable.preflight_vr('fixture.dll')
 def test_modified_package_rejected_but_user_options_are_editable(self):
  p=self.root/'Payload';p.mkdir();(p/'test.dll').write_bytes(b'component')
  (p/'manifest.json').write_text(json.dumps({'files':{'test.dll':portable.digest(b'component')}}))
  portable.verify_package(p);(p/'Options.ini').write_text('[HP2VR]\nQuality=performance\n')
  portable.verify_package(p);self.assertEqual(portable.read_options(p/'Options.ini')['quality'],'performance')
  (p/'test.dll').write_bytes(b'changed')
  with self.assertRaisesRegex(ValueError,'damaged'):portable.verify_package(p)
 def test_original_is_unchanged_and_unknown_build_rejected(self):
  source=inside('system/Game.exe');before=source.read_bytes();data=bytearray(before)
  import struct
  pe=struct.unpack_from('<I',data,60)[0];data[pe+22]|=0x20
  output=self.root/'private-Game.exe'
  portable.prepare_image(source,output,portable.digest(before),portable.digest(data))
  self.assertEqual(source.read_bytes(),before);self.assertEqual(output.read_bytes(),data)
  with self.assertRaisesRegex(ValueError,'Unsupported'):portable.prepare_image(source,output,'0'*64,portable.digest(data))
 def test_fresh_public_install_keeps_vanilla_saves(self):
  external=self.root/'Save';slot=external/'Slot1';slot.mkdir(parents=True)
  (slot/'Save0.usa').write_bytes(bytes.fromhex('c1832a9e')+b'original'*100)
  manager=SharedSaves(self.root/'state.json',self.root/'journal.json',external)
  before=snapshot(slot);manager.initialize();self.assertEqual(snapshot(slot),before)
  self.assertIsNone(manager.read()['active_vr'])
 def test_cross_volume_exchange_survives_interrupt_without_data_loss(self):
  external=self.root/'Save';slot=external/'Slot1';slot.mkdir(parents=True)
  old=bytes.fromhex('c1832a9e')+b'old'*100;new=bytes.fromhex('c1832a9e')+b'new'*100
  (slot/'Save0.usa').write_bytes(old)
  manager=SharedSaves(self.root/'state.json',self.root/'journal.json',external);manager.initialize()
  vr=self.root/'vr';vr.mkdir();write_text(vr/'frontend.flag','fixture');manager.stage(vr);manager.begin(vr)
  (vr/'Save/Slot1/Save0.usa').write_bytes(new)
  original=os.replace
  def interrupt(source,target):
   if Path(source).name=='install-Slot1':raise OSError('simulated power loss')
   return original(source,target)
  with patch('shared_saves.same_volume',return_value=False),patch('shared_saves.os.replace',side_effect=interrupt):
   with self.assertRaisesRegex(OSError,'power loss'):manager.finish(vr)
  self.assertTrue(manager.journal.exists())
  archived=list((external/'.hp2vr-transactions').glob('*/retired/Slot1/Save0.usa'))
  self.assertEqual(len(archived),1);self.assertEqual(archived[0].read_bytes(),old)
  manager.recover();self.assertEqual((slot/'Save0.usa').read_bytes(),new)
  self.assertEqual(archived[0].read_bytes(),old);self.assertIsNone(manager.read()['active_vr'])
 def test_all_controller_profiles_supply_movement_hands_and_ten_buttons(self):
  for suffix in ('','_left'):
   manifest=json.loads(inside('config/vr/actions'+suffix+'.json').read_text())
   self.assertEqual({b['controller_type'] for b in manifest['default_bindings']},{'oculus_touch','knuckles','pico_controller','vive_focus3_controller'})
   names={a['name'] for a in manifest['actions']}
   for entry in manifest['default_bindings']:
    binding=json.loads(inside('config/vr/'+entry['binding_url']).read_text());self.assertEqual(binding['controller_type'],entry['controller_type'])
    actions=binding['bindings']['/actions/game']
    outputs=[v['output'] for s in actions['sources'] for v in s['inputs'].values()]
    self.assertEqual(len(outputs),12);self.assertEqual(len(set(outputs)),12)
    self.assertTrue(set(outputs).issubset(names))
    self.assertEqual({p['output'] for p in actions['poses']},{'/actions/game/in/'+x for x in ('left_hand','right_hand','left_grip','right_grip')})
    self.assertEqual(len(actions['haptics']),3)
   self.assertEqual({r['language_tag'] for r in manifest['localization']},{'de_DE','en_US'})

 def test_native_upgrade_gets_separate_cache_and_script_update_reuses_code(self):
  manifest=dict(files={'bin/'+n:'1'*64 for n in ('hp2vr-native.dll','hp2vr-native-launcher.exe','hp2vr-flat-launcher.exe')},runtime_image_sha256='2'*64,version='1.0.1')
  old=portable.build_relative(manifest)
  manifest['version']='1.0.2';self.assertEqual(portable.build_relative(manifest),old)
  manifest['files']['bin/hp2vr-native.dll']='3'*64
  new=portable.build_relative(manifest);self.assertNotEqual(new,old)
  previous=self.root/old/'hp2vr-native.dll';previous.parent.mkdir(parents=True);previous.write_bytes(b'previous')
  source=self.root/'new.dll';source.write_bytes(b'new')
  portable.ensure_file(source,self.root/new/'hp2vr-native.dll')
  self.assertEqual(previous.read_bytes(),b'previous')
  self.assertEqual((self.root/new/'hp2vr-native.dll').read_bytes(),b'new')
  manifest['files']['bin/hp2vr-native.dll']='../escape'
  with self.assertRaises(ValueError):portable.build_relative(manifest)

if __name__=='__main__':unittest.main()
