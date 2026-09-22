// Explicitly requested Steam Link A/B only; no changes during game startup.
#include "vr_runtime.h"
#include <cwchar>
int wmain(int argc,wchar_t** argv){
 if(argc!=3)return 2;
 bool read=!wcscmp(argv[2],L"read"),apply=!wcscmp(argv[2],L"apply-100"),restore=!wcscmp(argv[2],L"restore-auto-default");
 if(!read&&!apply&&!restore)return 2;
 VrRuntime vr;if(!vr.start(argv[1],EVRApplicationType_VRApplication_Background)){std::fprintf(stderr,"SteamVR unavailable: %s\n",vr.error.c_str());return 20;}
 auto s=static_cast<VR_IVRSettings_FnTable*>(vr.table(IVRSettings_Version));char section[]="driver_vrlink",automatic[]="automaticBandwidth",bandwidth[]="targetBandwidth";EVRSettingsError e=EVRSettingsError_VRSettingsError_None;
 bool beforeAuto=s->GetBool(section,automatic,&e);if(e)return 21;float beforeRate=s->GetFloat(section,bandwidth,&e);if(e)return 21;
 std::printf("{\"event\":\"steam_link_bandwidth_before\",\"automatic\":%s,\"mbps\":%.6g}\n",beforeAuto?"true":"false",beforeRate);
 if(apply){
  if(!beforeAuto||std::fabs(beforeRate-200.f)>.01f)return 22;
  s->SetFloat(section,bandwidth,100.f,&e);if(e)return 23;
  s->SetBool(section,automatic,false,&e);
  if(e){EVRSettingsError rollback=EVRSettingsError_VRSettingsError_None;s->RemoveKeyInSection(section,bandwidth,&rollback);return 23;}
 }
 if(restore){
  // Accept the untouched original or a partially restored state, too. Never
  // overwrite an unrelated user-selected bitrate. Remove only our two keys.
  if(std::fabs(beforeRate-100.f)>.01f&&std::fabs(beforeRate-200.f)>.01f)return 24;
  EVRSettingsError autoError=EVRSettingsError_VRSettingsError_None,rateError=EVRSettingsError_VRSettingsError_None;
  s->RemoveKeyInSection(section,automatic,&autoError);
  s->RemoveKeyInSection(section,bandwidth,&rateError);
  if(autoError||rateError)return 25;
 }
 bool afterAuto=s->GetBool(section,automatic,&e);if(e)return 26;float afterRate=s->GetFloat(section,bandwidth,&e);if(e)return 26;
 std::printf("{\"event\":\"steam_link_bandwidth_after\",\"automatic\":%s,\"mbps\":%.6g,\"changed\":%s}\n",afterAuto?"true":"false",afterRate,read?"false":"true");
 if(apply&&(afterAuto||std::fabs(afterRate-100.f)>.01f))return 27;
 if(restore&&(!afterAuto||std::fabs(afterRate-200.f)>.01f))return 28;
 return 0;
}
