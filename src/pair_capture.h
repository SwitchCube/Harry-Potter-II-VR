// One bounded two-view experiment on the original game thread. No executable memory allocation.
#pragma once
#include "pair_target.h"

static DWORD gPairFunctions[4]={};
static DWORD gPairEngine=0;
static bool gPairActive=false, gPairDone=false, gPairSecondary=false, gPairSkipCallback=false;
static DWORD gPairThread=0,gPairTick=0,gPairRenderer=0,gPairFrameA=0,gPairFrameB=0;
static DWORD gPairDevice=0,gPairSurface=0,gPairScratch=0,gPairReturn=0,gPairAnchor=0;
static DWORD gPairActor=0,gPairSavedPose[6]={};
static float gPairPosition[3]={},gPairOffsetPosition[3]={};
static long gPairRotation[3]={},gLastCameraRotation[3]={};
static unsigned gPairScriptsA=0,gPairSuppressed=0,gPairUpdatesSuppressed=0,gPairCallbacksSkipped=0;
static CONTEXT gPairCaller={};
static unsigned gPairStage=0;
static ULONGLONG gPairFirstDraw=0;
static std::map<std::pair<DWORD,DWORD>,unsigned> gPairPrimaryEvents;

static DWORD pair_u32(DWORD address){DWORD value=0;read_memory(address,&value,4);return value;}
static void pair_write(DWORD address,const void* data,SIZE_T bytes){
    SIZE_T written=0;
    require(WriteProcessMemory(gProcess,reinterpret_cast<void*>(address),data,bytes,&written)&&written==bytes,"Pair write to own child process");
}
static DWORD float_bits(float x){DWORD v;std::memcpy(&v,&x,4);return v;}
static std::string pair_function_name(DWORD function){
    // Local Core FName::operator*: the FName value is an entry pointer; text begins at entry+12.
    DWORD entry=pair_u32(function+0x20);require(entry!=0,"Unnamed script function");
    std::string name;
    for(unsigned i=0;i<96;++i){wchar_t c=0;read_memory(entry+12+i*2,&c,2);if(!c)return name;
        require((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_',"Unexpected function name character");name+=static_cast<char>(c);}
    throw std::runtime_error("Unterminated function name");
}
static void pair_debug_registers(CONTEXT& c){
    c.Dr0=gAddresses[0];c.Dr1=gAddresses[1];c.Dr2=gAddresses[2];c.Dr3=gAddresses[3];c.Dr6=0;c.Dr7=0;
    for(unsigned i=0;i<4;++i)if(gAddresses[i])c.Dr7|=1UL<<(2*i);
}
static void pair_module(const std::wstring& name,DWORD base){
    if(_wcsicmp(name.c_str(),L"Engine.dll")==0)gPairEngine=base;
    for(unsigned i=0;i<4;++i)if(_wcsicmp(name.c_str(),kPairTargets[i].module)==0){
        verify_bytes(base+kPairTargets[i].exportRva,kPairTargets[i].thunk,5);
        verify_bytes(base+kPairTargets[i].entryRva,kProlog,5);
        gPairFunctions[i]=base+kPairTargets[i].entryRva;
    }
}
static DWORD pair_com(DWORD object,size_t offset){
    DWORD method=pair_u32(pair_u32(object)+static_cast<DWORD>(offset));
    MEMORY_BASIC_INFORMATION info={};
    require(VirtualQueryEx(gProcess,reinterpret_cast<void*>(method),&info,sizeof(info))==sizeof(info),"COM method mapping");
    require(info.State==MEM_COMMIT && (info.Protect&(PAGE_EXECUTE|PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY)),"COM method must be executable");
    return method;
}
// All used original methods are reviewed thiscall/ret-N. COM methods are stdcall.
// Use the real game stack below its saved caller ESP; no alternate stack or engine C++ ABI compiler assumptions.
static void pair_call(CONTEXT& c,unsigned next,DWORD function,DWORD self,std::initializer_list<DWORD> args){
    require(gCounts[0]==gPairTick,"Simulation advanced during pair");
    std::vector<DWORD> stack;stack.push_back(gPairReturn);stack.insert(stack.end(),args);
    DWORD stackPointer=gPairAnchor-static_cast<DWORD>(stack.size()*4);
    pair_write(stackPointer,stack.data(),stack.size()*4);
    c.Esp=stackPointer;c.Ecx=self;c.Eip=function;c.EFlags&=~0x100UL;
    gPairStage=next;
    std::printf("{\"event\":\"pair_call\",\"stage\":%u,\"target\":%lu,\"tick\":%lu}\n",next,function,gPairTick);
}
static BYTE pair_channel(DWORD value,DWORD mask){
    require(mask!=0,"Missing RGB mask");
    unsigned shift=0;while(!(mask&1)){mask>>=1;++shift;}
    return static_cast<BYTE>((((value>>shift)&mask)*255UL+mask/2)/mask);
}
static void pair_capture(const wchar_t* name){
    DDSURFACEDESC2 desc={};read_memory(gPairScratch,&desc,sizeof(desc));
    const DWORD width=desc.dwWidth,height=desc.dwHeight,bits=desc.ddpfPixelFormat.dwRGBBitCount;
    require(desc.dwSize==sizeof(desc)&&width>=64&&width<=2048&&height>=64&&height<=2048,"Unexpected capture dimensions");
    require((bits==16||bits==32)&&(desc.ddpfPixelFormat.dwFlags&DDPF_RGB),"Unsupported capture pixel format");
    require(std::abs(desc.lPitch)>=static_cast<LONG>(width*(bits/8))&&std::abs(desc.lPitch)<=16384,"Invalid capture pitch");
    DWORD stride=(width*3+3)&~3UL;
    std::vector<BYTE> pixels(stride*height),row(width*(bits/8));
    DWORD base=reinterpret_cast<DWORD>(desc.lpSurface);
    for(DWORD y=0;y<height;++y){
        read_memory(base+y*desc.lPitch,row.data(),row.size());
        for(DWORD x=0;x<width;++x){
            DWORD value=0;std::memcpy(&value,row.data()+x*(bits/8),bits/8);
            BYTE* out=pixels.data()+y*stride+x*3;
            out[0]=pair_channel(value,desc.ddpfPixelFormat.dwBBitMask);
            out[1]=pair_channel(value,desc.ddpfPixelFormat.dwGBitMask);
            out[2]=pair_channel(value,desc.ddpfPixelFormat.dwRBitMask);
        }
    }
    BITMAPFILEHEADER fileHeader={};BITMAPINFOHEADER imageHeader={};
    fileHeader.bfType=0x4d42;fileHeader.bfOffBits=sizeof(fileHeader)+sizeof(imageHeader);
    fileHeader.bfSize=fileHeader.bfOffBits+static_cast<DWORD>(pixels.size());
    imageHeader.biSize=sizeof(imageHeader);imageHeader.biWidth=width;imageHeader.biHeight=-static_cast<LONG>(height);
    imageHeader.biPlanes=1;imageHeader.biBitCount=24;imageHeader.biSizeImage=static_cast<DWORD>(pixels.size());
    no_links(gProfile.substr(0,gProfile.size()-1));
    std::wstring output=gProfile+name;
    HANDLE file=CreateFileW(output.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
    require(file!=INVALID_HANDLE_VALUE,"Create pair bitmap inside staged profile");
    auto save=[&](const void* data,DWORD length){DWORD done=0;require(WriteFile(file,data,length,&done,nullptr)&&done==length,"Write pair bitmap");};
    save(&fileHeader,sizeof(fileHeader));save(&imageHeader,sizeof(imageHeader));save(pixels.data(),pixels.size());CloseHandle(file);
    std::printf("{\"event\":\"pair_image\",\"file\":\"%ls\",\"width\":%lu,\"height\":%lu,\"source_bpp\":%lu,\"tick\":%lu}\n",name,width,height,bits,gPairTick);
}
static void pair_lock(CONTEXT& c,unsigned stage){
    DDSURFACEDESC2 desc={};desc.dwSize=sizeof(desc);pair_write(gPairScratch,&desc,sizeof(desc));
    pair_call(c,stage,pair_com(gPairSurface,offsetof(IDirectDrawSurface7Vtbl,Lock)),0,
        {gPairSurface,0,gPairScratch,DDLOCK_READONLY|DDLOCK_WAIT|DDLOCK_NOSYSLOCK,0});
}
static void pair_begin_draw(CONTEXT& c,DWORD thread){
    if(!gPairEnabled||gPairDone||gPairActive)return;
    if(!gPairFirstDraw)gPairFirstDraw=GetTickCount64();
    if(GetTickCount64()-gPairFirstDraw<10000)return;
    for(DWORD target:gPairFunctions)require(target!=0,"Missing verified pair target");
    DWORD frame=pair_u32(c.Esp+4),ret=pair_u32(c.Esp);
    require(ret==gPairEngine+kDrawReturnRva,"Unreviewed original DrawWorld caller");
    gPairRenderer=c.Ecx;gPairFrameA=frame;gPairThread=thread;gPairTick=gCounts[0];gPairReturn=ret;
    require(pair_u32(gPairRenderer+kRendererDepthOffset)==1,"Unexpected master frame nesting");
    DWORD viewport=pair_u32(frame),renderDevice=pair_u32(viewport+kViewportDeviceOffset);
    DWORD unlock=pair_u32(pair_u32(renderDevice)+0x74);
    const Target& d3d=kPairTargets[3];DWORD d3dBase=gPairFunctions[3]-d3d.entryRva;
    require(unlock==gPairFunctions[3]||unlock==d3dBase+d3d.exportRva,"Viewport does not use verified D3D7 renderer");
    gPairDevice=pair_u32(renderDevice+kD3D7Offset);
    gPairActor=pair_u32(viewport+0x30);read_memory(gPairActor+0x11c,gPairSavedPose,sizeof(gPairSavedPose));
    read_memory(frame+0x34,gPairPosition,sizeof(gPairPosition));
    std::memcpy(gPairRotation,gLastCameraRotation,sizeof(gPairRotation));
    float right[3];read_memory(frame+0x40,right,sizeof(right));
    float norm=0;for(float v:right)norm+=v*v;
    require(std::isfinite(norm)&&std::fabs(norm-1.0f)<0.002f,"Camera right basis not normalized");
    for(unsigned i=0;i<3;++i)gPairOffsetPosition[i]=gPairPosition[i]+(gPairControl?0.0f:8.0f)*right[i];
    gPairActive=true;gPairStage=0;
    gAddresses[1]=gPairFunctions[2];gAddresses[2]=ret;
    std::printf("{\"event\":\"pair_begin\",\"tick\":%lu,\"frame_a\":%lu,\"separation_units\":%d,\"position_a\":[%.9g,%.9g,%.9g],\"position_b\":[%.9g,%.9g,%.9g]}\n",
        gPairTick,frame,gPairControl?0:8,gPairPosition[0],gPairPosition[1],gPairPosition[2],gPairOffsetPosition[0],gPairOffsetPosition[1],gPairOffsetPosition[2]);
}
static bool pair_handle(const DEBUG_EVENT& event,CONTEXT& c){
    if(!gPairActive)return false;
    if(c.Eip==gAddresses[0])throw std::runtime_error("Unexpected second simulation tick during pair");
    if(c.Eip==gRenderBase+0x279ed && gPairSkipCallback){
        require(event.dwThreadId==gPairThread,"Extra draw moved thread");
        // Skip the optional outer callback, keeping DrawWorld's marks, frame stamp, culling and cleanup.
        c.Eip=gRenderBase+0x27a0b;gPairSkipCallback=false;++gPairCallbacksSkipped;
        gAddresses[1]=gPairFunctions[2];return true;
    }
    if(c.Eip==gPairFunctions[2]){
        require(event.dwThreadId==gPairThread,"Script dispatch on another thread during pair");
        DWORD ret=pair_u32(c.Esp);
        DWORD function=pair_u32(c.Esp+4);std::string name=pair_function_name(function);
        DWORD caller=ret==gPairEngine+kActorDispatchReturnRva?pair_u32(c.Esp+16):ret;
        WORD parameterBytes=0,returnOffset=0;read_memory(function+0x7a,&parameterBytes,2);read_memory(function+0x7c,&returnOffset,2);
        std::printf("{\"event\":\"pair_script_dispatch\",\"secondary\":%s,\"return_address\":%lu,\"function\":%lu,\"name\":\"%s\",\"object\":%lu,\"wrapper_caller\":%lu,\"parameter_bytes\":%u,\"return_offset\":%u}\n",
            gPairSecondary?"true":"false",ret,function,name.c_str(),c.Ecx,caller,parameterBytes,returnOffset);
        auto key=std::make_pair(c.Ecx,function);
        if(gPairSecondary){
            bool update=name=="Update"&&caller==gRenderBase+kUpdateReturnRva;
            bool overlay=name=="RenderOverlays"&&caller==gRenderBase+kOverlayReturnRva;
            require((update||overlay)&&gPairPrimaryEvents[key]>0,"Unexpected or newly visible script event in extra view; refusing duplicate event");
            require(parameterBytes==4&&returnOffset==0xffff,"Unreviewed script parameters or return value");
            if(update){require(pair_u32(pair_u32(c.Esp+8))==0,"Unreviewed Update input parameter");++gPairUpdatesSuppressed;}
            c.Eip=ret;c.Esp+=16;++gPairSuppressed; // Reviewed ProcessEvent returns void and ret 12.
        }else {++gPairScriptsA;++gPairPrimaryEvents[key];}
        return true;
    }
    if(c.Eip!=gPairReturn)return false;
    require(event.dwThreadId==gPairThread&&gCounts[0]==gPairTick,"Pair changed thread or tick");
    if(gPairStage!=0)require(c.Esp==gPairAnchor,"Remote call stack cleanup differs from reviewed ABI");
    // HRESULT-returning COM stages must succeed; other stages return engine pointers or reference counts.
    if(gPairStage==1||gPairStage==2||gPairStage==3||gPairStage==4||gPairStage==5||gPairStage==6||
       gPairStage==9||gPairStage==10||gPairStage==11||gPairStage==12)
        require(static_cast<LONG>(c.Eax)==0,"D3D7 pair operation failed");
    switch(gPairStage){
    case 0:
        gPairCaller=c;gPairAnchor=c.Esp;
        gPairScratch=reinterpret_cast<DWORD>(VirtualAllocEx(gProcess,nullptr,4096,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));
        require(gPairScratch!=0,"Allocate pair data (non-executable)");
        pair_call(c,1,pair_com(gPairDevice,offsetof(IDirect3DDevice7Vtbl,EndScene)),0,{gPairDevice});break;
    case 1:
        pair_call(c,2,pair_com(gPairDevice,offsetof(IDirect3DDevice7Vtbl,GetRenderTarget)),0,{gPairDevice,gPairScratch+256});break;
    case 2:
        gPairSurface=pair_u32(gPairScratch+256);require(gPairSurface!=0,"Missing actual render target");pair_lock(c,3);break;
    case 3:
        pair_capture(L"view-a.bmp");
        pair_call(c,4,pair_com(gPairSurface,offsetof(IDirectDrawSurface7Vtbl,Unlock)),0,{gPairSurface,0});break;
    case 4:
        pair_call(c,5,pair_com(gPairDevice,offsetof(IDirect3DDevice7Vtbl,Clear)),0,{gPairDevice,0,0,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,0,0x3f800000,0});break;
    case 5:
        pair_call(c,6,pair_com(gPairDevice,offsetof(IDirect3DDevice7Vtbl,BeginScene)),0,{gPairDevice});break;
    case 6:
        gPairSecondary=true;gAddresses[1]=gPairEngine+kTargets[1].entryRva;
        pair_call(c,7,gPairFunctions[0],gPairRenderer,{pair_u32(gPairFrameA),float_bits(gPairOffsetPosition[0]),float_bits(gPairOffsetPosition[1]),float_bits(gPairOffsetPosition[2]),
            static_cast<DWORD>(gPairRotation[0]),static_cast<DWORD>(gPairRotation[1]),static_cast<DWORD>(gPairRotation[2]),0});break;
    case 7:{
        gPairFrameB=c.Eax;require(gPairFrameB&&gPairFrameB!=gPairFrameA,"Second master frame not independent");
        require(pair_u32(gPairRenderer+kRendererDepthOffset)==2,"Second master frame lifetime not nested");
        auto& camera=gCameras[gPairThread];require(camera.frame==gPairFrameB&&camera.tick==gPairTick,"Second frame lacks camera proof");
        float origin[3];read_memory(gPairFrameB+0x34,origin,sizeof(origin));
        for(unsigned i=0;i<3;++i)require(std::fabs(origin[i]-gPairOffsetPosition[i])<0.001f,"Wrong second camera origin");
        require(pair_u32(gPairFrameA+0xa8)==pair_u32(gPairFrameB+0xa8)&&pair_u32(gPairFrameA+0xac)==pair_u32(gPairFrameB+0xac),"Unequal view dimensions");
        camera.drawn=true;++gDrawVerified;++gCounts[2];
        gPairSkipCallback=true;gAddresses[1]=gRenderBase+0x279ed;
        std::printf("{\"event\":\"pair_second_draw\",\"frame_b\":%lu,\"tick\":%lu,\"independent_frame\":true}\n",gPairFrameB,gPairTick);
        pair_call(c,8,gRenderBase+kTargets[2].entryRva,gPairRenderer,{gPairFrameB});break;}
    case 8:
        require(gCameras[gPairThread].occluded,"Second view did not run independent visibility");
        std::printf("{\"event\":\"pair_second_visibility_verified\",\"frame_b\":%lu,\"tick\":%lu,\"position\":[%.9g,%.9g,%.9g]}\n",gPairFrameB,gPairTick,gPairOffsetPosition[0],gPairOffsetPosition[1],gPairOffsetPosition[2]);
        pair_call(c,9,pair_com(gPairDevice,offsetof(IDirect3DDevice7Vtbl,EndScene)),0,{gPairDevice});break;
    case 9:pair_lock(c,10);break;
    case 10:
        pair_capture(L"view-b.bmp");
        pair_call(c,11,pair_com(gPairSurface,offsetof(IDirectDrawSurface7Vtbl,Unlock)),0,{gPairSurface,0});break;
    case 11:pair_call(c,12,pair_com(gPairDevice,offsetof(IDirect3DDevice7Vtbl,BeginScene)),0,{gPairDevice});break;
    case 12:pair_call(c,13,gPairFunctions[1],gPairRenderer,{});break;
    case 13:
        require(pair_u32(gPairRenderer+kRendererDepthOffset)==1,"Extra master frame was not balanced");
        pair_write(gPairScratch+512,gPairPosition,sizeof(gPairPosition));pair_write(gPairScratch+528,gPairRotation,sizeof(gPairRotation));
        // Direct restoration call is not the CreateMasterFrame path; do not treat it as another new camera.
        gAddresses[1]=gPairFunctions[2];
        pair_call(c,14,gPairEngine+kTargets[1].entryRva,gPairFrameA,{gPairScratch+512,gPairScratch+528});break;
    case 14:pair_call(c,15,pair_com(gPairSurface,offsetof(IDirectDrawSurface7Vtbl,Release)),0,{gPairSurface});break;
    case 15:{
        DWORD pose[6];read_memory(gPairActor+0x11c,pose,sizeof(pose));
        require(std::memcmp(pose,gPairSavedPose,sizeof(pose))==0,"Player location/rotation changed between views");
        require(VirtualFreeEx(gProcess,reinterpret_cast<void*>(gPairScratch),0,MEM_RELEASE)!=0,"Free pair data");
        gPairScratch=0;gPairActive=false;gPairDone=true;gPairSecondary=false;
        c=gPairCaller;gAddresses[1]=gPairEngine+kTargets[1].entryRva;gAddresses[2]=gRenderBase+kTargets[2].entryRva;
        std::printf("{\"event\":\"pair_complete\",\"tick\":%lu,\"extra_ticks\":0,\"primary_script_calls\":%u,\"secondary_script_calls_suppressed\":%u,\"secondary_update_calls_suppressed\":%u,\"secondary_optional_callbacks_skipped\":%u,\"master_depth_restored\":1,\"player_pose_unchanged\":true,\"headset\":false}\n",
            gPairTick,gPairScriptsA,gPairSuppressed,gPairUpdatesSuppressed,gPairCallbacksSkipped);break;}
    default:throw std::runtime_error("Invalid pair stage");
    }
    return true;
}
