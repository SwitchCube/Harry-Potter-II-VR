#pragma once
ReflectionBodyScope::ReflectionBodyScope(void* a):actor(a){offset=field(a,L"bBehindView",4,L"BoolProperty");using Find=void*(__cdecl*)(void*,const wchar_t*);void* property=reinterpret_cast<Find>(targets.at("property"))(mem<void*>(a,0x24),L"bBehindView");mask=mem<DWORD>(property,0x5c);need(mask&&!(mask&(mask-1)),"Invalid behind-view bit");saved=mem<DWORD>(a,offset)&mask;mem<DWORD>(a,offset)|=mask;}
ReflectionBodyScope::~ReflectionBodyScope(){mem<DWORD>(actor,offset)=(mem<DWORD>(actor,offset)&~mask)|saved;}
static hpvr::StepSmoother stepSmoother;static float stepOffset=0;
static Vec priorStepPosition={};static unsigned stepGeneration=~0u;static ULONGLONG stepTime=0;
static Vec playerViewAnchor(){Vec p=mem<Vec>(vrPlayer,0x11c);p.z+=mem<float>(vrPlayer,eyeHeightOffset)+vrSettings.eyeOffset+stepOffset;return p;}
static void advanceViewAnchor(void* actor){
 Vec p=mem<Vec>(actor,0x11c);ULONGLONG now=GetTickCount64();float dt=stepTime?(now-stepTime)*.001f:0;
 auto d=hpvr::sub(p,priorStepPosition);bool jump=stepGeneration!=levelGeneration||hpvr::dot(d,d)>10000;
 bool ground=!isBroom(actor)&&!worldSceneActive&&!lessonWorldActive&&!cardRewardActive&&actorState(actor)==L"PlayerWalking"&&mem<BYTE>(actor,field(actor,L"Physics",1,L"ByteProperty"))==1;
 stepOffset=stepSmoother.update(p.z,dt,ground,jump);stepTime=now;priorStepPosition=p;stepGeneration=levelGeneration;
 if(startup.mode==5&&profileFlag(L"replay-mechanics-stairs.flag"))log("{\"event\":\"stairs_view_sample\",\"raw_z\":%.6g,\"view_z\":%.6g,\"dt\":%.6g,\"grounded\":%s,\"phase\":%u}",p.z,p.z+stepOffset,dt,ground?"true":"false",mechanicsStage);
 if(startup.mode==5&&std::fabs(stepOffset)>.5f){static unsigned count=0;if(++count%60==1)log("{\"event\":\"step_view\",\"raw_z\":%.6g,\"view_offset\":%.6g,\"physics_unchanged\":true}",p.z,stepOffset);}
}
static bool vendorEngaged(){return vrPlayer&&objectField(vrPlayer,L"CurrVendorManager")!=nullptr;}
static bool vendorPrompt(){return vendorEngaged()&&gameConsole&&boolField(gameConsole,L"bVendorBar");}
static bool vendorYes=false,vendorReply=false,vendorButtonHeld=false,vendorStickHeld=false,vendorWasPrompt=false,vendorArmed=false;
static ULONGLONG vendorReplyAt=0;
static void vendorInput(bool usable,float lx,float ly,float rx,float ry,bool accept,bool cancel){
 bool prompt=vendorPrompt();if(!prompt||!usable||GetTickCount64()-vendorReplyAt>1000)vendorReply=false;
 if(prompt&&!vendorWasPrompt){vendorYes=false;vendorArmed=false;vendorStickHeld=false;vendorButtonHeld=true;log("{\"event\":\"vendor_prompt\",\"world_360\":true,\"npc_anchored\":true}");}
 vendorWasPrompt=prompt;if(!prompt)return;
 float axis=std::fabs(lx)>std::fabs(ly)?-lx:ly;float other=std::fabs(rx)>std::fabs(ry)?-rx:ry;if(std::fabs(other)>std::fabs(axis))axis=other;
 if(!usable){vendorArmed=false;return;}
 if(!vendorArmed){if(std::fabs(axis)<.25f&&!accept&&!cancel){vendorArmed=true;vendorButtonHeld=false;}return;}
 if(std::fabs(axis)<.25f)vendorStickHeld=false;
 if(std::fabs(axis)>.65f&&!vendorStickHeld){vendorYes=axis>0;vendorStickHeld=true;log("{\"event\":\"vendor_selection\",\"yes\":%s}",vendorYes?"true":"false");}
 if((accept||cancel)&&!vendorButtonHeld){if(cancel)vendorYes=false;vendorReply=true;vendorReplyAt=GetTickCount64();log("{\"event\":\"vendor_confirm\",\"yes\":%s}",vendorYes?"true":"false");}
 vendorButtonHeld=accept||cancel;
}
static bool vendorInternal(void* self,void* frame,void* result){
 if(startup.mode<3||startup.mode>5||!frame||!vendorEngaged())return false;void* manager=objectField(vrPlayer,L"CurrVendorManager");if(self!=manager)return false;
 auto name=objectName(mem<void*>(frame,4));
 if(name==L"IsMouseOverVendorYes"||name==L"IsMouseOverVendorNo"){if(result)*static_cast<int*>(result)=vendorPrompt()&&(vendorYes==(name==L"IsMouseOverVendorYes"));return true;}
 if(name==L"GetVendorBarX"||name==L"GetVendorBarY"){
  void* params=mem<void*>(frame,0x10);void* canvas=params?mem<void*>(params,0):nullptr;need(canvas&&result,"Vendor canvas missing");
  *static_cast<float*>(result)=0;return true;
 }
 if(name==L"PlayerInput"){
  RestoreFields reply;reply.set(vrPlayer,field(vrPlayer,L"bVendorReply",1,L"ByteProperty"),BYTE(vendorReply));
  bool pending=vendorReply;originalInternal(self,frame,result);
  if(pending){bool consumed=!vendorPrompt();log("{\"event\":\"vendor_reply_delivery\",\"consumed\":%s,\"state\":\"%ls\"}",consumed?"true":"false",actorState(self).c_str());if(consumed)vendorReply=false;}
  return true;
 }
 return false;
}

static void replayPolish(){
 static bool scanned=false;if(scanned)return;scanned=true;
 auto array=GetProcAddress(GetModuleHandleW(L"Core.dll"),"?GObjObjects@UObject@@0V?$TArray@PAVUObject@@@@A");int count=mem<int>(reinterpret_cast<void*>(array),4);using Indexed=void*(__cdecl*)(int);
 for(int i=0;i<count;++i){void* o=reinterpret_cast<Indexed>(targets.at("indexedobject"))(i);if(!o||!hasField(o,L"Location")||mem<void*>(o,0x94)!=mem<void*>(vrPlayer,0x94))continue;
  auto type=objectName(mem<void*>(o,0x24)),name=objectName(o);bool seller=hasField(o,L"CharacterSells");bool brush=hasField(o,L"PolyFlags");
  if(seller||brush||name.find(L"irror")!=std::wstring::npos){Vec p=mem<Vec>(o,0x11c);Rot r=mem<Rot>(o,0x128);void* tn=mem<void*>(o,field(o,L"Tag",4,L"NameProperty"));const wchar_t* tag=tn?static_cast<wchar_t*>(tn)+6:L"None";unsigned flags=brush?mem<DWORD>(o,field(o,L"PolyFlags",4,L"IntProperty")):0;
   log("{\"event\":\"polish_actor\",\"name\":\"%ls\",\"class\":\"%ls\",\"tag\":\"%ls\",\"position\":[%.5f,%.5f,%.5f],\"rotation\":[%d,%d,%d],\"sells\":%d,\"poly_flags\":%u}",name.c_str(),type.c_str(),tag,p.x,p.y,p.z,r.pitch,r.yaw,r.roll,seller?int(mem<BYTE>(o,field(o,L"CharacterSells",1,L"ByteProperty"))):-1,flags);
  }
 }
}

static void* namedFixture(const wchar_t* wanted){
 auto array=GetProcAddress(GetModuleHandleW(L"Core.dll"),"?GObjObjects@UObject@@0V?$TArray@PAVUObject@@@@A");int count=mem<int>(reinterpret_cast<void*>(array),4);using Indexed=void*(__cdecl*)(int);
 for(int i=0;i<count;++i){void* o=reinterpret_cast<Indexed>(targets.at("indexedobject"))(i);if(o&&objectName(o)==wanted&&hasField(o,L"Location")&&mem<void*>(o,0x94)==mem<void*>(vrPlayer,0x94))return o;}return nullptr;
}
static void fixturePosition(Vec p,int yaw){
 using FarMove=int(__thiscall*)(void*,void*,Vec,int,int);need(reinterpret_cast<FarMove>(targets.at("farmove"))(mem<void*>(vrPlayer,0x94),vrPlayer,p,0,0)!=0,"Private fixture position blocked");
 mem<Rot>(vrPlayer,0x128)={0,yaw,0};mem<Rot>(vrPlayer,field(vrPlayer,L"DesiredRotation",12,L"StructProperty"))={0,yaw,0};trackingOrigin.ready=false;
}
static void replayMirror(){
 static ULONGLONG began=0;static bool changed=false;
 if(!began){log("{\"event\":\"fixture_player_start\",\"goyle\":%s}",boolField(vrPlayer,L"bIsGoyle")?"true":"false");{ScopedScreenRelative human(vrPlayer,true,L"bIsGoyle",false);human.oldBit=0;}originalDispatch(vrPlayer,climbFunction(vrPlayer,L"SetNewMesh",0),nullptr,nullptr);fixturePosition({64,-1880,-19.5f},-16384);began=GetTickCount64();mechanicsAt=began;log("{\"event\":\"mirror_initial_model\",\"mesh\":\"%ls\",\"hidden\":%s,\"behind_view\":%s}",objectName(mem<void*>(vrPlayer,field(vrPlayer,L"Mesh",4,L"ObjectProperty"))).c_str(),boolField(vrPlayer,L"bHidden")?"true":"false",boolField(vrPlayer,L"bBehindView")?"true":"false");
  for(float x:{64.0f,-704.0f,480.0f})for(int y=-2400;y<=-900;y+=32){Vec p;if(traceMechanic({x,float(y),300},{x,float(y),-600},Vec{},p))log("{\"event\":\"stairs_surface\",\"x\":%.6g,\"y\":%d,\"z\":%.6g}",x,y,p.z);}
  log("{\"event\":\"mirror_fixture\",\"mover\":\"Mover50\",\"tag\":\"Mirror02\"}");
 }else if(!changed&&GetTickCount64()-began>8000){ScopedScreenRelative goyle(vrPlayer,true,L"bIsGoyle",true);goyle.oldBit=goyle.mask;originalDispatch(vrPlayer,climbFunction(vrPlayer,L"SetNewMesh",0),nullptr,nullptr);changed=true;log("{\"event\":\"mirror_goyle_fixture\"}");}
}
static unsigned vendorTestRound=0;static ULONGLONG vendorTestPromptAt=0;static bool vendorTestWas=false;
static void vendorReplayInput(){
 vrInput.move={};vrInput.turn={};vrInput.confirm=false;
 bool prompt=vendorPrompt();if(prompt&&!vendorTestWas)vendorTestPromptAt=GetTickCount64();vendorTestWas=prompt;if(!prompt)return;
 auto ms=GetTickCount64()-vendorTestPromptAt;
 if(ms>600&&ms<1000)vrInput.move.y=1;
 if(vendorTestRound==1&&ms>1300&&ms<1700)vrInput.turn.y=-1;
 if(ms>2100&&ms<2300)vrInput.confirm=true;
}
static void replayVendor(){
 static void* npc=nullptr;static ULONGLONG lastEnd=0;static bool active=false;static std::wstring previous;
 if(!npc){log("{\"event\":\"fixture_player_start\",\"goyle\":%s}",boolField(vrPlayer,L"bIsGoyle")?"true":"false");{ScopedScreenRelative human(vrPlayer,true,L"bIsGoyle",false);human.oldBit=0;}originalDispatch(vrPlayer,climbFunction(vrPlayer,L"SetNewMesh",0),nullptr,nullptr);npc=namedFixture(L"GOldMaleGry3");need(npc!=nullptr,"Original vendor fixture missing");}
 if(!vendorTestRound||(!vendorEngaged()&&active&&vendorTestRound<2&&GetTickCount64()-lastEnd>1000)){
  Vec p=mem<Vec>(npc,0x11c);fixturePosition(hpvr::add(p,Vec{80,0,0}),32768);
  void* manager=objectField(npc,L"managerVendor");
  if(!manager){std::wstring type=L"HGame.VendorManager";struct Arg{wchar_t* p;int n,c;}arg={&type[0],int(type.size()+1),int(type.size()+1)};
   {ScopedScreenRelative cheats(vrPlayer,true,L"bCheatsEnabled",true);originalDispatch(vrPlayer,fixtureBaseSummon(),&arg,nullptr);}manager=fixtureFind(L"VendorManager");need(manager!=nullptr,"Original vendor manager missing");mem<void*>(npc,field(npc,L"managerVendor",4,L"ObjectProperty"))=manager;
  }
  {ScopedScreenRelative show(npc,true,L"bHidden",false);show.oldBit=0;}
  mem<int>(npc,field(npc,L"nCurrIngrCount",4,L"IntProperty"))=2;
  void* statuses=objectField(vrPlayer,L"managerStatus");for(void* group=objectField(statuses,L"sgList");group;group=objectField(group,L"sgNext"))for(void* item=objectField(group,L"siList");item;item=objectField(item,L"siNext"))if(objectName(mem<void*>(item,0x24))==L"StatusItemJellybeans")mem<int>(item,field(item,L"nCount",4,L"IntProperty"))=100;
  originalDispatch(manager,climbFunction(manager,L"SetVendor",4),&npc,nullptr);
  void* saved=nullptr;using MakeName=void(__thiscall*)(void*,const wchar_t*,int);reinterpret_cast<MakeName>(targets.at("name"))(&saved,L"VendorIdle",0);
  originalDispatch(manager,climbFunction(manager,L"DoEngageVendor",4),&saved,nullptr);++vendorTestRound;active=false;
  log("{\"event\":\"vendor_fixture_engage\",\"round\":%u}",vendorTestRound);
 }
 if(vendorEngaged()){
  active=true;lastEnd=GetTickCount64();void* manager=objectField(vrPlayer,L"CurrVendorManager");auto state=actorState(manager);
  if(state!=previous){previous=state;log("{\"event\":\"vendor_stock_state\",\"state\":\"%ls\",\"price\":%d,\"purchases\":%d}",state.c_str(),mem<int>(manager,field(manager,L"nCurrPrice",4,L"IntProperty")),mem<int>(manager,field(manager,L"nItemsBoughtInCurrTransaction",4,L"IntProperty")));log("{\"event\":\"vendor_stock_inventory\",\"round\":%u,\"state\":\"%ls\",\"remaining\":%d}",vendorTestRound,state.c_str(),mem<int>(npc,field(npc,L"nCurrIngrCount",4,L"IntProperty")));}
 }
}

static void replayStairs(){
 if(!mechanicsStage){fixturePosition({-704,-1840,-19.0f},16384);mechanicsStage=1;log("{\"event\":\"stairs_fixture\",\"direction\":\"down\"}");}
 Vec p=mem<Vec>(vrPlayer,0x11c);
 if(mechanicsStage==1&&p.y>-1160){mechanicsStage=2;log("{\"event\":\"stairs_fixture\",\"direction\":\"up\",\"z\":%.6g}",p.z);}
 if(mechanicsStage==2&&p.y<-1830){mechanicsStage=3;log("{\"event\":\"stairs_complete\",\"z\":%.6g}",p.z);}
}
