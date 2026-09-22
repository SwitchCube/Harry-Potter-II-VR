#pragma once
// Startup cap only: avoid reallocations or quality oscillation during gameplay.
// Integrated/unknown memory uses the conservative tier; users can opt out.
inline unsigned vrMemoryCap(unsigned requested,unsigned long long dedicatedBytes){
 const unsigned long long gib=1024ull*1024*1024;
 unsigned cap=dedicatedBytes<2*gib?1280:(dedicatedBytes<4*gib?1536:2048);
 return requested<cap?requested:cap;
}
