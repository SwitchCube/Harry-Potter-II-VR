#pragma once
// Included after the diagnostic renderer: shares only verified game interfaces.
static hpvr::Origin trackingOrigin;
static hpvr::Projection vrProjection;
static Vec eyePosition[2];static Rot eyeRotation[2];
static void* vrFrame=nullptr,*vrPlayer=nullptr,*vrWeapon=nullptr;
static unsigned fovOffset=0,eyeHeightOffset=0;
static bool headBlocked=false;
static unsigned blockedHeadFrames=0,trackingLostFrames=0;
struct ScopedRenderFov {
 void* actor;float saved;
 explicit ScopedRenderFov(void* a):actor(a),saved(mem<float>(a,fovOffset)){mem<float>(a,fovOffset)=vrProjection.fov;}
 ~ScopedRenderFov(){mem<float>(actor,fovOffset)=saved;}
};
using ChildFrame=void*(__thiscall*)(void*,void*,void*,void*,int,int,float,const void*,const void*,void*);
static ChildFrame originalChildFrame=nullptr;
static unsigned childProjectionChecks[2]={};
static void* __fastcall childFrameHook(void* renderer,void*,void* parent,void* span,void* level,int surface,int zone,float mirror,const void* plane,const void* coords,void* bounds){
 void* child=originalChildFrame(renderer,parent,span,level,surface,zone,mirror,plane,coords,bounds);
 if(vrRendering&&child&&parent){
  FpuState fpu;float a=mem<float>(parent,0xdc),b=mem<float>(child,0xdc);
  bool matches=std::isfinite(a)&&std::isfinite(b)&&std::fabs(a-b)<.05f;
  if(++childProjectionChecks[secondary?1:0]<=3||!matches)log("{\"event\":\"child_projection\",\"eye\":%u,\"parent_focal\":%.6g,\"child_focal\":%.6g,\"matched\":%s}",secondary?1u:0u,a,b,matches?"true":"false");
  need(matches,"Sky/portal projection differs from parent VR eye");fpu.restore();
 }
 return child;
}
using Mesh=void(__thiscall*)(void*,void*,void*,void*,const void*,DWORD);
static Mesh originalMesh=nullptr,originalLodMesh=nullptr;
using LineCheck=int(__thiscall*)(void*,void*,void*,const Vec&,const Vec&,DWORD,Vec,BYTE);
static bool clearHead(void* actor,Vec from,Vec to){
 DWORD hit[11]={};float one=1;std::memcpy(&hit[9],&one,4);hit[10]=0xffffffff;
 // AActor::execTrace loads XLevel from +0x94, trace flags 6 exclude actors.
 void* level=mem<void*>(actor,0x94);need(level!=nullptr,"Missing original XLevel");
 return reinterpret_cast<LineCheck>(targets.at("line"))(level,hit,actor,to,from,6,Vec{2,2,2},0)!=0;
}
static hpvr::Mat copyMatrix(const HmdMatrix34_t& m){hpvr::Mat result;std::memcpy(&result,&m,sizeof(result));return result;}
static bool reflectedView(void* frame){
 // CreateChildFrame writes Mirror at +0x20 and Parent at +0x08 (verified original).
 for(unsigned depth=0;frame&&depth<32;++depth){void* parent=mem<void*>(frame,8);if(!parent)return false;if(mem<float>(frame,0x20)!=mem<float>(parent,0x20))return true;frame=parent;}return false;
}
static void noteReflectedPlayer(void* frame,void* actor){
 if(!vrRendering||actor!=vrPlayer||!reflectedView(frame))return;
 static std::map<void*,unsigned> seen;unsigned bit=secondary?2:1;
 auto mesh=mem<void*>(actor,propertyOffset(actor,L"Mesh",4,L"ObjectProperty"));
 if(seen[mesh]&bit)return;seen[mesh]|=bit;if(startup.mode==5&&profileFlag(L"replay-mechanics-mirror.flag"))mechanicCaptureIndex=objectName(mesh)==L"skGoyleMesh"?6:5;
 log("{\"event\":\"reflected_player\",\"eye\":%u,\"mesh\":\"%ls\",\"current_game_model\":true}",secondary?1:0,mesh?objectName(mesh).c_str():L"None");
}
static void __fastcall meshHook(void* self,void*,void* frame,void* sprite,void* actor,const void* coords,DWORD flags){
 if(vrRendering&&((actor==vrPlayer&&!worldSceneActive)||(actor==vrWeapon&&!usingSword()))&&!reflectedView(frame)){if(actor==vrPlayer&&canDrawHandSword()){originalMesh(self,frame,sprite,vrWeapon,coords,flags);noteSwordMesh(vrWeapon);}++hiddenMeshes;return;}originalMesh(self,frame,sprite,actor,coords,flags);if(vrRendering){noteCarryMesh(actor);noteSwordMesh(actor);noteReflectedPlayer(frame,actor);}
}
static void __fastcall lodMeshHook(void* self,void*,void* frame,void* sprite,void* actor,const void* coords,DWORD flags){
 if(vrRendering&&((actor==vrPlayer&&!worldSceneActive)||(actor==vrWeapon&&!usingSword()))&&!reflectedView(frame)){++hiddenMeshes;return;}originalLodMesh(self,frame,sprite,actor,coords,flags);if(vrRendering){noteCarryMesh(actor);noteSwordMesh(actor);noteReflectedPlayer(frame,actor);}
}
static void* __fastcall createHook(void* renderer,void*,void* viewport,Vec position,Rot rotation,void* bounds){
 if(levelLoadDepth){vrRendering=false;vrFrame=nullptr;return createFrame(renderer,viewport,position,rotation,bounds);}
 if(mem<DWORD>(renderer,0xa8)!=0||secondary)return createFrame(renderer,viewport,position,rotation,bounds);
 vrFrame=nullptr;vrRendering=false;if(startup.mode==3)runtime.renderReady=false;
 if(!firstDrawTime)firstDrawTime=GetTickCount64();
 if(GetTickCount64()-firstDrawTime<(startup.mode==3?1000u:10000u))return createFrame(renderer,viewport,position,rotation,bounds);
 FpuState fpu;
 try{
  void* actor=mem<void*>(viewport,0x30);need(actor!=nullptr,"No viewport actor");
  if(actor!=vrPlayer){
   need(propertyOffset(actor,L"Location",12,L"StructProperty")==0x11c&&propertyOffset(actor,L"Rotation",12,L"StructProperty")==0x128,"Player reflection differs");
   fovOffset=propertyOffset(actor,L"FOVAngle",4,L"FloatProperty");
   eyeHeightOffset=propertyOffset(actor,L"BaseEyeHeight",4,L"FloatProperty");
   unsigned weaponOffset=propertyOffset(actor,L"Weapon",4,L"ObjectProperty");vrWeapon=mem<void*>(actor,weaponOffset);
   vrPlayer=actor;trackingOrigin.ready=false;
   log("{\"event\":\"vr_player\",\"class\":\"%ls\",\"fov_offset\":%u,\"eye_height_offset\":%u,\"units_per_metre\":%.6g}",objectName(mem<void*>(actor,0x24)).c_str(),fovOffset,eyeHeightOffset,vrSettings.units);
  }
  cinemaActive=detectCinema(actor);
  vrProfile.begin();
  hpvr::Mat h=hpvr::identity(),eye[2]={hpvr::identity(),hpvr::identity()};float tangents[2][4];
  if(startup.mode==3){
   bool validPose;{VrStageTimer measured(VrProfile::PoseWait);validPose=runtime.sample(true);}
   if(!validPose||!runtime.compositor->CanRenderScene()){
    ++trackingLostFrames;vrProfile.active=false;vrProfile.previousEnd=0;runtime.compositor->ClearLastSubmittedFrame();fpu.restore();return createFrame(renderer,viewport,position,rotation,bounds);
   }
   h=copyMatrix(runtime.poses[0].mDeviceToAbsoluteTracking);
   for(int e=0;e<2;++e)eye[e]=copyMatrix(runtime.eyeToHead[e]);std::memcpy(tangents,runtime.tangents,sizeof(tangents));
  }else{
   // Explicitly labelled hardware-free replay through the SAME real game renderer/GPU path.
   static bool sweep=profileFlag(L"replay-look-around.flag");
   float angle=sweep?vrFrames*.004f:.2f*std::sin(vrFrames*.008f);h.m[0][0]=std::cos(angle);h.m[0][2]=-std::sin(angle);h.m[2][0]=std::sin(angle);h.m[2][2]=std::cos(angle);
   if(sweep){auto pitch=hpvr::identity();float a=.9f*std::sin(vrFrames*.0017f);pitch.m[1][1]=pitch.m[2][2]=std::cos(a);pitch.m[1][2]=-std::sin(a);pitch.m[2][1]=std::sin(a);h=hpvr::multiply(h,pitch);}
   h.m[0][3]=.05f*std::sin(vrFrames*.01f);eye[0].m[0][3]=-.032f;eye[1].m[0][3]=.032f;
   float t[2][4]={{-1.1f,1,-1,1},{-1,1.1f,-1,1}};if(sweep){float quest[2][4]={{-1.3763818f,.83909953f,-1.4281479f,.96568882f},{-.83909953f,1.3763818f,-1.4281479f,.96568882f}};std::memcpy(t,quest,sizeof(t));}
   std::memcpy(tangents,t,sizeof(t));
  }
  if(!trackingOrigin.ready)trackingOrigin.recenter(h,(lessonWorldActive?rotation.yaw:mem<Rot>(actor,0x128).yaw)*(hpvr::pi/32768.0f));
  mechanicView(actor,h,rotation);
  if(cinemaActive){vrProfile.active=false;vrProfile.previousEnd=0;fpu.restore();return createFrame(renderer,viewport,position,rotation,bounds);}
  advanceViewAnchor(actor);
  // The original exercise camera is centred on its world-space arrow shape.
  // Use that eye anchor only while the stock PlayGame state owns player input.
  Vec anchor=position;if(!lessonWorldActive&&!worldSceneActive){anchor=playerViewAnchor();}
  for(int e=0;e<2;++e){auto pose=hpvr::multiply(h,eye[e]);eyePosition[e]=trackingOrigin.position(pose,anchor,vrSettings.units);eyeRotation[e]=hpvr::rotation(pose,trackingOrigin.baseYaw);pitchFlightEye(anchor,eyePosition[e],eyeRotation[e]);}
  if(startup.mode==5&&worldSceneActive&&profileFlag(L"replay-mechanics-cinema.flag")){static unsigned samples=0;if(++samples%60==1)log(R"({"event":"cinematic_camera_sample","spatial_option":%s,"anchor":[%.6g,%.6g,%.6g],"player_distance":%.6g,"eye_distance":%.6g,"camera_yaw":%d})",vrPresentation.spatialScenes?"true":"false",anchor.x,anchor.y,anchor.z,std::sqrt(hpvr::dot(hpvr::sub(anchor,mem<Vec>(actor,0x11c)),hpvr::sub(anchor,mem<Vec>(actor,0x11c)))),std::sqrt(hpvr::dot(hpvr::sub(eyePosition[0],eyePosition[1]),hpvr::sub(eyePosition[0],eyePosition[1]))),rotation.yaw);}
  headBlocked=!clearHead(actor,anchor,eyePosition[0])||!clearHead(actor,anchor,eyePosition[1]);if(headBlocked)++blockedHeadFrames;
  refreshHandForRender();
  // Viewport sizes are read from the verified first frame below, then recomputed once.
  fpu.restore();
  void* first=createFrame(renderer,viewport,eyePosition[0],eyeRotation[0],bounds);
  need(first&&mem<DWORD>(renderer,0xa8)==1,"VR primary master creation");
  unsigned w=mem<DWORD>(first,0xa8),hgt=mem<DWORD>(first,0xac);need(w>=64&&w<=4096&&hgt>=64&&hgt<=4096,"Unreviewed scene dimensions");
  protectVrResolution(viewport,w,hgt);
  vrProjection=hpvr::projection(tangents,float(w)/hgt);
  if(startup.mode==3&&vrFrames==0)for(unsigned e=0;e<2;++e){
   auto matrix=runtime.system->GetProjectionMatrix(static_cast<EVREye>(e),.1f,100);
   float error=hpvr::projectionMatrixError(vrProjection,e,matrix.m);auto b=vrProjection.eye[e];
   log("{\"event\":\"projection_matrix_reference\",\"eye\":%u,\"max_uv_error\":%.9g,\"bounds\":[%.8g,%.8g,%.8g,%.8g]}",e,error,b.uMin,b.vMin,b.uMax,b.vMax);
   need(error<.0001f,"Eye crop differs from runtime projection matrix");
  }
  vrRendering=true;vrFrame=first;fpu.restore();return first;
 }catch(const std::exception& e){log("{\"event\":\"fatal\",\"stage\":\"vr_create\",\"reason\":\"%s\"}",e.what());stopOwnProcess(130);}
}
static void gpuCapture(IDirectDrawSurface7* surface,unsigned eye,const wchar_t* file,bool verify){
 if(!nativeCapturesEnabled())file=nullptr;
 DDSURFACEDESC2 d={};d.dwSize=sizeof(d);{VrStageTimer measured(VrProfile::SurfaceLock);hr(surface->Lock(nullptr,&d,DDLOCK_READONLY|DDLOCK_WAIT|DDLOCK_NOSYSLOCK,nullptr),"Lock actual eye target");}
 try{
  if(bridge.device&&(bridge.width!=d.dwWidth||bridge.height!=d.dwHeight)){
   if(startup.mode==3)runtime.compositor->ClearLastSubmittedFrame();
   log("{\"event\":\"bridge_resize\",\"width\":%lu,\"height\":%lu}",d.dwWidth,d.dwHeight);
  }
  bridge.open(startup.mode==3?runtime.adapter:0,d.dwWidth,d.dwHeight);verify=verify||!(bridge.verifiedMask&(1u<<eye));bridge.upload(eye,d,verify,file!=nullptr);
  if(verify&&eye==0)log("{\"event\":\"vr_bridge\",\"mode\":\"%s\",\"direct_source_upload\":%s}",bridge.modeName(),bridge.directBgrx?"true":"false");
  if(verify){++gpuVerifiedEyes;log("{\"event\":\"gpu_texture_verified\",\"eye\":%u,\"generation\":%u,\"level_generation\":%u,\"width\":%u,\"height\":%u}",eye,bridge.generation,levelGeneration,bridge.width,bridge.height);}
 }catch(...){surface->Unlock(nullptr);throw;}
 hr(surface->Unlock(nullptr),"Unlock actual eye target");
 // Reuse the exact CPU pixels uploaded above: do not reread uncached D3D7 memory
 // one pixel at a time just to write diagnostic images, or hold the GPU lock.
 if(file){d.lpSurface=bridge.pixels.data();d.lPitch=LONG(bridge.width*4);VrStageTimer measured(VrProfile::Capture);writeBmp(file,d);}
 bridge.releaseCpu();
}
static float prepareEyeProjection(void* frame,void* actor,unsigned eye){
 FpuState fpu;float saved=mem<float>(actor,fovOffset);mem<float>(actor,fovOffset)=vrProjection.fov;
 reinterpret_cast<FinishFrame>(targets.at("size"))(frame);computeCoords(frame,eyePosition[eye],eyeRotation[eye]);mem<float>(actor,fovOffset)=saved;
 float actual=mem<float>(frame,0xdc),expected=mem<DWORD>(frame,0xa8)*.5f/vrProjection.tanH;
 need(std::isfinite(actual)&&std::fabs(actual-expected)<.05f,"Actual eye projection differs from runtime tangents");fpu.restore();return actual;
}
// Hiding the draw must not skip stock skeletal preparation. ApplyAnim(false)
// also updates Harry's animation channels and the root-motion reference between
// climbs. GetRootMovement's physics-only ApplyAnim(true) does not do that.
static unsigned hiddenAnimationPreparations=0;
static void prepareHiddenPlayerAnimation(void* actor){
 if(startup.mode==5&&profileFlag(L"replay-stale-animation.flag"))return;
 void* mesh=mem<void*>(actor,propertyOffset(actor,L"Mesh",4,L"ObjectProperty"));
 if(!mesh)return;
 need(objectName(mem<void*>(mesh,0x24))==L"SkeletalMesh","Hidden player mesh is not the reviewed skeletal class");
 BYTE pose[24];std::memcpy(pose,static_cast<BYTE*>(actor)+0x11c,24);
 using ApplyAnim=void(__thiscall*)(void*,void*,void*,bool);
 FpuState fpu;fpu.restore();reinterpret_cast<ApplyAnim>(targets.at("applyanim"))(mesh,actor,nullptr,false);
 need(!std::memcmp(pose,static_cast<BYTE*>(actor)+0x11c,24),"Animation preparation changed physics pose");
 if(++hiddenAnimationPreparations==1)log("{\"event\":\"hidden_animation_preparation\",\"stock_apply_anim\":true,\"physics_only\":false,\"pose_unchanged\":true}");
 fpu.restore();
}
static void vrWorld(void* renderer,void* frame){
 FpuState fpu;
 try{
  need(GetCurrentThreadId()==gameThread&&mem<DWORD>(renderer,0xa8)==1,"VR game thread/master lifetime");
  ++draws;void* viewport=mem<void*>(frame,0),*actor=mem<void*>(viewport,0x30),*rd=mem<void*>(viewport,0x5c);
  auto device=mem<IDirect3DDevice7*>(rd,0x9a4);need(device!=nullptr,"Missing D3D7 device");
  ScopedRenderFov eyeFov(actor);
  ReflectionBodyScope reflectedBody(actor); // Body enters original visibility lists; primary-eye mesh remains suppressed.
  if(!worldSceneActive)prepareHiddenPlayerAnimation(actor);
  updateThrowAim();updateFlightCues();CarryRenderScope carriedPose;SwordRenderScope swordPose;
  BYTE playerPose[24];std::memcpy(playerPose,static_cast<BYTE*>(actor)+0x11c,24);unsigned tick=ticks;
  if(!firstVrPairTime)firstVrPairTime=GetTickCount64();
  bool captureFirst=vrFrames==0,captureMotion=vrFrames==120;bool captureMechanic=mechanicCaptureIndex&&!captureFirst&&!captureMotion;
  float focalA=prepareEyeProjection(frame,actor,0);
  secondOcclusion=false;primaryEvents.clear();renderedTextures.clear();preparedScriptedTextures.clear();active=true;secondary=false;
  static bool textureFixtureDone=false;bool checkTextures=!textureFixtureDone&&scriptedTextureFixture.size()==4;
  unsigned textureHashes[4]={};if(checkTextures)for(unsigned i=0;i<4;++i)if(i!=1){scriptedTickHook(scriptedTextureFixture[i],nullptr,0);textureHashes[i]=texturePixelHash(scriptedTextureFixture[i]);}
  hr(device->Clear(0,nullptr,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,0,1,0),"Clear left eye");fpu.restore();{VrStageTimer measured(VrProfile::WorldLeft);originalWorld(renderer,frame);}
  drawWand(device,frame,0);
  if(headBlocked)hr(device->Clear(0,nullptr,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,0,1,0),"Head collision fade");
  hr(device->EndScene(),"End left eye");IDirectDrawSurface7* surface=nullptr;hr(device->GetRenderTarget(&surface),"Get eye surface");
  bool captureLesson=lessonWorldActive&&lessonCaptureRequested&&!captureFirst&&!captureMotion;
  gpuCapture(surface,0,captureFirst?L"native-a.bmp":captureMotion?L"native-motion-a.bmp":captureLesson?L"native-lesson.bmp":captureMechanic?mechanicCapturePath(0):nullptr,captureFirst);
  if(captureLesson){lessonCaptureRequested=false;log("{\"event\":\"lesson_world_capture\",\"stereo\":true,\"original_camera_anchor\":true}");}
  hr(device->Clear(0,nullptr,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,0,1,0),"Clear right eye");hr(device->BeginScene(),"Begin right eye");
  secondary=true;
  if(checkTextures){for(unsigned i=0;i<4;++i){scriptedTickHook(scriptedTextureFixture[i],nullptr,0);unsigned hash=texturePixelHash(scriptedTextureFixture[i]);need(i==1||hash==textureHashes[i],"Second eye erased scripted score pixels");log("{\"event\":\"scripted_texture_fixture\",\"texture\":\"%ls\",\"right_only\":%s,\"pixel_hash\":%u,\"duplicate_preserved\":%s}",objectName(scriptedTextureFixture[i]).c_str(),i==1?"true":"false",hash,i==1?"false":"true");}textureFixtureDone=true;}
  float savedFov=mem<float>(actor,fovOffset);mem<float>(actor,fovOffset)=vrProjection.fov;fpu.restore();
  void* second=createFrame(renderer,viewport,eyePosition[1],eyeRotation[1],nullptr);mem<float>(actor,fovOffset)=savedFov;
  need(second&&second!=frame&&mem<DWORD>(renderer,0xa8)==2,"VR right master creation");
  float focalB=prepareEyeProjection(second,actor,1);need(std::fabs(focalA-focalB)<.001f,"Different stereo focal lengths");
  Vec origin=mem<Vec>(second,0x34);need(std::fabs(origin.x-eyePosition[1].x)<.001f&&std::fabs(origin.y-eyePosition[1].y)<.001f&&std::fabs(origin.z-eyePosition[1].z)<.001f,"Right eye actual origin differs");
  void* engine=mem<void*>(renderer,0x2c),*audio=mem<void*>(engine,0x5c);mem<void*>(engine,0x5c)=nullptr;fpu.restore();
  {VrStageTimer measured(VrProfile::WorldRight);originalWorld(renderer,second);}mem<void*>(engine,0x5c)=audio;++extraDraws;
  drawWand(device,second,1);
  if(headBlocked)hr(device->Clear(0,nullptr,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,0,1,0),"Right head collision fade");
  hr(device->EndScene(),"End right eye");gpuCapture(surface,1,captureFirst?L"native-b.bmp":captureMotion?L"native-motion-b.bmp":captureMechanic?mechanicCapturePath(1):nullptr,captureFirst);if(captureMechanic)mechanicCaptureIndex=0;
  {VrStageTimer measured(VrProfile::Finish);hr(device->BeginScene(),"Resume game scene");surface->Release();secondary=false;finishFrame(renderer);fpu.restore();
  computeCoords(frame,eyePosition[0],eyeRotation[0]);need(secondOcclusion&&ticks==tick&&mem<DWORD>(renderer,0xa8)==1,"VR visibility/tick/lifetime failed");
  need(!std::memcmp(playerPose,static_cast<BYTE*>(actor)+0x11c,24),"Rendering changed gameplay player pose");
  active=false;bridge.flush();}
  if(startup.mode==3){
   VrStageTimer measured(VrProfile::Submit);bool both=true;
   for(int e=0;e<2;++e){Texture_t texture={bridge.textures[e],ETextureType_TextureType_DirectX,EColorSpace_ColorSpace_Gamma};auto b=vrProjection.eye[e];VRTextureBounds_t bounds={b.uMin,b.vMin,b.uMax,b.vMax};
    auto result=runtime.submitEye(e,bridge.textures[e],&bounds);
    if(VrRuntime::transientSubmit(result)){both=false;log("{\"event\":\"submit_deferred\",\"path\":\"world\",\"eye\":%d,\"error\":%d}",e,int(result));continue;}
    if(result!=EVRCompositorError_VRCompositorError_None)throw std::runtime_error("OpenVR Submit eye="+std::to_string(e)+" error="+std::to_string(result));}
   if(both)++submittedPairs;
  }
  ++vrFrames;completed=true;vrRendering=false;vrProfile.end(vrFrames,captureFirst||captureMotion);vrProfile.reportInterval(logFile,vrFrames);
  if(startup.mode==3&&(vrFrames==1||vrFrames%300==0)){
   log("{\"event\":\"hmd_activity\",\"frame\":%u,\"level\":%d}",vrFrames,int(runtime.system->GetTrackedDeviceActivityLevel(0)));
   Compositor_FrameTiming timing={};timing.m_nSize=sizeof(timing);
   if(runtime.compositor->GetFrameTiming(&timing,0))log("{\"event\":\"vr_timing\",\"runtime_frame\":%u,\"client_interval_ms\":%.6g,\"render_gpu_ms\":%.6g,\"compositor_gpu_ms\":%.6g,\"dropped\":%u,\"reprojection_flags\":%u}",timing.m_nFrameIndex,timing.m_flClientFrameIntervalMs,timing.m_flTotalRenderGpuMs,timing.m_flCompositorRenderGpuMs,timing.m_nNumDroppedFrames,timing.m_nReprojectionFlags);
  }
  if(captureFirst||captureMotion||vrFrames%300==0)log("{\"event\":\"vr_frame\",\"frame\":%u,\"mode\":\"%s\",\"submitted_pairs\":%u,\"gpu_verified_eyes\":%u,\"width\":%u,\"height\":%u,\"fov\":%.5f,\"focal_a\":%.6g,\"focal_b\":%.6g,\"left\":[%.5f,%.5f,%.5f],\"right\":[%.5f,%.5f,%.5f],\"rotation\":[%d,%d,%d],\"extra_ticks\":0,\"pose_unchanged\":true,\"head_blocked\":%s}",vrFrames,startup.mode==3?"runtime":"replay",submittedPairs,gpuVerifiedEyes,bridge.width,bridge.height,vrProjection.fov,focalA,focalB,eyePosition[0].x,eyePosition[0].y,eyePosition[0].z,eyePosition[1].x,eyePosition[1].y,eyePosition[1].z,eyeRotation[0].pitch,eyeRotation[0].yaw,eyeRotation[0].roll,headBlocked?"true":"false");
  fpu.restore();
 }catch(const std::exception& e){log("{\"event\":\"fatal\",\"stage\":\"vr_world\",\"reason\":\"%s\"}",e.what());stopOwnProcess(131);}
}
static void __fastcall worldHook(void* renderer,void*,void* frame){
 if(startup.mode<3){diagnosticWorldHook(renderer,nullptr,frame);return;}
 if(vrRendering&&frame==vrFrame&&!secondary)vrWorld(renderer,frame);else originalWorld(renderer,frame);
}
