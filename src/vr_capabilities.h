#pragma once
#include <dxgi1_4.h>
// DXGI_ADAPTER_DESC uses SIZE_T: its VRAM count truncates on this x86 game.
// DXGI 1.4 exposes a 64-bit local memory budget on supported Windows drivers.
inline bool vrGpuBudget(int adapterIndex,unsigned long long& bytes){
 bytes=0;IDXGIFactory1* factory=nullptr;IDXGIAdapter* adapter=nullptr;IDXGIAdapter3* modern=nullptr;
 bool ok=adapterIndex>=0&&SUCCEEDED(CreateDXGIFactory1(__uuidof(IDXGIFactory1),reinterpret_cast<void**>(&factory)));
 if(ok)ok=SUCCEEDED(factory->EnumAdapters(adapterIndex,&adapter));
 if(ok)ok=SUCCEEDED(adapter->QueryInterface(__uuidof(IDXGIAdapter3),reinterpret_cast<void**>(&modern)));
 if(ok){DXGI_QUERY_VIDEO_MEMORY_INFO memory={};ok=SUCCEEDED(modern->QueryVideoMemoryInfo(0,DXGI_MEMORY_SEGMENT_GROUP_LOCAL,&memory))&&memory.Budget>0;if(ok)bytes=memory.Budget;}
 if(modern)modern->Release();if(adapter)adapter->Release();if(factory)factory->Release();return ok;
}
