#include "vr_runtime.h"
#include <cstdlib>
static unsigned submits=0,handoffs=0;static EVRCompositorError reply=EVRCompositorError_VRCompositorError_None;
static void check(bool v,const char* message){if(!v){std::fprintf(stderr,"Runtime check failed: %s\n",message);std::exit(1);}}
static EVRCompositorError __stdcall submit(EVREye,Texture_t* texture,VRTextureBounds_t*,EVRSubmitFlags flags){
 ++submits;check(flags==EVRSubmitFlags_Submit_TextureWithPose,"explicit render pose");
 auto* withPose=reinterpret_cast<VRTextureWithPose_t*>(texture);check(withPose->mDeviceToAbsoluteTracking.m[0][3]==3.25f,"render pose survived newer input sample");return reply;
}
static void __stdcall handoff(){++handoffs;}
int main(){
 VR_IVRCompositor_FnTable api={};api.Submit=submit;api.PostPresentHandoff=handoff;
 VrRuntime r;r.compositor=&api;r.renderPose.m[0][3]=3.25f;r.poses[0].mDeviceToAbsoluteTracking.m[0][3]=42;
 check(r.submitEye(0,nullptr)==EVRCompositorError_VRCompositorError_AlreadySubmitted&&submits==0,"no submit without render sample");
 r.renderReady=true;r.renderSerial=1;
 check(r.submitEye(0,nullptr)==EVRCompositorError_VRCompositorError_None&&r.submittedMask==1,"left accepted");
 check(r.submitEye(0,nullptr)==EVRCompositorError_VRCompositorError_AlreadySubmitted&&submits==1,"duplicate rejected locally");
 check(r.submitEye(1,nullptr)==EVRCompositorError_VRCompositorError_None&&!r.renderReady&&r.submittedMask==3,"complete pair closes frame");
 r.presentHandoff();r.presentHandoff();check(handoffs==1,"one handoff per pair");
 r.renderReady=true;r.submittedMask=0;r.renderSerial=2;reply=EVRCompositorError_VRCompositorError_DoNotHaveFocus;
 check(VrRuntime::transientSubmit(r.submitEye(0,nullptr))&&r.submittedMask==0,"focus loss does not complete eye");
 r.presentHandoff();check(handoffs==1,"no handoff on rejected pair");
 reply=EVRCompositorError_VRCompositorError_None;r.submitEye(0,nullptr);r.submitEye(1,nullptr);r.presentHandoff();check(handoffs==2,"next frame recovers");
 std::puts("VR frame lifecycle: explicit pose, duplicate/loading guard, focus rejection, one handoff and recovery passed");
}
