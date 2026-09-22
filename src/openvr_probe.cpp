// External diagnostic only: no HP2 integration, no tracking and no image submission.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdio>
#include "openvr.h"

static_assert(sizeof(void*) == 4, "This diagnostic must test the HP2 process architecture (x86).");
static_assert(sizeof(vr::HmdMatrix34_t) == 48, "Unexpected OpenVR matrix layout");
static_assert(sizeof(vr::HmdMatrix44_t) == 64, "Unexpected OpenVR projection layout");

static void json_string(const char* value) {
    std::putchar('"');
    for (const unsigned char* c = reinterpret_cast<const unsigned char*>(value); *c; ++c) {
        if (*c == '"' || *c == '\\') std::printf("\\%c", *c);
        else if (*c < 32) std::printf("\\u%04x", *c);
        else std::putchar(*c);
    }
    std::putchar('"');
}

int wmain(int argc, wchar_t** argv) {
    if (argc != 2) {
        std::fputs("Usage: hp2vr-openvr-probe.exe <absolute-path-to-reviewed-win32-openvr_api.dll>\n", stderr);
        return 2;
    }
    // Caller verifies the source-lock digest before this load.
    HMODULE module = LoadLibraryExW(argv[1], nullptr,
        LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
    if (!module) {
        std::printf("{\"probe_version\":1,\"bits\":32,\"stage\":\"LoadLibraryExW\",\"win32_error\":%lu}\n", GetLastError());
        return 3;
    }
    auto installed = reinterpret_cast<decltype(&vr::VR_IsRuntimeInstalled)>(GetProcAddress(module, "VR_IsRuntimeInstalled"));
    auto runtime_path = reinterpret_cast<decltype(&vr::VR_GetRuntimePath)>(GetProcAddress(module, "VR_GetRuntimePath"));
    if (!installed || !runtime_path) {
        std::puts("{\"probe_version\":1,\"bits\":32,\"stage\":\"resolve_exports\",\"error\":\"required C API missing\"}");
        FreeLibrary(module);
        return 4;
    }
    char path[32768] = {};
    uint32_t required = 0;
    const bool available = installed();
    const bool got_path = runtime_path(path, sizeof(path), &required);
    std::printf("{\"probe_version\":1,\"bits\":32,\"runtime_installed\":%s,\"runtime_path_ok\":%s,\"runtime_path\":",
        available ? "true" : "false", got_path ? "true" : "false");
    json_string(got_path ? path : "");
    std::printf(",\"pointer_bytes\":%zu,\"matrix34_bytes\":%zu,\"matrix44_bytes\":%zu,\"pose_bytes\":%zu,\"texture_bytes\":%zu,\"system_interface\":",
        sizeof(void*), sizeof(vr::HmdMatrix34_t), sizeof(vr::HmdMatrix44_t),
        sizeof(vr::TrackedDevicePose_t), sizeof(vr::Texture_t));
    json_string(vr::IVRSystem_Version);
    std::fputs(",\"runtime_initialized\":false,\"tracking_tested\":false,\"submit_tested\":false,\"hp2_integration\":false}\n", stdout);
    FreeLibrary(module);
    return available && got_path ? 0 : 5;
}
