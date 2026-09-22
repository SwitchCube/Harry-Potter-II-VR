#pragma once
#include "vr_locale.h"
// The release keeps immutable payloads in the install folder and mutable state
// in a per-installation user directory. Development launches retain their layout.
static std::wstring vrDataRoot(const std::wstring& fallback){
 wchar_t value[1024]={};DWORD n=GetEnvironmentVariableW(L"HP2VR_DATA_ROOT",value,1024);
 if(!n)return fallback;
 if(n>=1024)throw std::runtime_error("VR data path too long");
 std::wstring path=full_path(value);no_links(path);return path;
}
