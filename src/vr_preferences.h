#pragma once
// Separate from calibration and original keyboard options; atomic app preferences.
struct VrModPreferences {
 std::wstring path;unsigned renderSize=2048;
 void load(const std::wstring& root){
  path=root+L"\\config\\vr-preferences.ini";VrBindings::validatePath(path);
  renderSize=GetPrivateProfileIntW(L"VR",L"RenderSize",vrSettings.renderSize,path.c_str());
  if(renderSize!=1024&&renderSize!=1280&&renderSize!=1536&&renderSize!=2048)renderSize=vrSettings.renderSize;
  vrSettings.renderSize=renderSize;
  int choice=GetPrivateProfileIntW(L"VR",L"Language",vrLanguageChoice,path.c_str());vrLanguageChoice=choice>=0&&choice<=2?choice:0;
  vrEnglish=vrLanguageChoice==2||(vrLanguageChoice==0&&vrAutoEnglish);++vrLocaleRevision;
 }
 void save(){
  VrBindings::validatePath(path);auto temporary=path+L"."+std::to_wstring(GetCurrentProcessId())+L".tmp";VrBindings::validatePath(temporary);
  std::string text="[VR]\r\nLanguage="+std::to_string(vrLanguageChoice)+"\r\nRenderSize="+std::to_string(renderSize)+"\r\n";
  HANDLE file=CreateFileW(temporary.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);if(file==INVALID_HANDLE_VALUE)throw std::runtime_error("Create VR preferences");
  DWORD written=0;bool ok=WriteFile(file,text.data(),DWORD(text.size()),&written,nullptr)&&written==text.size()&&FlushFileBuffers(file);CloseHandle(file);
  if(!ok||!MoveFileExW(temporary.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))throw std::runtime_error("Save VR preferences");
  vrEnglish=vrLanguageChoice==2||(vrLanguageChoice==0&&vrAutoEnglish);++vrLocaleRevision;
 }
};
static VrModPreferences vrModPreferences;
