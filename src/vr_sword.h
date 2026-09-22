#pragma once
// The real Gryffindor sword uses local -Z as its blade axis. Its grip centre
// was measured from the pinned source model; all placements use the accepted fist.
static bool usingSword(){return vrPlayer&&vrWeapon&&boolField(vrPlayer,L"bHarryUsingSword")&&boolField(vrWeapon,L"bUsingSword");}
static bool canDrawHandSword(){return usingSword()&&wandValid&&!menuVisible&&!cardRewardActive&&!worldSceneActive;}
static Vec swordModelPoint(Vec p){return hpvr::swordPoint(p,heldCentre(nullptr),wandForward,wandUp,vrSettings.units);}
static Vec attackOrigin(){return usingSword()?swordModelPoint({-.43018f,-.31240f,-55.82732f}):wandTip;}
static Rot swordRotation(){return hpvr::swordRot(wandForward,wandUp);}
SwordRenderScope::SwordRenderScope(){
 if(!usingSword()||!wandValid||menuVisible||cinemaActive||worldSceneActive||cardRewardActive)return;
 object=vrWeapon;location=mem<Vec>(object,0x11c);rotation=mem<Rot>(object,0x128);bone=mem<BYTE>(object,field(object,L"AnimBone",1,L"ByteProperty"));scale=mem<float>(object,field(object,L"DrawScale",4,L"FloatProperty"));
 weaponLocation=mem<Vec>(vrPlayer,field(vrPlayer,L"WeaponLoc",12,L"StructProperty"));weaponRotation=mem<Rot>(vrPlayer,field(vrPlayer,L"WeaponRot",12,L"StructProperty"));
 mem<Vec>(object,0x11c)=swordModelPoint({0,0,0});mem<Rot>(object,0x128)=swordRotation();mem<BYTE>(object,field(object,L"AnimBone",1,L"ByteProperty"))=0;mem<float>(object,field(object,L"DrawScale",4,L"FloatProperty"))=vrSettings.units/50;
 mem<Vec>(vrPlayer,field(vrPlayer,L"WeaponLoc",12,L"StructProperty"))=mem<Vec>(object,0x11c);mem<Rot>(vrPlayer,field(vrPlayer,L"WeaponRot",12,L"StructProperty"))=mem<Rot>(object,0x128);
}
SwordRenderScope::~SwordRenderScope(){if(!object)return;mem<Vec>(object,0x11c)=location;mem<Rot>(object,0x128)=rotation;mem<BYTE>(object,field(object,L"AnimBone",1,L"ByteProperty"))=bone;mem<float>(object,field(object,L"DrawScale",4,L"FloatProperty"))=scale;mem<Vec>(vrPlayer,field(vrPlayer,L"WeaponLoc",12,L"StructProperty"))=weaponLocation;mem<Rot>(vrPlayer,field(vrPlayer,L"WeaponRot",12,L"StructProperty"))=weaponRotation;}
static void noteSwordMesh(void* actor){
 if(actor!=vrWeapon||!usingSword()||!wandValid||worldSceneActive)return;static unsigned mask=0;unsigned bit=secondary?2:1;if(mask&bit)return;mask|=bit;
 Vec centre=hpvr::add(mem<Vec>(actor,0x11c),hpvr::scale(hpvr::rotateModel(hpvr::swordGripPoint(),mem<Rot>(actor,0x128)),mem<float>(actor,field(actor,L"DrawScale",4,L"FloatProperty"))));float error=std::sqrt(hpvr::dot(hpvr::sub(centre,heldCentre(nullptr)),hpvr::sub(centre,heldCentre(nullptr))));
 log(R"({"event":"sword_mesh","eye":%u,"mesh":"%ls","grip_error":%.6g,"blade_length":%.6g})",secondary?1:0,objectName(objectField(actor,L"Mesh")).c_str(),error,57.542944*vrSettings.units/50);
 if(startup.mode==5&&profileFlag(L"replay-mechanics-sword.flag")){need(error<.01f,"Sword handle escaped physical grip");mechanicCaptureIndex=7;}
}
static bool swordTick(void* self,void* fn,void* params,void* result){
 if(self!=vrWeapon||objectName(fn)!=L"Tick"||!canDrawHandSword())return false;
 SwordRenderScope held;originalDispatch(self,fn,params,result);return true;
}
// Only the explicit hardware-free fixture aims automatically. The actual VR
// path always takes the user's tracked controller, including the accepted calibration.
static void swordReplayHand(hpvr::Mat& grip){
 if(startup.mode!=5||!profileFlag(L"replay-mechanics-sword.flag")||!usingSword()||cinemaActive||worldSceneActive)return;
 void* boss=fixtureFind(L"Basilisk");if(!boss)return;void* head=objectField(boss,L"_BasiliskHeadColObj");if(!head)return;
 Vec forward=hpvr::inverseConvert(hpvr::yaw(hpvr::sub(mem<Vec>(head,0x11c),heldCentre(nullptr)),-trackingOrigin.baseYaw));float length=std::sqrt(hpvr::dot(forward,forward));if(length<1)return;forward=hpvr::scale(forward,1/length);
 Vec right={-forward.z,0,forward.x};length=std::sqrt(hpvr::dot(right,right));if(length<.001f)return;right=hpvr::scale(right,1/length);Vec up={right.y*forward.z-right.z*forward.y,right.z*forward.x-right.x*forward.z,right.x*forward.y-right.y*forward.x};
 hpvr::Mat desired={{{right.x,up.x,-forward.x,0},{right.y,up.y,-forward.y,0},{right.z,up.z,-forward.z,0}}};
 auto calibration=hpvr::wandPose(hpvr::identity(),Vec{},vrSettings.wandAngles),inverse=hpvr::identity();
 for(int row=0;row<3;++row)for(int col=0;col<3;++col)inverse.m[row][col]=calibration.m[col][row];
 auto oriented=hpvr::multiply(desired,inverse);for(int row=0;row<3;++row)for(int col=0;col<3;++col)grip.m[row][col]=oriented.m[row][col];
 updateHand(grip,grip);
}
static bool swordHitAudit(void* self,void* fn,void* frame,void* params,void* result){
 if(startup.mode!=5||objectName(fn)!=L"BasilHitBySpell"||objectName(mem<void*>(self,0x24))!=L"Basilisk"||!profileFlag(L"replay-mechanics-sword.flag"))return false;
 int before=mem<int>(self,field(self,L"Health",4,L"IntProperty"));
 if(frame)originalInternal(self,frame,result);else originalDispatch(self,fn,params,result);
 log(R"({"event":"sword_boss_hit","health_before":%d,"health_after":%d,"second_phase":%s})",before,mem<int>(self,field(self,L"Health",4,L"IntProperty")),boolField(self,L"bDidFirstBattle")?"true":"false");return true;
}
static ULONGLONG swordFixtureAt=0,swordIntroAt=0;
static void* swordIntroScene=nullptr;
static void swordReplayInput(float& forward,float& strafe,bool& jump,bool& cast){
 forward=strafe=0;jump=cast=false;if(!swordFixtureAt)return;unsigned ms=unsigned(GetTickCount64()-swordFixtureAt);unsigned phase=ms/4000;
 if(ms%4000<2400)cast=true;
 if(phase==1)strafe=.35f;if(phase==2)strafe=-.35f;if(phase==3&&ms%4000<1200)forward=.3f;if(phase==4&&ms%4000<1200)forward=-.3f;
 jump=phase==5&&ms%4000<100;
}
static void replaySword(){
 if(levelGeneration<1)return;
 static bool started=false;if(!started){
  auto array=GetProcAddress(GetModuleHandleW(L"Core.dll"),"?GObjObjects@UObject@@0V?$TArray@PAVUObject@@@@A");int count=mem<int>(reinterpret_cast<void*>(array),4);using Indexed=void*(__cdecl*)(int);
  for(int i=0;i<count;++i){void* o=reinterpret_cast<Indexed>(targets.at("indexedobject"))(i);if(!o||objectName(mem<void*>(o,0x24))!=L"CutScene"||mem<void*>(o,0x94)!=mem<void*>(vrPlayer,0x94))continue;
   wchar_t* filename=mem<wchar_t*>(o,field(o,L"FileName",12,L"StrProperty"));if(filename&&std::wcsstr(filename,L"17150BasiliskIntro")){originalDispatch(o,climbFunction(o,L"Play",0),nullptr,nullptr);started=true;swordIntroScene=o;swordIntroAt=GetTickCount64();log(R"({"event":"sword_intro_fixture","original_scene":"%ls"})",filename);break;}
  }need(started,"Original Basilisk intro was not found");
 }
 static bool skipped=false;if(!skipped&&swordIntroScene&&GetTickCount64()-swordIntroAt>15000&&actorState(swordIntroScene)==L"Running"){
  need(boolField(swordIntroScene,L"bSkipAllowed"),"Original Basilisk intro must permit skip");originalDispatch(swordIntroScene,climbFunction(swordIntroScene,L"FastForward",0),nullptr,nullptr);skipped=true;log(R"({"event":"sword_intro_skip","original_fastforward":true})");
 }
 if(!swordFixtureAt){
  if(!usingSword()||cinemaActive||worldSceneActive||actorState(vrPlayer)!=L"PlayerWalking")return;
  swordFixtureAt=GetTickCount64();log(R"({"event":"sword_fixture","stock_sword":true,"stock_intro_completed":true})");
 }
 static ULONGLONG sampled=0;if(GetTickCount64()-sampled>400){sampled=GetTickCount64();Vec p=mem<Vec>(vrPlayer,0x11c);log(R"({"event":"sword_sample","elapsed":%llu,"state":"%ls","charge":%.6g,"position":[%.6g,%.6g,%.6g],"input":[%.6g,%.6g]})",sampled-swordFixtureAt,actorState(vrPlayer).c_str(),mem<float>(vrWeapon,field(vrWeapon,L"fSwordFXTime",4,L"FloatProperty")),p.x,p.y,p.z,requestedForward,requestedStrafe);}
}
