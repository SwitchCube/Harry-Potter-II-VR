"""Independent native/game/GPU evidence checks. A replay never proves headset output."""
import argparse,hashlib,json,math
from project_paths import read_trace_text,inside,write_text
from check_pair_trace import compare_bitmaps, read_bitmap

def check(run_path):
 run=json.loads(inside(run_path).read_text(encoding='utf-8-sig'))
 if run['exit_code'] or run['external_profile_changed'] or run['external_profile_added']:raise ValueError('Run failed or external profile changed')
 events=[json.loads(line) for line in read_trace_text(run['trace']).splitlines() if line.strip()]
 def only(rows,name):
  values=[e for e in rows if e.get('event')==name]
  if len(values)!=1:raise ValueError('Expected one '+name)
  return values[0]
 summary=only(events,'summary');only(events,'native_debugger_detached')
 if not summary['profile_redirected'] or summary['game_exit_code'] or summary['forced_termination']:raise ValueError('Unclean original process exit')
 profile=inside(run['profile']);native=[json.loads(x) for x in inside(profile/'hp2vr-native.jsonl').read_text().splitlines() if x.strip()]
 if any(e['event'] in ('fatal','init_failed','graphics_error') for e in native):raise ValueError('Native failure')
 init=only(native,'native_initialized');shutdown=only(native,'native_shutdown')
 if run.get('audio_stall_replay'):
  stall=only(native,'audio_stall_fixture');buffers=only(native,'audio_buffer_size')
  starts=[e for e in native if e['event']=='dialogue_start' and e['sound']==stall['sound']]
  if len(starts)!=1 or not starts[0]['accepted'] or stall['ms']!=2500:raise ValueError('Original speech stall not reproduced')
  underruns=[e for e in native if e['event']=='dialogue_buffer_underrun' and e['sound']==stall['sound']]
  stock=bool(run.get('stock_audio_replay'))
  if buffers['pcm_chunk_bytes']!=(32768 if stock else 65536):raise ValueError('Unexpected audio buffer size')
  if bool(underruns)!=stock:raise ValueError('Audio stress comparison failed')
  endings=[e for e in native if e['event'] in ('dialogue_stop','dialogue_finished') and e['sound']==stall['sound']]
  if not endings or endings[0]['elapsed_ms']<starts[0]['duration']*1000:raise ValueError('Stress sentence was stopped early')
  return dict(verified=True,mode=run['mode'],headset_verified=False,original_process_exit=0,audio_stress=dict(stock=stock,stall=stall,buffers=buffers,underruns=underruns,start=starts[0],end=endings[0]))
 if run.get('look_around_replay'):
  texture_checks=[e for e in native if e['event']=='scripted_texture_fixture']
  if texture_checks:
   if len(texture_checks)!=4 or len({e['texture'] for e in texture_checks})!=4:raise ValueError('Not all original house textures exercised')
   if sum(e['right_only'] for e in texture_checks)!=1 or sum(e['duplicate_preserved'] for e in texture_checks)!=3:raise ValueError('Scripted texture eye preparation mismatch')
 if run.get('window_replay'):
  fixtures=[e for e in native if e['event']=='desktop_window_fixture']
  if len(fixtures)!=3 or len({(e['client_width'],e['client_height']) for e in fixtures})!=3:raise ValueError('Desktop resize fixture incomplete')
  frames=[e for e in native if e['event']=='vr_frame']
  expected=tuple(map(int,run['render_resolution'].split('x')))
  if not frames or any((e['width'],e['height'])!=expected for e in frames):raise ValueError('Desktop resize changed VR resolution')
  if len([e for e in native if e['event']=='desktop_resize_isolated'])<3:raise ValueError('Desktop resize not isolated')
 if run.get('lesson_replay'):
  if init['mode']!=5:raise ValueError('Lesson fixture outside diagnostic mode')
  samples=[e for e in native if e['event']=='lesson_original_input']
  if not samples or max(e['coverage'] for e in samples)!=255:raise ValueError('Original lesson did not consume all directions from both sticks')
  matched=[e for e in samples if e['mask'] and e['mask']==e['expected'] and not e['cinema']]
  if not matched or any(e['body_distance']>.1 for e in matched):raise ValueError('Interactive lesson moved the player body')
  presentations=[e for e in native if e['event']=='lesson_presentation']
  if not any(e['interactive_world'] for e in presentations) or not any(e['event']=='cinema_mode' and e['active'] for e in native):raise ValueError('Missing explanation-to-world transition')
  only(native,'lesson_world_capture')
  for hand in (0,1):
   if {e['mask'] for e in matched if e['hand']==hand}!={1,2,4,8}:raise ValueError('Incomplete original arrow recognition')
  if not any(e['mask']==e['expected']==0 for e in samples):raise ValueError('Lesson stick release was not received')
  w,h,p=read_bitmap(inside(profile/'native-lesson.bmp').read_bytes())
  if len(set(p))<128:raise ValueError('Blank original lesson image')
  return dict(verified=True,mode=run['mode'],headset_verified=False,original_process_exit=0,lesson_inputs=samples,lesson_image=dict(width=w,height=h),dialogue=[e for e in native if e['event'].startswith('dialogue')])
 if not shutdown['completed'] or not shutdown['extra_draws']:raise ValueError('No completed extra game view')
 images=compare_bitmaps(inside(profile/'native-a.bmp').read_bytes(),inside(profile/'native-b.bmp').read_bytes())
 out=dict(verified=True,mode=run['mode'],headset_verified=False,original_process_exit=0,images=images,native=shutdown)
 if run.get('hud_fallback'):
  attempts=[e for e in native if e['event']=='hud_target_attempt']
  rejected=[e for e in attempts if e['path']=='offscreen-argb' and e['simulated_failure'] and e['hr']==-2005532292]
  fallback=[e for e in native if e['event']=='hud_target_fallback']
  if not rejected:raise ValueError('Reported HUD video-memory error not reproduced')
  if run['hud_fallback']=='texture':
   if not any(e['path']=='texture-argb' and not e['simulated_failure'] and e['hr']==0 for e in attempts) or fallback:raise ValueError('Texture HUD fallback not exercised')
  elif not fallback or any(e['hr']==0 for e in attempts):raise ValueError('Borrowed HUD fallback not exercised')
  out['hud_fallback']=dict(path=run['hud_fallback'],rejections=len(rejected),borrowed_surfaces=len(fallback))
 if init['mode']>=3:
  vr=only(native,'vr_shutdown');frames=[e for e in native if e['event']=='vr_frame']
  if not frames or vr['frames']!=shutdown['extra_draws'] or (vr['gpu_verified_eyes']<2 or vr['gpu_verified_eyes']%2):raise ValueError('GPU/paired-frame evidence incomplete')
  for e in frames:
   if abs(e['focal_a']-e['focal_b'])>.001 or abs(e['focal_a']-e.get('width',images['width'])*.5/math.tan(math.radians(e['fov'])*.5))>.05:raise ValueError('Incorrect actual eye focal lengths')
   if e['extra_ticks'] or not e['pose_unchanged'] or not all(math.isfinite(n) for n in e['left']+e['right']):raise ValueError('Invalid frame')
   if not 1<math.dist(e['left'],e['right'])<10:raise ValueError('Eye separation outside diagnostic range')
  gpu=[e for e in native if e['event']=='gpu_texture_verified']
  if gpu:
   if len(gpu)!=vr['gpu_verified_eyes']:raise ValueError('GPU verification count differs')
   for generation in {(e['generation'],e.get('level_generation',0)) for e in gpu}:
    pair=[e for e in gpu if (e['generation'],e.get('level_generation',0))==generation]
    if len(pair)!=2 or {e['eye'] for e in pair}!={0,1} or len({(e['width'],e['height']) for e in pair})!=1:raise ValueError('Incomplete recreated eye texture pair')
  children=[e for e in native if e['event']=='child_projection']
  if any(not e['matched'] or abs(e['parent_focal']-e['child_focal'])>.05 for e in children):raise ValueError('Child sky/portal FOV mismatch')
  out['child_projection']=children
  if init['mode']!=3 and (vr['submitted_pairs'] or any(e['mode']!='replay' for e in frames)):raise ValueError('Replay labelled as real VR')
  if init['mode']==3:
   if vr['submitted_pairs']<1:raise ValueError('No actual compositor submission')
   refs=[e for e in native if e['event']=='projection_matrix_reference']
   if len(refs)!=2 or {e['eye'] for e in refs}!={0,1} or any(not math.isfinite(e['max_uv_error']) or e['max_uv_error']>=.0001 for e in refs):raise ValueError('Actual runtime projection matrix was not matched for both eyes')
   out['projection_matrix_reference']=refs
  if vr.get('elapsed_ms',0)>0:out['application_first_pair_to_shutdown_rate_hz']=vr['frames']*1000/vr['elapsed_ms']
  profiles=[e for e in native if e['event']=='profile_summary']
  if profiles:
   summary=only(native,'profile_summary');stages=[e for e in native if e['event']=='profile_stage']
   if summary['samples']+summary['excluded_capture_warmup']!=vr['frames']:raise ValueError('Profiling sample count differs from completed pairs')
   if len(stages)!=10 or len({e['stage'] for e in stages})!=10:raise ValueError('Profiling stages incomplete')
   values=[summary[k] for k in ('pair_mean_ms','outside_pair_mean_ms','steady_pair_rate_hz')]+[e[k] for e in stages for k in ('mean_ms','max_ms')]
   if any(not math.isfinite(n) or n<0 for n in values):raise ValueError('Invalid profiling duration')
   out['wall_clock_profile']=dict(summary=summary,stages=stages)
   if summary.get('first_to_last_pair_ms',0)>0:out['application_pair_rate_including_capture_hz']=vr['frames']*1000/summary['first_to_last_pair_ms']
  out['runtime_frame_timings']=[e for e in native if e['event']=='vr_timing']
  out['hmd_activity']=[e for e in native if e['event']=='hmd_activity']
  if run.get('travel_replay'):
   travel=[e for e in native if e['event']=='travel_replay']
   transitions=[e for e in native if e['event']=='level_transition_end']
   if [e['stage'] for e in travel]!=[1,2] or len(transitions)<2 or not all(e['loaded'] for e in transitions):raise ValueError('Requested map-travel replay did not complete')
   if len([e for e in native if e['event']=='vr_player'])<3:raise ValueError('Player was not rebound after map loads')
   out['map_transitions']=transitions
  if run.get('resume_requested'):
   commands=[e for e in native if e['event']=='original_command' and e['command']=='loadgame 0' and e['handled']]
   transitions=[e for e in native if e['event']=='level_transition_end' and e['loaded']]
   if len(commands)!=1 or not transitions:raise ValueError('Session checkpoint not loaded')
   out['checkpoint_resumed']=True
  if run.get('save_replay'):
   commands=[e for e in native if e['event']=='original_command' and e['command'] in ('savegame 0','loadgame 0')]
   if [e['command'] for e in commands]!=['savegame 0','loadgame 0'] or not all(e['handled'] for e in commands):raise ValueError('Save/load replay incomplete')
   save=inside(profile/'Save/Slot1/Save0.usa')
   if not save.is_file() or save.stat().st_size<1024:raise ValueError('Original save missing')
   if len([e for e in native if e['event']=='level_transition_end' and e['loaded']])<3:raise ValueError('Saved map not reloaded')
   out['save_load_commands']=commands
  if run.get('resize_replay'):
   sizes=[(e['width'],e['height']) for e in native if e['event']=='bridge_resize']
   if sizes!=[(1280,960),(2048,2048)] or vr['gpu_verified_eyes']<6:raise ValueError('Resize or recreated texture verification incomplete')
   out['resize_dimensions']=sizes
  wand=[e for e in native if e['event']=='wand_draw']
  if wand:
   if len(wand)!=2 or {e['eye'] for e in wand}!={0,1} or any(e['vertices']<3 for e in wand):raise ValueError('Tracked wand missing from one eye')
   if math.dist(wand[0]['base'],wand[1]['base'])>.001 or math.dist(wand[0]['tip'],wand[1]['tip'])>.001:raise ValueError('Wand moved between eye renders')
   out['wand_render']=wand
  if any(e['event']=='hud_capture_started' for e in native):
   hud=only(native,'hud_texture')
   width,height,pixels=read_bitmap(inside(profile/'native-hud.bmp').read_bytes())
   if not hud['gpu_verified'] or (width,height)!=(hud['width'],hud['height']) or not 0<=hud['visible_pixels']<width*height:raise ValueError('HUD GPU/content/transparency evidence incomplete')
   if hud['visible_pixels'] and (len(set(pixels))<16 or all(p==0 for p in pixels)):raise ValueError('Empty HUD capture')
   out['hud_texture']=hud
  if run.get('book_replay'):
   books=[e for e in native if e['event']=='save_book_replay']
   if len(books)!=1 or books[0]['handler']!='Touch':raise ValueError('Stock save book Touch not exercised')
   save=inside(profile/'Save/Slot1/Save0.usa')
   staged=json.loads(inside(profile/'staging.json').read_text())
   initial=next((f['staged_sha256'] for f in staged['files'] if f['relative_path'].lower()=='save/slot1/save0.usa'),None)
   if not save.is_file() or save.stat().st_size<1024 or hashlib.sha256(save.read_bytes()).hexdigest()==initial:raise ValueError('Book did not write a new private checkpoint')
   if any(e['event']=='original_command' and e['command'].startswith('savegame') for e in native):raise ValueError('Book evidence mixed with quicksave')
   out['save_book']=books[0];out['book_checkpoint_sha256']=hashlib.sha256(save.read_bytes()).hexdigest()
  health=[e for e in native if e['event']=='health_texture']
  if health:
   for e in health:
    w,h,p=read_bitmap(inside(profile/f"native-health-{e['icons']}.bmp").read_bytes())
    if (w,h)!=(e['width'],e['height']) or not e['gpu_verified'] or not 0<e['visible_pixels']<w*h or len(set(p))<16:raise ValueError('Hand health GPU/image missing')
   out['health_textures']=health
   visibility=[e for e in native if e['event']=='health_visibility']
   if any(e['visible'] and (not e['held'] or e['hand']!='left') for e in visibility):raise ValueError('Hand health displayed without physical left grip')
   out['health_visibility']=visibility
  if run.get('health_replay'):
   if {e['icons'] for e in health}!=set(range(1,7)):raise ValueError('One through six original health containers not rendered')
   for e in health:
    w,h,p=read_bitmap(inside(profile/f"native-health-{e['icons']}.bmp").read_bytes())
    if (w,h)!=(e['width'],e['height']) or not e['gpu_verified'] or not 0<e['visible_pixels']<w*h or len(set(p))<16:raise ValueError('Health GPU/image evidence differs')
    if e['count']!=e['potential']-50:raise ValueError('Partial last health container not exercised')
   visibility=[e for e in native if e['event']=='health_visibility']
   if not any(e['visible'] for e in visibility) or not any(not e['visible'] and not e['held'] for e in visibility):raise ValueError('Health hold/release not exercised')
   if any(e['visible'] and not e['held'] for e in visibility):raise ValueError('Health visible without grip')
   out['health_textures']=health;out['health_visibility']=visibility
  menus=[e for e in native if e['event']=='menu_texture']
  if menus:
   if not menus[0]['gpu_verified'] or not (profile/'native-menu.bmp').is_file():raise ValueError('Original menu GPU texture not verified')
   alpha=only(native,'menu_alpha')
   if not alpha['alpha_target'] or not (0<alpha['visible_pixels']<=alpha['total_pixels']) or (menus[0]['height']>menus[0]['width']*.8 and alpha['bottom_visible']):raise ValueError('Menu still includes the captured world rectangle')
   if run.get('binding_replay') and not any(e['event']=='presentation_toolbar' and e['gpu_verified'] for e in native):raise ValueError('Cinematic option missing from menu')
   if run.get('binding_replay'):
    clicks=[e for e in native if e['event']=='presentation_toolbar_input']
    if [e['spatial'] for e in clicks]!=[True,False] or any(not e['click_consumed'] or not e['book_back_available'] for e in clicks):raise ValueError('Menu setting click/back routing failed')
    saves=[e for e in native if e['event']=='presentation_saved']
    if [e['spatial_scenes'] for e in saves]!=[True,False] or not (profile/'config/vr-presentation.ini').is_file():raise ValueError('Menu toggle did not save in private profile')
    out['presentation_clicks']=clicks
   out['menu_texture']=menus[0];out['menu_alpha']=alpha
  if run.get('climb_replay'):
   start=only(native,'climb_replay_start');facing=[e for e in native if e['event']=='locomotion_facing'];mounts=[e for e in native if e['event']=='original_mount'];done=[e for e in native if e['event']=='climb_replay_complete']
   if not facing:raise ValueError('No original PlayerWalking tick observed')
   if not run.get('climb_jump_replay') and any(e['event']=='input_key' and e['key']==128 and e['down'] for e in native):raise ValueError('Climb test unexpectedly pressed jump')
   if run.get('climb_no_jump_replay'):
    if done or any(e['delta'][2]>48 for e in mounts) or any(e['event']=='input_key' and e['key']==128 and e['down'] for e in native):raise ValueError('High ledge baseline must require jumping')
    blocked=[e for e in native if e['event']=='climb_tick' and e['physics']==1 and 1070<e['position'][0]<1078 and -437<e['position'][2]<-433]
    if len(blocked)<2 or blocked[-1]['time_ms']-blocked[0]['time_ms']<1000:raise ValueError('No-jump test did not sustain an approach at the high ledge')
   elif run.get('stock_facing_replay'):
    if mounts or done or not any(abs(math.remainder(e['desired_yaw']-e['head_yaw'],65536))>16000 for e in facing):raise ValueError('Baseline did not reproduce the facing mismatch')
   else:
    if len(done)!=1 or not mounts or done[0]['mount_finish_ticks']<1 or done[0]['position'][2]-start['position'][2]<25:raise ValueError('Stock automatic climb did not complete')
    if any(not e['corrected'] or abs(math.remainder(e['desired_yaw']-e['head_yaw'],65536))>1 for e in facing):raise ValueError('PlayerTick undid VR facing')
   if run.get('climb_course_replay') and (len(mounts)<2 or not done or done[0]['position'][0]>=-860 or done[0]['position'][2]<=-480):raise ValueError('Consecutive low ledges not completed')
   if run.get('climb_jump_replay') and not run.get('climb_no_jump_replay') and not run.get('climb_block_replay') and (not done or done[0]['position'][0]>=990 or done[0]['position'][2]<=-320):raise ValueError('High ledge not completed')
   if not run.get('stock_facing_replay') and not run.get('climb_no_jump_replay') and (run.get('climb_course_replay') or run.get('climb_jump_replay')):
    end=done[0];settled=[e for e in native if e['event']=='climb_tick' and e.get('time_ms',0)>=end['time_ms']+1000]
    if len(settled)<2 or any(e['physics']!=1 or abs(e['position'][2]-end['position'][2])>2 for e in settled):raise ValueError('Climb landing did not remain stable after release')
    if run.get('climb_jump_replay') and not any(e['event']=='climb_tick' and e['physics']==2 and e['velocity'][2]>0 for e in native):raise ValueError('Original airborne jump physics not observed')
    if run.get('climb_jump_replay') and [e['down'] for e in native if e['event']=='input_key' and e['key']==128]!=[True,False]:raise ValueError('High ledge test requires exactly one jump')
   if run.get('climb_block_replay'):
    block=only(native,'block_fixture')
    if block['name']!='GridMover1' or done[0].get('base')!=block['name']:raise ValueError('Did not climb actual spell block')
    bases=[e for e in native if e['event']=='block_player_base']
    settled_steps={e['step'] for e in settled}
    if len([e for e in bases if e['step'] in settled_steps and e['base']==block['name']])!=len(settled):raise ValueError('Landing lost attachment to spell block')
    if run.get('climb_block_push_replay'):
     push=only(native,'block_push_complete')
     if push['position'][0]-block['position'][0]<100:raise ValueError('Block did not move')
     if not any(e['event']=='block_bump' and e['class']=='spellFlipendo' for e in native):raise ValueError('No actual Flipendo collision on block')
     if [e['down'] for e in native if e['event']=='input_key' and e['key']==129]!=[True,False]:raise ValueError('Push requires one cast press/release')
     out['block_push']=push
    if run.get('climb_block_standing_replay'):
     standing=only(native,'block_standing_jump')
     if standing['waited_ms']<1000 or standing['speed']>=1 or standing['physics']!=1:raise ValueError('Standing jump did not wait at wall on the ground')
     out['block_standing_jump']=standing
    out['block']=block
   out['climb']=dict(start=start,mounts=mounts,completion=done,facing=facing,baseline=bool(run.get('stock_facing_replay')),jump_pressed=bool(run.get('climb_jump_replay') and not run.get('climb_no_jump_replay')),no_jump_baseline=bool(run.get('climb_no_jump_replay')))
  out['vr']=vr;out['motion_images']=compare_bitmaps(inside(profile/'native-a.bmp').read_bytes(),inside(profile/'native-motion-a.bmp').read_bytes())
  if init['mode'] in (4,5) and (out['motion_images']['identical'] or images['identical']):raise ValueError('No stereo/motion image difference')
  if run.get('mechanics_replay'):
   from check_mechanics_trace import check_mechanics
   out['mechanics']=check_mechanics(run,native)
  if init['mode']==5 and not run.get('climb_replay') and not run.get('lesson_replay') and not run.get('mechanics_replay'):
   inputs=only(native,'input_shutdown');positions=[e for e in native if e['event']=='input_replay_position']
   if len(positions)!=2 or math.dist(positions[0]['position'],positions[1]['position'])<.01:raise ValueError('Original player did not move')
   keys=[e for e in native if e['event']=='input_key']
   if keys:
    if [e['down'] for e in keys if e['key']==129]!=[True,False]:raise ValueError('Held cast key did not produce exactly press/release')
    if len(keys)!=inputs['transitions']:raise ValueError('Input transition count differs')
   elif inputs['transitions']!=2:raise ValueError('Held cast key did not produce exactly press/release')
   traces=[e for e in native if e['event']=='wand_cursor' and e.get('trace_matches_tip')]
   if not traces or any(math.dist(e['tip'],e['actual_trace_start'])>.002 for e in traces):raise ValueError('Hand origin not observed in original target trace')
   movement=[e for e in native if e['event']=='movement_replay']
   if movement:
    if len(movement)!=5 or {e['phase'] for e in movement}!=set(range(5)):raise ValueError('Four directions and diagonal were not exercised')
    for e in movement:
     angle=e['head_yaw']*math.pi/32768;f,side=e['input'];expected=(f*math.cos(angle)-side*math.sin(angle),f*math.sin(angle)+side*math.cos(angle))
     ax,ay,az=e['acceleration'];length=math.hypot(ax,ay);reference=math.hypot(*expected)
     if length<100 or abs(az)>.001 or (ax*expected[0]+ay*expected[1])/(length*reference)<.999:raise ValueError('Wrong or ineffective original locomotion acceleration')
    lengths=[math.hypot(*e['acceleration'][:2]) for e in movement]
    if max(lengths)/min(lengths)>1.01:raise ValueError('Unequal directional or diagonal acceleration')
    if math.dist(positions[0]['position'],positions[1]['position'])<1:raise ValueError('Locomotion displacement too small')
    out['movement_directions']=movement
   jumps=[e for e in native if e['event']=='jump_replay']
   if jumps:
    if len(jumps)!=2 or jumps[1]['height']-jumps[0]['height']<2:raise ValueError('Original jump did not lift the player')
    for key in (128,130,131,132,133,134,135):
     if [e['down'] for e in keys if e['key']==key]!=[True,False]:raise ValueError('Incorrect reserved action transitions: '+str(key))
    out['jump_replay']=jumps
   out['input']=inputs;out['wand_trace_matches']=len(traces)
 else:
  done=only(native,'native_pair_complete')
  if done['extra_ticks'] or not done['pose_unchanged'] or shutdown['extra_draws']!=1:raise ValueError('Invalid bounded diagnostic')
  out['pair']=done
 return out

def main():
 p=argparse.ArgumentParser();p.add_argument('--run',required=True);p.add_argument('--output',required=True);args=p.parse_args()
 result=check(args.run);write_text(args.output,json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))
if __name__=='__main__':main()
