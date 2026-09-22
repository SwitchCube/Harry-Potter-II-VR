#pragma once
// The stock PlayerWalking.PlayerTick writes DesiredRotation again AFTER PlayerMove.
// Keep its camera-facing reference valid through the whole event. APawn::Mount
// tests the real body forward vector against the wall normal, not the VR image.
static unsigned locomotionTicks=0,originalMountEvents=0,mountFinishTicks=0,climbObservedTicks=0;
static bool dispatchLocomotion(void* self,void* function,void* params,void* result){
 if(startup.mode==5&&climbBlock&&self==climbBlock&&objectName(function)==L"Bump"&&params){
  void* other=*static_cast<void**>(params);if(other)log("{\"event\":\"block_bump\",\"other\":\"%ls\",\"class\":\"%ls\"}",objectName(other).c_str(),objectName(mem<void*>(other,0x24)).c_str());
 }
 if(startup.mode<3||startup.mode>5||!vrPlayer||self!=vrPlayer)return false;
 FpuState fpu;
 try{
  auto name=objectName(function);
  if(name==L"Mount"){
   ++originalMountEvents;
   Vec delta=params&&mem<WORD>(function,0x7a)==12?*static_cast<Vec*>(params):Vec{};
   log("{\"event\":\"original_mount\",\"number\":%u,\"delta\":[%.5f,%.5f,%.5f]}",originalMountEvents,delta.x,delta.y,delta.z);
   return false;
  }
  if(name!=L"PlayerTick")return false;
  void* owner=mem<void*>(function,0x18); // UFunction::GetOuterUState, fingerprinted Core.
  auto state=owner?objectName(owner):L"";
  if(state==L"MountFinish")++mountFinishTicks;
  if(startup.mode==5&&profileFlag(L"replay-climb.flag")&&climbFixturePlaced&&(++climbObservedTicks%10==1)){
   Vec p=mem<Vec>(self,0x11c),v=mem<Vec>(self,field(self,L"Velocity",12,L"StructProperty"));Rot r=mem<Rot>(self,0x128),d=mem<Rot>(self,field(self,L"DesiredRotation",12,L"StructProperty"));
   if(climbBlock){auto b=objectField(self,L"Base");log("{\"event\":\"block_player_base\",\"step\":%u,\"base\":\"%ls\"}",controlsFrames,b?objectName(b).c_str():L"None");}
   void* anim=mem<void*>(self,field(self,L"AnimSequence",4,L"NameProperty"));
   log("{\"event\":\"climb_tick\",\"time_ms\":%llu,\"step\":%u,\"state\":\"%ls\",\"physics\":%u,\"position\":[%.5f,%.5f,%.5f],\"velocity\":[%.5f,%.5f,%.5f],\"body_yaw\":%d,\"desired_yaw\":%d,\"anim\":\"%ls\",\"frame\":%.5f}",GetTickCount64(),controlsFrames,state.c_str(),mem<BYTE>(self,field(self,L"Physics",1,L"ByteProperty")),p.x,p.y,p.z,v.x,v.y,v.z,r.yaw,d.yaw,anim?static_cast<wchar_t*>(anim)+6:L"None",mem<float>(self,field(self,L"AnimFrame",4,L"FloatProperty")));
  }
  if(isBroom(self)||state!=L"PlayerWalking"||!trackingOrigin.ready||menuVisible||cinemaActive||worldSceneActive||cardRewardActive)return false;
  bool allowed=(startup.mode==5||(hadUsableInput&&controlsArmed))&&
   !boolField(vrPlayer,L"bKeepStationary")&&!boolField(vrPlayer,L"bE3DemoLockout")&&
   !boolField(vrPlayer,L"bFixedFaceDirection")&&!boolField(vrPlayer,L"bLockedOnTarget")&&
   !boolField(vrPlayer,L"bIsCaptured");
  if(!allowed)return false; // Keep facing while idle too, before a standing jump.
  bool fix=!(startup.mode==5&&profileFlag(L"replay-stock-facing.flag"));
  RestoreFields camera;if(fix)aimCamera(camera,false);
  fpu.restore();originalDispatch(self,function,params,result);
  auto desired=mem<Rot>(self,field(self,L"DesiredRotation",12,L"StructProperty"));
  if(++locomotionTicks==1||locomotionTicks%120==0){
   auto body=mem<Rot>(self,0x128);auto position=mem<Vec>(self,0x11c);
   log("{\"event\":\"locomotion_facing\",\"corrected\":%s,\"head_yaw\":%d,\"desired_yaw\":%d,\"body_yaw\":%d,\"position\":[%.5f,%.5f,%.5f],\"mounts\":%u,\"mount_finish_ticks\":%u}",fix?"true":"false",movementYaw,desired.yaw,body.yaw,position.x,position.y,position.z,originalMountEvents,mountFinishTicks);
  }
  fpu.restore();return true;
 }catch(const std::exception& e){log("{\"event\":\"fatal\",\"stage\":\"locomotion_facing\",\"reason\":\"%s\"}",e.what());stopOwnProcess(139);}
}
// Bounded diagnostic fixture at the wall approached in the user's Willow session.
// Uses the stock FarMoveActor path behind SetLocation; initial yaw is fixture-only.
// Never enabled in normal VR or normal profiles.
static void* climbFunction(void* actor,const wchar_t* label,unsigned size){
 using MakeName=void(__thiscall*)(void*,const wchar_t*,int);using FindField=void*(__thiscall*)(void*,void*,int);
 void* name=nullptr;reinterpret_cast<MakeName>(targets.at("name"))(&name,label,0);
 void* fn=reinterpret_cast<FindField>(targets.at("field"))(actor,name,0);
 need(fn&&objectName(mem<void*>(fn,0x24))==L"Function"&&mem<WORD>(fn,0x7a)==size,"Climb diagnostic function signature differs");return fn;
}
static void placeClimbPlayer(Vec start){
 using FarMove=int(__thiscall*)(void*,void*,Vec,int,int);
 need(reinterpret_cast<FarMove>(targets.at("farmove"))(mem<void*>(vrPlayer,0x94),vrPlayer,start,0,0)!=0,"Climb diagnostic spawn is blocked");
 mem<Rot>(vrPlayer,0x128)=Rot{0,0,0};
 void* cam=objectField(vrPlayer,L"Cam");Rot yaw={0,0,0};originalDispatch(cam,climbFunction(cam,L"InitRotation",12),&yaw,nullptr);
}
static bool replayBlockClimb(){
 if(!profileFlag(L"replay-climb-block.flag"))return false;
 if(!climbBlock){
  auto array=GetProcAddress(GetModuleHandleW(L"Core.dll"),"?GObjObjects@UObject@@0V?$TArray@PAVUObject@@@@A");need(array!=nullptr,"Original object array missing");
  int count=mem<int>(reinterpret_cast<void*>(array),4);need(count>0&&count<1000000,"Original object count out of range");using Indexed=void*(__cdecl*)(int);
  for(int i=0;i<count;++i){void* o=reinterpret_cast<Indexed>(targets.at("indexedobject"))(i);if(o&&objectName(mem<void*>(o,0x24))==L"GridMover"&&objectName(o)==L"GridMover1"&&mem<void*>(o,0x94)==mem<void*>(vrPlayer,0x94)){need(!climbBlock,"Duplicate block fixture");climbBlock=o;}}
  need(climbBlock!=nullptr,"Willow spell block fixture missing");climbBlockInitial=mem<Vec>(climbBlock,0x11c);
  log("{\"event\":\"block_fixture\",\"name\":\"%ls\",\"position\":[%.5f,%.5f,%.5f],\"move_increment\":%.5f,\"projectile_target\":%s}",objectName(climbBlock).c_str(),climbBlockInitial.x,climbBlockInitial.y,climbBlockInitial.z,mem<float>(climbBlock,field(climbBlock,L"MoveIncrement",4,L"FloatProperty")),boolField(climbBlock,L"bProjTarget")?"true":"false");
  if(profileFlag(L"replay-climb-block-push.flag")){
   placeClimbPlayer(Vec{climbBlockInitial.x-180,climbBlockInitial.y,-483.5f});climbFixturePlaced=true;climbFixtureStart=controlsFrames;climbBlockPhase=1;return true;
  }
  climbBlockPhase=3;
 }
 Vec block=mem<Vec>(climbBlock,0x11c);
 if(climbBlockPhase==1){
  static unsigned observed=0;if(++observed%30==1)log("{\"event\":\"block_motion\",\"position\":[%.5f,%.5f,%.5f],\"interpolating\":%s}",block.x,block.y,block.z,boolField(climbBlock,L"bInterpolating")?"true":"false");
  if(block.x-climbBlockInitial.x<100||boolField(climbBlock,L"bInterpolating")||boolField(climbBlock,L"bDoingInterpolation"))return true;
  climbBlockPhase=3;
  log("{\"event\":\"block_push_complete\",\"position\":[%.5f,%.5f,%.5f]}",block.x,block.y,block.z);
 }
 if(climbBlockPhase==3){
  Vec start={block.x-180,block.y,-483.5f};placeClimbPlayer(start);originalMountEvents=mountFinishTicks=0;climbFixturePlaced=true;climbFixtureStart=controlsFrames;climbBlockPhase=4;
  log("{\"event\":\"climb_replay_start\",\"position\":[%.5f,%.5f,%.5f],\"head_yaw\":0,\"initial_body_yaw\":0,\"jump_pressed\":false}",start.x,start.y,start.z);return true;
 }
 Vec p=mem<Vec>(vrPlayer,0x11c);BYTE physics=mem<BYTE>(vrPlayer,field(vrPlayer,L"Physics",1,L"ByteProperty"));
 if(originalMountEvents&&mountFinishTicks&&physics==1&&objectField(vrPlayer,L"Base")==climbBlock&&p.z>block.z+100){
  climbFixtureDone=true;
  log("{\"event\":\"climb_replay_complete\",\"time_ms\":%llu,\"original_mounts\":%u,\"mount_finish_ticks\":%u,\"position\":[%.5f,%.5f,%.5f],\"base\":\"%ls\",\"jump_pressed\":true}",GetTickCount64(),originalMountEvents,mountFinishTicks,p.x,p.y,p.z,objectName(climbBlock).c_str());
 }
 return true;
}
static void replayClimb(){
 if(startup.mode!=5||!profileFlag(L"replay-climb.flag")||climbFixtureDone||cinemaActive||controlsFrames<240)return;
 if(boolField(vrPlayer,L"bKeepStationary")||boolField(vrPlayer,L"bIsCaptured"))return;
 if(replayBlockClimb())return;
 if(!climbFixturePlaced){
  using FarMove=int(__thiscall*)(void*,void*,Vec,int,int);
  bool jumpCase=profileFlag(L"replay-climb-jump.flag");Vec start=jumpCase?Vec{1200,-1536,-435.5f}:Vec{-400,1797.92249f,-595.5f};
  need(reinterpret_cast<FarMove>(targets.at("farmove"))(mem<void*>(vrPlayer,0x94),vrPlayer,start,0,0)!=0,"Climb diagnostic spawn is blocked");
  mem<Rot>(vrPlayer,0x128)=Rot{0,0,0};
  void* cam=objectField(vrPlayer,L"Cam");Rot yaw={0,0,0};originalDispatch(cam,climbFunction(cam,L"InitRotation",12),&yaw,nullptr);
  originalMountEvents=mountFinishTicks=0;climbFixturePlaced=true;climbFixtureStart=controlsFrames;
  log("{\"event\":\"climb_replay_start\",\"position\":[%.5f,%.5f,%.5f],\"head_yaw\":%d,\"initial_body_yaw\":%d,\"jump_pressed\":false}",start.x,start.y,start.z,32768,0);return;
 }
 Vec p=mem<Vec>(vrPlayer,0x11c);BYTE physics=mem<BYTE>(vrPlayer,field(vrPlayer,L"Physics",1,L"ByteProperty"));
 bool goal=profileFlag(L"replay-climb-course.flag")?(originalMountEvents>=2&&p.x<-860&&p.z>-480):profileFlag(L"replay-climb-jump.flag")?(p.x<990&&p.z>-320):(p.z>-570);
 if(originalMountEvents&&mountFinishTicks&&physics==1&&goal){climbFixtureDone=true;log("{\"event\":\"climb_replay_complete\",\"time_ms\":%llu,\"original_mounts\":%u,\"mount_finish_ticks\":%u,\"position\":[%.5f,%.5f,%.5f],\"jump_pressed\":%s}",GetTickCount64(),originalMountEvents,mountFinishTicks,p.x,p.y,p.z,profileFlag(L"replay-climb-jump.flag")?"true":"false");}
}
