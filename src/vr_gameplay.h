#pragma once
// Integration with stock carry/throw, broom and cinematic states. No replacement
// pawn, save format, damage rules, challenge director or reward accounting.
static bool hasField(void* o,const wchar_t* n){
 if(!o)return false;using Find=void*(__cdecl*)(void*,const wchar_t*);
 return reinterpret_cast<Find>(targets.at("property"))(mem<void*>(o,0x24),n)!=nullptr;
}
static bool isBroom(void* o){return o&&objectName(mem<void*>(o,0x24))==L"BroomHarry";}
static std::wstring actorState(void* o){void* f=o?mem<void*>(o,0x0c):nullptr;void* s=f?mem<void*>(f,0x1c):nullptr;return s?objectName(s):L"None";}
static void* carriedActor(){return vrPlayer&&!isBroom(vrPlayer)?objectField(vrPlayer,L"CarryingActor"):nullptr;}
static Vec heldCentre(void* object){
 // The accepted wand calibration puts its handle centre in the physical fist.
 // Carryable models use their centre instead of the wand's lower endpoint.
 return hpvr::add(wandBase,hpvr::scale(wandForward,vrSettings.wandMetres*vrSettings.units*.15f));
}
static void* throwAuditObject=nullptr;static Vec throwAuditTarget={};
static Vec throwVelocity={},throwPoint={},throwStart={};static bool throwAimValid=false;
static bool traceMechanic(Vec from,Vec to,Vec extent,Vec& point){
 DWORD hit[11]={};float one=1;std::memcpy(&hit[9],&one,4);hit[10]=0xffffffff;
 // Original AActor::execTrace(true): 6 + 0x11, including actors and movers.
 bool clear=reinterpret_cast<LineCheck>(targets.at("line"))(mem<void*>(vrPlayer,0x94),hit,vrPlayer,to,from,23,extent,0)!=0;
 float time=1;std::memcpy(&time,&hit[9],4);need(std::isfinite(time)&&time>=0&&time<=1,"Invalid stock throw trace");
 point=hpvr::add(from,hpvr::scale(hpvr::sub(to,from),clear?1:time));return !clear;
}
static void updateThrowAim(){
 throwAimValid=false;void* o=carriedActor();if(!o||!wandValid||menuVisible||cinemaActive||worldSceneActive)return;
 Vec centre=heldCentre(o);float radius=mem<float>(o,field(o,L"CollisionRadius",4,L"FloatProperty")),height=mem<float>(o,field(o,L"CollisionHeight",4,L"FloatProperty"));
 radius=std::max(1.0f,std::min(radius,60.0f));height=std::max(1.0f,std::min(height,60.0f));
 if(objectName(mem<void*>(o,0x24))==L"GNOME"){radius=10;height=13;} // Original stateBeingThrown changes this cylinder.
 Vec extent={radius,radius,height},unused;
 Vec anchor=mem<Vec>(vrPlayer,0x11c);anchor.z+=mem<float>(vrPlayer,eyeHeightOffset)+vrSettings.eyeOffset;
 if(traceMechanic(anchor,centre,extent,unused))return;
 Vec target;traceMechanic(centre,hpvr::add(centre,hpvr::scale(wandForward,512)),Vec{},target);
 float distance=std::sqrt(hpvr::dot(hpvr::sub(target,centre),hpvr::sub(target,centre)));
 if(distance<radius+2)return;
 auto region=field(o,L"Region",12,L"StructProperty");void* zone=mem<void*>(o,region);need(zone!=nullptr,"Throw zone missing");
 Vec gravity=mem<Vec>(zone,field(zone,L"ZoneGravity",12,L"StructProperty"));
 float speed=hasField(o,L"fThrowVelocity")?mem<float>(o,field(o,L"fThrowVelocity",4,L"FloatProperty")):400;
 speed=std::max(200.0f,std::min(speed,1200.0f));float time=std::max(.2f,std::min(distance/speed,1.25f));
 throwStart=centre;throwVelocity=hpvr::velocityTo(centre,target,gravity,time);throwPoint=target;
 // Show the first collision of the actual ballistic arc, including object size,
 // so a ceiling or a nearer wall never leaves a misleading distant crosshair.
 Vec previous=centre;for(int i=1;i<=48;++i){Vec next=hpvr::trajectory(centre,throwVelocity,gravity,time*i/48);if(traceMechanic(previous,next,extent,throwPoint))break;previous=next;}
 throwAimValid=true;
}
CarryRenderScope::CarryRenderScope(){
 object=carriedActor();if(!object||!wandValid||menuVisible||cinemaActive||worldSceneActive){object=nullptr;return;}
 location=mem<Vec>(object,0x11c);rotation=mem<Rot>(object,0x128);
 boneOffset=field(object,L"AnimBone",1,L"ByteProperty");bone=mem<BYTE>(object,boneOffset);
 mem<BYTE>(object,boneOffset)=0;mem<Vec>(object,0x11c)=heldCentre(object);mem<Rot>(object,0x128)=wandRotation;
}
CarryRenderScope::~CarryRenderScope(){if(object&&carriedActor()==object&&!boolField(object,L"bDeleteMe")){mem<Vec>(object,0x11c)=location;mem<Rot>(object,0x128)=rotation;mem<BYTE>(object,boneOffset)=bone;}}
static bool carryFire(void* self,void* function){
 if(self!=vrPlayer||objectName(function)!=L"AltFire"||!carriedActor())return false;
 // The stock throw animation is retained. Starting it explicitly also lets a
 // cancelled (tracking lost / hand blocked) throw be retried with the next press.
 if(actorState(self)==L"PlayerWalking"&&!boolField(self,L"bThrow")){updateThrowAim();if(throwAimValid){
  void* channel=objectField(self,L"HarryAnimChannel");need(channel!=nullptr,"Carry animation channel missing");
  originalDispatch(channel,climbFunction(channel,L"GotoStateThrow",0),nullptr,nullptr);
  ScopedScreenRelative pending(self,true,L"bThrow",true);pending.oldBit=pending.mask;
 }}return true;
}
static bool throwingScope=false;
static bool handleThrow(void* self,void* function,void* frame,void* params,void* result){
 if(self!=vrPlayer||objectName(function)!=L"ThrowCarryingActor"||throwingScope)return false;
 void* o=carriedActor();if(!o||!boolField(vrPlayer,L"bThrow"))return false;
 if(startup.mode==5&&profileFlag(L"replay-mechanics-carry.flag")&&objectName(mem<void*>(o,0x24))==L"GNOME"){static bool injected=false;if(!injected){injected=true;wandValid=false;log("{\"event\":\"throw_tracking_loss_fixture\"}");}}
 updateThrowAim();
 if(!throwAimValid){ScopedScreenRelative cancel(vrPlayer,true,L"bThrow",false);cancel.oldBit=0;if(frame)originalInternal(self,frame,result);else originalDispatch(self,function,params,result);log("{\"event\":\"throw_cancelled\",\"reason\":\"blocked_or_untracked_hand\"}");return true;}
 Vec velocity=throwVelocity,target=throwPoint,start=throwStart;if(startup.mode==5){throwAuditObject=o;throwAuditTarget=target;}
 using FarMove=int(__thiscall*)(void*,void*,Vec,int,int);
 need(reinterpret_cast<FarMove>(targets.at("farmove"))(mem<void*>(o,0x94),o,start,0,0)!=0,"Held object placement failed");
 // Keep the stock detach, collision flags, Instigator and stateBeingThrown;
 // prevent the old camera-based auto-target selection from overriding the hand.
 throwingScope=true;{ScopedScreenRelative autoTarget(o,hasField(o,L"bAccurateThrowing"),L"bAccurateThrowing",false);
 if(frame)originalInternal(self,frame,result);else originalDispatch(self,function,params,result);}throwingScope=false;
 need(carriedActor()!=o,"Stock throw did not release object");
 mem<Vec>(o,field(o,L"Velocity",12,L"StructProperty"))=velocity;
 log("{\"event\":\"controller_throw\",\"object\":\"%ls\",\"class\":\"%ls\",\"start\":[%.6g,%.6g,%.6g],\"velocity\":[%.6g,%.6g,%.6g],\"crosshair\":[%.6g,%.6g,%.6g],\"state\":\"%ls\"}",objectName(o).c_str(),objectName(mem<void*>(o,0x24)).c_str(),start.x,start.y,start.z,velocity.x,velocity.y,velocity.z,target.x,target.y,target.z,actorState(o).c_str());
 if(startup.mode==5)log("{\"event\":\"throw_release_extent\",\"class\":\"%ls\",\"radius\":%.6g,\"height\":%.6g}",objectName(mem<void*>(o,0x24)).c_str(),mem<float>(o,field(o,L"CollisionRadius",4,L"FloatProperty")),mem<float>(o,field(o,L"CollisionHeight",4,L"FloatProperty")));
 if(startup.mode==3)vrInput.pulse(runtime,vrSettings.hapticStrength,.08f);return true;
}
static void broomAxes(RestoreFields& scope){
 float sensitivity=mem<float>(vrPlayer,field(vrPlayer,L"fJoyBroomSensitivity",4,L"FloatProperty"));
 need(std::isfinite(sensitivity)&&sensitivity>.0000001f&&sensitivity<10,"Broom sensitivity invalid");
 scope.set(vrPlayer,field(vrPlayer,L"aJoyBroomYaw",4,L"FloatProperty"),requestedStrafe/sensitivity);
 scope.set(vrPlayer,field(vrPlayer,L"aJoyBroomPitch",4,L"FloatProperty"),-requestedForward/sensitivity);
 for(auto n:{L"aBroomYaw",L"aBroomPitch"})scope.set(vrPlayer,field(vrPlayer,n,4,L"FloatProperty"),0.0f);
 for(auto n:{L"bBroomYawLeft",L"bBroomYawRight",L"bBroomPitchUp",L"bBroomPitchDown"})scope.set(vrPlayer,field(vrPlayer,n,1,L"ByteProperty"),BYTE(0));
 for(auto n:{L"ePitchControlDevice",L"eYawControlDevice"})scope.set(vrPlayer,field(vrPlayer,n,1,L"ByteProperty"),BYTE(2));
}
static void broomImpact(void* self,void* fn,void* params){
 if(startup.mode==3&&(self!=vrPlayer||!isBroom(self)))return;
 if(!params||objectName(fn)!=L"TakeDamage"||mem<WORD>(fn,0x7a)!=36)return;
 void* from=mem<void*>(params,4);void* type=mem<void*>(params,32);if(!from||!type||std::wcscmp(static_cast<wchar_t*>(type)+6,L"Kicked"))return;
 if(startup.mode==5&&from==vrPlayer&&isBroom(vrPlayer))log("{\"event\":\"broom_stock_contact\",\"target\":\"%ls\"}",objectName(mem<void*>(self,0x24)).c_str());
 if(startup.mode==3&&self==vrPlayer&&isBroom(self)){static ULONGLONG last=0;if(GetTickCount64()-last<180)return;last=GetTickCount64();Vec d=hpvr::yaw(hpvr::sub(mem<Vec>(from,0x11c),mem<Vec>(self,0x11c)),-mem<Rot>(self,0x128).yaw*hpvr::pi/32768);vrInput.pulseSide(runtime,d.y<0,vrSettings.hapticStrength,.16f);}
}
static bool mechanicsInternal(void* self,void* frame,void* result){
 if(startup.mode<3||startup.mode>5||!vrPlayer||!frame)return false;void* fn=mem<void*>(frame,4);if(swordHitAudit(self,fn,frame,nullptr,result))return true;broomImpact(self,fn,mem<void*>(frame,0x10));
 if((worldSceneActive||lessonWorldActive||cardRewardActive||vendorWorldActive||vendorEngaged())&&gameHud&&self==objectField(gameHud,L"managerCutScene")&&objectName(fn)==L"DrawBorder")return true;
 if(startup.mode==5&&self==vrPlayer&&isBroom(self)&&objectName(fn)==L"DoKick")log("{\"event\":\"broom_stock_kick\"}");
 if(carryFire(self,fn)||handleThrow(self,fn,frame,nullptr,result))return true;
 if(self==vrPlayer&&isBroom(self)&&(objectName(fn)==L"PlayerMove"||objectName(fn)==L"PlayerTrack")){
  RestoreFields axes;broomAxes(axes);originalInternal(self,frame,result);return true;
 }return false;
}
static bool mechanicsDispatch(void* self,void* fn,void* params,void* result){
 if(startup.mode<3||startup.mode>5||!vrPlayer)return false;
 if(swordTick(self,fn,params,result)||swordHitAudit(self,fn,nullptr,params,result))return true;
 broomImpact(self,fn,params);
 if(startup.mode==5&&self==throwAuditObject&&(objectName(fn)==L"HitWall"||objectName(fn)==L"Landed")){
  Vec p=mem<Vec>(self,0x11c);log("{\"event\":\"throw_first_contact\",\"class\":\"%ls\",\"handler\":\"%ls\",\"position\":[%.6g,%.6g,%.6g],\"target\":[%.6g,%.6g,%.6g],\"error\":%.6g}",objectName(mem<void*>(self,0x24)).c_str(),objectName(fn).c_str(),p.x,p.y,p.z,throwAuditTarget.x,throwAuditTarget.y,throwAuditTarget.z,std::sqrt(hpvr::dot(hpvr::sub(p,throwAuditTarget),hpvr::sub(p,throwAuditTarget))));log("{\"event\":\"throw_contact_extent\",\"class\":\"%ls\",\"radius\":%.6g,\"height\":%.6g}",objectName(mem<void*>(self,0x24)).c_str(),mem<float>(self,field(self,L"CollisionRadius",4,L"FloatProperty")),mem<float>(self,field(self,L"CollisionHeight",4,L"FloatProperty")));throwAuditObject=nullptr;
 }
 if((worldSceneActive||lessonWorldActive||cardRewardActive||vendorWorldActive||vendorEngaged())&&gameHud&&self==objectField(gameHud,L"managerCutScene")&&objectName(fn)==L"DrawBorder")return true;
 return carryFire(self,fn)||handleThrow(self,fn,nullptr,params,result);
}
static Rot worldSceneRotation={};
static void mechanicView(void* actor,const hpvr::Mat& h,Rot camera){
 static void* last=nullptr;static int previousYaw=0;static unsigned generation=~0u;static bool previousScene=false;
 if(generation!=levelGeneration){last=nullptr;previousScene=false;generation=levelGeneration;}
 if(worldSceneActive){if(!previousScene)trackingOrigin.recenter(h,camera.yaw*hpvr::pi/32768);else trackingOrigin.baseYaw=std::remainder(trackingOrigin.baseYaw+hpvr::yawDelta(worldSceneRotation.yaw,camera.yaw),2*hpvr::pi);worldSceneRotation=camera;}
 if(previousScene&&!worldSceneActive&&!cardRewardActive)trackingOrigin.ready=false;
 previousScene=worldSceneActive;bool broom=isBroom(actor)&&!worldSceneActive;
 int yaw=mem<Rot>(actor,0x128).yaw;
 if(broom&&last==actor&&trackingOrigin.ready)trackingOrigin.baseYaw=std::remainder(trackingOrigin.baseYaw+hpvr::yawDelta(previousYaw,yaw),2*hpvr::pi);
 if(broom&&last!=actor){trackingOrigin.recenter(h,yaw*hpvr::pi/32768);log("{\"event\":\"broom_vr\",\"flight_relative_head\":true,\"free_head_look\":true,\"analogue_stick\":true}");}
 last=broom?actor:nullptr;previousYaw=yaw;
}
static void pitchFlightEye(Vec anchor,Vec& position,Rot& rotation){
 if(!isBroom(vrPlayer)&&!worldSceneActive)return;Rot body=worldSceneActive?worldSceneRotation:mem<Rot>(vrPlayer,0x128);float yaw=body.yaw*hpvr::pi/32768,pitch=std::remainder(float(body.pitch),65536.0f)*hpvr::pi/32768;
 position=hpvr::add(anchor,hpvr::flightPitch(hpvr::sub(position,anchor),yaw,pitch));rotation=hpvr::pitchedHead(rotation,yaw,pitch);
}
static bool nearbyFlight[2]={};static Vec nearbyFlightPosition[2]={};
static void updateFlightCues(){
 nearbyFlight[0]=nearbyFlight[1]=false;if(!isBroom(vrPlayer)||cinemaActive||worldSceneActive||menuVisible)return;
 void* other=objectField(vrPlayer,L"KickTarget");if(!other||boolField(other,L"bDeleteMe"))return;
 Vec delta=hpvr::sub(mem<Vec>(other,0x11c),mem<Vec>(vrPlayer,0x11c));float yaw=mem<Rot>(vrPlayer,0x128).yaw*hpvr::pi/32768;Vec relative=hpvr::yaw(delta,-yaw);
 float range=mem<float>(vrPlayer,field(vrPlayer,L"TrackingOffsetRange_Horz",4,L"FloatProperty"))*2;
 if(hpvr::dot(delta,delta)>range*range||std::fabs(relative.y)<15||std::fabs(relative.z)>range*.7f)return;
 unsigned side=relative.y>0?1:0;nearbyFlight[side]=true;
 if(startup.mode==5&&profileFlag(L"replay-mechanics-broom.flag")){static unsigned mask=0;if(!(mask&(1u<<side))){mask|=1u<<side;log("{\"event\":\"broom_opponent_cue\",\"side\":%u,\"class\":\"%ls\",\"distance\":%.6g}",side,objectName(mem<void*>(other,0x24)).c_str(),std::sqrt(hpvr::dot(delta,delta)));}}nearbyFlightPosition[side]=mem<Vec>(other,0x11c);
 static ULONGLONG lastPulse[2]={};if(startup.mode==3&&GetTickCount64()-lastPulse[side]>1100){lastPulse[side]=GetTickCount64();vrInput.pulseSide(runtime,side==0,vrSettings.hapticStrength*.55f,.09f);}
}
static bool skippableScene(void* actor){
 // Refresh only four times per second; never keep scene pointers through travel/GC.
 static unsigned generation=~0u;static ULONGLONG checked=0;static bool skippable=false;static std::map<void*,bool> sceneClasses;
 if(generation==levelGeneration&&GetTickCount64()-checked<250)return skippable;
 if(generation!=levelGeneration)sceneClasses.clear();generation=levelGeneration;checked=GetTickCount64();skippable=false;
 auto array=GetProcAddress(GetModuleHandleW(L"Core.dll"),"?GObjObjects@UObject@@0V?$TArray@PAVUObject@@@@A");need(array!=nullptr,"Scene object array missing");
 int count=mem<int>(reinterpret_cast<void*>(array),4);need(count>0&&count<1000000,"Scene count invalid");using Indexed=void*(__cdecl*)(int);
 for(int i=0;i<count;++i){void* o=reinterpret_cast<Indexed>(targets.at("indexedobject"))(i);if(!o)continue;void* type=mem<void*>(o,0x24);if(!sceneClasses.count(type))sceneClasses[type]=hasField(o,L"bSkipAllowed")&&hasField(o,L"bPlaying")&&hasField(o,L"Location");if(!sceneClasses[type]||mem<void*>(o,0x94)!=mem<void*>(actor,0x94))continue;
  if(!boolField(o,L"bDeleteMe")&&boolField(o,L"bPlaying")&&boolField(o,L"bSkipAllowed"))skippable=true;
 }return skippable;
}

static const wchar_t* mechanicCapturePath(unsigned eye){
 static const wchar_t* names[7][2]={{L"native-carry-a.bmp",L"native-carry-b.bmp"},{L"native-mushroom-a.bmp",L"native-mushroom-b.bmp"},{L"native-bronze-a.bmp",L"native-bronze-b.bmp"},{L"native-silver-a.bmp",L"native-silver-b.bmp"},{L"native-mirror-a.bmp",L"native-mirror-b.bmp"},{L"native-goyle-a.bmp",L"native-goyle-b.bmp"},{L"native-sword-a.bmp",L"native-sword-b.bmp"}};
 return mechanicCaptureIndex>=1&&mechanicCaptureIndex<=7?names[mechanicCaptureIndex-1][eye]:nullptr;
}
static void noteCarryMesh(void* actor){
 if(startup.mode!=5||!profileFlag(L"replay-mechanics.flag")||actor!=carriedActor())return;
 static std::map<void*,unsigned> observed;unsigned bit=secondary?2:1;if(observed[actor]&bit)return;observed[actor]|=bit;
 Vec p=mem<Vec>(actor,0x11c),expected=heldCentre(actor);float error=std::sqrt(hpvr::dot(hpvr::sub(p,expected),hpvr::sub(p,expected)));
 log("{\"event\":\"held_original_mesh\",\"class\":\"%ls\",\"eye\":%u,\"hand_error\":%.6g}",objectName(mem<void*>(actor,0x24)).c_str(),secondary?1u:0u,error);need(error<.01f,"Original held model escaped controller pose");
}
