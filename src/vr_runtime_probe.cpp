// Active runtime/pose diagnostic. Does not submit synthetic content or claim game integration.
#include "vr_runtime.h"
#include "vr_math.h"

static void escaped(const char* s){
    std::putchar('"');for(;*s;++s){if(*s=='"'||*s=='\\')std::putchar('\\');if(static_cast<unsigned char>(*s)>=32)std::putchar(*s);}std::putchar('"');
}
int wmain(int argc,wchar_t** argv){
    setvbuf(stdout,nullptr,_IONBF,0);
    if(argc!=2)return 2;
    VrRuntime vr;
    if(!vr.start(argv[1],EVRApplicationType_VRApplication_Background)){std::printf("{\"event\":\"vr_init_failed\",\"error\":");escaped(vr.error.c_str());std::puts("}");return 20;}
    std::printf("{\"event\":\"runtime_ready\",\"width\":%u,\"height\":%u,\"adapter\":%d,\"pose_bytes\":%u}\n",vr.width,vr.height,vr.adapter,unsigned(sizeof(TrackedDevicePose_t)));
    for(int e=0;e<2;++e)std::printf("{\"event\":\"eye\",\"eye\":%d,\"offset\":[%.8g,%.8g,%.8g],\"tangents\":[%.8g,%.8g,%.8g,%.8g]}\n",e,vr.eyeToHead[e].m[0][3],vr.eyeToHead[e].m[1][3],vr.eyeToHead[e].m[2][3],vr.tangents[e][0],vr.tangents[e][1],vr.tangents[e][2],vr.tangents[e][3]);
    auto crop=hpvr::projection(vr.tangents,4.0f/3);
    for(unsigned e=0;e<2;++e){auto matrix=vr.system->GetProjectionMatrix(static_cast<EVREye>(e),.1f,100);
        float error=hpvr::projectionMatrixError(crop,e,matrix.m);
        std::printf("{\"event\":\"projection_matrix_reference\",\"eye\":%u,\"max_uv_error\":%.9g,\"passed\":%s}\n",e,error,error<.0001f?"true":"false");
        if(error>=.0001f)return 22;
    }
    auto apps=static_cast<VR_IVRApplications_FnTable*>(vr.table(IVRApplications_Version));
    std::printf("{\"event\":\"scene_status\",\"application_type\":\"background\",\"state\":%d,\"scene_pid\":%u}\n",int(apps->GetSceneApplicationState()),apps->GetCurrentSceneProcessId());
    unsigned valid=0,active=0;
    for(unsigned i=0;i<50;++i){valid+=vr.sample(false)?1:0;active+=vr.system->GetTrackedDeviceActivityLevel(0)==EDeviceActivityLevel_k_EDeviceActivityLevel_UserInteraction?1:0;Sleep(100);}
    std::printf("{\"event\":\"probe_complete\",\"valid_hmd_poses\":%u,\"user_interaction_samples\":%u,\"final_activity_level\":%d,\"samples\":50,\"hp2_integration\":false,\"submitted\":false}\n",valid,active,int(vr.system->GetTrackedDeviceActivityLevel(0)));
    return valid?0:21;
}
