#pragma once
using ReadInput=void(__thiscall*)(void*,float,void*);
using Internal=void(__thiscall*)(void*,void*,void*);
using InputEvent=int(__thiscall*)(void*,void*,int,int,float);
using PreProcess=int(__thiscall*)(void*,int,int,float);
static PreProcess originalPreProcess=nullptr;
static bool syntheticHeld[256]={},sendingInput=false;
static ReadInput originalReadInput=nullptr;static Internal originalInternal=nullptr;
static Vec wandBase={},wandTip={},wandForward={},wandUp={};static Rot wandRotation={};
static bool lastSavePolled=false;
static bool wandValid=false,recenterHeld=false,controlsArmed=false,saveHeld=false,saveRequested=false;
static hpvr::TurnControl turnControl;
static hpvr::LessonStick lessonStick;static bool lessonActive=false;
static unsigned lessonExpected=0,lessonTestPhase=0;
static float requestedForward=0,requestedStrafe=0;static int movementYaw=0,replayPhase=-1;
static unsigned lastMovementReplay=~0u;
static void* currentInput=nullptr;static void* currentViewport=nullptr;
static void* gameHud=nullptr;
static void* commandOutput=nullptr;static void* gameConsole=nullptr;static bool menuVisible=false,lastInputMenu=false,hadUsableInput=false;static unsigned menuFrames=0;
static unsigned controlsFrames=0,camAimScopes=0,cursorTraceMatches=0,moveScopes=0;
static float lastTurnReported=0;static bool lastTurnActive=false;
static std::map<std::pair<void*,std::wstring>,unsigned> reflectedCache;
static unsigned field(void* object,const wchar_t* name,unsigned size,const wchar_t* type){
 auto key=std::make_pair(mem<void*>(object,0x24),std::wstring(name));auto it=reflectedCache.find(key);if(it!=reflectedCache.end())return it->second;
 unsigned offset=propertyOffset(object,name,size,type);reflectedCache[key]=offset;return offset;
}
static void* objectField(void* o,const wchar_t* n){return mem<void*>(o,field(o,n,4,L"ObjectProperty"));}
struct RestoreFields {
 struct Saved {void* object;unsigned offset;std::vector<BYTE> bytes;};std::vector<Saved> saved;
 template<class T>void set(void* o,unsigned offset,const T& value){Saved s={o,offset,std::vector<BYTE>(sizeof(T))};std::memcpy(s.bytes.data(),static_cast<BYTE*>(o)+offset,sizeof(T));saved.push_back(s);mem<T>(o,offset)=value;}
 ~RestoreFields(){for(auto i=saved.rbegin();i!=saved.rend();++i)std::memcpy(static_cast<BYTE*>(i->object)+i->offset,i->bytes.data(),i->bytes.size());}
};
// Only restore this property bit; other booleans can legitimately change inside PlayerMove.
struct ScopedScreenRelative {
 void* object=nullptr;unsigned offset=0;DWORD mask=0,oldBit=0;
 ScopedScreenRelative(void* actor,bool enable,const wchar_t* name=L"bScreenRelativeMovement",bool value=true){
  if(!enable)return;object=actor;offset=field(actor,name,4,L"BoolProperty");
  using FindProperty=void*(__cdecl*)(void*,const wchar_t*);
  void* property=reinterpret_cast<FindProperty>(targets.at("property"))(mem<void*>(actor,0x24),name);
  mask=mem<DWORD>(property,0x5c);need(mask&&!(mask&(mask-1)),"Invalid reflected boolean mask");
  oldBit=mem<DWORD>(object,offset)&mask;if(value)mem<DWORD>(object,offset)|=mask;else mem<DWORD>(object,offset)&=~mask;
 }
 ~ScopedScreenRelative(){if(object)mem<DWORD>(object,offset)=(mem<DWORD>(object,offset)&~mask)|oldBit;}
};
static bool boolField(void* object,const wchar_t* name){
 using FindProperty=void*(__cdecl*)(void*,const wchar_t*);
 unsigned offset=field(object,name,4,L"BoolProperty");
 void* property=reinterpret_cast<FindProperty>(targets.at("property"))(mem<void*>(object,0x24),name);
 DWORD mask=mem<DWORD>(property,0x5c);need(mask&&!(mask&(mask-1)),"Invalid reflected boolean");
 return (mem<DWORD>(object,offset)&mask)!=0;
}
static int __fastcall preProcessHook(void* self,void*,int key,int action,float delta){
 // Windows polls real key state each frame. Keep only our reserved held keys alive;
 // the matching explicit synthetic release always reaches the original path.
 if(key>=0&&key<256&&action==3&&syntheticHeld[key]&&!sendingInput)return 0;
 return originalPreProcess(self,key,action,delta);
}
static void setGameKey(void* input,void* viewport,int key,bool down){
 bool held=mem<BYTE>(input,0xeb0+key)!=0;
 if(down&&(syntheticHeld[key]||held))return;
 if(!down&&!syntheticHeld[key])return; // Only release keys that VR pressed.
 syntheticHeld[key]=down;sendingInput=true;
 reinterpret_cast<InputEvent>(targets.at("inputevent"))(currentEngine,viewport,key,down?1:3,0);
 sendingInput=false;++inputTransitions;
 log("{\"event\":\"input_key\",\"key\":%d,\"down\":%s}",key,down?"true":"false");
}
static hpvr::Mat cachedAimPose={},cachedGripPose={};static bool handPoseCached=false;
static void updateHand(const hpvr::Mat& rawPose,const hpvr::Mat& gripPose){
 cachedAimPose=rawPose;cachedGripPose=gripPose;handPoseCached=true;
 for(const auto& row:rawPose.m)for(float value:row)if(!std::isfinite(value)){wandValid=false;return;}
 for(const auto& row:gripPose.m)for(float value:row)if(!std::isfinite(value)){wandValid=false;return;}
 auto pose=hpvr::heldWandPose(gripPose,vrSettings.wandOffset,vrSettings.wandAngles,vrSettings.wandMetres);
 Vec anchor=playerViewAnchor();
 wandBase=trackingOrigin.position(pose,anchor,vrSettings.units);wandForward=hpvr::yaw(hpvr::convert(hpvr::scale(hpvr::axis(pose,2),-1)),trackingOrigin.baseYaw);
 wandUp=hpvr::yaw(hpvr::convert(hpvr::axis(pose,1)),trackingOrigin.baseYaw);wandRotation=hpvr::rotation(pose,trackingOrigin.baseYaw);
 wandTip=hpvr::add(wandBase,hpvr::scale(wandForward,vrSettings.wandMetres*vrSettings.units));
 wandValid=!headBlocked&&clearHead(vrPlayer,anchor,wandBase)&&clearHead(vrPlayer,wandBase,wandTip);
}
static void refreshHandForRender(){
 if(!handPoseCached||!wandValid)return;updateHand(cachedAimPose,cachedGripPose);
 if(startup.mode==5&&profileFlag(L"replay-mechanics-stairs.flag")){
  auto pose=hpvr::heldWandPose(cachedGripPose,vrSettings.wandOffset,vrSettings.wandAngles,vrSettings.wandMetres);
  Vec relative=trackingOrigin.position(pose,Vec{},vrSettings.units);auto error=hpvr::sub(hpvr::sub(wandBase,relative),playerViewAnchor());
  float distance=std::sqrt(hpvr::dot(error,error));need(distance<.01f,"Hand and eye use different simulation anchor");
  if(vrFrames%20==0)log("{\"event\":\"stairs_hand_alignment\",\"error\":%.6g}",distance);
 }
}
static void __fastcall readInputHook(void* self,void*,float delta,void* output){
 FpuState fpu;
 try{
  void* viewport=mem<void*>(self,0xea0);void* actor=viewport?mem<void*>(viewport,0x30):nullptr;
  requestedForward=requestedStrafe=0;currentInput=self;currentViewport=viewport;commandOutput=output;
  if(!vrPlayer||actor!=vrPlayer||!trackingOrigin.ready){originalReadInput(self,delta,output);return;}
  vrWeapon=objectField(actor,L"Weapon");gameHud=objectField(actor,L"myHUD");
  gameConsole=objectField(viewport,L"Console");
  menuVisible=gameConsole&&boolField(gameConsole,L"bUWindowActive")&&!vendorPrompt();
  bool lesson=objectField(actor,L"CurrSpellLesson")!=nullptr;if(lesson!=lessonActive){lessonActive=lesson;lessonStick=hpvr::LessonStick{};turnControl.held=false;log("{\"event\":\"lesson_controls\",\"active\":%s,\"both_sticks\":true}",lesson?"true":"false");}
  if(lastInputMenu&&!menuVisible)controlsArmed=false;lastInputMenu=menuVisible;
  if(controlsFrames==0){using KeyName=const wchar_t*(__thiscall*)(void*,int);auto keyName=reinterpret_cast<KeyName>(targets.at("keyname"));need(std::wstring(keyName(self,128))==L"F17"&&std::wstring(keyName(self,129))==L"F18","Virtual key names differ");}
  bool usable=false,runtimeAvailable=false;wandValid=false;handPoseCached=false;float forward=0,strafe=0;bool jump=false,cast=false,menu=false,confirm=false,potion=false,walk=false,map=false,boost=false;
  if(startup.mode==3){
   runtimeAvailable=runtime.sample(false)&&vrInput.poll(runtime);usable=runtimeAvailable;updateBindingPanel(runtimeAvailable);
   if(vrInput.quicksave!=lastSavePolled){log("{\"event\":\"quicksave_button\",\"down\":%s}",vrInput.quicksave?"true":"false");lastSavePolled=vrInput.quicksave;}
   if(usable&&!controlsArmed){
    bool neutral=std::hypot(vrInput.move.x,vrInput.move.y)<vrSettings.deadzone&&std::hypot(vrInput.turn.x,vrInput.turn.y)<.25f&&!vrInput.jump&&!vrInput.cast&&!vrInput.menu&&!vrInput.confirm&&!vrInput.potion&&!vrInput.walk&&!vrInput.map&&!vrInput.boost&&!vrInput.quicksave;
    controlsArmed=neutral;usable=controlsArmed;
   }
   if(usable){
    if(vrInput.recenter&&!recenterHeld){trackingOrigin.recenter(copyMatrix(runtime.poses[0].mDeviceToAbsoluteTracking),hpvr::heading(copyMatrix(runtime.poses[0].mDeviceToAbsoluteTracking))+trackingOrigin.baseYaw);vrMenu.placed=false;}
    recenterHeld=vrInput.recenter;
    if(vrInput.quicksave&&!saveHeld&&!menuVisible&&!boolField(actor,L"bIsCaptured"))saveRequested=true;
    saveHeld=vrInput.quicksave;
    float turn=vrInput.turn.bActive&&std::isfinite(vrInput.turn.x)?vrInput.turn.x:0;
    if(vrInput.turn.bActive!=lastTurnActive||std::fabs(turn-lastTurnReported)>.15f){
     log("{\"event\":\"turn_axis\",\"active\":%s,\"x\":%.6g}",vrInput.turn.bActive?"true":"false",turn);lastTurnActive=vrInput.turn.bActive;lastTurnReported=turn;
    }
    float step=turnControl.update((menuVisible||cinemaActive||worldSceneActive||cardRewardActive||vendorEngaged()||lesson||isBroom(actor))?0:turn,delta,vrSettings.smoothTurn,vrSettings.snapDegrees,vrSettings.smoothDegrees);
    if(step){hpvr::turnAtHead(trackingOrigin,copyMatrix(runtime.poses[0].mDeviceToAbsoluteTracking),step);
     if(!vrSettings.smoothTurn)log("{\"event\":\"snap_turn\",\"raw_x\":%.6g,\"degrees\":%.6g,\"base_yaw\":%.6g}",turn,step*180/hpvr::pi,trackingOrigin.baseYaw);
    }
    movementYaw=hpvr::rotation(copyMatrix(runtime.poses[0].mDeviceToAbsoluteTracking),trackingOrigin.baseYaw).yaw;
    const auto& hand=vrSettings.left?vrInput.left:vrInput.right;
    const auto& grip=vrSettings.left?vrInput.leftGrip:vrInput.rightGrip;
    if(hand.bActive&&hand.pose.bPoseIsValid&&hand.pose.bDeviceIsConnected&&grip.bActive&&grip.pose.bPoseIsValid&&grip.pose.bDeviceIsConnected){
     updateHand(copyMatrix(hand.pose.mDeviceToAbsoluteTracking),copyMatrix(grip.pose.mDeviceToAbsoluteTracking));
     if(controlsFrames%600==0){auto a=hpvr::translation(copyMatrix(hand.pose.mDeviceToAbsoluteTracking)),g=hpvr::translation(copyMatrix(grip.pose.mDeviceToAbsoluteTracking));log("{\"event\":\"grip_alignment\",\"aim\":[%.5f,%.5f,%.5f],\"grip\":[%.5f,%.5f,%.5f]}",a.x,a.y,a.z,g.x,g.y,g.z);}
    }
    auto move=vrInput.move.bActive?hpvr::radialStick(vrInput.move.x,vrInput.move.y,vrSettings.deadzone):hpvr::Stick2{0,0};
    forward=move.y*vrSettings.moveScale;strafe=move.x*vrSettings.moveScale;
    jump=vrInput.jump;cast=vrInput.cast;menu=vrInput.menu;confirm=vrInput.confirm;potion=vrInput.potion;walk=vrInput.walk;map=vrInput.map;boost=vrInput.boost;
   }else{vrInput.clear();controlsArmed=false;turnControl.held=false;recenterHeld=false;saveHeld=false;}
  }else if(startup.mode==5){
   updateBindingPanel(true);usable=true;auto hand=hpvr::identity();hand.m[0][3]=.2f;hand.m[1][3]=-.23f;hand.m[2][3]=-.30f;if(profileFlag(L"replay-mechanics-carry.flag")){hand.m[1][3]=-.10f;hand.m[2][3]=-.65f;}auto grip=hand;grip.m[1][3]-=.08f;updateHand(hand,grip);swordReplayHand(grip);
   movementYaw=eyeRotation[0].yaw;
   // Exercise all four directions, then a diagonal through the original PlayerMove.
   replayPhase=controlsFrames>=180&&controlsFrames<480?int((controlsFrames-180)/60):-1;
   unsigned part=controlsFrames>=180?(controlsFrames-180)%60:0;
   if(replayPhase>=0&&part<30){
    if(replayPhase==0)forward=.5f;
    if(replayPhase==1)forward=-.5f;
    if(replayPhase==2)strafe=-.5f;
    if(replayPhase==3)strafe=.5f;
    if(replayPhase==4){forward=.35355339f;strafe=.35355339f;}
   }
   cast=controlsFrames>=550&&controlsFrames<800&&wandValid;
   jump=controlsFrames>=850&&controlsFrames<855;
   if(controlsFrames>=920&&controlsFrames<960){unsigned direction=(controlsFrames-920)/10;if(direction==0)strafe=-1;if(direction==1)strafe=1;if(direction==2)forward=1;if(direction==3)forward=-1;}
   potion=controlsFrames>=970&&controlsFrames<975;walk=controlsFrames>=980&&controlsFrames<985;
   menu=controlsFrames==1000||controlsFrames==1200;
   if(controlsFrames==850||controlsFrames==900)log("{\"event\":\"jump_replay\",\"step\":%u,\"height\":%.6g}",controlsFrames,mem<Vec>(actor,0x11c).z);
   if(controlsFrames==180||controlsFrames==211)log("{\"event\":\"input_replay_position\",\"step\":%u,\"position\":[%.5f,%.5f,%.5f]}",controlsFrames,mem<Vec>(actor,0x11c).x,mem<Vec>(actor,0x11c).y,mem<Vec>(actor,0x11c).z);
  }
  if(startup.mode==5&&GetFileAttributesW((std::wstring(startup.profile)+L"\\replay-climb.flag").c_str())!=INVALID_FILE_ATTRIBUTES){
   forward=strafe=0;jump=cast=menu=potion=walk=map=boost=false;
   confirm=cinemaActive&&(controlsFrames%120==0);
   if(climbFixturePlaced&&!climbFixtureDone){
    bool jumpCase=profileFlag(L"replay-climb-jump.flag");movementYaw=32768;forward=controlsFrames>climbFixtureStart+30?1.0f:0.0f;
    static unsigned jumpAt=0;if(jumpCase&&!jumpAt&&mem<Vec>(actor,0x11c).x<1100)jumpAt=controlsFrames;
    jump=jumpAt&&controlsFrames-jumpAt<5&&!profileFlag(L"replay-climb-no-jump.flag");
    if(climbBlock){
     movementYaw=0;forward=0;jump=false;
     if(climbBlockPhase==1){
      auto p=mem<Vec>(actor,0x11c);wandTip=hpvr::add(p,Vec{20,0,20});wandBase=hpvr::sub(wandTip,Vec{14,0,0});wandForward={1,0,0};wandUp={0,0,1};wandRotation={0,0,0};wandValid=true;
      cast=controlsFrames>=climbFixtureStart+30&&controlsFrames<climbFixtureStart+100;
     }
     if(climbBlockPhase==4){
      forward=controlsFrames>climbFixtureStart+30?1.0f:0.0f;
      static unsigned blockJumpAt=0;
      if(!blockJumpAt){
       Vec p=mem<Vec>(actor,0x11c),v=mem<Vec>(actor,field(actor,L"Velocity",12,L"StructProperty"));
       if(profileFlag(L"replay-climb-block-standing.flag")){
        static ULONGLONG blockedSince=0;bool atWall=p.x>mem<Vec>(climbBlock,0x11c).x-90&&std::hypot(v.x,v.y)<1&&mem<BYTE>(actor,field(actor,L"Physics",1,L"ByteProperty"))==1;
        if(!atWall)blockedSince=0;else if(!blockedSince)blockedSince=GetTickCount64();
        if(blockedSince&&GetTickCount64()-blockedSince>=1000){blockJumpAt=controlsFrames;log("{\"event\":\"block_standing_jump\",\"waited_ms\":%llu,\"position\":[%.5f,%.5f,%.5f],\"speed\":%.5f,\"physics\":1}",GetTickCount64()-blockedSince,p.x,p.y,p.z,std::hypot(v.x,v.y));}
       }else if(p.x>mem<Vec>(climbBlock,0x11c).x-100)blockJumpAt=controlsFrames;
      }
      jump=blockJumpAt&&controlsFrames-blockJumpAt<5;
     }
    }
   }
  }
  if(startup.mode==5&&profileFlag(L"replay-lesson.flag")){
   forward=strafe=0;jump=cast=menu=confirm=potion=walk=map=boost=false;
   static ULONGLONG start=0;if(lesson&&!start)start=GetTickCount64();
   vrInput.move.x=vrInput.move.y=vrInput.turn.x=vrInput.turn.y=0;lessonExpected=0;
   if(start&&lesson){unsigned time=unsigned(GetTickCount64()-start);lessonTestPhase=(time/600)%8;unsigned dir=lessonTestPhase%4;
    if(time%600<300){float x=dir==2?-1:dir==3?1:0,y=dir==0?1:dir==1?-1:0;
     if(lessonTestPhase<4){vrInput.move.x=x;vrInput.move.y=y;}else{vrInput.turn.x=x;vrInput.turn.y=y;}
     lessonExpected=1u<<dir;
    }
   }
  }
  if(startup.mode==5&&profileFlag(L"replay-mechanics.flag")){potion=map=confirm=false;mechanicReplayInput(forward,strafe,jump,cast,menu,boost,walk);}
  if(startup.mode==5&&profileFlag(L"replay-mechanics-vendor.flag"))vendorReplayInput();
  vendorInput(usable,vrInput.move.x,vrInput.move.y,vrInput.turn.x,vrInput.turn.y,cast||confirm||jump||vrInput.confirm,menu);
  if(vendorEngaged()){forward=strafe=0;jump=cast=menu=confirm=potion=map=boost=walk=false;}
  bool focusPause=startup.mode==3&&hadUsableInput&&!runtimeAvailable&&!menuVisible;
  hadUsableInput=runtimeAvailable;
  statusInspect=usable&&!menuVisible&&vrInput.showHealth&&!cinemaActive&&!worldSceneActive&&!lesson&&!isBroom(actor);
  if(statusInspect){static bool pageLatched=false;float swipe=vrInput.move.x;if(std::fabs(swipe)<.3f)pageLatched=false;if(std::fabs(swipe)>.7f&&!pageLatched){statusPage=(statusPage+(swipe>0?1:2))%3;pageLatched=true;log("{\"event\":\"hand_status_page\",\"page\":%d}",statusPage);}forward=strafe=0;}
  bool gameplay=usable&&!menuVisible&&!menu&&!cinemaActive&&!worldSceneActive&&!cardRewardActive&&!lesson;
  unsigned lessonDirections=lessonStick.update({vrInput.move.x,vrInput.move.y},{vrInput.turn.x,vrInput.turn.y},usable&&!menuVisible&&!menu&&lesson);
  if(!gameplay)wandValid=false;
  if(focusPause)log("{\"event\":\"focus_pause_requested\"}");
  fpu.restore();setGameKey(self,viewport,128,gameplay&&jump);setGameKey(self,viewport,129,gameplay&&cast&&(wandValid||isBroom(actor)));setGameKey(self,viewport,130,gameplay&&potion);
  setGameKey(self,viewport,125,gameplay&&(vrInput.showHealth||(isBroom(actor)&&walk)));
  setGameKey(self,viewport,127,usable&&map);setGameKey(self,viewport,135,gameplay&&walk);setGameKey(self,viewport,126,gameplay&&boost);
  setGameKey(self,viewport,131,lesson?(lessonDirections&4)!=0:gameplay&&!isBroom(actor)&&strafe<-.55f);setGameKey(self,viewport,132,lesson?(lessonDirections&8)!=0:gameplay&&!isBroom(actor)&&strafe>.55f);
  setGameKey(self,viewport,133,lesson?(lessonDirections&1)!=0:gameplay&&!isBroom(actor)&&forward>.55f);setGameKey(self,viewport,134,lesson?(lessonDirections&2)!=0:gameplay&&!isBroom(actor)&&forward<-.55f);
  setGameKey(self,viewport,27,(usable&&menu&&!bindingPanelConsumed)||focusPause);setGameKey(self,viewport,13,usable&&!menuVisible&&(gameplay||cinemaActive||(worldSceneActive&&skippableScene(actor)))&&confirm);
  setGameKey(self,viewport,1,usable&&menuVisible&&!bindingPanelConsumed&&!menuPointerConsumed&&(confirm||cast));
  if(menuVisible){
   if(startup.mode==3&&usable){
    const auto& hand=vrSettings.left?vrInput.left:vrInput.right;
    bool pointed=hand.bActive&&hand.pose.bPoseIsValid&&hand.pose.bDeviceIsConnected&&vrMenu.point(copyMatrix(hand.pose.mDeviceToAbsoluteTracking));
    if(!pointed)vrMenu.moveCursor(strafe,forward,std::max(0.0f,std::min(delta,.05f)));
   }
   forward=strafe=0;wandValid=false;++menuFrames;
  }
  if(!gameplay){forward=strafe=0;}
  originalReadInput(self,delta,output);
  // PlayerInput rewrites movement axes after ReadInput. Apply locomotion at
  // PlayerMove instead, keeping the original tutorial/physics processing.
  if(usable){requestedForward=forward;requestedStrafe=strafe;}
  ++controlsFrames;
  if(startup.mode==3&&(controlsFrames==1||controlsFrames%600==0)){
   Vec position=mem<Vec>(actor,0x11c);
   log("{\"event\":\"vr_controls\",\"frame\":%u,\"usable\":%s,\"hand_valid\":%s,\"move\":[%.5f,%.5f],\"body_position\":[%.5f,%.5f,%.5f]}",controlsFrames,usable?"true":"false",wandValid?"true":"false",forward,strafe,position.x,position.y,position.z);
  }
  fpu.restore();
 }catch(const std::exception& e){log("{\"event\":\"fatal\",\"stage\":\"input\",\"reason\":\"%s\"}",e.what());stopOwnProcess(132);}
}
static void aimCamera(RestoreFields& scope,bool fromWand){
 void* cam=objectField(vrPlayer,L"Cam");need(cam!=nullptr&&cam!=vrPlayer,"Missing independent camera actor");
 Rot rot=fromWand?wandRotation:eyeRotation[0];
 if(!fromWand){rot.pitch=0;rot.roll=0;rot.yaw=movementYaw;} // Locomotion uses horizontal heading only.
 scope.set(cam,0x128,rot);
 if(fromWand){
  void* target=objectField(cam,L"CamTarget");need(target&&target!=vrPlayer,"Camera target must be independent actor");
  scope.set(cam,0x11c,attackOrigin());scope.set(target,0x11c,attackOrigin());
  scope.set(cam,field(cam,L"vForward",12,L"StructProperty"),wandForward);
 }
 ++camAimScopes;
}
static void __fastcall internalHook(void* self,void*,void* frame,void* result){
 if(frame){try{if(scaledHud(self,frame,result)||healthDraw(self,frame,result)||hideStatusItem(self,frame))return;beginHud(self,mem<void*>(frame,4));beginMenu(self,mem<void*>(frame,4));}catch(const std::exception& e){log("{\"event\":\"fatal\",\"stage\":\"hud_internal\",\"reason\":\"%s\"}",e.what());stopOwnProcess(137);}}
 // A trigger release during tracking/focus loss must not cast with the old camera.
 if(startup.mode==3&&self==vrWeapon&&!wandValid&&frame){
  void* node=mem<void*>(frame,4);const wchar_t* name=mem<wchar_t*>(node,0x20)+6;
  if(!std::wcscmp(name,L"CastSpell")){log("{\"event\":\"cast_blocked\",\"reason\":\"invalid_hand_or_blocked_origin\"}");return;}
 }
 try{if(vendorDraw(self,frame,result)||vendorInternal(self,frame,result)||mechanicsInternal(self,frame,result))return;}catch(const std::exception& e){log("{\"event\":\"fatal\",\"stage\":\"mechanics_internal\",\"reason\":\"%s\"}",e.what());stopOwnProcess(140);}
 if(!trackingOrigin.ready||!vrPlayer||(!wandValid&&self!=vrPlayer)){originalInternal(self,frame,result);return;}
 FpuState fpu;
 try{
  void* fn=mem<void*>(frame,4);const wchar_t* name=mem<wchar_t*>(fn,0x20)+6;
  bool cursor=!std::wcscmp(name,L"UpdateCursor")&&self==objectField(vrPlayer,L"SpellCursor");
  bool projectile=!std::wcscmp(name,L"ProjectileFire2")&&self==vrWeapon;
  bool tip=!std::wcscmp(name,L"GetWandEndPoint")&&self==vrWeapon;
  bool move=!std::wcscmp(name,L"PlayerMove")&&self==vrPlayer;
  if(!cursor&&!projectile&&!tip&&!move){originalInternal(self,frame,result);return;}
  RestoreFields scope;ScopedScreenRelative screenMovement(vrPlayer,move);
  if(move){++moveScopes;if(moveScopes==1)log("{\"event\":\"movement_reference\",\"previous_screen_relative\":%s,\"temporary_screen_relative\":true,\"mask\":%lu,\"offset\":%u}",screenMovement.oldBit?"true":"false",screenMovement.mask,screenMovement.offset);}
  if((cursor||projectile)&&wandValid)aimCamera(scope,true);else if(move)aimCamera(scope,false);
  if(cursor&&wandValid&&usingSword())scope.set(vrPlayer,field(vrPlayer,L"AimRotOffset",12,L"StructProperty"),Rot{0,0,0});
  Vec expectedOrigin=attackOrigin();
  if(projectile&&wandValid){
   float offset=-20;
   if(usingSword()){
    float span=mem<float>(vrWeapon,field(vrWeapon,L"fSwordFXTimeSpan",4,L"FloatProperty"));need(span>0,"Invalid sword charge span");
    offset=mem<float>(vrWeapon,field(vrWeapon,L"fSwordLength",4,L"FloatProperty"))*mem<float>(vrWeapon,field(vrWeapon,L"fSwordFXTime",4,L"FloatProperty"))/span;
    Vec target;traceMechanic(expectedOrigin,hpvr::add(expectedOrigin,hpvr::scale(wandForward,4096)),Vec{},target);
    scope.set(objectField(vrPlayer,L"SpellCursor"),0x11c,target);
   }
   scope.set(vrPlayer,field(vrPlayer,L"WeaponLoc",12,L"StructProperty"),hpvr::add(expectedOrigin,hpvr::scale(wandUp,offset)));
   scope.set(vrPlayer,field(vrPlayer,L"WeaponRot",12,L"StructProperty"),wandRotation);
  }
  if(move){
   float forward=requestedForward,side=requestedStrafe;
   bool stationary=boolField(vrPlayer,L"bKeepStationary")||boolField(vrPlayer,L"bE3DemoLockout");
   if(stationary){forward=0;side=0;}
   if((side<0&&boolField(vrPlayer,L"bLockOutStrafeLeft"))||(side>0&&boolField(vrPlayer,L"bLockOutStrafeRight")))side=0;
   float acceleration=mem<float>(vrPlayer,field(vrPlayer,L"AccelRate",4,L"FloatProperty"));
   need(std::isfinite(acceleration)&&acceleration>0&&acceleration<=100000,"Invalid player acceleration");
   mem<float>(vrPlayer,field(vrPlayer,L"aForward",4,L"FloatProperty"))+=(acceleration/.08f)*forward;
   mem<float>(vrPlayer,field(vrPlayer,L"aStrafe",4,L"FloatProperty"))+=(acceleration/.08f)*side;
   // Harry scales aForward/aStrafe by .08, but aSideMove by .1.
   mem<float>(vrPlayer,field(vrPlayer,L"aSideMove",4,L"FloatProperty"))+=(acceleration/.1f)*side;
  }
  fpu.restore();originalInternal(self,frame,result);
  if(move&&startup.mode==5&&replayPhase>=0&&requestedForward*requestedForward+requestedStrafe*requestedStrafe>0&&lastMovementReplay!=unsigned(replayPhase)){
   lastMovementReplay=unsigned(replayPhase);Vec a=mem<Vec>(vrPlayer,field(vrPlayer,L"Acceleration",12,L"StructProperty"));
   log("{\"event\":\"movement_replay\",\"phase\":%d,\"input\":[%.6g,%.6g],\"head_yaw\":%d,\"acceleration\":[%.6g,%.6g,%.6g]}",replayPhase,requestedForward,requestedStrafe,movementYaw,a.x,a.y,a.z);
  }
  if(move&&moveScopes%300==0){Vec acceleration=mem<Vec>(vrPlayer,field(vrPlayer,L"Acceleration",12,L"StructProperty"));
   log("{\"event\":\"movement_result\",\"head_yaw\":%d,\"acceleration\":[%.6g,%.6g,%.6g]}",eyeRotation[0].yaw,acceleration.x,acceleration.y,acceleration.z);
  }
  if(tip&&wandValid&&result){need(mem<WORD>(fn,0x7a)>=12,"Wand endpoint return size");*static_cast<Vec*>(result)=attackOrigin();}
  if(cursor&&wandValid){++wandCursorCalls;Vec start=mem<Vec>(self,field(self,L"vLOS_Start",12,L"StructProperty"));Vec targetTip=attackOrigin();bool matches=std::fabs(start.x-targetTip.x)<.001f&&std::fabs(start.y-targetTip.y)<.001f&&std::fabs(start.z-targetTip.z)<.001f;if(matches)++cursorTraceMatches;if((matches&&cursorTraceMatches==1)||wandCursorCalls==1){log("{\"event\":\"wand_cursor\",\"call\":%u,\"trace_matches_tip\":%s,\"tip\":[%.5f,%.5f,%.5f],\"actual_trace_start\":[%.5f,%.5f,%.5f]}",wandCursorCalls,matches?"true":"false",targetTip.x,targetTip.y,targetTip.z,start.x,start.y,start.z);}}
  if(projectile&&wandValid){++wandProjectileCalls;void* p=result?*static_cast<void**>(result):nullptr;log("{\"event\":\"wand_projectile\",\"spawned\":%s}",p?"true":"false");if(p){if(startup.mode==3){auto e=vrInput.pulse(runtime,vrSettings.hapticStrength,.055f);static bool warned=false;if(e!=EVRInputError_VRInputError_None&&!warned){log("{\"event\":\"haptic_unavailable\",\"error\":%d}",int(e));warned=true;}}Vec pos=mem<Vec>(p,0x11c);if(usingSword())log("{\"event\":\"sword_projectile\",\"origin_error\":%.6g,\"class\":\"%ls\",\"position\":[%.6g,%.6g,%.6g]}",std::sqrt(hpvr::dot(hpvr::sub(pos,expectedOrigin),hpvr::sub(pos,expectedOrigin))),objectName(mem<void*>(p,0x24)).c_str(),pos.x,pos.y,pos.z);need(std::fabs(pos.x-expectedOrigin.x)<.1f&&std::fabs(pos.y-expectedOrigin.y)<.1f&&std::fabs(pos.z-expectedOrigin.z)<.1f,"Original spell used a fallback origin; integration needs review");}}
  fpu.restore();
 }catch(const std::exception& e){log("{\"event\":\"fatal\",\"stage\":\"script_integration\",\"reason\":\"%s\"}",e.what());stopOwnProcess(133);}
}

using LoadMap=void*(__thiscall*)(void*,const void*,void*,const void*,void*);
static LoadMap originalLoadMap=nullptr;
static void* __fastcall loadMapHook(void* self,void*,const void* url,void* pending,const void* travel,void* error){
 FpuState fpu;
 ++levelLoadDepth;scriptedTextureFixture.clear();wandValid=false;requestedForward=requestedStrafe=0;
 if(currentInput&&currentViewport){
  for(int key=0;key<256;++key)if(syntheticHeld[key])setGameKey(currentInput,currentViewport,key,false);
 }
 currentInput=currentViewport=gameConsole=gameHud=nullptr;menuVisible=false;
 controlsArmed=false;hadUsableInput=false;lastInputMenu=false;turnControl.held=false;recenterHeld=false;
 vrPlayer=vrWeapon=vrFrame=nullptr;trackingOrigin.ready=false;vrRendering=false;
 saveHeld=saveRequested=false;
 if(startup.mode==3)runtime.compositor->ClearLastSubmittedFrame();
 closeUi();closeCinema();closeHud();vrMenu.close();menuBridge.close();bridge.verifiedMask=0;runtime.renderReady=false;lessonActive=false;lessonStick=hpvr::LessonStick{};reflectedCache.clear();headBlocked=false;vrProfile.active=false;vrProfile.previousEnd=0;
 ++levelGeneration;log("{\"event\":\"level_transition_begin\",\"generation\":%u}",levelGeneration);
 if(startup.mode==3)runtime.compositor->ClearLastSubmittedFrame();
 fpu.restore();void* result=originalLoadMap(self,url,pending,travel,error);--levelLoadDepth;
 log("{\"event\":\"level_transition_end\",\"generation\":%u,\"loaded\":%s}",levelGeneration,result?"true":"false");
 fpu.restore();return result;
}

static void originalCommand(const wchar_t* command,bool viewportCommand=false){
 using Exec=int(__thiscall*)(void*,const wchar_t*,void*);FpuState fpu;fpu.restore();
 auto logExport=GetProcAddress(GetModuleHandleW(L"Core.dll"),"?GLog@@3PAVFOutputDevice@@A");need(logExport!=nullptr,"Original log export missing");
 need(!viewportCommand||currentViewport,"Missing command viewport");
 bool resizing=allowViewportResize;allowViewportResize=viewportCommand;
 int handled=reinterpret_cast<Exec>(targets.at(viewportCommand?"viewportexec":"exec"))(static_cast<BYTE*>(viewportCommand?currentViewport:currentEngine)+(viewportCommand?0x2c:0x28),command,*reinterpret_cast<void**>(logExport));
 allowViewportResize=resizing;
 log("{\"event\":\"original_command\",\"command\":\"%ls\",\"handled\":%s}",command,handled?"true":"false");
 need(handled!=0,"Original game command not handled");fpu.restore();
}
static bool profileFlag(const wchar_t* name){return GetFileAttributesW((std::wstring(startup.profile)+L"\\"+name).c_str())!=INVALID_FILE_ATTRIBUTES;}
// Diagnostic-only call of the stock SavePoint.Touch handler. No replacement
// save implementation: the book performs its own active/health checks and calls
// the original PlayerPawn.SaveGame, including its normal queued processing.
static void replaySaveBook(){
 if(startup.mode!=5||!profileFlag(L"replay-book.flag")||controlsFrames<1350)return;
 static bool done=false;static unsigned lastAttempt=0;if(done||controlsFrames-lastAttempt<100)return;lastAttempt=controlsFrames;
 auto array=GetProcAddress(GetModuleHandleW(L"Core.dll"),"?GObjObjects@UObject@@0V?$TArray@PAVUObject@@@@A");need(array!=nullptr,"Original object array missing");
 int count=mem<int>(reinterpret_cast<void*>(array),4);need(count>0&&count<1000000,"Original object count out of range");
 using Indexed=void*(__cdecl*)(int);using MakeName=void(__thiscall*)(void*,const wchar_t*,int);using FindField=void*(__thiscall*)(void*,void*,int);
 for(int i=0;i<count;++i){void* o=reinterpret_cast<Indexed>(targets.at("indexedobject"))(i);if(!o||objectName(mem<void*>(o,0x24))!=L"SavePoint"||mem<void*>(o,0x94)!=mem<void*>(vrPlayer,0x94)||!boolField(o,L"bActive"))continue;
  void* name=nullptr;reinterpret_cast<MakeName>(targets.at("name"))(&name,L"Touch",0);
  void* fn=reinterpret_cast<FindField>(targets.at("field"))(o,name,0);need(fn&&objectName(mem<void*>(fn,0x24))==L"Function"&&mem<WORD>(fn,0x7a)==4,"Book Touch signature differs");
  log("{\"event\":\"save_book_replay\",\"book\":\"%ls\",\"handler\":\"Touch\",\"original_save\":true}",objectName(o).c_str());
  done=true;void* other=vrPlayer;originalDispatch(o,fn,&other,nullptr);return;
 }
}
static void replayTravel(){
 if(!commandOutput||active||levelLoadDepth||!currentViewport)return;
 static bool frontendResolution=false;
 if(startup.mode==3&&!frontendResolution&&profileFlag(L"frontend.flag")){frontendResolution=true;std::wstring command=L"setres "+std::to_wstring(vrSettings.renderSize)+L"x"+std::to_wstring(vrSettings.renderSize)+L"w";originalCommand(command.c_str(),true);return;}
 if(startup.mode==4&&vrPlayer&&vrFrames>400&&profileFlag(L"replay-look-around.flag")){
  static bool inspected=false;if(!inspected){inspected=true;auto array=GetProcAddress(GetModuleHandleW(L"Core.dll"),"?GObjObjects@UObject@@0V?$TArray@PAVUObject@@@@A");need(array!=nullptr,"Missing texture fixture array");
   int count=mem<int>(reinterpret_cast<void*>(array),4);need(count>0&&count<1000000,"Texture object count invalid");using Indexed=void*(__cdecl*)(int);
   for(int i=0;i<count;++i){void* o=reinterpret_cast<Indexed>(targets.at("indexedobject"))(i);if(!o||objectName(mem<void*>(o,0x24))!=L"ScriptedTexture")continue;
    void* notify=objectField(o,L"NotifyActor");if(notify&&objectName(mem<void*>(notify,0x24))==L"CeremonySTextures")scriptedTextureFixture.push_back(o);log("{\"event\":\"scripted_texture_inventory\",\"name\":\"%ls\",\"notify\":\"%ls\",\"actor_class\":\"%ls\"}",objectName(o).c_str(),notify?objectName(notify).c_str():L"None",notify?objectName(mem<void*>(notify,0x24)).c_str():L"None");
   }
  }
 }
 replayDesktopWindow();
 prepareLessonLevel();
 if(!vrPlayer)return;
 static bool resumeChecked=false;
 if(!resumeChecked&&vrFrames>=2){resumeChecked=true;if(profileFlag(L"resume-save.flag")){originalCommand(L"loadgame 0");return;}}
 if(saveRequested){
  saveRequested=false;
  if(!menuVisible&&!boolField(vrPlayer,L"bIsCaptured")){
   WIN32_FILE_ATTRIBUTE_DATA before[6]={};bool existed[6]={};
   auto savePath=[](unsigned slot){return std::wstring(startup.profile)+L"\\Save\\Slot"+std::to_wstring(slot+1)+L"\\Save0.usa";};
   for(unsigned slot=0;slot<6;++slot)existed[slot]=GetFileAttributesExW(savePath(slot).c_str(),GetFileExInfoStandard,&before[slot])!=0;
   originalCommand(L"savegame 0");bool saved=false;unsigned writtenSlot=0;
   for(unsigned slot=0;slot<6;++slot){WIN32_FILE_ATTRIBUTE_DATA after={};if(GetFileAttributesExW(savePath(slot).c_str(),GetFileExInfoStandard,&after)&&!(after.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)&&(after.nFileSizeHigh||after.nFileSizeLow>1024)&&(!existed[slot]||CompareFileTime(&before[slot].ftLastWriteTime,&after.ftLastWriteTime)!=0||before[slot].nFileSizeLow!=after.nFileSizeLow||before[slot].nFileSizeHigh!=after.nFileSizeHigh)){saved=true;writtenSlot=slot+1;}}
   log("{\"event\":\"quicksave\",\"saved\":%s,\"slot\":%u}",saved?"true":"false",writtenSlot);
   if(saved&&startup.mode==3)vrInput.pulse(runtime,vrSettings.hapticStrength,.1f);
  }
 }
 if(startup.mode!=5)return;
 replayClimb();replayLesson();replayMechanics();
 replaySaveBook();
 static unsigned stage=0,saveStage=0,resizeStage=0;
 if(profileFlag(L"replay-resize.flag")&&resizeStage<2&&controlsFrames>=(resizeStage?1450u:1250u)){
  originalCommand(resizeStage?L"setres 2048x2048w":L"setres 1280x960w",true);++resizeStage;return;
 }
 if(profileFlag(L"replay-save.flag")&&saveStage<2&&controlsFrames>=(saveStage?2800u:2100u)){
  originalCommand(saveStage?L"loadgame 0":L"savegame 0");++saveStage;return;
 }
 if(!profileFlag(L"replay-travel.flag")||stage>=2||controlsFrames<(stage?2600u:1600u))return;
 const wchar_t* command=stage?L"open Adv8Forest.unr":L"open Grounds_Night.unr";++stage;
 log("{\"event\":\"travel_replay\",\"stage\":%u}",stage);
 originalCommand(command);
}
