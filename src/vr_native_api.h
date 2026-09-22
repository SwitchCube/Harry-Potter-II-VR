#pragma once
#include <windows.h>
struct NativeStartup {
    DWORD size,version,mode; // 1=one A/A diagnostic, 2=one A/B diagnostic, 3=active VR, 4=render replay, 5=input replay
    wchar_t root[260],profile[260];
};
