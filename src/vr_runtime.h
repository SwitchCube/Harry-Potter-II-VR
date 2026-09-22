#pragma once
#include <windows.h>
#include <cstdio>
#include <cmath>
#include <string>
#include <stdexcept>
#include <cstdint>
// Valve's generated C header typedefs bool under __WIN32; use its stdbool path in C++.
// _WIN32 remains defined, preserving the documented Win32 stdcall function tables.
#pragma push_macro("__WIN32")
#undef __WIN32
#include "openvr_capi.h"
#pragma pop_macro("__WIN32")

// OpenVR's documented C function tables avoid mixing C++ runtimes/vtables.
struct VrRuntime {
    HMODULE module=nullptr;
    VR_IVRSystem_FnTable* system=nullptr;
    VR_IVRCompositor_FnTable* compositor=nullptr;
    VR_IVRInput_FnTable* input=nullptr;
    void (__cdecl *shutdown)()=nullptr;
    intptr_t (__cdecl *generic)(const char*,EVRInitError*)=nullptr;
    bool initialized=false;
    HmdMatrix34_t eyeToHead[2]={};
    float tangents[2][4]={};
    uint32_t width=0,height=0;
    int adapter=-1;
    TrackedDevicePose_t poses[64]={};
    std::string error;
    HmdMatrix34_t renderPose={};bool renderReady=false;unsigned submittedMask=0;
    uint64_t renderSerial=0,handoffSerial=0;
    static bool transientSubmit(EVRCompositorError e){return e==EVRCompositorError_VRCompositorError_DoNotHaveFocus||e==EVRCompositorError_VRCompositorError_AlreadySubmitted||e==EVRCompositorError_VRCompositorError_RequestFailed;}
    EVRCompositorError submitEye(int eye,void* image,VRTextureBounds_t* bounds=nullptr){
        if(!renderReady||(submittedMask&(1u<<eye)))return EVRCompositorError_VRCompositorError_AlreadySubmitted;
        VRTextureWithPose_t texture={image,ETextureType_TextureType_DirectX,EColorSpace_ColorSpace_Gamma,renderPose};
        auto result=compositor->Submit(static_cast<EVREye>(eye),reinterpret_cast<Texture_t*>(&texture),bounds,EVRSubmitFlags_Submit_TextureWithPose);
        if(result==EVRCompositorError_VRCompositorError_None){submittedMask|=1u<<eye;if(submittedMask==3)renderReady=false;}
        return result;
    }
    void presentHandoff(){if(compositor&&submittedMask==3&&handoffSerial!=renderSerial){compositor->PostPresentHandoff();handoffSerial=renderSerial;}}


    ~VrRuntime(){close();}
    void close(){
        if(initialized&&shutdown)shutdown();
        initialized=false;system=nullptr;compositor=nullptr;input=nullptr;
        if(module){FreeLibrary(module);module=nullptr;}
    }
    void* table(const char* version){
        EVRInitError err=EVRInitError_VRInitError_None;
        std::string name=std::string("FnTable:")+version;
        intptr_t result=generic(name.c_str(),&err);
        if(err||!result)throw std::runtime_error("OpenVR function table unavailable: "+name+" error="+std::to_string(err));
        return reinterpret_cast<void*>(result);
    }
    bool start(const wchar_t* dll,EVRApplicationType appType=EVRApplicationType_VRApplication_Scene){
        try{
            module=LoadLibraryExW(dll,nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
            if(!module)throw std::runtime_error("Load OpenVR DLL: "+std::to_string(GetLastError()));
            auto init=reinterpret_cast<intptr_t(__cdecl*)(EVRInitError*,EVRApplicationType)>(GetProcAddress(module,"VR_InitInternal"));
            shutdown=reinterpret_cast<void(__cdecl*)()>(GetProcAddress(module,"VR_ShutdownInternal"));
            generic=reinterpret_cast<intptr_t(__cdecl*)(const char*,EVRInitError*)>(GetProcAddress(module,"VR_GetGenericInterface"));
            auto describe=reinterpret_cast<const char*(__cdecl*)(EVRInitError)>(GetProcAddress(module,"VR_GetVRInitErrorAsEnglishDescription"));
            if(!init||!shutdown||!generic)throw std::runtime_error("Missing OpenVR C exports");
            EVRInitError err=EVRInitError_VRInitError_None;
            init(&err,appType);
            if(err)throw std::runtime_error("VR_Init error "+std::to_string(err)+": "+(describe?describe(err):"unknown"));
            initialized=true;
            system=static_cast<VR_IVRSystem_FnTable*>(table(IVRSystem_Version));
            if(appType==EVRApplicationType_VRApplication_Scene){
                compositor=static_cast<VR_IVRCompositor_FnTable*>(table(IVRCompositor_Version));
                input=static_cast<VR_IVRInput_FnTable*>(table(IVRInput_Version));
            }
            system->GetRecommendedRenderTargetSize(&width,&height);
            system->GetDXGIOutputInfo(&adapter);
            if(!width||!height||width>16384||height>16384)throw std::runtime_error("Invalid OpenVR render size");
            for(int eye=0;eye<2;++eye){
                auto e=static_cast<EVREye>(eye);
                eyeToHead[eye]=system->GetEyeToHeadTransform(e);
                system->GetProjectionRaw(e,&tangents[eye][0],&tangents[eye][1],&tangents[eye][2],&tangents[eye][3]);
                for(float f:tangents[eye])if(!std::isfinite(f))throw std::runtime_error("Nonfinite eye projection");
                if(!(tangents[eye][0]<0&&tangents[eye][1]>0&&tangents[eye][2]<0&&tangents[eye][3]>0))
                    throw std::runtime_error("Unsupported OpenVR projection bounds");
            }
            if(compositor)compositor->SetTrackingSpace(ETrackingUniverseOrigin_TrackingUniverseStanding);
            return true;
        }catch(const std::exception& e){error=e.what();close();return false;}
    }
    bool sample(bool wait){
        if(!initialized)return false;
        if(wait){
            renderReady=false;submittedMask=0;
            if(!compositor)return false;
            auto result=compositor->WaitGetPoses(poses,64,nullptr,0);
            if(result!=EVRCompositorError_VRCompositorError_None)return false;
        }else system->GetDeviceToAbsoluteTrackingPose(ETrackingUniverseOrigin_TrackingUniverseStanding,0,poses,64);
        const auto& h=poses[0];
        if(!h.bPoseIsValid||!h.bDeviceIsConnected)return false;
        for(const auto& row:h.mDeviceToAbsoluteTracking.m)for(float f:row)if(!std::isfinite(f))return false;
        if(wait){renderPose=h.mDeviceToAbsoluteTracking;renderReady=true;++renderSerial;}
        return true;
    }
};
static_assert(sizeof(void*)==4,"HP2 integration is x86");
static_assert(sizeof(HmdMatrix34_t)==48,"Unexpected OpenVR C matrix ABI");
static_assert(sizeof(Texture_t)==12,"Unexpected OpenVR x86 texture ABI");

static_assert(sizeof(VRTextureWithPose_t)==60,"Unexpected OpenVR x86 explicit-pose texture ABI");
