#pragma once
// HP2 entrance: independent package and unticked rendering. No travel/save changes.
static void* hallLevel=nullptr;static hpvr::Origin hallOrigin;static Vec hallAnchor={};static float hallHeading=0;static bool hallWasVisible=false;
static hpvr::Mat hallHead=hpvr::identity();
static VrHudCapture hallSnapshot;static unsigned hallFrames=0;
struct Hp2Region{void* zone;int leaf;DWORD zoneNumber;};
static bool hallWanted(){
 if(cinemaActive)return true;if(!menuVisible||!gameConsole)return false;
 auto book=objectField(gameConsole,L"menuBook");auto page=book?objectField(book,L"curPage"):nullptr;if(!page)return false;
 auto type=objectName(mem<void*>(page,0x24));return type==L"FEMainPage"||type==L"FEStoryBookPage"||type==L"FESlotPage"||type==L"FECreditsPage";
}
static void hallPrepare(const hpvr::Mat& h){
 FpuState fpu;hallHead=h;
 if(!hallLevel){
  using Class=void*(__cdecl*)();using Load=void*(__cdecl*)(void*,void*,const wchar_t*,const wchar_t*,DWORD,void*);using Root=void(__thiscall*)(void*);
  auto engine=GetModuleHandleW(L"Engine.dll"),core=GetModuleHandleW(L"Core.dll");auto cls=reinterpret_cast<Class>(GetProcAddress(engine,"?StaticClass@ULevel@@SAPAVUClass@@XZ"));auto load=reinterpret_cast<Load>(GetProcAddress(core,"?StaticLoadObject@UObject@@SAPAV1@PAVUClass@@PAV1@PBG2KPAVUPackageMap@@@Z"));auto root=reinterpret_cast<Root>(GetProcAddress(core,"?AddToRoot@UObject@@QAEXXZ"));need(cls&&load&&root,"HP2 entrance loading ABI missing");
  auto folder=std::wstring(startup.root)+L"\\cache\\lobby";CreateDirectoryW(folder.c_str(),nullptr);auto path=folder+L"\\HP2VRLobby.unr";VrBindings::validatePath(path);
  wchar_t gameRoot[1024]={};GetEnvironmentVariableW(L"HP2VR_GAME_ROOT",gameRoot,1024);std::wstring input=(*gameRoot?std::wstring(gameRoot):std::wstring(startup.root))+L"\\Maps\\Entryhall_hub.unr";
  // Use our own immutable copy, never overwrite the installed entrance map.
  if(GetFileAttributesW(path.c_str())==INVALID_FILE_ATTRIBUTES)need(CopyFileW(input.c_str(),path.c_str(),TRUE)!=0,"Prepare HP2 entrance background");
  fpu.restore();hallLevel=load(cls(),nullptr,L"HP2VRLobby.MyLevel",path.c_str(),0,nullptr);need(hallLevel&&hallLevel!=mem<void*>(vrPlayer,0x94),"HP2 independent entrance unavailable");root(hallLevel);
  // HP2 CreateMasterFrame +1fede / +1ff57 explicitly reads actors+2c and model+98.
  auto actors=mem<void**>(hallLevel,0x2c);int count=mem<int>(hallLevel,0x30);need(actors&&count>0&&count<100000,"HP2 hall actor array");void* start=nullptr;
  for(int i=0;i<count;++i)if(actors[i]&&objectName(mem<void*>(actors[i],0x24))==L"PlayerStart"&&(!start||objectName(actors[i])==L"PlayerStart0")){start=actors[i];if(objectName(start)==L"PlayerStart0")break;}
  need(start,"HP2 entrance spectator start missing");hallAnchor=mem<Vec>(start,field(start,L"Location",12,L"StructProperty"));hallAnchor.z+=30;hallHeading=mem<Rot>(start,field(start,L"Rotation",12,L"StructProperty")).yaw*hpvr::pi/32768;
  log("{\"event\":\"hp2_hall_loaded\",\"independent_map\":true,\"ticked\":false,\"anchor\":[%.6g,%.6g,%.6g],\"heading\":%.6g}",hallAnchor.x,hallAnchor.y,hallAnchor.z,hallHeading);
 }
 if(!hallWasVisible){hallOrigin.recenter(h,hallHeading);hallWasVisible=true;log("{\"event\":\"hp2_hall_entered\",\"fixed_game_space_screen\":true}");}fpu.restore();
}
static hpvr::Mat hallScreenPose(float distance){
 auto result=hpvr::identity();float angle=hallHeading-hallOrigin.baseYaw;float c=std::cos(angle),s=std::sin(angle);result.m[0][0]=result.m[2][2]=c;result.m[0][2]=-s;result.m[2][0]=s;
 auto point=hpvr::add(hallOrigin.reference,Vec{std::sin(angle)*distance,0,-std::cos(angle)*distance});result.m[0][3]=point.x;result.m[1][3]=point.y;result.m[2][3]=point.z;return result;
}
static void renderHall(IDirect3DDevice7* device){
 if(!hallActive||!hallLevel||!hallRenderer||!currentViewport||(startup.mode==3&&!runtime.renderReady))return;
 FpuState fpu;auto actor=mem<void*>(currentViewport,0x30);auto renderer=hallRenderer;unsigned depth=mem<DWORD>(renderer,0xa8),tick=ticks;
 need(depth<=1,"HP2 hall master lifetime differs");auto originalLevel=mem<void*>(actor,0x94);auto originalRegion=mem<Hp2Region>(actor,field(actor,L"Region",12,L"StructProperty"));BYTE pose[24];std::memcpy(pose,static_cast<BYTE*>(actor)+0x11c,24);
 hallSnapshot.lastCapture=0;need(beginCapture(hallSnapshot,device),"HP2 hall snapshot");hr(device->EndScene(),"Save desktop scene before hall");
 if(!hallSnapshot.borrowed)hr(hallSnapshot.target->Blt(nullptr,hallSnapshot.saved,nullptr,DDBLT_WAIT,nullptr),"Retain original film/menu image");hallSnapshot.restore(device);
 IDirectDrawSurface7* surface=nullptr;hr(device->GetRenderTarget(&surface),"Hall render surface");DWORD stateBlock=0;hr(device->CreateStateBlock(D3DSBT_ALL,&stateBlock),"Save original D3D state for entrance");hr(device->CaptureStateBlock(stateBlock),"Capture original D3D state for entrance");
 auto h=hallHead;Vec positions[2]={};float focals[2]={};if(!firstVrPairTime)firstVrPairTime=GetTickCount64();
 for(unsigned eye=0;eye<2;++eye){
  auto e=startup.mode==3?copyMatrix(runtime.eyeToHead[eye]):hpvr::identity();if(startup.mode!=3)e.m[0][3]=eye?.032f:-.032f;auto tracked=hpvr::multiply(h,e);Vec position=hallOrigin.position(tracked,hallAnchor,vrSettings.units);Rot rotation=hpvr::rotation(tracked,hallOrigin.baseYaw);
  RestoreFields scope;scope.set(actor,0x94,hallLevel);auto levelInfo=mem<void*>(mem<void*>(hallLevel,0x2c),0);scope.set(actor,field(actor,L"Level",4,L"ObjectProperty"),levelInfo);
  using Region=Hp2Region*(__thiscall*)(void*,Hp2Region*,void*,Vec);auto query=reinterpret_cast<Region>(GetProcAddress(GetModuleHandleW(L"Engine.dll"),"?PointRegion@UModel@@QBE?AUFPointRegion@@PAVAZoneInfo@@VFVector@@@Z"));need(query,"HP2 PointRegion ABI");Hp2Region region={};query(mem<void*>(hallLevel,0x98),&region,levelInfo,position);need(region.zone&&region.leaf>=-1,"HP2 hall region invalid");scope.set(actor,field(actor,L"Region",12,L"StructProperty"),region);
  auto show=field(actor,L"ShowFlags",4,L"IntProperty");scope.set(actor,show,mem<DWORD>(actor,show)&~DWORD(8));scope.set(actor,fovOffset,vrProjection.fov);scope.set(mem<void*>(renderer,0x2c),0x5c,static_cast<void*>(nullptr));
  hallRendering=true;bool wasVr=vrRendering;vrRendering=true;fpu.restore();auto frame=createFrame(renderer,currentViewport,position,rotation,nullptr);need(frame&&mem<void*>(frame,4)==hallLevel&&mem<DWORD>(renderer,0xa8)==depth+1,"HP2 hall frame level/lifetime");
  positions[eye]=position;focals[eye]=mem<float>(frame,0xdc);need(std::fabs(focals[eye]-mem<DWORD>(frame,0xa8)*.5f/vrProjection.tanH)<.05f,"Hall eye projection mismatch");
  hr(device->Clear(0,nullptr,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,0,1,0),"Clear entrance eye");hr(device->BeginScene(),"Begin entrance eye");fpu.restore();originalWorld(renderer,frame);drawTutorial(device,frame,eye);hr(device->EndScene(),"Finish entrance eye");
  gpuCapture(surface,eye,hallFrames==0?(eye?L"hall-eye-b.bmp":L"hall-eye-a.bmp"):nullptr,false);finishFrame(renderer);vrRendering=wasVr;hallRendering=false;
 }
 if(hallSnapshot.borrowed)hallSnapshot.copyBackground(true);else hr(surface->Blt(nullptr,hallSnapshot.target,nullptr,DDBLT_WAIT,nullptr),"Restore original desktop scene");surface->Release();hr(device->ApplyStateBlock(stateBlock),"Restore original render state");device->DeleteStateBlock(stateBlock);hr(device->BeginScene(),"Resume original game after entrance");bridge.flush();
 need(tick==ticks&&mem<DWORD>(renderer,0xa8)==depth&&mem<void*>(actor,0x94)==originalLevel&&!std::memcmp(pose,static_cast<BYTE*>(actor)+0x11c,24)&&!std::memcmp(&originalRegion,&mem<Hp2Region>(actor,field(actor,L"Region",12,L"StructProperty")),12),"Entrance rendering changed game state");
 if(startup.mode==3){bool both=true;for(int eye=0;eye<2;++eye){auto b=vrProjection.eye[eye];VRTextureBounds_t bounds={b.uMin,b.vMin,b.uMax,b.vMax};auto result=runtime.submitEye(eye,bridge.textures[eye],&bounds);if(result!=EVRCompositorError_VRCompositorError_None){both=false;if(!VrRuntime::transientSubmit(result))throw std::runtime_error("Submit HP2 entrance");}}if(both)++submittedPairs;}
 if(++hallFrames==1)log("{\"event\":\"hp2_hall_stereo\",\"master_depth\":%u,\"source_state_unchanged\":true,\"extra_ticks\":0}",depth);++vrFrames;++hallTotalFrames;completed=true;
 if(hallFrames==1||hallFrames%300==0)log("{\"event\":\"hp2_hall_pair\",\"frame\":%u,\"width\":%u,\"height\":%u,\"fov\":%.6g,\"focal_a\":%.6g,\"focal_b\":%.6g,\"left\":[%.6g,%.6g,%.6g],\"right\":[%.6g,%.6g,%.6g],\"extra_ticks\":0,\"pose_unchanged\":true,\"mode\":\"%s\"}",vrFrames,bridge.width,bridge.height,vrProjection.fov,focals[0],focals[1],positions[0].x,positions[0].y,positions[0].z,positions[1].x,positions[1].y,positions[1].z,startup.mode==3?"runtime":"replay");fpu.restore();
}
static void closeHall(){
 if(hallLevel){using Root=void(__thiscall*)(void*);auto remove=reinterpret_cast<Root>(GetProcAddress(GetModuleHandleW(L"Core.dll"),"?RemoveFromRoot@UObject@@QAEXXZ"));if(remove)remove(hallLevel);hallLevel=nullptr;}
 hallSnapshot.close();hallOrigin.ready=false;hallWasVisible=false;hallActive=hallRendering=false;hallFrames=0;
}

static void hallLeft(){hallWasVisible=false;}
