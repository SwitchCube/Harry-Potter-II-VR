"""Validate original simulation observations, separately from headset acceptance."""
import math

def check_mechanics(run,events):
 def rows(name):return [e for e in events if e['event']==name]
 kind=run['mechanics_replay']
 if kind=='sword':
  meshes=rows('sword_mesh');shots=rows('sword_projectile')
  if {e['eye'] for e in meshes}!={0,1} or any(e['mesh']!='skGryf_SwordMesh' or e['grip_error']>.01 for e in meshes):raise ValueError('Original sword grip not verified in both eyes')
  if len(shots)<3 or any(e['class']!='spellSwordFire' or e['origin_error']>.1 for e in shots):raise ValueError('Repeated original sword attacks not verified')
  samples=rows('sword_sample');hits=rows('sword_boss_hit')
  if not any(e['health_after']<e['health_before'] for e in hits):raise ValueError('Controller-directed sword shot did not damage the original Basilisk')
  if not any(e['second_phase'] and 0<e['health_after']<e['health_before'] for e in hits):raise ValueError('A genuine second-phase damage event is required')
  if not any(e['charge']>=1.99 for e in samples):raise ValueError('Original full sword charge not reached')
  movements=[]
  for phase,axis,sign in ((1,1,1),(2,1,-1),(3,0,1),(4,0,-1)):
   group=[e for e in samples if phase*4000<=e['elapsed']<(phase+1)*4000 and e['input'][axis]*sign>.2 and e['state']=='PlayerWalking']
   if len(group)<2 or math.dist(group[0]['position'],group[-1]['position'])<2:raise ValueError('Endfight movement direction not exercised: '+str(phase))
   movements.append(dict(phase=phase,displacement=math.dist(group[0]['position'],group[-1]['position'])))
  if any(e['state']=='stateDead' and e['elapsed']<18000 for e in samples):raise ValueError('Fixture died before completing the in-arena movement checks')
  return dict(kind=kind,meshes=meshes,shots=shots,hits=hits,movements=movements,peak_charge=max(e['charge'] for e in samples))
 if kind=='stairs':
  if not rows('stairs_complete') or len(rows('stairs_hand_alignment'))<10:raise ValueError('Complete down/up route and hand alignment required')
  samples=rows('stairs_view_sample')
  raw=[];view=[]
  for a,b in zip(samples,samples[1:]):
   if a['grounded'] and b['grounded'] and a['phase']==b['phase'] and b['phase'] in (1,2) and 0<b['dt']<.05:
    raw.append(abs(b['raw_z']-a['raw_z']));view.append(abs(b['view_z']-a['view_z']))
  if not raw or max(raw)<10 or max(view)>=max(raw):raise ValueError('Actual stair discontinuities were not reduced')
  return dict(kind=kind,max_raw_step=max(raw),max_view_step=max(view),hand_checks=len(rows('stairs_hand_alignment')),completion=rows('stairs_complete'))
 if kind=='mirror':
  mirrors=rows('reflected_player')
  if {(e['mesh'],e['eye']) for e in mirrors}!={(m,e) for m in ('skharryMesh','skGoyleMesh') for e in (0,1)}:raise ValueError('Harry and Goyle must be reflected in both eyes')
  return dict(kind=kind,reflections=mirrors)
 if kind=='vendor':
  if {e['yes'] for e in rows('vendor_confirm')}!={True,False}:raise ValueError('Both vendor answers required')
  states=rows('vendor_stock_state');inventory=rows('vendor_stock_inventory')
  beans=[e['count'] for e in rows('hand_status_changed') if e['item']=='StatusItemJellybeans']
  if not any(e['state']=='MakePurchase' and e['purchases']==1 and e['price']==75 for e in states):raise ValueError('Original purchase not reached')
  if not beans or beans[0]!=100 or beans[-1]!=25:raise ValueError('Original price not fully deducted')
  if not any(e['round']==2 and e['state']=='VendorTransaction' and e['remaining']==1 for e in inventory):raise ValueError('Original seller did not deliver the item')
  if not any(e['state']=='NotEnoughBeans' and e['purchases']==1 for e in states):raise ValueError('Original affordability refusal missing')
  if any(e['active'] for e in rows('cinema_mode')):raise ValueError('Trade briefly returned to cinema screen')
  panels=rows('vendor_panel');anchors=rows('vendor_anchor');samples=rows('vendor_anchor_sample')
  if {e['yes'] for e in panels}!={True,False} or any(not e['gpu_verified'] or not e['separate_from_subtitles'] for e in panels):raise ValueError('Both separate vendor highlights required')
  if not anchors or any(e['head_attached'] for e in anchors) or len(samples)<2 or any(e['roundtrip_error']>.001 for e in samples):raise ValueError('Vendor world attachment failed')
  if any(abs(e['determinant']-1)>.0001 or e['front_distance']<=0 for e in samples):raise ValueError('Vendor surface is reflected or facing away from player')
  return dict(kind=kind,states=states,answers=rows('vendor_confirm'),inventory=inventory,beans_before=beans[0],beans_after=beans[-1],panels=panels,anchors=anchors,anchor_samples=samples)
 if kind=='polish':
  if not rows('polish_actor'):raise ValueError('No original actors inspected')
  return dict(kind=kind,actors=rows('polish_actor'))
 if kind=='cinema':
  if len(rows('unskippable_fixture_world'))!=1 or len(rows('unskippable_fixture_restored'))!=1:raise ValueError('Original scene skip policy switch incomplete')
  option=rows('spatial_option_fixture')
  if [e['spatial'] for e in option]!=[True,False] or any(not e['persisted'] for e in option):raise ValueError('Optional cinematic camera switch not exercised')
  if not any(e['active'] and e['skippable'] and e['option'] for e in rows('world_cutscene')):raise ValueError('Skippable original scene did not render in stereo')
  camera=[e for e in rows('cinematic_camera_sample') if e['spatial_option']]
  if not camera or max(e['player_distance'] for e in camera)<20 or any(abs(e['eye_distance']-3.2)>.01 for e in camera):raise ValueError('Stereo cinematic camera not independent of player')
  return dict(kind=kind,presentations=rows('world_cutscene'),option=option,camera=camera)
 if kind=='carry':
  throws=rows('controller_throw')
  if len(throws)!=2 or {e['class'] for e in throws}!={'GNOME','HorklumpsHead'}:raise ValueError('Both original carryable types must be thrown')
  if len(rows('carry_fixture_held'))!=2 or len(rows('carry_fixture_released'))!=2:raise ValueError('Original pickup/release sequence incomplete')
  if {e['eye'] for e in rows('throw_crosshair')}!={0,1}:raise ValueError('Crosshair absent in either eye')
  for e in throws:
   if e['state']!='stateBeingThrown' or math.dist(e['start'],e['crosshair'])<20 or not all(math.isfinite(v) for k in ('start','velocity','crosshair') for v in e[k]):raise ValueError('Invalid original throw state/trajectory')
  contacts=rows('throw_first_contact')
  extents={e['class']:e for e in rows('throw_contact_extent')}
  if len(contacts)!=2:raise ValueError('Both actual first contacts required')
  for e in contacts:
   size=extents[e['class']];delta=[a-b for a,b in zip(e['position'],e['target'])]
   if not 0<size['radius']<=30 or not 0<size['height']<=30 or math.hypot(*delta[:2])>size['radius']+1 or abs(delta[2])>size['height']+1:raise ValueError('Reticle is outside original object collision volume at contact')
  meshes=rows('held_original_mesh')
  if len(meshes)!=4 or any(e['hand_error']>.01 for e in meshes):raise ValueError('Held mesh not at controller in both eyes')
  if len(rows('throw_tracking_loss_fixture'))!=1 or len(rows('throw_cancelled'))!=1:raise ValueError('Tracking-loss cancellation and successful retry were not exercised')
  return dict(kind=kind,throws=throws,contacts=contacts,held_meshes=meshes)
 if kind=='reward':
  rewards=rows('card_reward_vr');done=rows('reward_fixture_complete')
  if [e['bronze'] for e in rewards]!=[True,False] or len(done)!=2:raise ValueError('Both card celebration states must complete')
  if any(e['cinema'] or e['state']!='PlayerWalking' for e in done):raise ValueError('Reward did not return to original walking state')
  return dict(kind=kind,rewards=rewards,completion=done)
 if kind=='broom':
  samples=rows('broom_fixture_sample')
  if not samples:raise ValueError('No original BroomHarry flight')
  for phase,axis,sign in [(0,1,1),(1,1,-1),(2,0,1),(3,0,-1)]:
   group=[e for e in samples if phase*1400<=e['elapsed']<phase*1400+900 and e['input'][axis]*sign>.5]
   field='yaw' if axis else 'pitch'
   if len(group)<2 or abs(math.remainder(group[-1][field]-group[0][field],65536))<50:raise ValueError('Original broom did not steer on axis '+str(phase))
  if not any(e['boost'] for e in samples) or not any(e['brake'] for e in samples) or not any(e['action'] for e in samples):raise ValueError('Missing original boost/brake/action')
  if {e['side'] for e in rows('broom_opponent_cue')}!={0,1} or len(rows('broom_stock_kick'))<2 or len(rows('broom_stock_contact'))<1:raise ValueError('Both rival sides and original shove action not exercised')
  return dict(kind=kind,samples=samples,opponent_cues=rows('broom_opponent_cue'),stock_kicks=len(rows('broom_stock_kick')))
 raise ValueError('Unknown mechanics fixture')
