// Version-gated x86 debugger for the user's HP2 installation. No code-byte patches.
// Hardware execution breakpoints observe real native calls; not a VR renderer.
#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0601
#include <windows.h>
#include <bcrypt.h>
#include <psapi.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <map>
#include <string>
#include <vector>
#include <stdexcept>
#include <cstddef>
#include <initializer_list>
#include <algorithm>
#include <utility>
#define CINTERFACE
#include <d3d.h>
#include "observed_target.h"

static_assert(sizeof(void*) == 4, "Build this observer for x86.");
static HANDLE gProcess = nullptr;
static DWORD gPid = 0;
static unsigned long gAddresses[4] = {};
static unsigned long gCounts[4] = {};
static unsigned long gCameraSamples = 0, gFrameSamples = 0;
static std::map<DWORD, HANDLE> gThreads;
static bool gProfileRedirected = false,gFirstBreakpoint=false;
static DWORD gProfileCoreBase=0;
static std::wstring gSystem, gProfile, gDataRoot;
#ifdef HP2VR_NATIVE_LAUNCHER
#include "runtime_image_target.h"
static std::wstring gRuntimeImage;
#endif
static DWORD gRenderBase = 0;
static bool gShiftCamera = false;
static unsigned long gCameraWrites = 0, gDrawVerified = 0, gOcclusionVerified = 0;
struct CameraRecord {
    DWORD frame, tick;
    float position[3];
    bool drawn, occluded;
};
static std::map<DWORD, CameraRecord> gCameras;
static bool gPairEnabled=false,gPairControl=false;
static std::map<DWORD,std::wstring> gPendingRelocations;

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

static void read_memory(DWORD address, void* data, SIZE_T length) {
    SIZE_T got = 0;
    require(ReadProcessMemory(gProcess, reinterpret_cast<const void*>(address), data, length, &got) && got == length,
        "ReadProcessMemory");
}

static void verify_bytes(DWORD address, const unsigned char* expected, SIZE_T length) {
    unsigned char actual[32] = {};
    require(length <= sizeof(actual), "Byte check length");
    read_memory(address, actual, length);
    require(std::memcmp(actual, expected, length) == 0, "Loaded code differs from reviewed original");
}

static void verify_mapped_code(const std::wstring& fileName, DWORD base, bool allowPending=false) {
    HANDLE f=CreateFileW(fileName.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,0,nullptr);
    require(f!=INVALID_HANDLE_VALUE,"Read fingerprinted image");
    DWORD size=GetFileSize(f,nullptr),got=0;
    require(size>sizeof(IMAGE_DOS_HEADER) && size<16*1024*1024,"Unexpected image size");
    std::vector<unsigned char> data(size);
    require(ReadFile(f,data.data(),size,&got,nullptr) && got==size,"Read image bytes");CloseHandle(f);
    auto dos=reinterpret_cast<IMAGE_DOS_HEADER*>(data.data());
    require(dos->e_magic==IMAGE_DOS_SIGNATURE && dos->e_lfanew>0 && static_cast<DWORD>(dos->e_lfanew)+sizeof(IMAGE_NT_HEADERS32)<size,"Invalid PE header");
    auto nt=reinterpret_cast<IMAGE_NT_HEADERS32*>(data.data()+dos->e_lfanew);
    require(nt->Signature==IMAGE_NT_SIGNATURE && nt->FileHeader.Machine==IMAGE_FILE_MACHINE_I386,"Unexpected mapped architecture");
    IMAGE_NT_HEADERS32 remote={};read_memory(base+dos->e_lfanew,&remote,sizeof(remote));
    require(std::memcmp(&nt->FileHeader,&remote.FileHeader,sizeof(IMAGE_FILE_HEADER))==0 &&
        nt->OptionalHeader.SizeOfImage==remote.OptionalHeader.SizeOfImage,"Mapped image header mismatch");
    auto sections=IMAGE_FIRST_SECTION(nt);
    auto fileOffset=[&](DWORD rva)->DWORD{
        for(unsigned i=0;i<nt->FileHeader.NumberOfSections;++i){
            const auto& s=sections[i];
            if(rva>=s.VirtualAddress && rva-s.VirtualAddress<s.SizeOfRawData)return s.PointerToRawData+rva-s.VirtualAddress;
        }
        throw std::runtime_error("Invalid relocation RVA");
    };
    const std::vector<unsigned char> pristine=data;
    LONG delta=base-nt->OptionalHeader.ImageBase;
    if(delta){
        auto dir=nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];
        require(dir.VirtualAddress && dir.Size,"Relocated image missing relocation table");
        DWORD pos=fileOffset(dir.VirtualAddress),end=pos+dir.Size;
        require(end<=size,"Relocation table outside file");
        while(pos+sizeof(IMAGE_BASE_RELOCATION)<=end){
            auto block=reinterpret_cast<IMAGE_BASE_RELOCATION*>(data.data()+pos);
            require(block->SizeOfBlock>=8 && pos+block->SizeOfBlock<=end,"Invalid relocation block");
            unsigned entries=(block->SizeOfBlock-8)/2;
            auto items=reinterpret_cast<WORD*>(data.data()+pos+8);
            for(unsigned j=0;j<entries;++j){
                unsigned type=items[j]>>12;
                if(type==IMAGE_REL_BASED_ABSOLUTE)continue;
                require(type==IMAGE_REL_BASED_HIGHLOW,"Unsupported x86 relocation type");
                DWORD at=fileOffset(block->VirtualAddress+(items[j]&0xfff));
                require(at+4<=size,"Relocation outside file");
                *reinterpret_cast<DWORD*>(data.data()+at)+=delta;
            }
            pos+=block->SizeOfBlock;
        }
    }
    unsigned checked=0;bool relocatedMatch=true,pristineMatch=delta!=0;
    for(unsigned i=0;i<nt->FileHeader.NumberOfSections;++i){
        const auto& s=sections[i];
        if(!(s.Characteristics&IMAGE_SCN_MEM_EXECUTE)||!s.SizeOfRawData)continue;
        require(s.PointerToRawData+s.SizeOfRawData<=size,"Executable section outside file");
        std::vector<unsigned char> mapped(s.SizeOfRawData);
        read_memory(base+s.VirtualAddress,mapped.data(),mapped.size());
        pristineMatch=pristineMatch && std::memcmp(mapped.data(),pristine.data()+s.PointerToRawData,mapped.size())==0;
        if(std::memcmp(mapped.data(),data.data()+s.PointerToRawData,mapped.size())!=0){
            relocatedMatch=false;
            if(!allowPending || !pristineMatch){
            unsigned shown=0;
            for(DWORD j=0;j<s.SizeOfRawData&&shown<24;++j)if(mapped[j]!=data[s.PointerToRawData+j]){
                std::printf("{\"event\":\"mapped_code_mismatch\",\"rva\":%lu,\"expected\":%u,\"actual\":%u}\n",s.VirtualAddress+j,data[s.PointerToRawData+j],mapped[j]);++shown;
            }
            }
        }
        ++checked;
    }
    require(checked>0,"No executable sections verified");
    if(!relocatedMatch && allowPending && pristineMatch){
        gPendingRelocations[base]=fileName;
        std::printf("{\"event\":\"original_image_before_relocation\",\"base\":%lu}\n",base);
        return;
    }
    require(relocatedMatch,"Mapped executable section differs from original");
}

static void update_thread(HANDLE thread) {
    CONTEXT c = {};
    c.ContextFlags = CONTEXT_DEBUG_REGISTERS;
    require(GetThreadContext(thread, &c) != 0, "GetThreadContext(debug)");
    c.Dr0 = gAddresses[0]; c.Dr1 = gAddresses[1]; c.Dr2 = gAddresses[2]; c.Dr3 = gAddresses[3];
    c.Dr6 = 0; c.Dr7 = 0;
    for (unsigned i=0; i<4; ++i) if (gAddresses[i]) c.Dr7 |= (1UL << (2*i));
    require(SetThreadContext(thread, &c) != 0, "SetThreadContext(debug)");
}

static void add_thread(DWORD id, HANDLE incoming) {
    HANDLE own = nullptr;
    require(DuplicateHandle(GetCurrentProcess(), incoming, GetCurrentProcess(), &own, 0, FALSE, DUPLICATE_SAME_ACCESS) != 0,
        "Duplicate thread handle");
    gThreads[id] = own;
    update_thread(own);
}

#include "pair_capture.h"
#ifdef HP2VR_NATIVE_LAUNCHER
#include "native_inject.h"
#include "vr_frontend.h"
#include "vr_process_tree.h"
#endif

static void process_module(HANDLE file, void* basePointer) {
    wchar_t raw[32768];
    DWORD count = file ? GetFinalPathNameByHandleW(file, raw, 32768, FILE_NAME_NORMALIZED | VOLUME_NAME_DOS) : 0;
    bool mapped = !count;
    if (!count) count = GetMappedFileNameW(gProcess, basePointer, raw, 32768);
    require(count && count < 32768, "Loaded module mapped path");
    std::wstring path(raw);
    if (path.compare(0, 4, L"\\\\?\\") == 0) path.erase(0, 4);
    if(mapped){
        wchar_t drive[] = {gSystem[0], L':', 0};
        wchar_t device[1024];
        require(QueryDosDeviceW(drive,device,1024) != 0,"Resolve mapped device path");
        std::wstring prefix(device);
        if(_wcsnicmp(path.c_str(),prefix.c_str(),prefix.size()) == 0)
            path = std::wstring(drive) + path.substr(prefix.size());
    }
    auto separator = path.find_last_of(L"\\/");
    std::wstring name = path.substr(separator+1);
    std::printf("{\"event\":\"module_seen\",\"module\":\"%ls\",\"mapped_fallback\":%s}\n",name.c_str(),mapped?"true":"false");
    const ModuleHash* known = nullptr;
    for (const auto& m : kHashes) if (_wcsicmp(name.c_str(), m.name) == 0) known = &m;
    if (!known) return;
    std::wstring expectedPath=gSystem+L"\\"+name;
    require(sha256_file(expectedPath) == known->sha, "Unknown game module SHA256");
#ifdef HP2VR_NATIVE_LAUNCHER
    if(name==L"Game.exe"&&!gRuntimeImage.empty()){
        require(_wcsicmp(path.c_str(),gRuntimeImage.c_str())==0,"Unexpected private runtime image path");
        require(sha256_file(path)==kRuntimeImageSha,"Changed private runtime image");expectedPath=path;
    }
#endif
    DWORD base = reinterpret_cast<DWORD>(basePointer);
    verify_mapped_code(expectedPath,base,true);
    if(_wcsicmp(path.c_str(),expectedPath.c_str()) != 0){
        // A previously renamed folder may remain in an image section's cached NT filename.
        // Only accept a missing cached path after matching all executable sections and PE headers.
        require(GetFileAttributesW(path.c_str())==INVALID_FILE_ATTRIBUTES &&
                (GetLastError()==ERROR_PATH_NOT_FOUND || GetLastError()==ERROR_FILE_NOT_FOUND),"Existing unexpected module path");
        std::printf("{\"event\":\"stale_mapped_name_verified_by_image\",\"module\":\"%ls\"}\n",name.c_str());
    }
    std::printf("{\"event\":\"module_verified\",\"module\":\"%ls\",\"base\":%lu}\n", name.c_str(), base);
    if (_wcsicmp(name.c_str(), L"Render.dll") == 0) gRenderBase = base;
    if(gPairEnabled)pair_module(name,base);

    if (_wcsicmp(name.c_str(), L"Core.dll") == 0) {
        require(!gProfileRedirected, "Core.dll loaded twice");
        verify_bytes(base + kUserDirEntryRva, kProlog, 5);
        const unsigned char cmpOp[] = {0x66,0x83,0x3d};
        verify_bytes(base + kUserDirEntryRva + 0x1b, cmpOp, 3);
        DWORD cacheOperand = 0, returnOperand = 0;
        read_memory(base + kUserDirEntryRva + 0x1e, &cacheOperand, sizeof(cacheOperand));
        read_memory(base + kUserDirEntryRva + 0x94, &returnOperand, sizeof(returnOperand));
        require(cacheOperand == base + kUserDirCacheRva && returnOperand == cacheOperand, "Unreviewed userdir cache reference");
        std::vector<wchar_t> empty(gProfile.size()+1);
        read_memory(cacheOperand, empty.data(), empty.size()*sizeof(wchar_t));
        for (wchar_t c : empty) require(c == 0, "Userdir already initialized; refusing late redirect");
        SIZE_T written = 0;
        require(WriteProcessMemory(gProcess, reinterpret_cast<void*>(cacheOperand), gProfile.c_str(),
            (gProfile.size()+1)*sizeof(wchar_t), &written) && written == (gProfile.size()+1)*sizeof(wchar_t), "Stage userdir cache");
        std::vector<wchar_t> check(gProfile.size()+1);
        read_memory(cacheOperand, check.data(), check.size()*sizeof(wchar_t));
        require(gProfile == check.data(), "Userdir write verification");
        gProfileRedirected = true;
        gProfileCoreBase = base;
        std::puts("{\"event\":\"profile_redirect_verified\",\"disk_code_patched\":false}");
    }
    for (unsigned i = 0; i < 4; ++i) {
        if (_wcsicmp(name.c_str(), kTargets[i].module) != 0) continue;
        verify_bytes(base + kTargets[i].exportRva, kTargets[i].thunk, 5);
        verify_bytes(base + kTargets[i].entryRva, kProlog, 5);
        gAddresses[i] = base + kTargets[i].entryRva;
        std::printf("{\"event\":\"hardware_breakpoint_configured\",\"label\":\"%s\",\"rva\":%lu}\n", kTargets[i].label, kTargets[i].entryRva);
    }
    for (const auto& t : gThreads) update_thread(t.second);
}

static bool handle_breakpoint(const DEBUG_EVENT& event) {
    if(!gPendingRelocations.empty()){
        for(const auto& pending:gPendingRelocations){
            verify_mapped_code(pending.second,pending.first);
            std::printf("{\"event\":\"relocated_code_verified_before_engine_call\",\"base\":%lu}\n",pending.first);
        }
        gPendingRelocations.clear();
    }
    auto thread = gThreads.find(event.dwThreadId);
    require(thread != gThreads.end(), "Missing exception thread");
    CONTEXT c = {};
    c.ContextFlags = CONTEXT_ALL;
    require(GetThreadContext(thread->second, &c) != 0, "GetThreadContext(exception)");
#ifdef HP2VR_NATIVE_LAUNCHER
    if(native_handle(event,c)){
        c.EFlags|=0x10000;
        require(SetThreadContext(thread->second,&c)!=0,"Resume native loader");
        for(const auto& t:gThreads)if(t.first!=event.dwThreadId)update_thread(t.second);
        return true;
    }
#endif
    if(gPairEnabled && pair_handle(event,c)){
        pair_debug_registers(c);c.EFlags|=0x10000;
        require(SetThreadContext(thread->second,&c)!=0,"Resume pair stage");
        for(const auto& t:gThreads)if(t.first!=event.dwThreadId)update_thread(t.second);
        return true;
    }
    bool ours = false;
    for (unsigned i = 0; i < 4; ++i) {
        if (!(c.Dr6 & (1 << i)) || c.Eip != gAddresses[i] || !gAddresses[i]) continue;
        ours = true;
        ++gCounts[i];
        DWORD args[4] = {};
        if (i == 1) {
            read_memory(c.Esp, args, sizeof(args));
            float position[3]; long rotation[3];
            read_memory(args[1], position, sizeof(position));
            read_memory(args[2], rotation, sizeof(rotation));
            if(gPairEnabled)std::memcpy(gLastCameraRotation,rotation,sizeof(rotation));
            require(std::isfinite(position[0]) && std::isfinite(position[1]) && std::isfinite(position[2]), "Non-finite observed camera");
            require(gRenderBase && args[0] == gRenderBase + kMasterCoordsReturnRva &&
                args[1] == c.Ebp + kPositionCallerEbpOffset &&
                args[2] == c.Ebp + kRotationCallerEbpOffset,
                "Camera input is not the reviewed master-frame stack copy");
            auto previous = gCameras.find(event.dwThreadId);
            require(previous == gCameras.end() || (previous->second.drawn && previous->second.occluded),
                "Previous master camera did not reach draw and occlusion");
            float originalX = position[0];
            if (gShiftCamera) {
                position[0] += 8.0f;
                SIZE_T written = 0;
                // Only the by-value camera argument in CreateMasterFrame's stack frame is changed.
                require(WriteProcessMemory(gProcess, reinterpret_cast<void*>(args[1]), position,
                    sizeof(position), &written) && written == sizeof(position), "Write bounded camera stack copy");
                float check[3]; read_memory(args[1], check, sizeof(check));
                require(std::memcmp(check,position,sizeof(check)) == 0, "Camera write verification");
                ++gCameraWrites;
            }
            CameraRecord record = {}; record.frame = c.Ecx; record.tick = gCounts[0];
            std::memcpy(record.position, position, sizeof(position));
            gCameras[event.dwThreadId] = record;
            if (gCameraSamples < 120) {
            std::printf("{\"event\":\"camera\",\"call\":%lu,\"tick\":%lu,\"frame\":%lu,\"thread\":%lu,\"position\":[%.9g,%.9g,%.9g],\"rotation\":[%ld,%ld,%ld]}\n",
                gCounts[i], gCounts[0], c.Ecx, event.dwThreadId, position[0],position[1],position[2],rotation[0],rotation[1],rotation[2]);
            ++gCameraSamples;
            std::printf("{\"event\":\"camera_input_verified\",\"frame\":%lu,\"original_x\":%.9g,\"applied_x\":%.9g,\"shift_x\":%d,\"caller_stack_copy\":true}\n",
                c.Ecx,originalX,position[0],gShiftCamera?8:0);
            }
        }
        if (i == 2 || i == 3) {
            read_memory(c.Esp, args, sizeof(args));
            auto camera = gCameras.find(event.dwThreadId);
            const bool master = camera != gCameras.end() && camera->second.frame == args[1];
            if (i == 2) require(master, "DrawWorld lacks matching master camera");
            if (master) {
                auto& record = camera->second;
                float origin[3]; read_memory(args[1] + kFrameCoordsOriginOffset, origin, sizeof(origin));
                require(record.tick == gCounts[0], "Game tick changed between camera and visibility");
                for(unsigned axis=0;axis<3;++axis)
                    require(std::isfinite(origin[axis]) && std::fabs(origin[axis]-record.position[axis]) < 0.001f,
                        "Render frame origin differs from observed camera input");
                if (i == 2) { require(!record.drawn,"Duplicate master draw"); record.drawn=true; ++gDrawVerified; }
                if (i == 3) { require(record.drawn && !record.occluded,"Unexpected master occlusion order"); record.occluded=true; ++gOcclusionVerified; }
                if ((i == 2 ? gDrawVerified : gOcclusionVerified) <= 120)
                    std::printf("{\"event\":\"camera_at_render_verified\",\"label\":\"%s\",\"tick\":%lu,\"frame\":%lu,\"position\":[%.9g,%.9g,%.9g]}\n",
                        kTargets[i].label,gCounts[0],args[1],origin[0],origin[1],origin[2]);
            }
            if (gFrameSamples < 240) {
            std::printf("{\"event\":\"render_entry\",\"label\":\"%s\",\"call\":%lu,\"tick\":%lu,\"frame\":%lu,\"thread\":%lu}\n",
                kTargets[i].label, gCounts[i],gCounts[0],args[1],event.dwThreadId);
            ++gFrameSamples;
            }
            if(i==2 && gPairEnabled)pair_begin_draw(c,event.dwThreadId);
#ifdef HP2VR_NATIVE_LAUNCHER
            if(i==2)native_begin(c,event.dwThreadId);
#endif
        }
    }
    if (ours) {
        // Resume flag suppresses exactly the triggering execution breakpoint once.
        c.EFlags |= 0x10000;
        c.Dr6 = 0;
        if(gPairEnabled)pair_debug_registers(c);
        require(SetThreadContext(thread->second, &c) != 0, "Resume observed instruction");
        if(gPairEnabled)for(const auto& t:gThreads)if(t.first!=event.dwThreadId)update_thread(t.second);
    }
    return ours;
}

static BOOL CALLBACK close_window(HWND window, LPARAM) {
    DWORD pid = 0;
    GetWindowThreadProcessId(window, &pid);
    if (pid == gPid && IsWindowVisible(window)) PostMessageW(window, WM_CLOSE, 0, 0);
    return TRUE;
}

#include "vr_portable.h"
int wmain(int argc, wchar_t** argv) {
    setvbuf(stdout, nullptr, _IONBF, 0);
    PROCESS_INFORMATION pi = {};
    bool exited = false, forced = false, closeRequested = false, manualExit = false;
    DWORD exitCode = STILL_ACTIVE;
    try {
        require(argc == 5 || argc == 6, "Usage: observer.exe <game.exe> <staged-profile> <seconds 5..60> <map.unr> [observe|shift-x8|pair-control|pair-capture]");
        std::wstring mode = argc == 6 ? argv[5] : L"observe";
#ifdef HP2VR_NATIVE_LAUNCHER
        manualExit=mode==L"native-play"||mode==L"native-play-replay"||mode==L"native-menu";
        frontendMode=mode==L"native-menu"||mode==L"native-menu-test";
        gNativeEnabled=frontendMode||manualExit||mode==L"native-control"||mode==L"native-capture"||mode==L"native-vr"||mode==L"native-replay"||mode==L"native-input-replay";
        gNativeMode=mode==L"native-control"?1:mode==L"native-capture"?2:(mode==L"native-replay"||mode==L"native-play-replay")?4:mode==L"native-input-replay"?5:3;
        require(gNativeEnabled,"Native launcher needs native-control, native-capture or native-vr");
#else
        require(mode == L"observe" || mode == L"shift-x8" || mode==L"pair-control" || mode==L"pair-capture", "Unknown camera test mode");
#endif
        gShiftCamera = mode == L"shift-x8";
        gPairEnabled=mode==L"pair-control"||mode==L"pair-capture";gPairControl=mode==L"pair-control";
        std::wstring exe = full_path(argv[1]);
        gSystem = exe.substr(0, exe.find_last_of(L"\\/"));
        std::wstring gameRoot = gSystem.substr(0, gSystem.find_last_of(L"\\/"));
        std::wstring root = gDataRoot = vrDataRoot(gameRoot);vrLoadLocale();
        wchar_t ownPath[32768];
        DWORD ownLength = GetModuleFileNameW(nullptr,ownPath,32768);
        require(ownLength && ownLength < 32768,"Observer executable path");
        std::wstring buildDirectory=full_path(ownPath);
        no_links(buildDirectory);
        buildDirectory.resize(buildDirectory.find_last_of(L"\\/"));
#ifdef HP2VR_NATIVE_LAUNCHER
        gNativeDll=buildDirectory+L"\\hp2vr-native.dll";no_links(gNativeDll);
#endif
        std::wstring buildRoot=buildDirectory.substr(0,buildDirectory.find_last_of(L"\\/"));
        require(_wcsicmp(buildRoot.c_str(),(root+L"\\build").c_str())==0 &&
            _wcsicmp(exe.c_str(),(gameRoot+L"\\system\\Game.exe").c_str())==0,
            "Game must be system/Game.exe in this observer's project root");
        gProfile = full_path(argv[2]);
        require(gProfile.size() < 236 && gProfile.size() > root.size()+1 &&
                _wcsnicmp(gProfile.c_str(), (root + L"\\cache\\").c_str(), root.size()+7) == 0, "Profile must be under project cache");
        no_links(gProfile); no_links(exe);
        gProfile += L"\\";
#ifdef HP2VR_NATIVE_LAUNCHER
        if(frontendMode){frontendSettings=root+L"\\config\\launcher.ini";no_links(frontendSettings);frontendVr=true;gNativeMode=3;}
#endif
        unsigned seconds = static_cast<unsigned>(_wtoi(argv[3]));
        require(manualExit ? std::wstring(argv[3])==L"0" : (seconds >= 5 && seconds <= 180), "Use exactly 0 for manual-exit native play; diagnostics require 5..180 seconds");
        require(!gPairEnabled || seconds >= 20,"Pair mode requires at least 20 seconds for scene warmup");
        std::wstring map = argv[4];
        require(map == L"Adv1Willow.unr" || map == L"PrivetDr.unr" || map == L"Entry.unr" || map == L"Entryhall_hub.unr" || map==L"Grounds_Night.unr" || map==L"Grounds_Day.unr" || map==L"Startup.unr" || map==L"Grandstaircase_hub.unr" || map==L"Ch1Rictusempra.unr" || map==L"Quidditch_Intro.unr" || map==L"Quidditch.unr" || map==L"Ch3Diffindo.unr" || map==L"frontend", "Unreviewed map argument");
        for (const auto& h : kHashes) require(sha256_file(gSystem+L"\\"+h.name) == h.sha, "Original fingerprint mismatch");
        std::wstring launchedExe=exe;
#ifdef HP2VR_NATIVE_LAUNCHER
        gRuntimeImage=buildDirectory+L"\\Game.exe";no_links(gRuntimeImage);
        require(sha256_file(gRuntimeImage)==kRuntimeImageSha,"Private LAA image fingerprint mismatch");launchedExe=gRuntimeImage;
        std::puts("{\"event\":\"private_runtime_image\",\"large_address_aware\":true,\"original_unchanged\":true}");
#endif
        std::wstring command = L"\"" + launchedExe + L"\"";
        if(map!=L"frontend")command+=L" "+map+L" -SAVESLOT=1";
        STARTUPINFOW startup = {}; startup.cb = sizeof(startup);
        // The game is interactive. Diagnostic launches are bounded; explicit native play waits for user exit.
        require(CreateProcessW(launchedExe.c_str(), &command[0], nullptr, nullptr, FALSE,
#ifdef HP2VR_NATIVE_LAUNCHER
                               frontendMode?DEBUG_PROCESS:DEBUG_ONLY_THIS_PROCESS,
#else
                               DEBUG_ONLY_THIS_PROCESS,
#endif
                               nullptr, gSystem.c_str(), &startup, &pi) != 0, "Create original game under debugger");
        gProcess = pi.hProcess; gPid = pi.dwProcessId;
#ifdef HP2VR_NATIVE_LAUNCHER
        if(frontendMode){frontendPid=gPid;frontendProcess=gProcess;storeNativeProcess();}
#endif
        require(DebugSetProcessKillOnExit(TRUE) != 0, "Set debugger failure cleanup");
        ULONGLONG started = GetTickCount64(), closeAt = 0;
        std::printf("{\"event\":\"launched\",\"pid\":%lu,\"bits\":32,\"seconds\":%u}\n",gPid,seconds);
        if(manualExit)std::puts("{\"event\":\"manual_exit_session\",\"automatic_close\":false}");
        while (!exited) {
            ULONGLONG elapsed = GetTickCount64()-started;
            #ifdef HP2VR_NATIVE_LAUNCHER
            if(frontendMode&&allNativeProcessesDone(exitCode)){exited=true;break;}
            frontendPump();
            require(frontendMode||!manualExit||gNativeDetached||elapsed<60000,"Native play initialization exceeded 60 seconds");
#endif
            if (!manualExit && !closeRequested && elapsed >= seconds*1000ULL) {
                closeRequested = true; closeAt = GetTickCount64();
                EnumWindows(close_window, 0);
                std::puts("{\"event\":\"close_requested\"}");
            }
            if (closeRequested && !forced && GetTickCount64()-closeAt > 7000) {
                require(TerminateProcess(gProcess, 124) != 0, "Terminate own bounded test process");
                forced = true;
            }
#ifdef HP2VR_NATIVE_LAUNCHER
            if(gNativeDetached&&!frontendMode){
                DWORD wait=WaitForSingleObject(gProcess,100);
                require(wait==WAIT_OBJECT_0||wait==WAIT_TIMEOUT,"Wait for native game");
                if(wait==WAIT_OBJECT_0){require(GetExitCodeProcess(gProcess,&exitCode)!=0,"Get native game exit code");exited=true;}
                continue;
            }
#endif
            DEBUG_EVENT ev = {};
            if (!WaitForDebugEvent(&ev, 100)) {
                require(GetLastError() == ERROR_SEM_TIMEOUT, "WaitForDebugEvent");
                continue;
            }
#ifdef HP2VR_NATIVE_LAUNCHER
            if(frontendMode){
                if(ev.dwDebugEventCode==CREATE_PROCESS_DEBUG_EVENT&&!nativeProcesses.count(ev.dwProcessId)){
                    if(!ownedChildImage(ev.u.CreateProcessInfo.hFile)){
                        // Runtime-owned services are outside the original-game debugger.
                        if(ev.u.CreateProcessInfo.hFile)CloseHandle(ev.u.CreateProcessInfo.hFile);
                        require(ContinueDebugEvent(ev.dwProcessId,ev.dwThreadId,DBG_CONTINUE)!=0,"Continue runtime child");
                        require(DebugActiveProcessStop(ev.dwProcessId)!=0,"Release runtime-owned service");continue;
                    }
                    beginChildProcess(ev);
                }
                activateNativeProcess(ev.dwProcessId);
            }
#endif
            DWORD status = DBG_CONTINUE;
            switch (ev.dwDebugEventCode) {
            case CREATE_PROCESS_DEBUG_EVENT:
                add_thread(ev.dwThreadId, ev.u.CreateProcessInfo.hThread);
                process_module(ev.u.CreateProcessInfo.hFile, ev.u.CreateProcessInfo.lpBaseOfImage);
                if (ev.u.CreateProcessInfo.hFile) CloseHandle(ev.u.CreateProcessInfo.hFile);
                break;
            case CREATE_THREAD_DEBUG_EVENT:
                add_thread(ev.dwThreadId, ev.u.CreateThread.hThread); break;
            case EXIT_THREAD_DEBUG_EVENT: {
                auto t = gThreads.find(ev.dwThreadId);
                if(t != gThreads.end()){CloseHandle(t->second);gThreads.erase(t);} break;
            }
            case LOAD_DLL_DEBUG_EVENT:
                process_module(ev.u.LoadDll.hFile, ev.u.LoadDll.lpBaseOfDll);
                if (ev.u.LoadDll.hFile) CloseHandle(ev.u.LoadDll.hFile);
                break;
            case UNLOAD_DLL_DEBUG_EVENT: {
                DWORD base=reinterpret_cast<DWORD>(ev.u.UnloadDll.lpBaseOfDll);
                // Windows may map an original DLL, unload it during loader
                // preparation, then map it at a different address. Never verify
                // the stale address on the first engine call.
                if(gPendingRelocations.erase(base))std::printf("{\"event\":\"pending_image_unloaded\",\"base\":%lu}\n",base);
                if(base==gProfileCoreBase){
                    require(!gCounts[0]&&!gCounts[1]&&!gCounts[2]&&!gCounts[3],"Active profile module unloaded");
                    gProfileCoreBase=0;gProfileRedirected=false;
                    std::puts("{\"event\":\"profile_image_unloaded_before_engine\",\"redirect_must_be_reinstalled\":true}");
                }
                // No hot unload of monitored modules is supported in this bounded probe.
                for(unsigned i=0;i<4;++i) if(gAddresses[i] &&
                    gAddresses[i]-kTargets[i].entryRva == base)
                    throw std::runtime_error("Monitored module unloaded");
                break;
            }
            case EXCEPTION_DEBUG_EVENT: {
                DWORD code = ev.u.Exception.ExceptionRecord.ExceptionCode;
                if(code == EXCEPTION_SINGLE_STEP && handle_breakpoint(ev)) break;
                if(code == EXCEPTION_BREAKPOINT && !gFirstBreakpoint){
                    require(gProfileRedirected,"Profile redirect not installed before loader breakpoint; refusing game execution");
                    gFirstBreakpoint=true;break;
                }
                status = DBG_EXCEPTION_NOT_HANDLED;
                std::printf("{\"event\":\"exception\",\"code\":%lu,\"first_chance\":%lu}\n",code,ev.u.Exception.dwFirstChance);
                break;
            }
            case EXIT_PROCESS_DEBUG_EVENT:
                exitCode=ev.u.ExitProcess.dwExitCode;
#ifdef HP2VR_NATIVE_LAUNCHER
                if(frontendMode){auto& p=nativeProcesses.at(gPid);p.exited=true;p.exitCode=exitCode;std::printf("{\"event\":\"frontend_process_exit\",\"pid\":%lu,\"exit_code\":%lu}\n",gPid,exitCode);}else
#endif
                exited=true;break;
            default: break;
            }
            require(ContinueDebugEvent(ev.dwProcessId,ev.dwThreadId,status) != 0, "ContinueDebugEvent");
#ifdef HP2VR_NATIVE_LAUNCHER
            if(gNativeDetachRequested){
                require(DebugSetProcessKillOnExit(FALSE)!=0,"Set native debugger detach policy");
                require(DebugActiveProcessStop(gPid)!=0,"Detach startup debugger");
                gNativeDetached=true;gNativeDetachRequested=false;
                std::puts("{\"event\":\"native_debugger_detached\"}");
            }
#endif
        }
        require(WaitForSingleObject(pi.hProcess,5000)==WAIT_OBJECT_0,"Child did not finish after exit debug event");
        std::printf("{\"event\":\"summary\",\"profile_redirected\":%s,\"tick_calls\":%lu,\"camera_calls\":%lu,\"draw_world_calls\":%lu,\"occlude_frame_calls\":%lu,\"game_exit_code\":%lu,\"forced_termination\":%s,\"stereo\":false,\"vr\":false}\n",
            gProfileRedirected?"true":"false",gCounts[0],gCounts[1],gCounts[2],gCounts[3],exitCode,forced?"true":"false");
        std::printf("{\"event\":\"camera_access_summary\",\"shift_x\":%d,\"camera_writes\":%lu,\"draw_origin_verified\":%lu,\"master_occlusion_origin_verified\":%lu}\n",
            gShiftCamera?8:0,gCameraWrites,gDrawVerified,gOcclusionVerified);
#ifdef HP2VR_NATIVE_LAUNCHER
        if(frontendMode){storeNativeProcess();for(auto& row:nativeProcesses){for(auto t:row.second.gThreads)CloseHandle(t.second);if(row.second.gProcess!=pi.hProcess)CloseHandle(row.second.gProcess);}nativeProcesses.clear();gThreads.clear();}
#endif
        for(const auto& t:gThreads) CloseHandle(t.second);
        CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
#ifdef HP2VR_NATIVE_LAUNCHER
        if(frontendMode&&frontendSwitchRequested&&exitCode==0&&!forced)return kFrontendSwitchExit;
        return (gNativeDetached||(frontendMode&&frontendSeen))&&exitCode==0&&!forced?0:6;
#endif
        return gProfileRedirected && gCounts[1]>0 && gDrawVerified==gCounts[1] && gOcclusionVerified==gCounts[1] &&
            (!gShiftCamera || gCameraWrites==gCounts[1]) && (!gPairEnabled || gPairDone) && exitCode==0 && !forced ? 0 : 6;
    } catch (const std::exception& e) {
        std::fprintf(stderr,"Observer aborted: %s\n",e.what());
        // Only the process this executable created is terminated; originals stay on disk.
#ifdef HP2VR_NATIVE_LAUNCHER
        if(frontendMode){storeNativeProcess();for(auto& row:nativeProcesses)if(!row.second.exited)TerminateProcess(row.second.gProcess,125);}
#endif
        if(pi.hProcess && !exited){TerminateProcess(pi.hProcess,125);WaitForSingleObject(pi.hProcess,2000);}
        return 10;
    }
}
