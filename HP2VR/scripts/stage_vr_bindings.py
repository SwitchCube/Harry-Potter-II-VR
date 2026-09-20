"""Touch actions use reserved keys only in the isolated project profile."""
import argparse,re
from project_paths import inside
p=argparse.ArgumentParser();p.add_argument('--profile',required=True);a=p.parse_args()
profile=inside(a.profile)
if not profile.is_relative_to(inside('cache')) or not (profile/'staging.json').is_file():raise ValueError('Not a staged test profile')
f=inside(profile/'User.ini')
if f.stat().st_nlink!=1:raise ValueError('Hardlinked User.ini')
s=f.read_bytes().decode('cp1252')
for key,value in {'Escape':'','F14':'Button bBroomBrake','F15':'Button bBroomBoost | Button bRun','F16':'Button bOpenMap','F20':'Button bSpellLessonLeft | Button bBroomYawLeft','F21':'Button bSpellLessonRight | Button bBroomYawRight','F22':'Button bSpellLessonUp | Button bBroomPitchUp','F23':'Button bSpellLessonDown | Button bBroomPitchDown','F24':'Button bRun | Button bBroomBrake','F17':'jump | Button bBroomAction | Button bDuelCycleSpell','F18':'AltFire | Button bBroomAction | Button bVendorReply','F19':'Button bDrinkWiggenwell | button bDuelCycleSpell'}.items():
 if len(re.findall(r'(?m)^'+key+'=[^\r\n]*',s))!=1:raise ValueError('Expected exactly one reserved key '+key)
 s=re.sub(r'(?m)^'+key+'=[^\r\n]*',key+'='+value,s)
with f.open('wb') as handle:handle.write(s.encode('cp1252'))
print('VR bindings staged in isolated profile')
