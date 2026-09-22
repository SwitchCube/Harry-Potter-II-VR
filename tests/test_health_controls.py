import json,sys,unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'scripts'))
from project_paths import inside

class HealthControls(unittest.TestCase):
    def test_physical_left_grip_in_both_dominant_hand_modes(self):
        for suffix in ('','_left'):
            actions=json.loads(inside(f'config/vr/actions{suffix}.json').read_text())
            declared={a['name']:a['type'] for a in actions['actions']}
            self.assertEqual(declared['/actions/game/in/show_health'],'boolean')
            b=json.loads(inside(f'config/vr/bindings_touch{suffix}.json').read_text())['bindings']['/actions/game']
            left=[s for s in b['sources'] if s['path']=='/user/hand/left/input/grip']
            self.assertEqual(len(left),1)
            self.assertEqual(left[0]['inputs']['click']['output'],'/actions/game/in/show_health')
            self.assertIn({'path':'/user/hand/left/pose/openxr_grip','output':'/actions/game/in/left_grip'},b['poses'])
            for source in b['sources']:
                for value in source['inputs'].values():self.assertIn(value['output'],declared)
            # The new health control must not steal the existing movement axis.
            movement=[s for s in b['sources'] if s.get('inputs',{}).get('position',{}).get('output')=='/actions/game/in/move']
            self.assertEqual(len(movement),1)

if __name__=='__main__':unittest.main()
