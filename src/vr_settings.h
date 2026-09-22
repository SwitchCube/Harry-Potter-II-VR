#pragma once
#include <cwchar>
struct VrSettings {
 unsigned renderSize=2048;bool autoQuality=false;
 float units=50,eyeOffset=0,snapDegrees=30,wandMetres=.28f;bool left=false,smoothTurn=false;float deadzone=.2f,moveScale=1,smoothDegrees=90,hapticStrength=.45f;
 bool hud=true;float hudWidth=1.35f;
 hpvr::Vec wandOffset={0,0,0},wandAngles={0,0,0};
 static float number(const std::wstring& path,const wchar_t* key,const wchar_t* fallback,float lo,float hi){
  wchar_t value[64];GetPrivateProfileStringW(L"VR",key,fallback,value,64,path.c_str());wchar_t* end=nullptr;double x=std::wcstod(value,&end);
  if(end==value||*end||!std::isfinite(x)||x<lo||x>hi)throw std::runtime_error(std::string("Invalid VR configuration: ")+std::string(key,key+std::wcslen(key)));return float(x);
 }
 void load(const std::wstring& root){
  std::wstring p=root+L"\\config\\hp2vr.ini";if(GetFileAttributesW(p.c_str())==INVALID_FILE_ATTRIBUTES)throw std::runtime_error("VR configuration missing");
  renderSize=unsigned(number(p,L"RenderSize",L"2048",1024,2048));
  autoQuality=number(p,L"AutoQuality",L"0",0,1)!=0;
  if(renderSize!=1024&&renderSize!=1280&&renderSize!=1536&&renderSize!=2048)throw std::runtime_error("RenderSize must be 1024, 1280, 1536 or 2048");
  units=number(p,L"UnitsPerMetre",L"50",20,100);eyeOffset=number(p,L"EyeHeightOffset",L"0",-30,30);
  snapDegrees=number(p,L"SnapTurnDegrees",L"30",15,90);wandMetres=number(p,L"WandLengthMetres",L"0.28",.1f,.5f);
  deadzone=number(p,L"StickDeadzone",L"0.20",.05f,.45f);
  moveScale=number(p,L"MovementSpeed",L"1",.2f,1.0f);smoothDegrees=number(p,L"SmoothTurnDegreesPerSecond",L"90",15,180);
  wandOffset={number(p,L"WandOffsetRightMetres",L"0",-.2f,.2f),number(p,L"WandOffsetUpMetres",L"0",-.2f,.2f),number(p,L"WandOffsetForwardMetres",L"0",-.2f,.2f)};
  wandAngles={number(p,L"WandPitchDegrees",L"0",-90,90),number(p,L"WandYawDegrees",L"0",-180,180),number(p,L"WandRollDegrees",L"0",-180,180)};
  hapticStrength=number(p,L"HapticStrength",L"0.45",0,1);
  hud=number(p,L"HudEnabled",L"1",0,1)!=0;hudWidth=number(p,L"HudWidthMetres",L"1.35",.5f,2.5f);
  wchar_t turn[32];GetPrivateProfileStringW(L"VR",L"TurnMode",L"snap",turn,32,p.c_str());
  if(!std::wcscmp(turn,L"smooth"))smoothTurn=true;else if(std::wcscmp(turn,L"snap"))throw std::runtime_error("TurnMode must be snap or smooth");
  wchar_t hand[32];GetPrivateProfileStringW(L"VR",L"DominantHand",L"right",hand,32,p.c_str());
  if(!std::wcscmp(hand,L"left"))left=true;else if(std::wcscmp(hand,L"right"))throw std::runtime_error("DominantHand must be left or right");
 }
};
