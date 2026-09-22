// Generated from fingerprinted local originals by prepare_observer.py; no guessed offsets.
#pragma once
struct Target { const char* label; const wchar_t* module; unsigned long exportRva, entryRva; unsigned char thunk[5]; };
static const Target kTargets[] = {
 {"Tick", L"Engine.dll", 0x3a58, 0xa9910, {0xe9,0xb3,0x5e,0x0a,0x00}},
 {"ComputeRenderCoords", L"Engine.dll", 0x18e3, 0x8a9a0, {0xe9,0xb8,0x90,0x08,0x00}},
 {"DrawWorld", L"Render.dll", 0x106e, 0x27930, {0xe9,0xbd,0x68,0x02,0x00}},
 {"OccludeFrame", L"Render.dll", 0x1014, 0x24c80, {0xe9,0x67,0x3c,0x02,0x00}},
};
static const unsigned char kProlog[5] = {0x55,0x8b,0xec,0x6a,0xff};
static const unsigned long kUserDirEntryRva = 0x78a70;
static const unsigned long kUserDirCacheRva = 0x15e0fc;
static const unsigned long kMasterCoordsReturnRva = 0x1fed2;
static const unsigned long kPositionCallerEbpOffset = 12;
static const unsigned long kRotationCallerEbpOffset = 24;
static const unsigned long kFrameCoordsOriginOffset = 0x34;
struct ModuleHash { const wchar_t* name; const char* sha; };
static const ModuleHash kHashes[] = {
 {L"Game.exe", "eba64695b352b103611dc8e827e6bc44e8376e31d8cf3da0b7e11d09edc1a694"},
 {L"Core.dll", "f6ad0a5fb348d868cb7499fdd727d3f6034acf0d8ba3795ae78809893b4d5249"},
 {L"Engine.dll", "efa56c753fe306c2ad2744c82e20258da39df9ce51b3b415d476f8282ae1a189"},
 {L"Render.dll", "eb71bc45dad3b744b9155f37e1ba59119a31743fc74a7fb8cb4dafa93797755a"},
 {L"D3DDrv.dll", "412078c350464c4b860893369bec733084b91f11a5e235026bf7ebf777644f02"},
};
