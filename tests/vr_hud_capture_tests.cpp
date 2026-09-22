#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <initguid.h>
#include <d3d.h>
#include <cstdio>
#include <cstdarg>
#include <stdexcept>
#include <string>
static void need(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
static void hr(HRESULT code,const char* why){if(FAILED(code))throw std::runtime_error(std::string(why)+" "+std::to_string(code));}
static void log(const char* format,...){va_list args;va_start(args,format);vprintf(format,args);va_end(args);puts("");}
#include "vr_hud_capture.h"
#include "vr_pixels.h"
static std::vector<unsigned char> readPixels(IDirectDrawSurface7* surface){
 DDSURFACEDESC2 d={};d.dwSize=sizeof(d);hr(surface->Lock(nullptr,&d,DDLOCK_READONLY|DDLOCK_WAIT,nullptr),"test read");
 std::vector<unsigned char> pixels(d.dwWidth*d.dwHeight*4);for(unsigned y=0;y<d.dwHeight;++y)std::memcpy(pixels.data()+y*d.dwWidth*4,static_cast<unsigned char*>(d.lpSurface)+y*d.lPitch,d.dwWidth*4);
 hr(surface->Unlock(nullptr),"test unlock");return pixels;
}
int main(){try{
 HWND window=CreateWindowExW(0,L"STATIC",L"HP2VR isolated HUD test",WS_OVERLAPPEDWINDOW,0,0,256,256,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);need(window!=nullptr,"test window");
 IDirectDraw7* dd=nullptr;hr(DirectDrawCreateEx(nullptr,reinterpret_cast<void**>(&dd),IID_IDirectDraw7,nullptr),"DirectDraw");hr(dd->SetCooperativeLevel(window,DDSCL_NORMAL),"cooperative level");
 DDSURFACEDESC2 d={};d.dwSize=sizeof(d);d.dwFlags=DDSD_CAPS|DDSD_WIDTH|DDSD_HEIGHT|DDSD_PIXELFORMAT;d.dwWidth=d.dwHeight=128;d.ddsCaps.dwCaps=DDSCAPS_3DDEVICE|DDSCAPS_OFFSCREENPLAIN|DDSCAPS_VIDEOMEMORY;
 d.ddpfPixelFormat.dwSize=sizeof(DDPIXELFORMAT);d.ddpfPixelFormat.dwFlags=DDPF_RGB;d.ddpfPixelFormat.dwRGBBitCount=32;d.ddpfPixelFormat.dwRBitMask=0xff0000;d.ddpfPixelFormat.dwGBitMask=0xff00;d.ddpfPixelFormat.dwBBitMask=0xff;
 IDirectDrawSurface7* scene=nullptr;hr(dd->CreateSurface(&d,&scene,nullptr),"scene target");
 IDirect3D7* d3d=nullptr;hr(dd->QueryInterface(IID_IDirect3D7,reinterpret_cast<void**>(&d3d)),"D3D7");
 IDirect3DDevice7* device=nullptr;hr(d3d->CreateDevice(IID_IDirect3DHALDevice,scene,&device),"D3D7 device");
 D3DVIEWPORT7 viewport={0,0,128,128,0,1};hr(device->SetViewport(&viewport),"viewport");hr(device->SetRenderState(D3DRENDERSTATE_ZENABLE,FALSE),"depth");
 for(unsigned mode=0;mode<3;++mode){
  hr(device->BeginScene(),"test begin");hr(device->Clear(0,nullptr,D3DCLEAR_TARGET,0x00314253,1,0),"test world");hr(device->EndScene(),"test end");auto before=readPixels(scene);hr(device->BeginScene(),"test resume");
  VrHudCapture outer;need(outer.begin(device,mode),"capture did not start");need(outer.borrowed==(mode==2),"unexpected fallback selection");
  D3DRECT rect={16,16,32,32};hr(device->Clear(1,&rect,D3DCLEAR_TARGET,0xff224466,1,0),"UI pixels");hr(device->EndScene(),"UI end");auto ui=readPixels(outer.target);
  std::vector<unsigned char> converted(ui.size());unsigned visible=copyHudBGRA(converted.data(),ui.data(),128,128,128*4,outer.sourceAlpha);
  need(visible==256,"background must be transparent");need(converted[(16*128+16)*4]==0x66,"UI color changed");
  hr(device->BeginScene(),"nested begin");VrHudCapture inner;need(inner.begin(device,2),"nested capture");
  hr(device->Clear(0,nullptr,D3DCLEAR_TARGET,0xff667788,1,0),"nested UI");hr(device->EndScene(),"nested end");inner.restore(device);inner.close();
  need(readPixels(outer.target)==ui,"nested capture changed parent pixels");
  outer.restore(device);need(readPixels(scene)==before,"capture changed original scene pixels");
  IDirectDrawSurface7* actual=nullptr;hr(device->GetRenderTarget(&actual),"restored target");need(actual==scene,"wrong restored target");actual->Release();
  D3DVIEWPORT7 restored={};hr(device->GetViewport(&restored),"restored viewport");need(!std::memcmp(&restored,&viewport,sizeof(viewport)),"viewport changed");
  outer.close();need(!outer.target&&!outer.owner&&!outer.saved&&outer.background.empty(),"capture retained resources");
 }
 device->Release();d3d->Release();scene->Release();dd->Release();DestroyWindow(window);
 puts("HUD targets: normal, texture fallback, forced video-memory exhaustion, nested UI, transparency and exact scene restoration passed");return 0;
}catch(const std::exception& e){fprintf(stderr,"FAIL: %s\n",e.what());return 1;}}
