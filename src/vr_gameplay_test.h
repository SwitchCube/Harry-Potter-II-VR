#pragma once
// All mutations here require the explicit hardware-free diagnostic profile flag.
static unsigned mechanicsStage=0;static ULONGLONG mechanicsAt=0;static void* mechanicsObject=nullptr;
static void* fixtureBaseSummon(){
 auto array=GetProcAddress(GetModuleHandleW(L"Core.dll"),"?GObjObjects@UObject@@0V?$TArray@PAVUObject@@@@A");int count=mem<int>(reinterpret_cast<void*>(array),4);using Indexed=void*(__cdecl*)(int);
 for(int i=0;i<count;++i){void* o=reinterpret_cast<Indexed>(targets.at("indexedobject"))(i);if(o&&objectName(mem<void*>(o,0x24))==L"Function"&&objectName(o)==L"Summon"&&objectName(mem<void*>(o,0x18))==L"PlayerPawn"){need(mem<WORD>(o,0x7a)==12,"Stock base summon signature");return o;}}
 throw std::runtime_error("Stock PlayerPawn.Summon missing");
}
static void* fixtureFind(const wchar_t* type){
 auto array=GetProcAddress(GetModuleHandleW(L"Core.dll"),"?GObjObjects@UObject@@0V?$TArray@PAVUObject@@@@A");need(array!=nullptr,"Fixture array missing");
 int count=mem<int>(reinterpret_cast<void*>(array),4);using Indexed=void*(__cdecl*)(int);void* found=nullptr;
 for(int i=0;i<count;++i){void* o=reinterpret_cast<Indexed>(targets.at("indexedobject"))(i);if(o&&objectName(mem<void*>(o,0x24))==type&&mem<void*>(o,0x94)==mem<void*>(vrPlayer,0x94)&&!boolField(o,L"bDeleteMe"))found=o;}return found;
}
static void mechanicReplayInput(float& forward,float& strafe,bool& jump,bool& cast,bool& menu,bool& boost,bool& walk){
 forward=strafe=0;jump=cast=menu=boost=walk=false;
 if(profileFlag(L"replay-mechanics-sword.flag")){swordReplayInput(forward,strafe,jump,cast);}
 else if(profileFlag(L"replay-mechanics-stairs.flag")){forward=mechanicsStage==1?1:mechanicsStage==2?-1:0;movementYaw=16384;}
 else if(profileFlag(L"replay-mechanics-mirror.flag")){jump=mechanicsAt&&GetTickCount64()-mechanicsAt>800&&GetTickCount64()-mechanicsAt<1100;}
 else if(profileFlag(L"replay-mechanics-broom.flag")){
  if(!mechanicsAt)return;unsigned phase=unsigned((GetTickCount64()-mechanicsAt)/1400);unsigned part=unsigned((GetTickCount64()-mechanicsAt)%1400);
  if(part<900){if(phase==0)strafe=.7f;if(phase==1)strafe=-.7f;if(phase==2)forward=.7f;if(phase==3)forward=-.7f;if(phase==4)boost=true;if(phase==5)walk=true;if(phase==6||phase==9||phase==11)jump=true;}
 }else if(profileFlag(L"replay-mechanics-carry.flag")){
  if((mechanicsStage==2||mechanicsStage==5)&&(GetTickCount64()-mechanicsAt>1800&&GetTickCount64()-mechanicsAt<2200||(mechanicsStage==2&&GetTickCount64()-mechanicsAt>3000&&GetTickCount64()-mechanicsAt<3400)))cast=true;
 }
}
static void replayMechanics(){
 if(startup.mode!=5||!profileFlag(L"replay-mechanics.flag")||!vrPlayer||controlsFrames<20)return;
 if(profileFlag(L"replay-mechanics-sword.flag")){replaySword();return;}
 if(profileFlag(L"replay-mechanics-stairs.flag")){replayStairs();return;}
 if(profileFlag(L"replay-mechanics-mirror.flag")){replayMirror();return;}
 if(profileFlag(L"replay-mechanics-vendor.flag")){replayVendor();return;}
 if(profileFlag(L"replay-mechanics-polish.flag")){replayPolish();return;}
 if(profileFlag(L"replay-mechanics-cinema.flag")){
  static std::vector<std::pair<void*,bool>> scenes;
  static bool isolated=false;if(!isolated){vrPresentation.load(startup.profile);vrPresentation.setSpatial(false);isolated=true;}
  if(!mechanicsStage){
   auto array=GetProcAddress(GetModuleHandleW(L"Core.dll"),"?GObjObjects@UObject@@0V?$TArray@PAVUObject@@@@A");int count=mem<int>(reinterpret_cast<void*>(array),4);using Indexed=void*(__cdecl*)(int);
   for(int i=0;i<count;++i){void* o=reinterpret_cast<Indexed>(targets.at("indexedobject"))(i);if(!o||objectName(mem<void*>(o,0x24))!=L"CutScene"||mem<void*>(o,0x94)!=mem<void*>(vrPlayer,0x94)||!boolField(o,L"bPlaying"))continue;scenes.push_back({o,boolField(o,L"bSkipAllowed")});ScopedScreenRelative flag(o,true,L"bSkipAllowed",false);flag.oldBit=0;}
   need(!scenes.empty(),"No original active cinematic fixture");mechanicsStage=1;mechanicsAt=GetTickCount64();log("{\"event\":\"unskippable_fixture_start\",\"active_scenes\":%u}",unsigned(scenes.size()));
  }else if(mechanicsStage==1&&GetTickCount64()-mechanicsAt>6000){
   need(worldSceneActive&&!cinemaActive,"Unskippable scene did not become stereo");log("{\"event\":\"unskippable_fixture_world\",\"frames\":%u}",vrFrames);
   for(auto row:scenes){ScopedScreenRelative flag(row.first,true,L"bSkipAllowed",row.second);flag.oldBit=row.second?flag.mask:0;}
   mechanicsStage=2;mechanicsAt=GetTickCount64();
  }else if(mechanicsStage==2&&GetTickCount64()-mechanicsAt>1000){need(cinemaActive&&!worldSceneActive,"Restored skippable scene did not return to cinema");mechanicsStage=3;mechanicsAt=GetTickCount64();log("{\"event\":\"unskippable_fixture_restored\"}");vrPresentation.setSpatial(true);VrPresentation persisted;persisted.load(startup.profile);need(persisted.spatialScenes,"Scene option did not persist");}
  else if(mechanicsStage==3&&GetTickCount64()-mechanicsAt>2500){need(worldSceneActive&&!cinemaActive&&skippableScene(vrPlayer),"Optional skippable scene did not become stereo");log(R"({"event":"spatial_option_fixture","spatial":true,"original_camera":true,"persisted":true})");vrPresentation.setSpatial(false);mechanicsStage=4;mechanicsAt=GetTickCount64();}
  else if(mechanicsStage==4&&GetTickCount64()-mechanicsAt>1500){need(cinemaActive&&!worldSceneActive,"Disabled 3D option did not restore screen");log(R"({"event":"spatial_option_fixture","spatial":false,"original_camera":true,"persisted":true})");mechanicsStage=5;}
  return;
 }
 if(profileFlag(L"replay-mechanics-broom.flag")){
  if(!isBroom(vrPlayer))return;if(!mechanicsAt&&(actorState(vrPlayer)!=L"PlayerWalking"||cinemaActive||worldSceneActive||boolField(vrPlayer,L"bIsCaptured")))return;if(!mechanicsAt){mechanicsAt=GetTickCount64();log("{\"event\":\"broom_fixture_start\"}");}
  unsigned phase=unsigned((GetTickCount64()-mechanicsAt)/1400);
  if(phase>=8&&phase<=11){
   static void* opponent=nullptr;if(!opponent){opponent=fixtureFind(L"Seeker");need(opponent!=nullptr,"Original seeker missing");originalDispatch(opponent,climbFunction(opponent,L"DoCapture",0),nullptr,nullptr);{ScopedScreenRelative visible(opponent,true,L"bHidden",false);visible.oldBit=0;}
    void* name=nullptr;using MakeName=void(__thiscall*)(void*,const wchar_t*,int);reinterpret_cast<MakeName>(targets.at("name"))(&name,L"Seeker",0);originalDispatch(vrPlayer,climbFunction(vrPlayer,L"SetKickTargetClass",4),&name,nullptr);
   }
   Vec offset=hpvr::yaw(Vec{0,phase<10?-45.0f:45.0f,0},mem<Rot>(vrPlayer,0x128).yaw*hpvr::pi/32768);
   using FarMove=int(__thiscall*)(void*,void*,Vec,int,int);if(!reinterpret_cast<FarMove>(targets.at("farmove"))(mem<void*>(opponent,0x94),opponent,hpvr::add(mem<Vec>(vrPlayer,0x11c),offset),0,0))return; // Retry only at a collision-free position on the live flight path.
   mem<Vec>(opponent,field(opponent,L"Velocity",12,L"StructProperty"))=Vec{};mem<Vec>(opponent,field(opponent,L"Acceleration",12,L"StructProperty"))=Vec{};
   originalDispatch(vrPlayer,climbFunction(vrPlayer,L"UpdateKickTarget",0),nullptr,nullptr);
  }
  static ULONGLONG observed=0;if(GetTickCount64()-observed>200){observed=GetTickCount64();auto r=mem<Rot>(vrPlayer,0x128);auto v=mem<Vec>(vrPlayer,field(vrPlayer,L"Velocity",12,L"StructProperty"));
   log("{\"event\":\"broom_fixture_sample\",\"elapsed\":%llu,\"state\":\"%ls\",\"yaw\":%d,\"pitch\":%d,\"view_yaw\":%d,\"input\":[%.4f,%.4f],\"velocity\":[%.5f,%.5f,%.5f],\"boost\":%u,\"brake\":%u,\"action\":%u}",observed-mechanicsAt,actorState(vrPlayer).c_str(),r.yaw,r.pitch,eyeRotation[0].yaw,requestedForward,requestedStrafe,v.x,v.y,v.z,mem<BYTE>(vrPlayer,field(vrPlayer,L"bBroomBoost",1,L"ByteProperty")),mem<BYTE>(vrPlayer,field(vrPlayer,L"bBroomBrake",1,L"ByteProperty")),mem<BYTE>(vrPlayer,field(vrPlayer,L"bBroomAction",1,L"ByteProperty")));}
  return;
 }
 if(profileFlag(L"replay-mechanics-carry.flag")){
  if(mechanicsStage==0||mechanicsStage==3){
   if(mechanicsStage==3&&mechanicsObject){originalDestroyActor(mem<void*>(mechanicsObject,0x94),mechanicsObject,0);mechanicsObject=nullptr;} // Remove only our completed diagnostic throw, so the second has a static landing surface.
   std::wstring className=mechanicsStage?L"HGame.HorklumpsHead":L"HGame.GNOME";
   struct StringArg{wchar_t* data;int size,capacity;}arg={&className[0],int(className.size()+1),int(className.size()+1)};
   {ScopedScreenRelative cheats(vrPlayer,true,L"bCheatsEnabled",true);originalDispatch(vrPlayer,fixtureBaseSummon(),&arg,nullptr);}
   mechanicsObject=fixtureFind(mechanicsStage?L"HorklumpsHead":L"GNOME");need(mechanicsObject!=nullptr,"Carry fixture spawn missing");
   {ScopedScreenRelative pickup(mechanicsObject,true,L"bObjectCanBePickedUp",true);void* o=mechanicsObject;originalDispatch(vrPlayer,climbFunction(vrPlayer,L"PickupActor",4),&o,nullptr);}
   ++mechanicsStage;mechanicsAt=GetTickCount64();log("{\"event\":\"carry_fixture_pickup\",\"class\":\"%ls\"}",objectName(mem<void*>(mechanicsObject,0x24)).c_str());
  }else if(mechanicsStage==1||mechanicsStage==4){if(carriedActor()==mechanicsObject&&actorState(vrPlayer)==L"PlayerWalking"){++mechanicsStage;mechanicCaptureIndex=mechanicsStage==2?1:2;mechanicsAt=GetTickCount64();log("{\"event\":\"carry_fixture_held\",\"class\":\"%ls\"}",objectName(mem<void*>(mechanicsObject,0x24)).c_str());}}
  else if(mechanicsStage==2||mechanicsStage==5){if(!carriedActor()&&GetTickCount64()-mechanicsAt>5000){++mechanicsStage;mechanicsAt=GetTickCount64();log("{\"event\":\"carry_fixture_released\",\"stage\":%u}",mechanicsStage);}}
  return;
 }
 if(profileFlag(L"replay-mechanics-reward.flag")){
  static ULONGLONG sampled=0;if(GetTickCount64()-sampled>500){sampled=GetTickCount64();void* sequence=mem<void*>(vrPlayer,field(vrPlayer,L"AnimSequence",4,L"NameProperty"));log("{\"event\":\"reward_fixture_sample\",\"state\":\"%ls\",\"progress\":%d,\"frame\":%.6g,\"rate\":%.6g,\"animation\":\"%ls\"}",actorState(vrPlayer).c_str(),mem<int>(vrPlayer,field(vrPlayer,L"nCelebrateProgress",4,L"IntProperty")),mem<float>(vrPlayer,field(vrPlayer,L"AnimFrame",4,L"FloatProperty")),mem<float>(vrPlayer,field(vrPlayer,L"AnimRate",4,L"FloatProperty")),sequence?static_cast<wchar_t*>(sequence)+6:L"None");}
  if(mechanicsStage==2&&GetTickCount64()-mechanicsAt<500)return;
  if(mechanicsStage==0||mechanicsStage==2){int bronze=mechanicsStage==0;originalDispatch(vrPlayer,climbFunction(vrPlayer,L"DoCelebrateCardSet",4),&bronze,nullptr);mechanicsAt=GetTickCount64();++mechanicsStage;}
  if((mechanicsStage==1||mechanicsStage==3)&&GetTickCount64()-mechanicsAt>2200&&GetTickCount64()-mechanicsAt<2350)mechanicCaptureIndex=mechanicsStage==1?3:4;
  if((mechanicsStage==1||mechanicsStage==3)&&GetTickCount64()-mechanicsAt>7000&&actorState(vrPlayer)==L"PlayerWalking"){log("{\"event\":\"reward_fixture_complete\",\"stage\":%u,\"state\":\"%ls\",\"cinema\":%s}",mechanicsStage,actorState(vrPlayer).c_str(),cinemaActive?"true":"false");mechanicsAt=GetTickCount64();++mechanicsStage;}
 }
}
