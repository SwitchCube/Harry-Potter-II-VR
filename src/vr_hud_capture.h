#pragma once
#include <vector>
#include <cstring>
// Prefer an independent ARGB target. Some legacy/hybrid drivers reject the
// offscreen allocation (including with DDERR_OUTOFVIDEOMEMORY), but permit a
// renderable texture. If neither is available, temporarily borrow the existing
// target: the submitted VR eyes are already independent D3D11 copies. Preserve
// every original pixel in CPU memory and restore it before the desktop present.
struct VrHudCapture {
 IDirectDrawSurface7* target=nullptr;IDirectDrawSurface7* saved=nullptr;IDirect3DDevice7* owner=nullptr;
 unsigned width=0,height=0;DWORD oldZ=0;D3DVIEWPORT7 oldViewport={};
 bool active=false,borrowed=false,sourceAlpha=true;ULONGLONG lastCapture=0;
 std::vector<unsigned char> background;
 static bool rgb32(const DDSURFACEDESC2& d){return d.ddpfPixelFormat.dwRGBBitCount==32&&d.ddpfPixelFormat.dwRBitMask==0x00ff0000&&d.ddpfPixelFormat.dwGBitMask==0x0000ff00&&d.ddpfPixelFormat.dwBBitMask==0x000000ff;}
 void close(){
  if(saved)saved->Release();saved=nullptr;if(target)target->Release();target=nullptr;
  if(owner)owner->Release();owner=nullptr;width=height=0;active=borrowed=false;sourceAlpha=true;
  std::vector<unsigned char>().swap(background);lastCapture=0;
 }
 void copyBackground(bool restore){
  if(!restore)background.resize(std::size_t(width)*height*4);
  DDSURFACEDESC2 d={};d.dwSize=sizeof(d);
  hr(target->Lock(nullptr,&d,(restore?DDLOCK_WRITEONLY:DDLOCK_READONLY)|DDLOCK_WAIT|DDLOCK_NOSYSLOCK,nullptr),"Lock borrowed HUD target");
  try{
   need(rgb32(d)&&d.lpSurface&&d.dwWidth==width&&d.dwHeight==height&&d.lPitch>=LONG(width*4),"Borrowed HUD target layout differs");
   for(unsigned y=0;y<height;++y){auto row=static_cast<unsigned char*>(d.lpSurface)+std::size_t(y)*d.lPitch;auto copy=background.data()+std::size_t(y)*width*4;
    if(restore)std::memcpy(row,copy,width*4);else std::memcpy(copy,row,width*4);
   }
  }catch(...){target->Unlock(nullptr);throw;}
  hr(target->Unlock(nullptr),"Unlock borrowed HUD target");
 }
 // failureMode is nonzero only in isolated replay/unit fixtures: 1 rejects the
 // first allocation, 2 rejects all extra GPU targets with the reported HRESULT.
 bool begin(IDirect3DDevice7* device,unsigned failureMode=0){
  if(active||GetTickCount64()-lastCapture<33)return false;
  IDirectDrawSurface7* scene=nullptr;hr(device->GetRenderTarget(&scene),"Get scene before HUD");
  DDSURFACEDESC2 d={};d.dwSize=sizeof(d);HRESULT described=scene->GetSurfaceDesc(&d);
  if(FAILED(described)){scene->Release();hr(described,"HUD dimensions");}
  if(target&&(owner!=device||width!=d.dwWidth||height!=d.dwHeight||(borrowed&&target!=scene)||FAILED(target->IsLost())))close();
  saved=scene;
  hr(device->GetRenderState(D3DRENDERSTATE_ZENABLE,&oldZ),"HUD depth state");
  hr(device->GetViewport(&oldViewport),"HUD viewport state");
  hr(device->EndScene(),"Finish scene before HUD");
  if(!target){
   IDirectDraw7* dd=nullptr;hr(saved->GetDDInterface(reinterpret_cast<void**>(&dd)),"HUD DirectDraw owner");
   DDSCAPS2 caps={};caps.dwCaps=DDSCAPS_VIDEOMEMORY;DWORD total=0,free=0;
   HRESULT budget=dd->GetAvailableVidMem(&caps,&total,&free);DDDEVICEIDENTIFIER2 gpu={};HRESULT identified=dd->GetDeviceIdentifier(&gpu,0);
   log(R"({"event":"hud_device","vendor_id":%lu,"device_id":%lu,"identified":%s,"memory_query_hr":%ld,"reported_total_mb":%lu,"reported_free_mb":%lu})",gpu.dwVendorId,gpu.dwDeviceId,SUCCEEDED(identified)?"true":"false",budget,total/1048576,free/1048576);
   HRESULT made=DDERR_OUTOFVIDEOMEMORY;
   for(unsigned attempt=0;attempt<2;++attempt){
    DDSURFACEDESC2 create={};create.dwSize=sizeof(create);create.dwFlags=DDSD_CAPS|DDSD_WIDTH|DDSD_HEIGHT|DDSD_PIXELFORMAT;
    create.dwWidth=d.dwWidth;create.dwHeight=d.dwHeight;
    create.ddsCaps.dwCaps=(attempt?DDSCAPS_TEXTURE:DDSCAPS_OFFSCREENPLAIN)|DDSCAPS_3DDEVICE|DDSCAPS_VIDEOMEMORY;
    create.ddpfPixelFormat.dwSize=sizeof(DDPIXELFORMAT);create.ddpfPixelFormat.dwFlags=DDPF_RGB|DDPF_ALPHAPIXELS;create.ddpfPixelFormat.dwRGBBitCount=32;
    create.ddpfPixelFormat.dwRBitMask=0x00ff0000;create.ddpfPixelFormat.dwGBitMask=0x0000ff00;create.ddpfPixelFormat.dwBBitMask=0x000000ff;create.ddpfPixelFormat.dwRGBAlphaBitMask=0xff000000;
    made=(failureMode==2||(failureMode==1&&attempt==0))?DDERR_OUTOFVIDEOMEMORY:dd->CreateSurface(&create,&target,nullptr);
    if(SUCCEEDED(made)&&target){DDSURFACEDESC2 actual={};actual.dwSize=sizeof(actual);made=target->GetSurfaceDesc(&actual);
     if(SUCCEEDED(made)&&(!rgb32(actual)||actual.dwWidth!=d.dwWidth||actual.dwHeight!=d.dwHeight||actual.ddpfPixelFormat.dwRGBAlphaBitMask!=0xff000000))made=DDERR_INVALIDPIXELFORMAT;
     if(SUCCEEDED(made))made=device->SetRenderTarget(target,0);
    }
    log(R"({"event":"hud_target_attempt","path":"%s","width":%lu,"height":%lu,"hr":%ld,"simulated_failure":%s})",attempt?"texture-argb":"offscreen-argb",d.dwWidth,d.dwHeight,made,(failureMode==2||(failureMode==1&&attempt==0))?"true":"false");
    if(SUCCEEDED(made)&&target)break;
    if(target){target->Release();target=nullptr;}
   }
   dd->Release();
   if(!target){
    need(rgb32(d),"No compatible 32-bit HUD target");target=saved;target->AddRef();borrowed=true;
    sourceAlpha=(d.ddpfPixelFormat.dwFlags&DDPF_ALPHAPIXELS)&&d.ddpfPixelFormat.dwRGBAlphaBitMask==0xff000000;
    log(R"({"event":"hud_target_fallback","path":"borrowed-scene","width":%lu,"height":%lu,"source_alpha":%s,"last_allocation_hr":%ld})",d.dwWidth,d.dwHeight,sourceAlpha?"true":"false",made);
   }
   owner=device;owner->AddRef();width=d.dwWidth;height=d.dwHeight;
  }
  hr(device->SetRenderTarget(target,0),"Select original HUD target");
  if(borrowed)copyBackground(false);
  hr(device->SetViewport(&oldViewport),"Preserve HUD viewport");
  hr(device->SetRenderState(D3DRENDERSTATE_ZENABLE,FALSE),"Disable HUD depth");hr(device->BeginScene(),"Begin original HUD");
  hr(device->Clear(0,nullptr,D3DCLEAR_TARGET,0,1,0),"Transparent HUD clear");active=true;return true;
 }
 // Caller ended the scene before reading the HUD texture.
 void restore(IDirect3DDevice7* device){
  if(!active)return;if(borrowed)copyBackground(true);
  hr(device->SetRenderTarget(saved,0),"Restore scene after HUD");hr(device->SetViewport(&oldViewport),"Restore viewport after HUD");
  hr(device->SetRenderState(D3DRENDERSTATE_ZENABLE,oldZ),"Restore HUD depth");
  saved->Release();saved=nullptr;active=false;lastCapture=GetTickCount64();
 }
};
