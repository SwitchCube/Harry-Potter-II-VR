#pragma once
#include <tlhelp32.h>
#include "vr_native_api.h"
#include "native_launcher_target.h"
static bool gNativeEnabled=false,gNativeDetached=false,gNativeDetachRequested=false;
static DWORD gNativeMode=0,gNativeStage=0,gNativeData=0,gNativeTrap=0,gNativeAnchor=0,gNativeThread=0;
static CONTEXT gNativeCaller={};
static std::wstring gNativeDll;

static DWORD remote_api(const char* name){
    FARPROC local=GetProcAddress(GetModuleHandleW(L"kernel32.dll"),name);require(local!=nullptr,"Resolve local Win32 loader API");
    HMODULE owner=nullptr;
    require(GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,reinterpret_cast<LPCWSTR>(local),&owner)!=0,"Resolve API owner");
    wchar_t path[32768];require(GetModuleFileNameW(owner,path,32768)>0,"API module path");
    std::wstring filename=path;filename=filename.substr(filename.find_last_of(L"\\/")+1);
    HANDLE snap=CreateToolhelp32Snapshot(TH32CS_SNAPMODULE,gPid);require(snap!=INVALID_HANDLE_VALUE,"Enumerate own child modules");
    MODULEENTRY32W entry={};entry.dwSize=sizeof(entry);DWORD found=0;
    for(BOOL ok=Module32FirstW(snap,&entry);ok;ok=Module32NextW(snap,&entry))if(_wcsicmp(filename.c_str(),entry.szModule)==0){
        // Both processes are x86 and must use the exact same system module file.
        require(sha256_file(path)==sha256_file(entry.szExePath),"Loader API module version differs");
        found=reinterpret_cast<DWORD>(entry.modBaseAddr)+reinterpret_cast<DWORD>(local)-reinterpret_cast<DWORD>(owner);break;
    }
    CloseHandle(snap);require(found!=0,"Loader API absent in child");return found;
}
static void native_write(DWORD address,const void* data,SIZE_T bytes){SIZE_T n=0;require(WriteProcessMemory(gProcess,reinterpret_cast<void*>(address),data,bytes,&n)&&n==bytes,"Write native startup data");}
static void native_call(CONTEXT& c,DWORD function,DWORD arg){
    DWORD stack[2]={gNativeTrap,arg};native_write(gNativeAnchor-8,stack,sizeof(stack));c.Esp=gNativeAnchor-8;c.Eip=function;c.EFlags&=~0x100UL;
}
static void native_begin(CONTEXT& c,DWORD thread){
    if(!gNativeEnabled||gNativeStage||gCounts[1]<5)return;
    require(sha256_file(gNativeDll)==kNativeDllSha,"Native DLL fingerprint changed");
    NativeStartup args={};args.size=sizeof(args);args.version=1;args.mode=gNativeMode;
    std::wstring root=gDataRoot,profile=gProfile.substr(0,gProfile.size()-1);
    require(root.size()<260&&profile.size()<260&&gNativeDll.size()<1024,"Native paths too long");
    std::wcscpy(args.root,root.c_str());std::wcscpy(args.profile,profile.c_str());
    gNativeData=reinterpret_cast<DWORD>(VirtualAllocEx(gProcess,nullptr,8192,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));require(gNativeData!=0,"Allocate native startup data");
    native_write(gNativeData,&args,sizeof(args));native_write(gNativeData+4096,gNativeDll.c_str(),(gNativeDll.size()+1)*2);
    gNativeCaller=c;gNativeTrap=c.Eip;gNativeAnchor=c.Esp;gNativeThread=thread;gNativeStage=1;
    native_call(c,remote_api("LoadLibraryW"),gNativeData+4096);
    std::puts("{\"event\":\"native_load_requested\",\"data_executable\":false}");
}
static bool native_handle(const DEBUG_EVENT& ev,CONTEXT& c){
    if(!gNativeEnabled||!gNativeStage||c.Eip!=gNativeTrap)return false;
    require(ev.dwThreadId==gNativeThread&&c.Esp==gNativeAnchor,"Native loader thread/stack mismatch");
    if(gNativeStage==1){
        require(c.Eax!=0,"LoadLibraryW native DLL failed");
        DWORD module=c.Eax;gNativeStage=2;native_call(c,module+kNativeInitRva,gNativeData);
        std::printf("{\"event\":\"native_library_loaded\",\"module\":%lu}\n",module);return true;
    }
    require(gNativeStage==2&&c.Eax==0,"Native DLL initialization failed");
    require(VirtualFreeEx(gProcess,reinterpret_cast<void*>(gNativeData),0,MEM_RELEASE)!=0,"Free native startup data");
    gNativeData=0;gNativeStage=3;gNativeDetachRequested=true;c=gNativeCaller;
    for(DWORD& a:gAddresses)a=0;
    c.Dr0=c.Dr1=c.Dr2=c.Dr3=c.Dr6=c.Dr7=0;
    std::puts("{\"event\":\"native_hooks_ready\",\"debug_breakpoints_cleared\":true}");return true;
}
