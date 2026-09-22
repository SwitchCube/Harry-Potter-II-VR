#pragma once
// Separate from calibration: a menu change cannot overwrite wand/eye settings.
struct VrPresentation {
 bool spatialScenes=false;std::wstring path;
 void load(const std::wstring& root){path=root+L"\\config\\vr-presentation.ini";VrBindings::validatePath(path);wchar_t value[32];GetPrivateProfileStringW(L"Presentation",L"SpatialScenes",L"0",value,32,path.c_str());spatialScenes=std::wstring(value)==L"1";}
 void setSpatial(bool value){
  VrBindings::validatePath(path);auto temp=path+L"."+std::to_wstring(GetCurrentProcessId())+L".tmp";VrBindings::validatePath(temp);
  const std::string text=std::string("[Presentation]\r\nSpatialScenes=")+(value?"1":"0")+"\r\n";
  HANDLE f=CreateFileW(temp.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);if(f==INVALID_HANDLE_VALUE)throw std::runtime_error("Create presentation settings");DWORD written=0;bool ok=WriteFile(f,text.data(),DWORD(text.size()),&written,nullptr)&&written==text.size()&&FlushFileBuffers(f);CloseHandle(f);
  if(!ok)throw std::runtime_error("Write presentation settings");VrBindings::validatePath(path);if(!MoveFileExW(temp.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))throw std::runtime_error("Commit presentation settings");spatialScenes=value;
 }
};
