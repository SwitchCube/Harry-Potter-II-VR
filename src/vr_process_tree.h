#pragma once
// Original front end spawns renderer probes and the actual game. Protect each
// process at its loader breakpoint, before it can initialize appUserDir.
struct NativeProcessState {
 bool exited=false;DWORD exitCode=STILL_ACTIVE;
 HANDLE gProcess=nullptr;
 DWORD gPid=0;
 unsigned long gCameraSamples=0;
 unsigned long gFrameSamples=0;
 bool gProfileRedirected=false;
 DWORD gRenderBase=0;
 unsigned long gCameraWrites=0;
 unsigned long gDrawVerified=0;
 unsigned long gOcclusionVerified=0;
 bool gFirstBreakpoint=false;
 bool gNativeDetached=false;
 bool gNativeDetachRequested=false;
 DWORD gNativeStage=0;
 DWORD gNativeData=0;
 DWORD gNativeTrap=0;
 DWORD gNativeAnchor=0;
 DWORD gNativeThread=0;
 CONTEXT gNativeCaller={};
 std::map<DWORD,HANDLE> gThreads;
 std::map<DWORD,CameraRecord> gCameras;
 std::map<DWORD,std::wstring> gPendingRelocations;
 unsigned long gAddresses[4]={},gCounts[4]={};
};
static std::map<DWORD,NativeProcessState> nativeProcesses;
static void storeNativeProcess(){if(!gPid)return;auto& p=nativeProcesses[gPid];
 p.gProcess=gProcess;
 p.gPid=gPid;
 p.gCameraSamples=gCameraSamples;
 p.gFrameSamples=gFrameSamples;
 p.gProfileRedirected=gProfileRedirected;
 p.gRenderBase=gRenderBase;
 p.gCameraWrites=gCameraWrites;
 p.gDrawVerified=gDrawVerified;
 p.gOcclusionVerified=gOcclusionVerified;
 p.gFirstBreakpoint=gFirstBreakpoint;
 p.gNativeDetached=gNativeDetached;
 p.gNativeDetachRequested=gNativeDetachRequested;
 p.gNativeStage=gNativeStage;
 p.gNativeData=gNativeData;
 p.gNativeTrap=gNativeTrap;
 p.gNativeAnchor=gNativeAnchor;
 p.gNativeThread=gNativeThread;
 p.gNativeCaller=gNativeCaller;
 p.gThreads=gThreads;
 p.gCameras=gCameras;
 p.gPendingRelocations=gPendingRelocations;
 std::copy(gAddresses,gAddresses+4,p.gAddresses);std::copy(gCounts,gCounts+4,p.gCounts);
}
static void activateNativeProcess(DWORD pid){storeNativeProcess();auto& p=nativeProcesses.at(pid);
 gProcess=p.gProcess;
 gPid=p.gPid;
 gCameraSamples=p.gCameraSamples;
 gFrameSamples=p.gFrameSamples;
 gProfileRedirected=p.gProfileRedirected;
 gRenderBase=p.gRenderBase;
 gCameraWrites=p.gCameraWrites;
 gDrawVerified=p.gDrawVerified;
 gOcclusionVerified=p.gOcclusionVerified;
 gFirstBreakpoint=p.gFirstBreakpoint;
 gNativeDetached=p.gNativeDetached;
 gNativeDetachRequested=p.gNativeDetachRequested;
 gNativeStage=p.gNativeStage;
 gNativeData=p.gNativeData;
 gNativeTrap=p.gNativeTrap;
 gNativeAnchor=p.gNativeAnchor;
 gNativeThread=p.gNativeThread;
 gNativeCaller=p.gNativeCaller;
 gThreads=p.gThreads;
 gCameras=p.gCameras;
 gPendingRelocations=p.gPendingRelocations;
 std::copy(p.gAddresses,p.gAddresses+4,gAddresses);std::copy(p.gCounts,p.gCounts+4,gCounts);
}
static bool ownedChildImage(HANDLE file){
 wchar_t path[32768]={};DWORD count=GetFinalPathNameByHandleW(file,path,32768,FILE_NAME_NORMALIZED|VOLUME_NAME_DOS);require(count&&count<32768,"Child executable path");std::wstring name=path;if(name.compare(0,4,L"\\\\?\\")==0)name.erase(0,4);
 return !_wcsicmp(name.c_str(),gRuntimeImage.c_str());
}
static void beginChildProcess(const DEBUG_EVENT& event){
 NativeProcessState child;child.gPid=event.dwProcessId;require(DuplicateHandle(GetCurrentProcess(),event.u.CreateProcessInfo.hProcess,GetCurrentProcess(),&child.gProcess,0,FALSE,DUPLICATE_SAME_ACCESS)!=0,"Own child process handle");nativeProcesses[event.dwProcessId]=std::move(child);
 std::printf("{\"event\":\"frontend_child_guarded\",\"pid\":%lu,\"before_loader\":true}\n",event.dwProcessId);
}
static bool allNativeProcessesDone(DWORD& code){
 storeNativeProcess();bool done=true;DWORD selected=0;code=0;
 for(auto& row:nativeProcesses){auto& p=row.second;if(!p.exited&&p.gNativeDetached&&WaitForSingleObject(p.gProcess,0)==WAIT_OBJECT_0){require(GetExitCodeProcess(p.gProcess,&p.exitCode)!=0,"Native child exit status");p.exited=true;}
  done=done&&p.exited;if(p.exited&&p.exitCode)code=p.exitCode;if(p.gNativeStage==3)selected=row.first;
 }
 if(done&&selected)activateNativeProcess(selected);return done;
}
