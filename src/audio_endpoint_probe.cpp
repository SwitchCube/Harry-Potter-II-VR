// Read-only Windows playback endpoint / format inventory. No default-device or
// endpoint property setter is used here.
#define INITGUID
#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <functiondiscoverykeys_devpkey.h>
#include <cstdio>
#include "vr_runtime.h"
static void printDevice(IMMDevice* device,const char* kind){
 LPWSTR id=nullptr;DWORD state=0;device->GetId(&id);device->GetState(&state);
 IPropertyStore* properties=nullptr;PROPVARIANT name;PropVariantInit(&name);
 if(SUCCEEDED(device->OpenPropertyStore(STGM_READ,&properties)))properties->GetValue(PKEY_Device_FriendlyName,&name);
 std::printf("%s id=%ls state=%lu name=%ls\n",kind,id?id:L"",state,name.vt==VT_LPWSTR?name.pwszVal:L"");
 if(state==DEVICE_STATE_ACTIVE){
  IAudioClient* audio=nullptr;HRESULT hr=device->Activate(__uuidof(IAudioClient),CLSCTX_ALL,nullptr,reinterpret_cast<void**>(&audio));
  if(SUCCEEDED(hr)){
   WAVEFORMATEX* format=nullptr;REFERENCE_TIME normal=0,minimum=0;audio->GetDevicePeriod(&normal,&minimum);
   if(SUCCEEDED(audio->GetMixFormat(&format))){
    std::printf("  mix rate=%lu channels=%u bits=%u tag=%u period_ms=%.3f min_ms=%.3f",format->nSamplesPerSec,format->nChannels,format->wBitsPerSample,format->wFormatTag,normal/10000.0,minimum/10000.0);
    if(format->wFormatTag==WAVE_FORMAT_EXTENSIBLE&&format->cbSize>=22)std::printf(" mask=%lu",reinterpret_cast<WAVEFORMATEXTENSIBLE*>(format)->dwChannelMask);
    std::printf("\n");CoTaskMemFree(format);
   }
   audio->Release();
  }else std::printf("  activate HRESULT=%08lx\n",static_cast<unsigned long>(hr));
 }
 PropVariantClear(&name);if(properties)properties->Release();CoTaskMemFree(id);
}
int wmain(int argc,wchar_t** argv){
 HRESULT hr=CoInitializeEx(nullptr,COINIT_MULTITHREADED);if(FAILED(hr))return 2;
 IMMDeviceEnumerator* devices=nullptr;hr=CoCreateInstance(__uuidof(MMDeviceEnumerator),nullptr,CLSCTX_ALL,__uuidof(IMMDeviceEnumerator),reinterpret_cast<void**>(&devices));if(FAILED(hr))return 3;
 const char* roles[]={"default-console","default-multimedia","default-communications"};
 for(unsigned role=0;role<3;++role){IMMDevice* device=nullptr;if(SUCCEEDED(devices->GetDefaultAudioEndpoint(eRender,static_cast<ERole>(role),&device))){printDevice(device,roles[role]);device->Release();}}
 IMMDeviceCollection* list=nullptr;hr=devices->EnumAudioEndpoints(eRender,DEVICE_STATE_ACTIVE,&list);if(FAILED(hr))return 4;
 UINT count=0;list->GetCount(&count);for(UINT i=0;i<count;++i){IMMDevice* device=nullptr;if(SUCCEEDED(list->Item(i,&device))){printDevice(device,"active");device->Release();}}
 list->Release();devices->Release();CoUninitialize();
 if(argc==2){VrRuntime vr;if(!vr.start(argv[1],EVRApplicationType_VRApplication_Background))return 5;
  char id[4096]={};ETrackedPropertyError error=ETrackedPropertyError_TrackedProp_Success;
  vr.system->GetStringTrackedDeviceProperty(0,ETrackedDeviceProperty_Prop_Audio_DefaultPlaybackDeviceId_String,id,sizeof(id),&error);
  std::printf("hmd-audio-id=%s error=%d\n",id,int(error));
  auto settings=static_cast<VR_IVRSettings_FnTable*>(vr.table(IVRSettings_Version));char section[]="audio";EVRSettingsError e;
  for(const char* key:{"setOsDefaultPlaybackDevice","enablePlaybackDeviceOverride","enablePlaybackMirror"}){bool value=settings->GetBool(section,const_cast<char*>(key),&e);std::printf("steam-audio %s=%s error=%d\n",key,value?"true":"false",int(e));}
  for(const char* key:{"playbackDeviceOverride","playbackDeviceOverrideName","lastHmdPlaybackDeviceId","activePlaybackMirrorDevice"}){char value[4096]={};settings->GetString(section,const_cast<char*>(key),value,sizeof(value),&e);std::printf("steam-audio %s=%s error=%d\n",key,value,int(e));}
 }
 return 0;
}
