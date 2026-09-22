#pragma once
#include "vr_health_math.h"
#include "vr_hud_capture.h"
static bool beginCapture(VrHudCapture& capture,IDirect3DDevice7* device){
 unsigned failure=0;
 if(startup.mode==4||startup.mode==5){
  if(profileFlag(L"replay-hud-borrowed.flag"))failure=2;
  else if(profileFlag(L"replay-hud-texture.flag"))failure=1;
 }
 return capture.begin(device,failure);
}
static VrHudCapture menuCapture;
static void beginMenu(void* self,void* function){
 if(startup.mode<3||startup.mode>5||!gameConsole||self!=gameConsole||!currentViewport||!vrFrames||levelLoadDepth||bindingPanelActive||!boolField(gameConsole,L"bUWindowActive")||vendorPrompt()||objectName(function)!=L"PostRender")return;
 auto renderer=mem<void*>(currentViewport,0x5c);auto device=mem<IDirect3DDevice7*>(renderer,0x9a4);need(device!=nullptr,"Menu device missing");FpuState fpu;beginCapture(menuCapture,device);fpu.restore();
}
static VrHudCapture hudCapture;
static VrBridge hudBridge;
static VrMenu vrHud("hud");
static bool hudCaptured=false;static unsigned hudLogicalWidth=0,hudLogicalHeight=0;
static void beginHud(void* self,void* function){
 if(self!=gameHud||startup.mode<3||!vrSettings.hud||!vrPlayer||!currentViewport||!vrFrames||menuVisible||cinemaActive||levelLoadDepth)return;
 if(objectName(function)!=L"PostRender")return;
 auto renderer=mem<void*>(currentViewport,0x5c);auto device=mem<IDirect3DDevice7*>(renderer,0x9a4);
 need(device!=nullptr,"HUD render device missing");FpuState fpu;
 if(beginCapture(hudCapture,device)){static bool first=true;if(first){log("{\"event\":\"hud_capture_started\",\"class\":\"%ls\"}",objectName(mem<void*>(self,0x24)).c_str());first=false;}}fpu.restore();
}
static void captureHud(IDirect3DDevice7* device){
 if(!hudCapture.active)return;
 auto surface=hudCapture.target;DDSURFACEDESC2 d={};d.dwSize=sizeof(d);
 RECT hudRect={0,0,LONG(hudLogicalWidth),LONG(hudLogicalHeight)};
 hr(surface->Lock(hudLogicalWidth&&hudLogicalHeight?&hudRect:nullptr,&d,DDLOCK_READONLY|DDLOCK_WAIT|DDLOCK_NOSYSLOCK,nullptr),"Read original HUD");
 try{
  if(hudLogicalWidth&&hudLogicalHeight){d.dwWidth=hudLogicalWidth;d.dwHeight=hudLogicalHeight;}
  if(hudBridge.device&&(hudBridge.width!=d.dwWidth||hudBridge.height!=d.dwHeight))vrHud.close();
  hudBridge.open(startup.mode==3?runtime.adapter:0,d.dwWidth,d.dwHeight,true,&bridge);
  unsigned visible=hudBridge.uploadArgb(d,!(hudBridge.verifiedMask&1u),hudCapture.sourceAlpha);
  static unsigned captures=0;if(++captures==1)log("{\"event\":\"hud_first_capture\",\"visible_pixels\":%u}",visible);
  if(startup.mode==5&&profileFlag(L"replay-mechanics-vendor.flag")&&vendorPrompt()){static bool done=false;if(!done&&vendorYes){auto copy=d;copy.lpSurface=hudBridge.pixels.data();copy.lPitch=LONG(hudBridge.width*4);writeBmp(L"native-vendor.bmp",copy);done=true;}}
  if(!hudCaptured){auto copy=d;copy.lpSurface=hudBridge.pixels.data();copy.lPitch=LONG(hudBridge.width*4);writeBmp(L"native-hud.bmp",copy);log("{\"event\":\"hud_texture\",\"width\":%lu,\"height\":%lu,\"visible_pixels\":%u,\"gpu_verified\":true}",d.dwWidth,d.dwHeight,visible);hudCaptured=true;}
 }catch(...){surface->Unlock(hudLogicalWidth&&hudLogicalHeight?&hudRect:nullptr);throw;}
 hr(surface->Unlock(hudLogicalWidth&&hudLogicalHeight?&hudRect:nullptr),"Unlock HUD");hudBridge.flush();
 if(startup.mode==3&&runtime.compositor->CanRenderScene()&&!headBlocked){vrHud.width=vrSettings.hudWidth;vrHud.submit(runtime,hudBridge.textures[0],hudBridge.width,hudBridge.height,true,true);}else vrHud.hide();
}

static bool scaledHud(void* self,void* frame,void* result){
 if(self!=gameHud||startup.mode<3||startup.mode==6||menuVisible||cinemaActive||!vrPlayer||!vrFrames||levelLoadDepth||objectName(mem<void*>(frame,4))!=L"PostRender")return false;
 void* fn=mem<void*>(frame,4);need(mem<WORD>(fn,0x7a)==4,"HUD PostRender signature differs");void* locals=mem<void*>(frame,0x10);void* canvas=locals?mem<void*>(locals,0):nullptr;need(canvas!=nullptr,"HUD Canvas missing");
 unsigned sx=field(canvas,L"SizeX",4,L"IntProperty"),sy=field(canvas,L"SizeY",4,L"IntProperty");int w=mem<int>(canvas,sx),h=mem<int>(canvas,sy);
 need(w>=64&&h>=64&&w<=4096&&h<=4096,"HUD Canvas extent invalid");hudLogicalWidth=std::min(w,(startup.mode==4&&profileFlag(L"replay-full-hud.flag"))?2048:1024);hudLogicalHeight=std::max(64,int(double(h)*hudLogicalWidth/w));
 RestoreFields scope;scope.set(canvas,sx,int(hudLogicalWidth));scope.set(canvas,sy,int(hudLogicalHeight));scope.set(canvas,field(canvas,L"ClipX",4,L"FloatProperty"),float(hudLogicalWidth));scope.set(canvas,field(canvas,L"ClipY",4,L"FloatProperty"),float(hudLogicalHeight));
 beginHud(self,fn);originalInternal(self,frame,result);return true;
}

// Render only the original health DrawItem into its own transparent surface.
// Full-size D3D7 targets preserve the engine viewport and Canvas scale; only the
// small bounded health rectangle crosses the CPU/D3D11 bridge.
static VrHudCapture healthCapture;
static VrBridge healthBridge;
static VrMenu vrHealth("left-health");
static bool healthFresh=false;
static ULONGLONG healthTime=0;
static unsigned healthProofMask=0;
static bool healthHeld(){
 if(!vrSettings.hud||!vrPlayer||menuVisible||cinemaActive||headBlocked||levelLoadDepth)return false;
 if(startup.mode==3)return ((controlsArmed&&vrInput.showHealth&&statusPage==0)||GetTickCount64()<healthAutoUntil)&&vrInput.leftGrip.bActive&&vrInput.leftGrip.pose.bPoseIsValid&&vrInput.leftGrip.pose.bDeviceIsConnected&&runtime.compositor->CanRenderScene();
 return startup.mode==5&&profileFlag(L"replay-health.flag")&&!(controlsFrames>=750&&controlsFrames<850);
}
static bool healthDraw(void* self,void* frame,void* result){
 if(startup.mode<3||!vrPlayer||!currentViewport||!vrFrames||menuVisible||levelLoadDepth||objectName(mem<void*>(frame,4))!=L"DrawItem"||objectName(mem<void*>(self,0x24))!=L"StatusItemHealth")return false;
 // Never leak the life display back into the head-attached gameplay HUD.
 if(!healthHeld())return true;
 FpuState fpu;auto renderer=mem<void*>(currentViewport,0x5c);auto device=mem<IDirect3DDevice7*>(renderer,0x9a4);
 RestoreFields diagnostic;bool fixture=startup.mode==5&&profileFlag(L"replay-health.flag");
 unsigned countOffset=field(self,L"nCount",4,L"IntProperty"),potentialOffset=field(self,L"nCurrCountPotential",4,L"IntProperty");
 int units=mem<int>(self,field(self,L"nUnitsPerIcon",4,L"IntProperty"));
 if(fixture){int icons=std::min(6u,controlsFrames/100+1);diagnostic.set(self,potentialOffset,icons*units);diagnostic.set(self,countOffset,(icons-1)*units+units/2);}
 int potential=mem<int>(self,potentialOffset),count=mem<int>(self,countOffset);
 int change=mem<int>(self,field(self,L"nCurrChange",4,L"IntProperty"));
 float fade=mem<float>(self,field(self,L"fCurrFadeTime",4,L"FloatProperty"));
 unsigned viewWidth=bridge.width,viewHeight=bridge.height;
 static int oldCount=-1,oldPotential=-1,oldChange=0;static float oldFade=0;static unsigned oldWidth=0,oldHeight=0;
 if(healthFresh&&count==oldCount&&potential==oldPotential&&change==oldChange&&fade==oldFade&&viewWidth==oldWidth&&viewHeight==oldHeight){healthTime=GetTickCount64();return true;}
 if(!beginCapture(healthCapture,device))return true;
 oldCount=count;oldPotential=potential;oldChange=change;oldFade=fade;oldWidth=viewWidth;oldHeight=viewHeight;
 void* texture=objectField(self,L"textureHudIcon");need(texture!=nullptr,"Original health texture missing");
 auto rect=hpvr::healthRect(potential,units,mem<int>(self,field(self,L"nActualIconW",4,L"IntProperty")),mem<int>(texture,field(texture,L"USize",4,L"IntProperty")),mem<int>(texture,field(texture,L"VSize",4,L"IntProperty")),healthCapture.width,healthCapture.height);
 void* locals=mem<void*>(frame,0x10);need(locals&&mem<WORD>(mem<void*>(frame,4),0x7a)==16,"Health DrawItem signature differs");void* canvas=mem<void*>(locals,0);
 RestoreFields canvasSize;canvasSize.set(canvas,field(canvas,L"SizeX",4,L"IntProperty"),int(viewWidth));canvasSize.set(canvas,field(canvas,L"SizeY",4,L"IntProperty"),int(viewHeight));canvasSize.set(canvas,field(canvas,L"ClipX",4,L"FloatProperty"),float(viewWidth));canvasSize.set(canvas,field(canvas,L"ClipY",4,L"FloatProperty"),float(viewHeight));canvasSize.set(locals,12,float(viewWidth)/640.0f);
 fpu.restore();originalInternal(self,frame,result);fpu.restore();
 hr(device->EndScene(),"Finish hand health");
 DDSURFACEDESC2 d={};d.dwSize=sizeof(d);RECT healthRect={0,0,LONG(rect.width),LONG(rect.height)};hr(healthCapture.target->Lock(&healthRect,&d,DDLOCK_READONLY|DDLOCK_WAIT|DDLOCK_NOSYSLOCK,nullptr),"Read hand health");
 try{
  // Fixed original geometry, not image-dependent bounding boxes. Count/resize
  // changes release the old overlay before reusing the shared D3D11 device.
  if(healthBridge.device&&(healthBridge.width!=rect.width||healthBridge.height!=rect.height))vrHealth.close();
  d.dwWidth=rect.width;d.dwHeight=rect.height;
  healthBridge.open(startup.mode==3?runtime.adapter:0,rect.width,rect.height,true,&bridge);
  bool verify=!(healthBridge.verifiedMask&1);unsigned pixels=healthBridge.uploadArgb(d,verify,healthCapture.sourceAlpha);
  need(pixels>0,"Original hand health rendered empty");
  unsigned bit=1u<<rect.icons;
  if(!(healthProofMask&bit)){
   wchar_t name[64];swprintf(name,64,L"native-health-%u.bmp",rect.icons);auto cpu=d;cpu.lpSurface=healthBridge.pixels.data();cpu.lPitch=LONG(rect.width*4);writeBmp(name,cpu);healthProofMask|=bit;
   log("{\"event\":\"health_texture\",\"icons\":%u,\"count\":%d,\"potential\":%d,\"width\":%u,\"height\":%u,\"visible_pixels\":%u,\"gpu_verified\":true,\"diagnostic\":%s}",rect.icons,count,potential,rect.width,rect.height,pixels,fixture?"true":"false");
  }
 }catch(...){healthCapture.target->Unlock(&healthRect);throw;}
 hr(healthCapture.target->Unlock(&healthRect),"Unlock health");healthBridge.flush();healthCapture.restore(device);hr(device->BeginScene(),"Resume original HUD after health");healthFresh=true;healthTime=GetTickCount64();fpu.restore();return true;
}
static void updateHealthOverlay(){
 bool show=healthHeld()&&healthFresh&&GetTickCount64()-healthTime<200;
 static bool previous=false;if(show!=previous){log("{\"event\":\"health_visibility\",\"visible\":%s,\"held\":%s,\"hand\":\"left\"}",show?"true":"false",healthHeld()?"true":"false");previous=show;}
 if(!show){vrHealth.hide();return;}
 if(startup.mode==3){
  auto plane=hpvr::healthPlane(copyMatrix(vrInput.leftGrip.pose.mDeviceToAbsoluteTracking),copyMatrix(runtime.poses[0].mDeviceToAbsoluteTracking));
  vrHealth.width=.14f*float(healthBridge.width)/healthBridge.height;
  vrHealth.submit(runtime,healthBridge.textures[0],healthBridge.width,healthBridge.height,false,true,&plane);
 }
}
