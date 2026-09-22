#include <windows.h>
#include <cstdio>
#include "vr_capabilities.h"
#include "vr_compat.h"
int main(){unsigned long long bytes=42;
 if(vrGpuBudget(-1,bytes)||bytes!=0||vrMemoryCap(2048,bytes)!=1280)return 1;
 if(vrGpuBudget(9999,bytes)||bytes!=0)return 2;
 bool detected=vrGpuBudget(0,bytes);
 std::printf("GPU capability: detected=%u, 64-bit budget=%llu MB, cap=%u\n",detected,bytes/1048576,vrMemoryCap(2048,bytes));
 return 0;}
