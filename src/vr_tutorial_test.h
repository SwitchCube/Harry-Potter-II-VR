#pragma once
// Explicit hardware-free HP2 fixture, never a physical-headset claim.
static bool tutorialRemapped=false;
static VR_IVRInput_FnTable tutorialFakeInput={};
static VR_IVRCompositor_FnTable tutorialFakeCompositor={};
static VR_IVRSystem_FnTable tutorialFakeSystem={};
static VR_IVRRenderModels_FnTable tutorialFakeModels={};
static RenderModel_Vertex_t tutorialFakeVertices[8]={};
static uint16_t tutorialFakeIndices[]={0,1,2,0,2,3,4,6,5,4,7,6,0,4,5,0,5,1,2,6,7,2,7,3,0,3,7,0,7,4,1,5,6,1,6,2};
static RenderModel_t tutorialFakeMesh={tutorialFakeVertices,8,tutorialFakeIndices,12,0};
static bool OPENVR_FNTABLE_CALLTYPE tutorialFakeReady(){return true;}
static void OPENVR_FNTABLE_CALLTYPE tutorialFakeClear(){}
static EVRInputError OPENVR_FNTABLE_CALLTYPE tutorialFakeBinding(VRActionHandle_t action,InputBindingInfo_t* out,uint32_t size,uint32_t count,uint32_t* returned){
 need(size==sizeof(*out)&&count>=1,"Binding ABI");*out={};*returned=1;
 bool inventory=action==10;
 strcpy(out->rchDevicePathName,inventory?(tutorialRemapped?"/user/hand/right":"/user/hand/left"):action==11||tutorialRemapped?"/user/hand/left":"/user/hand/right");
 strcpy(out->rchInputPathName,inventory?(tutorialRemapped?"/input/trigger":"/input/grip"):tutorialRemapped?"/input/grip":action==1?"/input/a":action==2?"/input/trigger":"/input/joystick");strcpy(out->rchSlotName,action==11?"position":"click");return EVRInputError_VRInputError_None;
}
static EVRInputError OPENVR_FNTABLE_CALLTYPE tutorialFakeOrigins(VRActionSetHandle_t,VRActionHandle_t action,VRInputValueHandle_t* out,uint32_t count){need(count>0,"Origin count");out[0]=action;return EVRInputError_VRInputError_None;}
static EVRInputError OPENVR_FNTABLE_CALLTYPE tutorialFakeInfo(VRInputValueHandle_t origin,InputOriginInfo_t* out,uint32_t size){
 need(size==sizeof(*out),"Origin ABI");*out={};out->trackedDeviceIndex=origin==10?(tutorialRemapped?1:2):origin==11||tutorialRemapped?2:1;out->devicePath=out->trackedDeviceIndex;
 strcpy(out->rchRenderModelComponentName,origin==10?(tutorialRemapped?"trigger":"grip"):tutorialRemapped?"grip":origin==1?"a":origin==2?"trigger":"joystick");return EVRInputError_VRInputError_None;
}
static EVRInputError OPENVR_FNTABLE_CALLTYPE tutorialFakeLabel(VRInputValueHandle_t,char* text,uint32_t size,int32_t){strncpy(text,"Fixture button",size);return EVRInputError_VRInputError_None;}
static uint32_t OPENVR_FNTABLE_CALLTYPE tutorialFakeProperty(TrackedDeviceIndex_t,ETrackedDeviceProperty,char* text,uint32_t size,ETrackedPropertyError* error){strncpy(text,"hp2_fixture_controller",size);*error=ETrackedPropertyError_TrackedProp_Success;return 23;}
static uint32_t OPENVR_FNTABLE_CALLTYPE tutorialFakeCount(char*){return 5;}
static const char* tutorialFakeNames[]={"body","a","trigger","joystick","grip"};
static uint32_t OPENVR_FNTABLE_CALLTYPE tutorialFakeComponent(char*,uint32_t index,char* text,uint32_t size){strncpy(text,tutorialFakeNames[index],size);return strlen(text)+1;}
static uint32_t OPENVR_FNTABLE_CALLTYPE tutorialFakePart(char*,char* name,char* text,uint32_t size){strncpy(text,name,size);return strlen(text)+1;}
static EVRRenderModelError OPENVR_FNTABLE_CALLTYPE tutorialFakeLoad(char*,RenderModel_t** model){*model=&tutorialFakeMesh;return EVRRenderModelError_VRRenderModelError_None;}
static void OPENVR_FNTABLE_CALLTYPE tutorialFakeFree(RenderModel_t*){}
static bool OPENVR_FNTABLE_CALLTYPE tutorialFakePose(char*,char* component,VRInputValueHandle_t,RenderModel_ControllerMode_State_t*,RenderModel_ComponentState_t* out){
 *out={};auto p=hpvr::identity();int n=0;for(int i=0;i<5;++i)if(!std::strcmp(component,tutorialFakeNames[i]))n=i;
 float scale=n?.28f:1.f;for(int i=0;i<3;++i)p.m[i][i]=scale;
 if(n){p.m[0][3]=(n-2)*.024f;p.m[1][3]=.040f;p.m[2][3]=-.025f;}
 std::memcpy(&out->mTrackingToComponentRenderModel,&p,sizeof(p));out->uProperties=EVRComponentProperty_VRComponentProperty_IsVisible;return true;
}
static void tutorialTestDriver(){
 for(unsigned i=0;i<8;++i){auto& v=tutorialFakeVertices[i];v.vPosition.v[0]=((i==1||i==2||i==5||i==6)?1:-1)*.03f;v.vPosition.v[1]=(i>=4?1:-1)*.035f;v.vPosition.v[2]=((i==2||i==3||i==6||i==7)?1:-1)*.07f;v.vNormal.v[1]=1;}
 tutorialFakeInput.GetActionBindingInfo=tutorialFakeBinding;tutorialFakeInput.GetActionOrigins=tutorialFakeOrigins;tutorialFakeInput.GetOriginTrackedDeviceInfo=tutorialFakeInfo;tutorialFakeInput.GetOriginLocalizedName=tutorialFakeLabel;
 tutorialFakeSystem.GetStringTrackedDeviceProperty=tutorialFakeProperty;tutorialFakeCompositor.CanRenderScene=tutorialFakeReady;tutorialFakeCompositor.ClearLastSubmittedFrame=tutorialFakeClear;
 tutorialFakeModels.GetComponentCount=tutorialFakeCount;tutorialFakeModels.GetComponentName=tutorialFakeComponent;tutorialFakeModels.GetComponentRenderModelName=tutorialFakePart;tutorialFakeModels.LoadRenderModel_Async=tutorialFakeLoad;tutorialFakeModels.FreeRenderModel=tutorialFakeFree;tutorialFakeModels.GetComponentStateForDevicePath=tutorialFakePose;
 runtime.input=&tutorialFakeInput;runtime.system=&tutorialFakeSystem;runtime.compositor=&tutorialFakeCompositor;tutorialModels.api=&tutorialFakeModels;
 vrInput.set=1;const char* keys[]={"jump","cast","menu","recenter","confirm","potion","map","boost","quicksave","show_health","move","turn"};for(unsigned i=0;i<12;++i)vrInput.actions[keys[i]]=i+1;
 for(unsigned i=0;i<3;++i){auto p=hpvr::identity();if(i){p.m[0][3]=i==1?.20f:-.20f;p.m[1][3]=-.15f;p.m[2][3]=-.40f;}std::memcpy(&runtime.poses[i].mDeviceToAbsoluteTracking,&p,sizeof(p));runtime.poses[i].bPoseIsValid=runtime.poses[i].bDeviceIsConnected=true;}
}

static void* tutorialTestLookup(const Hp2TutorialLine& entry){
 using Class=void*(__cdecl*)();auto cls=reinterpret_cast<Class>(GetProcAddress(GetModuleHandleW(L"Engine.dll"),"?StaticClass@USound@@SAPAVUClass@@XZ"));
 auto localize=reinterpret_cast<TutorialLocalize>(GetProcAddress(GetModuleHandleW(L"Core.dll"),"?Localize@@YAPBGPBG000H@Z"));auto load=reinterpret_cast<TutorialLoad>(GetProcAddress(GetModuleHandleW(L"Core.dll"),"?StaticLoadObject@UObject@@SAPAV1@PAVUClass@@PAV1@PBG2KPAVUPackageMap@@@Z"));
 auto text=localize(L"all",entry.id,L"HPdialog",nullptr,0);need(text&&std::wstring(text)==entry.text,"HP2 localized subtitle not replaced");unsigned before=tutorialReplacements;auto name=std::wstring(L"AllDialog.")+entry.id;auto value=load(cls(),nullptr,name.c_str(),nullptr,2,nullptr);
 need(value&&tutorialReplacements==before+1&&tutorialSounds.at(value)==&entry,"HP2 original sound lookup not adapted once");
 auto cached=originalTutorialLoad(cls(),nullptr,name.c_str(),nullptr,2,nullptr);need(cached&&cached!=value,"Original cached sound reference fixture");
 auto duration=reinterpret_cast<TutorialDuration>(targets.at("soundduration"));need(std::fabs(duration(cached)-entry.seconds)<.08f&&tutorialResolveSound(cached)==value,"Cached dialog duration/playback disagrees");
 log("{\"event\":\"hp2_tutorial_lookup_verified\",\"id\":\"%ls\",\"english\":%s,\"seconds\":%.6g}",entry.id,vrEnglish?"true":"false",entry.seconds);return value;
}
static void tutorialTestTick(){
 if(startup.mode!=5||!profileFlag(L"replay-vr-tutorial.flag")||!vrPlayer||!gameConsole||vrFrames<25)return;
 static unsigned phase=0;static ULONGLONG at=0;auto now=GetTickCount64();tutorialTestDriver();tutorialModels.update(true,false);if(now-at<2300)return;
 if(phase==0){
  bool english=vrEnglish;vrBindings.defaults();vrEnglish=false;for(auto& line:tutorialLines)tutorialTestLookup(line);vrEnglish=true;for(auto& line:tutorialLinesEn)tutorialTestLookup(line);vrEnglish=english;
  resetTutorial();tutorialLessonSeen.clear();const Hp2TutorialLine* frog=nullptr;for(auto& line:tutorialLines)if(!_wcsicmp(line.id,L"PC_Nar_WhompTut1_66"))frog=&line;need(frog,"Frog catalogue missing");
  void* speaker=nullptr;void* functionName=nullptr;using MakeName=void(__thiscall*)(void*,const wchar_t*,int);using FindFunction=void*(__thiscall*)(void*,void*,int);reinterpret_cast<MakeName>(targets.at("name"))(&functionName,L"PlayDialog",0);
  auto level=mem<void*>(vrPlayer,0x94);auto actors=mem<void**>(level,0x2c);int count=mem<int>(level,0x30);for(int i=0;i<count;++i)if(actors[i]&&reinterpret_cast<FindFunction>(targets.at("field"))(actors[i],functionName,0)){speaker=actors[i];break;}need(speaker,"No original HPawn PlayDialog fixture actor");
  BYTE args[512]={};auto fn=vrScriptFunction(speaker,L"PlayDialog");auto id=vrParameter(fn,L"dlgId",12);vrAssignString(args+id,frog->id);mem<float>(args,vrParameter(fn,L"Volume",4))=1;mem<float>(args,vrParameter(fn,L"Radius",4))=20000;vrScriptCall(speaker,L"PlayDialog",args);using Delete=void(__thiscall*)(void*);reinterpret_cast<Delete>(GetProcAddress(GetModuleHandleW(L"Core.dll"),"??1FString@@QAE@XZ"))(args+id);need(tutorialLine==frog,"Native HPawn PlayDialog did not arm frog grip hint");need(std::fabs(mem<float>(args,vrParameter(fn,L"ReturnValue",4))-frog->seconds)<.1f,"Native PlayDialog used wrong timing");

 }else if(phase==1){need(tutorialModels.drawnMask==3&&tutorialModels.devices.size()==1&&tutorialModels.devices[0].index==2&&tutorialModels.devices[0].highlights[0]=="grip","HP2 default inventory cue wrong hand/component/eye");
  vrBindings.assign(VrBindings::Status,VrBindings::Jump);beginTutorial(*tutorialLine,tutorialLine->seconds);
 }else if(phase==2){need(tutorialModels.drawnMask==3&&tutorialModels.devices[0].index==1&&tutorialModels.devices[0].highlights[0]=="a","HP2 app remap not reflected in geometry");tutorialRemapped=true;beginTutorial(*tutorialLine,tutorialLine->seconds);
 }else if(phase==3){need(tutorialModels.drawnMask==3&&tutorialModels.devices[0].index==2&&tutorialModels.devices[0].highlights[0]=="grip","SteamVR remap not reflected in geometry");tutorialRemapped=false;resetTutorial();tutorialLessonSeen.clear();beginTutorial(tutorialLines[0],2);unsigned serial=tutorialSerial;beginTutorial(tutorialLines[1],2);need(tutorialSerial==serial,"Lesson repeated its controller cue");resetTutorial();
  log("{\"event\":\"hp2_tutorial_test_complete\",\"lookups\":60,\"native_audio_playback\":true,\"both_eyes\":true,\"app_remap\":true,\"steamvr_remap\":true,\"lesson_once\":true,\"mock_driver\":true,\"passed\":true}");
 }
 if(phase<4){++phase;at=now;}
}
