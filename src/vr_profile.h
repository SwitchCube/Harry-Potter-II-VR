#pragma once
#include <windows.h>
#include <cstdio>
#include <algorithm>
#include <psapi.h>
// Wall-clock API durations, not asynchronous GPU timestamps. GPU waits may be
// charged to Lock/UpdateSubresource. OutsidePair includes the remaining game loop.
struct VrProfile {
 enum Stage {PoseWait,WorldLeft,WorldRight,SurfaceLock,CpuPack,Upload,Verify,Capture,Submit,Finish,Count};
 static const char* name(unsigned n){static const char* names[]={"pose_wait","world_left","world_right","surface_lock","cpu_pack","d3d11_upload","gpu_verification","bmp_capture","submit","finish"};return names[n];}
 LONGLONG frequency=0,start=0,previousEnd=0,frameGap=0,firstStart=0,lastEnd=0;
 LONGLONG current[Count]={},totals[Count]={},maxima[Count]={},excludedTotals[Count]={};
 LONGLONG framesTotal=0,gapsTotal=0,excludedTime=0;unsigned samples=0,excluded=0,captures=0;bool active=false;
 double interval[120]={};unsigned intervalSize=0;double intervalPair=0,intervalOutside=0;
 void reportInterval(FILE* f,unsigned frame){
  if(intervalSize<120||!f)return;double sorted[120];std::copy(interval,interval+120,sorted);std::sort(sorted,sorted+120);
  PROCESS_MEMORY_COUNTERS_EX memory={};memory.cb=sizeof(memory);GetProcessMemoryInfo(GetCurrentProcess(),(PROCESS_MEMORY_COUNTERS*)&memory,sizeof(memory));
  MEMORYSTATUSEX vm={};vm.dwLength=sizeof(vm);GlobalMemoryStatusEx(&vm);
  std::fprintf(f,"{\"event\":\"frame_interval\",\"frame\":%u,\"samples\":120,\"median_ms\":%.4f,\"p95_ms\":%.4f,\"max_ms\":%.4f,\"pair_ms\":%.4f,\"outside_ms\":%.4f,\"private_mb\":%.2f,\"free_address_mb\":%.2f}\n",frame,sorted[60],sorted[114],sorted[119],intervalPair/120,intervalOutside/120,double(memory.PrivateUsage)/1048576,double(vm.ullAvailVirtual)/1048576);std::fflush(f);
  intervalSize=0;intervalPair=intervalOutside=0;
 }
 static LONGLONG now(){LARGE_INTEGER n;QueryPerformanceCounter(&n);return n.QuadPart;}
 void begin(){if(!frequency){LARGE_INTEGER f;QueryPerformanceFrequency(&f);frequency=f.QuadPart;}start=now();if(!firstStart)firstStart=start;frameGap=previousEnd?start-previousEnd:0;std::fill(current,current+Count,0);active=true;}
 void end(unsigned frame,bool captured){LONGLONG end=now();active=false;
  if(frame>5&&!captured&&previousEnd){++samples;if(intervalSize<120){interval[intervalSize++]=ms(end-start+frameGap);intervalPair+=ms(end-start);intervalOutside+=ms(frameGap);}framesTotal+=end-start;gapsTotal+=frameGap;for(unsigned n=0;n<Count;++n){totals[n]+=current[n];maxima[n]=std::max(maxima[n],current[n]);}}
  else {++excluded;excludedTime+=end-start;for(unsigned n=0;n<Count;++n)excludedTotals[n]+=current[n];}
  if(captured)++captures;
  previousEnd=lastEnd=end;
 }
 double ms(LONGLONG ticks)const{return frequency?double(ticks)*1000/double(frequency):0;}
 void print(FILE* file)const{if(!file||!samples)return;
  std::fprintf(file,"{\"event\":\"profile_summary\",\"samples\":%u,\"excluded_capture_warmup\":%u,\"capture_frames\":%u,\"first_to_last_pair_ms\":%.6f,\"excluded_pair_total_ms\":%.6f,\"pair_mean_ms\":%.6f,\"outside_pair_mean_ms\":%.6f,\"steady_pair_rate_hz\":%.6f}\n",samples,excluded,captures,ms(lastEnd-firstStart),ms(excludedTime),ms(framesTotal)/samples,ms(gapsTotal)/samples,1000*samples/ms(framesTotal+gapsTotal));
  for(unsigned n=0;n<Count;++n)std::fprintf(file,"{\"event\":\"profile_stage\",\"stage\":\"%s\",\"mean_ms\":%.6f,\"max_ms\":%.6f,\"excluded_total_ms\":%.6f}\n",name(n),ms(totals[n])/samples,ms(maxima[n]),ms(excludedTotals[n]));
  std::fflush(file);
 }
};
static VrProfile vrProfile;
struct VrStageTimer {
 VrProfile::Stage stage;LONGLONG start;bool enabled;
 explicit VrStageTimer(VrProfile::Stage s):stage(s),start(0),enabled(vrProfile.active){if(enabled)start=VrProfile::now();}
 ~VrStageTimer(){if(enabled)vrProfile.current[stage]+=VrProfile::now()-start;}
};
