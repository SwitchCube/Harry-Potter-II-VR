// Original Flat launcher: no debugger, injection, patches or profile redirection.
#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0601
#include <windows.h>
#include <tlhelp32.h>
#include <bcrypt.h>
#include <cstdio>
#include <cwchar>
#include <string>
#include <vector>
#include <map>
#include <stdexcept>
#include "observed_target.h"
static void require(bool ok, const char* what) {
    if (!ok) {
        std::fprintf(stderr, "%s (Win32=%lu)\n", what, GetLastError());
        throw std::runtime_error(what);
    }
}

static std::wstring full_path(const wchar_t* path) {
    wchar_t result[32768];
    DWORD n = GetFullPathNameW(path, 32768, result, nullptr);
    require(n && n < 32768, "GetFullPathNameW");
    return result;
}

static void no_links(std::wstring path) {
    for (;;) {
        DWORD a = GetFileAttributesW(path.c_str());
        require(a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_REPARSE_POINT), "Missing path or reparse point");
        auto split = path.find_last_of(L"\\/");
        if (split == std::wstring::npos || split <= 2) break;
        path.resize(split);
    }
}

static std::string sha256_file(const std::wstring& path) {
    no_links(path);
    HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE,
        nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    require(file != INVALID_HANDLE_VALUE, "Open file for SHA256");
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    require(BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) == 0, "SHA256 provider");
    DWORD objectSize = 0, got = 0;
    require(BCryptGetProperty(algorithm, BCRYPT_OBJECT_LENGTH, reinterpret_cast<PUCHAR>(&objectSize), sizeof(objectSize), &got, 0) == 0, "SHA256 size");
    std::vector<unsigned char> object(objectSize);
    require(BCryptCreateHash(algorithm, &hash, object.data(), objectSize, nullptr, 0, 0) == 0, "SHA256 create");
    unsigned char buffer[65536];
    do {
        require(ReadFile(file, buffer, sizeof(buffer), &got, nullptr) != 0, "SHA256 read");
        if (got) require(BCryptHashData(hash, buffer, got, 0) == 0, "SHA256 update");
    } while (got);
    unsigned char digest[32];
    require(BCryptFinishHash(hash, digest, sizeof(digest), 0) == 0, "SHA256 finish");
    BCryptDestroyHash(hash); BCryptCloseAlgorithmProvider(algorithm, 0); CloseHandle(file);
    char hex[65];
    for (unsigned i = 0; i < 32; ++i) std::sprintf(hex + i * 2, "%02x", digest[i]);
    return hex;
}

#include "vr_frontend.h"
struct OriginalProcess { HANDLE handle; ULONGLONG created; bool exited; };
static ULONGLONG createdAt(HANDLE process){
 FILETIME c,e,k,u;require(GetProcessTimes(process,&c,&e,&k,&u)!=0,"Original process creation time");
 return (ULONGLONG(c.dwHighDateTime)<<32)|c.dwLowDateTime;
}
static bool isOriginal(HANDLE process,const std::wstring& exe){
 wchar_t path[32768];DWORD size=32768;
 return QueryFullProcessImageNameW(process,0,path,&size)&&!_wcsicmp(path,exe.c_str());
}
// Retain handles to the original front end and its original renderer/game children.
// Read-only observation keeps the session lock alive when the front end exits first.
static void discover(std::map<DWORD,OriginalProcess>& family,const std::wstring& exe){
 HANDLE snap=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);
 if(snap==INVALID_HANDLE_VALUE)return;
 PROCESSENTRY32W entry={};entry.dwSize=sizeof(entry);
 for(BOOL ok=Process32FirstW(snap,&entry);ok;ok=Process32NextW(snap,&entry)){
  auto parent=family.find(entry.th32ParentProcessID);
  if(parent==family.end()||family.count(entry.th32ProcessID)||_wcsicmp(entry.szExeFile,L"Game.exe"))continue;
  // Compare lifetime as well as PID: the menu may already have exited.
  FILETIME pc,pe,pk,pu;
  if(!GetProcessTimes(parent->second.handle,&pc,&pe,&pk,&pu))continue;
  ULONGLONG parentExit=(ULONGLONG(pe.dwHighDateTime)<<32)|pe.dwLowDateTime;
  HANDLE child=OpenProcess(SYNCHRONIZE|PROCESS_QUERY_LIMITED_INFORMATION,FALSE,entry.th32ProcessID);
  if(!child)continue;
  ULONGLONG created=createdAt(child);
  if(created<parent->second.created||(parentExit&&created>parentExit)||!isOriginal(child,exe)){CloseHandle(child);continue;}
  family.emplace(entry.th32ProcessID,OriginalProcess{child,created,false});
  std::printf("{\"event\":\"vanilla_child\",\"pid\":%lu,\"parent_pid\":%lu}\n",entry.th32ProcessID,entry.th32ParentProcessID);
 }
 CloseHandle(snap);
}
#include "vr_portable.h"
int wmain(int argc,wchar_t** argv){
 std::setvbuf(stdout,nullptr,_IONBF,0);
 std::map<DWORD,OriginalProcess> family;
 try{
  require(argc==2,"Usage: hp2vr-flat-launcher.exe original-system-Game.exe");
  std::wstring exe=full_path(argv[1]);no_links(exe);
  std::wstring system=exe.substr(0,exe.find_last_of(L"\\"));
  std::wstring root=system.substr(0,system.find_last_of(L"\\"));
  require(!_wcsicmp(exe.c_str(),(root+L"\\system\\Game.exe").c_str()),"Flat requires original system/Game.exe");
  wchar_t self[32768];require(GetModuleFileNameW(nullptr,self,32768)!=0,"Flat launcher location");
  root=vrDataRoot(root);vrLoadLocale();
  require(!_wcsnicmp(self,(root+L"\\build\\").c_str(),root.size()+7),"Flat launcher must be inside project build");
  // Flat executes the user's original game without interpreting its internal ABI.
  frontendMode=true;frontendVr=false;frontendSettings=root+L"\\config\\launcher.ini";no_links(frontendSettings);
  std::wstring command=L"\""+exe+L"\"";std::vector<wchar_t> buffer(command.begin(),command.end());buffer.push_back(0);
  STARTUPINFOW startup={};startup.cb=sizeof(startup);PROCESS_INFORMATION process={};
  require(CreateProcessW(exe.c_str(),buffer.data(),nullptr,nullptr,FALSE,0,nullptr,system.c_str(),&startup,&process)!=0,"Launch unchanged original game");
  CloseHandle(process.hThread);frontendPid=process.dwProcessId;frontendProcess=process.hProcess;
  family.emplace(frontendPid,OriginalProcess{frontendProcess,createdAt(frontendProcess),false});
  std::printf("{\"event\":\"vanilla_started\",\"pid\":%lu,\"debugger\":false,\"injected\":false,\"profile_redirected\":false,\"arguments\":\"\"}\n",frontendPid);
  frontendPump();DWORD result=0;
  for(;;){
   discover(family,exe);bool running=false;
   for(auto& row:family){
    if(row.second.exited)continue;
    if(WaitForSingleObject(row.second.handle,0)==WAIT_TIMEOUT){running=true;continue;}
    DWORD code=0;GetExitCodeProcess(row.second.handle,&code);row.second.exited=true;
    if(code)result=code;
    std::printf("{\"event\":\"vanilla_process_exit\",\"pid\":%lu,\"exit_code\":%lu}\n",row.first,code);
   }
   if(!running)break;Sleep(20);
  }
  for(auto& row:family)CloseHandle(row.second.handle);
  std::printf("{\"event\":\"vanilla_summary\",\"exit_code\":%lu,\"processes\":%u,\"switch_requested\":%s}\n",result,unsigned(family.size()),frontendSwitchRequested?"true":"false");
  return result?int(result):(frontendSwitchRequested?kFrontendSwitchExit:0);
 }catch(const std::exception& error){
  std::fprintf(stderr,"Original Flat launcher: %s\n",error.what());
  // Never terminate the user's unmodified game on a helper error.
  for(auto& row:family){WaitForSingleObject(row.second.handle,INFINITE);CloseHandle(row.second.handle);}
  return 10;
 }
}
