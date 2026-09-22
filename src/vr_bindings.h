#pragma once
#include "vr_locale.h"
// The input manifest remains stable. Remapping happens inside this application.
struct VrBindings {
 enum {Jump,Cast,Menu,Recenter,Confirm,Potion,Map,Boost,Save,Status,Count};
 int source[Count]={0,1,2,3,4,5,6,7,8,9};bool swapSticks=false;std::wstring path;
 static const wchar_t* label(int i){static const wchar_t* names[]={tr(L"Springen / Flugaktion",L"Jump / flying action"),tr(L"Zaubern / Werfen",L"Cast / throw"),tr(L"Spielmen\u00fc",L"Game menu"),tr(L"Blick zentrieren",L"Recenter view"),tr(L"Best\u00e4tigen / Weiter",L"Confirm / continue"),tr(L"Heiltrank / Duellzauber",L"Potion / duel spell"),tr(L"Karte / Aufgaben",L"Map / objectives"),tr(L"Gehen / Besen-Boost",L"Walk / broom boost"),tr(L"Zus\u00e4tzlich speichern",L"Extra save"),tr(L"Handanzeige / Besenbremse",L"Hand display / brake")};return names[i];}
 static const wchar_t* physical(int i,bool left){static const wchar_t* right[]={L"A",tr(L"Rechter Trigger",L"Right trigger"),L"B",tr(L"Linker Stickklick",L"Left stick click"),L"X",L"Y",tr(L"Linker Trigger",L"Left trigger"),tr(L"Rechter Griff",L"Right grip"),tr(L"Rechter Stickklick",L"Right stick click"),tr(L"Linker Griff",L"Left grip")};static const wchar_t* lh[]={L"X",tr(L"Linker Trigger",L"Left trigger"),L"Y",tr(L"Rechter Stickklick",L"Right stick click"),L"A",L"B",tr(L"Rechter Trigger",L"Right trigger"),tr(L"Rechter Griff",L"Right grip"),tr(L"Linker Stickklick",L"Left stick click"),tr(L"Linker Griff",L"Left grip")};return i<0?tr(L"Nicht belegt",L"Not assigned"):(left?lh:right)[i];}
 static void validatePath(const std::wstring& p){
  for(size_t n=3;n<=p.size();++n)if(n==p.size()||p[n]==L'\\'){std::wstring q=p.substr(0,n);DWORD a=GetFileAttributesW(q.c_str());if(a==INVALID_FILE_ATTRIBUTES){if(n!=p.size())throw std::runtime_error("Settings directory missing");}else if(a&FILE_ATTRIBUTE_REPARSE_POINT)throw std::runtime_error("Settings path is a link");}
  HANDLE h=CreateFileW(p.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,0,nullptr);if(h!=INVALID_HANDLE_VALUE){BY_HANDLE_FILE_INFORMATION i={};bool ok=GetFileInformationByHandle(h,&i)&&i.nNumberOfLinks==1;CloseHandle(h);if(!ok)throw std::runtime_error("Settings file is linked");}
 }
 void defaults(){for(int i=0;i<Count;++i)source[i]=i;swapSticks=false;}
 void load(const std::wstring& root){path=root+L"\\config\\controller-bindings.ini";validatePath(path);bool used[Count]={};
  for(int i=0;i<Count;++i){wchar_t key[32];swprintf(key,32,L"Action%d",i);int n=GetPrivateProfileIntW(L"Bindings",key,i,path.c_str());if(n<0||n>=Count||used[n]){defaults();return;}source[i]=n;used[n]=true;}
  swapSticks=GetPrivateProfileIntW(L"Bindings",L"SwapSticks",0,path.c_str())!=0;
 }
 void save(){validatePath(path);std::string text="[Bindings]\r\n";for(int i=0;i<Count;++i)text+="Action"+std::to_string(i)+"="+std::to_string(source[i])+"\r\n";text+="SwapSticks="+std::to_string(swapSticks?1:0)+"\r\n";
  auto temporary=path+L"."+std::to_wstring(GetCurrentProcessId())+L".tmp";validatePath(temporary);
  HANDLE f=CreateFileW(temporary.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);if(f==INVALID_HANDLE_VALUE)throw std::runtime_error("Create controller settings");DWORD written=0;bool ok=WriteFile(f,text.data(),text.size(),&written,nullptr)&&written==text.size()&&FlushFileBuffers(f);CloseHandle(f);if(!ok)throw std::runtime_error("Write controller settings");validatePath(path);if(!MoveFileExW(temporary.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))throw std::runtime_error("Commit controller settings");
 }
 void assign(int action,int button){int old=source[action];for(int i=0;i<Count;++i)if(source[i]==button)source[i]=old;source[action]=button;save();}
};
static VrBindings vrBindings;
