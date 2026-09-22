// Version-specific integration. All engine entry points come from reviewed local exports.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d.h>
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <cmath>
#include <map>
#include <vector>
#include <string>
#include <stdexcept>
#include <float.h>
#include <xmmintrin.h>
#include "MinHook.h"
#include "native_targets.h"
#include "vr_native_api.h"
#include "vr_math.h"
#include "vr_controls.h"
#include "vr_gameplay_math.h"
#include "vr_polish_math.h"
#include "vr_wand_pose.h"
#include "vr_runtime.h"
#include "vr_bridge.h"
#include "vr_compat.h"
#include "vr_capabilities.h"
#include "vr_menu.h"

using Vec=hpvr::Vec;
using Rot=hpvr::Rot;
static_assert(sizeof(void*)==4&&sizeof(Vec)==12&&sizeof(Rot)==12,"Reviewed original x86 POD call layout");
template<class T>static T& mem(void* p,unsigned n){return *reinterpret_cast<T*>(static_cast<BYTE*>(p)+n);}
using DrawWorld=void(__thiscall*)(void*,void*);
using CreateFrame=void*(__thiscall*)(void*,void*,Vec,Rot,void*);
using FinishFrame=void(__thiscall*)(void*);
using Coords=void(__thiscall*)(void*,Vec&,Rot&);
using Dispatch=void(__thiscall*)(void*,void*,void*,void*);
using Tick=void(__thiscall*)(void*,float);
static DrawWorld originalWorld=nullptr,originalOcclude=nullptr;
static CreateFrame createFrame=nullptr;
static FinishFrame finishFrame=nullptr,originalDestroy=nullptr;
static Coords computeCoords=nullptr;
static Dispatch originalDispatch=nullptr;
static Tick originalTick=nullptr;
static std::map<std::string,void*> targets;
static VrRuntime runtime;
#include "vr_bindings.h"
#include "vr_input.h"
static VrInput vrInput;
#include "vr_presentation.h"
static VrPresentation vrPresentation;
#include "vr_settings.h"
static VrSettings vrSettings;
static void* currentEngine=nullptr;
static unsigned inputTransitions=0,wandCursorCalls=0,wandProjectileCalls=0;
static VrBridge bridge,menuBridge;static VrMenu vrMenu;
static void dispatchUi(void*,void*,void*,void*);
static bool dispatchLocomotion(void*,void*,void*,void*);
static void replayClimb();
static void replayLesson();
static void prepareLessonLevel();
static bool profileFlag(const wchar_t*);
static bool climbFixturePlaced=false,climbFixtureDone=false;
static unsigned climbFixtureStart=0;
static void* climbBlock=nullptr;static Vec climbBlockInitial={};static unsigned climbBlockPhase=0;
static void closeHud();
static void beginHud(void*,void*);
static void beginMenu(void*,void*);static bool vendorDraw(void*,void*,void*);static void closePresentation();
static bool healthDraw(void*,void*,void*);
static bool scaledHud(void*,void*,void*);
static void closeUi();
static void updateStatus();
static void updateStatusOverlay();
static bool hideStatusItem(void*,void*);
static bool statusInspect=false;
static int statusPage=0;
static ULONGLONG healthAutoUntil=0;
static void drawWand(IDirect3DDevice7*,void*,unsigned);
static void replayTravel();
static bool bindingPanelActive=false,bindingPanelConsumed=false,menuPointerConsumed=false;
static void updateBindingPanel(bool usable);
static bool cinemaActive=false,lessonCaptureRequested=false,lessonWorldActive=false;
static bool worldSceneActive=false,cardRewardActive=false,vendorWorldActive=false;
static void replayStairs();static void replayPolish();static void replayMirror();static void replayVendor();static void vendorReplayInput();static bool vendorEngaged();static bool vendorPrompt();static bool vendorInternal(void*,void*,void*);static void vendorInput(bool,float,float,float,float,bool,bool);
static Vec playerViewAnchor();static void advanceViewAnchor(void*);static void refreshHandForRender();
static ULONGLONG rewardRainStarted=0;static Vec rewardRainCentre={};static bool rewardRainBronze=false;
static bool isBroom(void*);static void* carriedActor();
static bool traceMechanic(Vec,Vec,Vec,Vec&);static bool swordTick(void*,void*,void*,void*);static bool swordHitAudit(void*,void*,void*,void*,void*);static void swordReplayHand(hpvr::Mat&);static bool canDrawHandSword();static bool usingSword();static Vec attackOrigin();static void noteSwordMesh(void*);static void replaySword();static void swordReplayInput(float&,float&,bool&,bool&);
struct SwordRenderScope {void* object=nullptr;Vec location={},weaponLocation={};Rot rotation={},weaponRotation={};BYTE bone=0;float scale=1;explicit SwordRenderScope();~SwordRenderScope();};
static bool mechanicsInternal(void*,void*,void*);static bool mechanicsDispatch(void*,void*,void*,void*);
static void mechanicView(void*,const hpvr::Mat&,Rot);static void pitchFlightEye(Vec,Vec&,Rot&);
static void updateThrowAim();static void updateFlightCues();static bool skippableScene(void*);static std::wstring actorState(void*);
static unsigned mechanicCaptureIndex=0;static const wchar_t* mechanicCapturePath(unsigned);static void noteCarryMesh(void*);
static void replayMechanics();static void mechanicReplayInput(float&,float&,bool&,bool&,bool&,bool&,bool&);
struct ReflectionBodyScope {void* actor;unsigned offset;DWORD mask,saved;explicit ReflectionBodyScope(void*);~ReflectionBodyScope();};
struct CarryRenderScope {void* object=nullptr;Vec location={};Rot rotation={};int bone=0;unsigned boneOffset=0;CarryRenderScope();~CarryRenderScope();};
static bool detectCinema(void* actor);
static void closeCinema();
static bool vrRendering=false;static unsigned levelLoadDepth=0,levelGeneration=0;
static unsigned vrFrames=0,submittedPairs=0,gpuVerifiedEyes=0,hiddenMeshes=0;
static ULONGLONG firstVrPairTime=0;
static FILE* logFile=nullptr;
static NativeStartup startup={};
static unsigned ticks=0,draws=0,extraDraws=0,secondaryScripts=0,primaryScripts=0;
static bool active=false,secondary=false,completed=false,failed=false,secondOcclusion=false;
static unsigned secondaryPreparationDepth=0,secondaryLifecycleEvents=0,destroyActorDepth=0;
using DestroyActor=int(__thiscall*)(void*,void*,int);
static DestroyActor originalDestroyActor=nullptr;
static int __fastcall destroyActorHook(void* level,void*,void* actor,int netForce){
 struct Scope{Scope(){++destroyActorDepth;}~Scope(){--destroyActorDepth;}}scope;
 return originalDestroyActor(level,actor,netForce);
}
static ULONGLONG firstDrawTime=0;
static DWORD gameThread=0;
Rot lastCameraRotation={};
static std::map<std::pair<void*,void*>,unsigned> primaryEvents;

static void log(const char* format,...){if(!logFile)return;va_list args;va_start(args,format);vfprintf(logFile,format,args);va_end(args);fputc('\n',logFile);fflush(logFile);}
static void need(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
[[noreturn]] static void stopOwnProcess(unsigned code){TerminateProcess(GetCurrentProcess(),code);for(;;)Sleep(INFINITE);}
static void hr(HRESULT result,const char* why){if(FAILED(result)){log("{\"event\":\"graphics_error\",\"hr\":%ld}",result);throw std::runtime_error(why);}}
static std::wstring objectName(void* object){
    auto entry=mem<wchar_t*>(object,0x20);need(entry!=nullptr,"Missing object name");
    std::wstring value(entry+6);need(value.size()<128,"Invalid object name");return value;
}
static unsigned propertyOffset(void* object,const wchar_t* name,unsigned size,const wchar_t* expectedClass){
    // UObject::FindObjectField hashes script functions, not all inherited properties.
    // This is the cdecl UProperty lookup actually called by execGetPropertyText.
    using FindProperty=void*(__cdecl*)(void*,const wchar_t*);
    void* property=reinterpret_cast<FindProperty>(targets.at("property"))(mem<void*>(object,0x24),name);
    need(property!=nullptr,"Missing reflected property");
    if(objectName(mem<void*>(property,0x24))!=expectedClass){std::wstring n(name);throw std::runtime_error("Reflected type differs: "+std::string(n.begin(),n.end()));}
    need(mem<DWORD>(property,0x38)==size,"Reflected element size differs");
    unsigned offset=mem<DWORD>(property,0x48);need(offset>=0x28&&offset+size<0x10000,"Reflected offset out of range");
    return offset;
}
struct FpuState {
    unsigned short cw;unsigned csr;
    FpuState(){asm volatile("fnstcw %0":"=m"(cw));csr=_mm_getcsr();}
    void restore()const{asm volatile("fldcw %0"::"m"(cw));_mm_setcsr(csr);}
};
static bool nativeCapturesEnabled(){static bool enabled=startup.mode!=3||!profileFlag(L"frontend.flag");return enabled;}
static void writeBmp(const wchar_t* name,const DDSURFACEDESC2& d){
    if(!nativeCapturesEnabled())return;
    need((d.ddpfPixelFormat.dwFlags&DDPF_RGB)&&d.ddpfPixelFormat.dwRGBBitCount==32,"Diagnostic needs RGB32 target");
    need(d.dwWidth>=64&&d.dwWidth<=2048&&d.dwHeight>=64&&d.dwHeight<=2048&&d.lPitch>=static_cast<LONG>(d.dwWidth*4),"Unexpected render target size");
    const unsigned stride=(d.dwWidth*3+3)&~3u;
    std::vector<BYTE> pixels(stride*d.dwHeight);
    auto channel=[](DWORD v,DWORD mask)->BYTE{unsigned s=0;while(mask&&!(mask&1)){++s;mask>>=1;}return mask?static_cast<BYTE>((((v>>s)&mask)*255+mask/2)/mask):0;};
    for(unsigned y=0;y<d.dwHeight;++y)for(unsigned x=0;x<d.dwWidth;++x){
        DWORD v=*reinterpret_cast<DWORD*>(static_cast<BYTE*>(d.lpSurface)+y*d.lPitch+x*4);auto out=pixels.data()+y*stride+x*3;
        out[0]=channel(v,d.ddpfPixelFormat.dwBBitMask);out[1]=channel(v,d.ddpfPixelFormat.dwGBitMask);out[2]=channel(v,d.ddpfPixelFormat.dwRBitMask);
    }
    BITMAPFILEHEADER f={};BITMAPINFOHEADER i={};f.bfType=0x4d42;f.bfOffBits=sizeof(f)+sizeof(i);f.bfSize=f.bfOffBits+pixels.size();
    i.biSize=sizeof(i);i.biWidth=d.dwWidth;i.biHeight=-static_cast<LONG>(d.dwHeight);i.biPlanes=1;i.biBitCount=24;i.biSizeImage=pixels.size();
    std::wstring path=std::wstring(startup.profile)+L"\\"+name;
    HANDLE file=CreateFileW(path.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);need(file!=INVALID_HANDLE_VALUE,"Cannot create native capture");
    auto save=[&](const void* p,DWORD bytes){DWORD n=0;need(WriteFile(file,p,bytes,&n,nullptr)&&n==bytes,"Native image write failed");};
    save(&f,sizeof(f));save(&i,sizeof(i));save(pixels.data(),pixels.size());CloseHandle(file);
    log("{\"event\":\"native_image\",\"file\":\"%ls\",\"width\":%lu,\"height\":%lu}",name,d.dwWidth,d.dwHeight);
}
static void capture(IDirectDrawSurface7* surface,const wchar_t* file){
    DDSURFACEDESC2 d={};d.dwSize=sizeof(d);
    hr(surface->Lock(nullptr,&d,DDLOCK_READONLY|DDLOCK_WAIT|DDLOCK_NOSYSLOCK,nullptr),"Surface lock failed");
    try{writeBmp(file,d);}catch(...){surface->Unlock(nullptr);throw;}
    hr(surface->Unlock(nullptr),"Surface unlock failed");
}
static void __fastcall tickHook(void* self,void*,float delta){
    if(active){log("{\"event\":\"fatal\",\"reason\":\"Tick during extra draw\"}");stopOwnProcess(126);}
    ++ticks;currentEngine=self;originalTick(self,delta);
    try{replayTravel();updateStatus();}catch(const std::exception& e){log("{\"event\":\"fatal\",\"stage\":\"post_tick_command\",\"reason\":\"%s\"}",e.what());stopOwnProcess(136);}
}
static std::map<std::pair<void*,void*>,bool> renderedTextures;
static std::map<void*,bool> preparedScriptedTextures;
static std::vector<void*> scriptedTextureFixture;
using ScriptedTick=void(__thiscall*)(void*,float);
static ScriptedTick originalScriptedTick=nullptr;
static void __fastcall scriptedTickHook(void* texture,void*,float delta){
 // UScriptedTexture::Tick restores SourceTexture pixels BEFORE RenderTexture.
 // Skip the whole second preparation, otherwise it would erase the house score.
 if(active&&startup.mode>=3){
  if(secondary&&preparedScriptedTextures.count(texture)){static unsigned seen=0;if(++seen<=8)log("{\"event\":\"scripted_texture_reused\",\"texture\":\"%ls\",\"whole_tick_suppressed\":true}",objectName(texture).c_str());return;}
  preparedScriptedTextures[texture]=true;
 }
 originalScriptedTick(texture,delta);
}
static unsigned texturePixelHash(void* texture){
 void* mip=mem<void*>(texture,0xa4);need(mip!=nullptr,"Missing original scripted mip");auto data=mem<BYTE*>(mip,0x1c);need(data!=nullptr,"Missing original scripted pixels");
 unsigned width=mem<unsigned>(texture,0x34),height=mem<unsigned>(texture,0x38);need(width>0&&height>0&&width<=2048&&height<=2048,"Unexpected scripted texture extent");
 unsigned hash=2166136261u;for(unsigned i=0;i<width*height;++i){hash^=data[i];hash*=16777619u;}return hash;
}
static void __fastcall dispatchHook(void* self,void*,void* function,void* params,void* result){
    if(active){
        // Scripted textures are shared by both eyes: render each texture once per pair.
        // A texture first visible to the right eye still needs its original preparation.
        if(startup.mode>=3&&objectName(function)==L"RenderTexture"){
            need(params&&mem<WORD>(function,0x7a)==4&&mem<WORD>(function,0x7c)==65535,"RenderTexture signature differs");
            void* texture=*static_cast<void**>(params);need(texture&&objectName(mem<void*>(texture,0x24))==L"ScriptedTexture","RenderTexture argument differs");
            auto textureKey=std::make_pair(self,texture);
            if(secondary){static unsigned observed=0;bool prior=renderedTextures.count(textureKey)!=0;
                if(++observed<=8)log("{\"event\":\"stereo_scripted_texture\",\"actor\":\"%ls\",\"class\":\"%ls\",\"texture\":\"%ls\",\"already_prepared\":%s}",objectName(self).c_str(),objectName(mem<void*>(self,0x24)).c_str(),objectName(texture).c_str(),prior?"true":"false");
                if(prior){++secondaryScripts;return;}
                renderedTextures[textureKey]=true;++secondaryPreparationDepth;originalDispatch(self,function,params,result);--secondaryPreparationDepth;return;
            }
            static unsigned primaryObserved=0;if(++primaryObserved<=8)log("{\"event\":\"primary_scripted_texture\",\"actor\":\"%ls\",\"class\":\"%ls\",\"texture\":\"%ls\"}",objectName(self).c_str(),objectName(mem<void*>(self,0x24)).c_str(),objectName(texture).c_str());
            renderedTextures[textureKey]=true;
        }
        auto key=std::make_pair(self,function);
        if(secondary){
            const auto name=objectName(function);
            // A first Update(0) in the second eye is still one original update.
            // Its nested lifecycle callbacks (e.g. particle Destroyed) must run
            // exactly as in the first eye; suppressing them corrupts cleanup.
            if(startup.mode>=3&&(secondaryPreparationDepth||destroyActorDepth)){
                if(name==L"Destroyed"&&primaryEvents.count(key)){log("{\"event\":\"fatal\",\"reason\":\"Duplicate actor destruction during stereo pair\"}");stopOwnProcess(138);}
                ++primaryEvents[key];
                if(name==L"Destroyed"){++secondaryLifecycleEvents;if(secondaryLifecycleEvents<=8)log("{\"event\":\"secondary_lifecycle\",\"name\":\"Destroyed\",\"class\":\"%ls\",\"native_destroy_depth\":%u}",objectName(mem<void*>(self,0x24)).c_str(),destroyActorDepth);}
                originalDispatch(self,function,params,result);return;
            }
            bool known=name==L"Update"||name==L"RenderOverlays";
            if(!known||(startup.mode<3&&!primaryEvents.count(key))||mem<WORD>(function,0x7a)!=4||mem<WORD>(function,0x7c)!=65535){
                log("{\"event\":\"fatal\",\"reason\":\"Unexpected secondary script\",\"name\":\"%ls\"}",name.c_str());stopOwnProcess(127);
            }
            if(name==L"Update"&&(!params||*static_cast<DWORD*>(params)!=0)){log("{\"event\":\"fatal\",\"reason\":\"Update input changed\"}");stopOwnProcess(128);}
            // A newly visible object may need its first render preparation this frame.
            if(startup.mode>=3&&!primaryEvents.count(key)){++primaryEvents[key];++secondaryPreparationDepth;originalDispatch(self,function,params,result);--secondaryPreparationDepth;return;}
            ++secondaryScripts;return;
        }
        ++primaryEvents[key];++primaryScripts;
    }
    dispatchUi(self,function,params,result);
}
static void __fastcall occludeHook(void* self,void*,void* frame){
    if(secondary)secondOcclusion=true;
    originalOcclude(self,frame);
}
static void __fastcall diagnosticWorldHook(void* renderer,void*,void* frame){
    ++draws;
    if(!firstDrawTime)firstDrawTime=GetTickCount64();
    if(completed||failed||GetTickCount64()-firstDrawTime<10000){originalWorld(renderer,frame);return;}
    try{
        need(GetCurrentThreadId()==gameThread,"Render moved to another thread");
        need(mem<DWORD>(renderer,0xa8)==1,"Unexpected master lifetime");
        void* viewport=mem<void*>(frame,0),*actor=mem<void*>(viewport,0x30),*renderDevice=mem<void*>(viewport,0x5c);
        Vec position=mem<Vec>(frame,0x34),right=mem<Vec>(frame,0x40);
        Rot rotation=lastCameraRotation;
        need(propertyOffset(actor,L"Location",12,L"StructProperty")==0x11c&&propertyOffset(actor,L"Rotation",12,L"StructProperty")==0x128,"Reflected actor pose does not match native evidence");
        unsigned eyeOffset=propertyOffset(actor,L"BaseEyeHeight",4,L"FloatProperty");
        log("{\"event\":\"reflected_player\",\"eye_height_offset\":%u,\"eye_height\":%.6g}",eyeOffset,mem<float>(actor,eyeOffset));
        BYTE pose[24];std::memcpy(pose,static_cast<BYTE*>(actor)+0x11c,24);
        DWORD startTick=ticks;FpuState fpu;
        auto device=mem<IDirect3DDevice7*>(renderDevice,0x9a4);need(device!=nullptr,"No D3D7 device");
        IDirectDrawSurface7* surface=nullptr;
        // Clear both views alike; otherwise untouched pixels can retain a previous frame/HUD.
        hr(device->Clear(0,nullptr,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,0,1,0),"Clear primary target");fpu.restore();
        active=true;primaryEvents.clear();originalWorld(renderer,frame);
        hr(device->EndScene(),"End primary scene");hr(device->GetRenderTarget(&surface),"Get actual target");
        capture(surface,L"native-a.bmp");
        hr(device->Clear(0,nullptr,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,0,1,0),"Clear secondary target");hr(device->BeginScene(),"Begin secondary scene");
        Vec other=position;float shift=startup.mode==2?8.0f:0.0f;other.x+=shift*right.x;other.y+=shift*right.y;other.z+=shift*right.z;
        fpu.restore();secondary=true;
        void* second=createFrame(renderer,viewport,other,rotation,nullptr);
        need(second&&second!=frame&&mem<DWORD>(renderer,0xa8)==2,"Second master creation failed");
        // URender::Init and UEngine::InitAudio prove these two fields. Audio callback runs only once.
        void* engine=mem<void*>(renderer,0x2c);void* audio=mem<void*>(engine,0x5c);mem<void*>(engine,0x5c)=nullptr;
        fpu.restore();originalWorld(renderer,second);mem<void*>(engine,0x5c)=audio;++extraDraws;
        hr(device->EndScene(),"End secondary scene");capture(surface,L"native-b.bmp");hr(device->BeginScene(),"Resume original scene");
        secondary=false;finishFrame(renderer);computeCoords(frame,position,rotation);surface->Release();fpu.restore();
        need(ticks==startTick&&secondOcclusion&&mem<DWORD>(renderer,0xa8)==1,"Secondary lifetime/tick/visibility failed");
        need(!std::memcmp(pose,static_cast<BYTE*>(actor)+0x11c,24),"Player pose changed");
        completed=true;active=false;
        log("{\"event\":\"native_pair_complete\",\"tick\":%u,\"frame_a\":%lu,\"frame_b\":%lu,\"script_calls_a\":%u,\"suppressed_b\":%u,\"shift\":%.1f,\"extra_ticks\":0,\"pose_unchanged\":true,\"fpu_control\":%u}",ticks,reinterpret_cast<DWORD>(frame),reinterpret_cast<DWORD>(second),primaryScripts,secondaryScripts,shift,fpu.cw);
    }catch(const std::exception& e){failed=true;log("{\"event\":\"fatal\",\"reason\":\"%s\"}",e.what());stopOwnProcess(129);}
}
static void __fastcall coordsHook(void* frame,void*,Vec& position,Rot& rotation){lastCameraRotation=rotation;computeCoords(frame,position,rotation);}
static void __fastcall destroyHook(void* engine,void*){
    log("{\"event\":\"native_shutdown\",\"ticks\":%u,\"draws\":%u,\"extra_draws\":%u,\"completed\":%s}",ticks,draws,extraDraws,completed?"true":"false");
    log("{\"event\":\"vr_shutdown\",\"frames\":%u,\"submitted_pairs\":%u,\"gpu_verified_eyes\":%u,\"hidden_meshes\":%u,\"elapsed_ms\":%llu}",vrFrames,submittedPairs,gpuVerifiedEyes,hiddenMeshes,firstVrPairTime?GetTickCount64()-firstVrPairTime:0);
    log("{\"event\":\"input_shutdown\",\"transitions\":%u,\"wand_cursor_calls\":%u,\"wand_projectile_calls\":%u}",inputTransitions,wandCursorCalls,wandProjectileCalls);
    vrProfile.print(logFile);closeUi();closeCinema();closeHud();vrMenu.close();menuBridge.close();runtime.close();bridge.close();
    originalDestroy(engine);
}
#include "vr_window.h"
#include "vr_audio.h"
#include "vr_game_render.h"
#include "vr_game_input.h"
#include "vr_locomotion.h"
#include "vr_gameplay.h"
#include "vr_gameplay_test.h"
#include "vr_polish.h"
#include "vr_sword.h"
#include "vr_lesson_test.h"
#include "vr_hud.h"
static void closeHud(){vrHealth.close();healthBridge.close();healthCapture.close();healthFresh=false;vrHud.close();hudBridge.close();hudCapture.close();}
#include "vr_panel.h"
#include "vr_vendor_panel.h"
#include "vr_binding_menu.h"
#include "vr_status.h"
#include "vr_cinema.h"
#include "vr_game_menu.h"
#include "vr_wand.h"
static void hook(const char* name,void* replacement,void** original){
    auto p=targets.at(name);need(MH_CreateHook(p,replacement,original)==MH_OK,"Create native hook");need(MH_QueueEnableHook(p)==MH_OK,"Queue native hook");
}
static LONG CALLBACK replayException(EXCEPTION_POINTERS* data){
 static unsigned count=0;
 if(data&&data->ExceptionRecord->ExceptionCode==0xc0000005&&count++<4){
  auto e=data->ExceptionRecord;auto c=data->ContextRecord;
  log("{\"event\":\"replay_access_violation\",\"eip\":%lu,\"ecx\":%lu,\"eax\":%lu,\"edx\":%lu,\"access\":%lu,\"address\":%lu}",c->Eip,c->Ecx,c->Eax,c->Edx,e->NumberParameters>0?e->ExceptionInformation[0]:0,e->NumberParameters>1?e->ExceptionInformation[1]:0);
 }
 return EXCEPTION_CONTINUE_SEARCH;
}
extern "C" __declspec(dllexport) DWORD __stdcall HP2VR_Initialize(NativeStartup* args){
    try{
        need(args&&args->size==sizeof(NativeStartup)&&args->version==1,"Invalid native startup ABI");startup=*args;
        need(startup.mode>=1&&startup.mode<=6,"Unknown native mode");
        std::wstring root=startup.root,profile=startup.profile;vrLoadLocale();
        if(startup.mode>=3&&startup.mode<=5){vrSettings.load(root);vrBindings.load(root);vrPresentation.load(root);}
        need(root.size()>4&&profile.find(root+L"\\cache\\")==0,"Native profile outside project");
        std::wstring path=profile+L"\\hp2vr-native.jsonl";
        logFile=_wfopen(path.c_str(),L"wx");need(logFile!=nullptr,"Open native log");gameThread=GetCurrentThreadId();
        if(startup.mode==5)AddVectoredExceptionHandler(1,replayException);
        for(const auto& t:kNativeTargets){
            auto module=GetModuleHandleW(t.module);need(module!=nullptr,"Missing original module");void* address=reinterpret_cast<BYTE*>(module)+t.rva;
            BYTE expected[20];std::memcpy(expected,t.prefix,sizeof(expected));
            DWORD delta=reinterpret_cast<DWORD>(module)-t.imageBase;
            for(unsigned i=0;i<t.relocationCount;++i){DWORD value;std::memcpy(&value,expected+t.relocations[i],4);value+=delta;std::memcpy(expected+t.relocations[i],&value,4);}
            if(std::memcmp(address,expected,16))throw std::runtime_error(std::string("Native target bytes changed: ")+t.label);targets[t.label]=address;
        }
        createFrame=reinterpret_cast<CreateFrame>(targets.at("create"));finishFrame=reinterpret_cast<FinishFrame>(targets.at("finish"));
        if(startup.mode==3){std::wstring dll=root+L"\\external\\openvr\\bin\\win32\\openvr_api.dll";if(!runtime.start(dll.c_str()))throw std::runtime_error(runtime.error);log("{\"event\":\"openvr_started\",\"width\":%u,\"height\":%u,\"adapter\":%d}",runtime.width,runtime.height,runtime.adapter);if(vrSettings.autoQuality){
            unsigned long long budget=0;bool detected=vrGpuBudget(runtime.adapter,budget);
            unsigned requested=vrSettings.renderSize;vrSettings.renderSize=vrMemoryCap(requested,budget);
            log(R"({"event":"render_quality","requested":%u,"selected":%u,"adapter_detected":%s,"local_memory_budget_mb":%llu})",requested,vrSettings.renderSize,detected?"true":"false",budget/1048576);
        }vrInput.start(runtime,root+(vrSettings.left?L"\\config\\vr\\actions_left.json":L"\\config\\vr\\actions.json"));}
        need(MH_Initialize()==MH_OK,"Initialize MinHook");
        if(startup.mode==6){
            hook("destroy",reinterpret_cast<void*>(destroyHook),reinterpret_cast<void**>(&originalDestroy));
            need(MH_ApplyQueued()==MH_OK,"Enable flat shutdown hook");
            log("{\"event\":\"native_initialized\",\"mode\":6,\"vr_runtime\":false}");return 0;
        }
        hook("tick",reinterpret_cast<void*>(tickHook),reinterpret_cast<void**>(&originalTick));
        hook("coords",reinterpret_cast<void*>(coordsHook),reinterpret_cast<void**>(&computeCoords));
        hook("world",reinterpret_cast<void*>(worldHook),reinterpret_cast<void**>(&originalWorld));
        hook("occlude",reinterpret_cast<void*>(occludeHook),reinterpret_cast<void**>(&originalOcclude));
        hook("dispatch",reinterpret_cast<void*>(dispatchHook),reinterpret_cast<void**>(&originalDispatch));
        if(startup.mode>=3)hook("destroyactor",reinterpret_cast<void*>(destroyActorHook),reinterpret_cast<void**>(&originalDestroyActor));
        hook("destroy",reinterpret_cast<void*>(destroyHook),reinterpret_cast<void**>(&originalDestroy));
        if(startup.mode>=3){
            hook("scriptedtick",reinterpret_cast<void*>(scriptedTickHook),reinterpret_cast<void**>(&originalScriptedTick));
            hook("viewportmessage",reinterpret_cast<void*>(viewportMessageHook),reinterpret_cast<void**>(&originalViewportMessage));
            hook("unlock",reinterpret_cast<void*>(unlockHook),reinterpret_cast<void**>(&originalUnlock));
            hook("loadmap",reinterpret_cast<void*>(loadMapHook),reinterpret_cast<void**>(&originalLoadMap));
            hook("preprocess",reinterpret_cast<void*>(preProcessHook),reinterpret_cast<void**>(&originalPreProcess));
            hook("readinput",reinterpret_cast<void*>(readInputHook),reinterpret_cast<void**>(&originalReadInput));
            hook("internal",reinterpret_cast<void*>(internalHook),reinterpret_cast<void**>(&originalInternal));
            configureAudioBuffers();
            hook("audioplay",reinterpret_cast<void*>(audioPlayHook),reinterpret_cast<void**>(&originalAudioPlay));
            hook("audiostop",reinterpret_cast<void*>(audioStopHook),reinterpret_cast<void**>(&originalAudioStop));
            hook("audioupdate",reinterpret_cast<void*>(audioUpdateHook),reinterpret_cast<void**>(&originalAudioUpdate));
            hook("childframe",reinterpret_cast<void*>(childFrameHook),reinterpret_cast<void**>(&originalChildFrame));
            hook("create",reinterpret_cast<void*>(createHook),reinterpret_cast<void**>(&createFrame));
            hook("mesh",reinterpret_cast<void*>(meshHook),reinterpret_cast<void**>(&originalMesh));
            hook("lodmesh",reinterpret_cast<void*>(lodMeshHook),reinterpret_cast<void**>(&originalLodMesh));
        }
        need(MH_ApplyQueued()==MH_OK,"Enable verified native hooks");
        log("{\"event\":\"native_initialized\",\"mode\":%lu,\"thread\":%lu,\"hook_count\":%u,\"vr_runtime\":%s}",startup.mode,gameThread,startup.mode>=3?19u:6u,startup.mode==3?"true":"false");return 0;
    }catch(const std::exception& e){log("{\"event\":\"init_failed\",\"reason\":\"%s\"}",e.what());return 1;}
}
BOOL WINAPI DllMain(HINSTANCE h,DWORD reason,LPVOID){if(reason==DLL_PROCESS_ATTACH)DisableThreadLibraryCalls(h);return TRUE;}
