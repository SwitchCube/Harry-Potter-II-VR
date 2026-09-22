#pragma once
static void dispatchUi(void* self,void* function,void* params,void* result){
 try{if(mechanicsDispatch(self,function,params,result))return;}catch(const std::exception& e){log("{\"event\":\"fatal\",\"stage\":\"mechanics_dispatch\",\"reason\":\"%s\"}",e.what());stopOwnProcess(140);}
 if(dispatchLocomotion(self,function,params,result))return;
 try{beginHud(self,function);beginMenu(self,function);}catch(const std::exception& e){log("{\"event\":\"fatal\",\"stage\":\"hud_begin\",\"reason\":\"%s\"}",e.what());stopOwnProcess(137);}
 if(startup.mode<3||!menuVisible||self!=gameConsole||!currentViewport||objectName(function)!=L"PostRender"){
  originalDispatch(self,function,params,result);return;
 }
 FpuState fpu;
 try{
  void* root=objectField(gameConsole,L"Root");
  if(!root){originalDispatch(self,function,params,result);return;}
  float width=mem<float>(root,field(root,L"WinWidth",4,L"FloatProperty"));
  float height=mem<float>(root,field(root,L"WinHeight",4,L"FloatProperty"));
  need(std::isfinite(width)&&std::isfinite(height)&&width>0&&height>0,"Invalid original menu dimensions");
  mem<float>(gameConsole,field(gameConsole,L"MouseX",4,L"FloatProperty"))=vrMenu.cursorX*width;
  mem<float>(gameConsole,field(gameConsole,L"MouseY",4,L"FloatProperty"))=vrMenu.cursorY*height;
  ScopedScreenRelative mouse(currentViewport,true,L"bWindowsMouseAvailable",false);
  fpu.restore();originalDispatch(self,function,params,result);fpu.restore();
 }catch(const std::exception& e){log("{\"event\":\"fatal\",\"stage\":\"menu_pointer\",\"reason\":\"%s\"}",e.what());stopOwnProcess(134);}
}
using Unlock=void(__thiscall*)(void*,int);
static Unlock originalUnlock=nullptr;
static bool lastMenuVisible=false,menuCaptured=false;
static unsigned menuCapturedFrames=0;
static void __fastcall unlockHook(void* renderer,void*,int blit){
 if(levelLoadDepth){originalUnlock(renderer,blit);return;}
 FpuState fpu;LONGLONG uiStarted=VrProfile::now();
 try{
  bool show=gameConsole&&vrPlayer&&boolField(gameConsole,L"bUWindowActive")&&!vendorPrompt();
  if(show!=lastMenuVisible){log("{\"event\":\"game_menu\",\"visible\":%s}",show?"true":"false");lastMenuVisible=show;}
  if(cinemaActive&&!show){auto device=mem<IDirect3DDevice7*>(renderer,0x9a4);renderCinema(device);}
  if(show&&bindingPanelActive){renderBindingPanel();}
  else if(show){
   auto device=mem<IDirect3DDevice7*>(renderer,0x9a4);need(device!=nullptr,"Menu render device missing");
   if(menuCapture.active){
    hr(device->EndScene(),"Finish original menu for capture");auto surface=menuCapture.target;DDSURFACEDESC2 d={};d.dwSize=sizeof(d);
    hr(surface->Lock(nullptr,&d,DDLOCK_READONLY|DDLOCK_WAIT|DDLOCK_NOSYSLOCK,nullptr),"Lock transparent menu");
    try{
     if(menuBridge.device&&(menuBridge.width!=d.dwWidth||menuBridge.height!=d.dwHeight))vrMenu.close();
     bridge.open(startup.mode==3?runtime.adapter:0,d.dwWidth,d.dwHeight);menuBridge.open(startup.mode==3?runtime.adapter:0,d.dwWidth,d.dwHeight,true,&bridge);
     unsigned visible=menuBridge.uploadArgb(d,!(menuBridge.verifiedMask&1u),menuCapture.sourceAlpha);
     if(!menuCaptured&&visible&&nativeCapturesEnabled()){auto cpu=d;cpu.lpSurface=menuBridge.pixels.data();cpu.lPitch=LONG(menuBridge.width*4);writeBmp(L"native-menu.bmp",cpu);menuCaptured=true;
      unsigned bottom=0;for(unsigned y=d.dwHeight*9/10;y<d.dwHeight;++y)for(unsigned x=0;x<d.dwWidth;++x)if(menuBridge.pixels[(y*d.dwWidth+x)*4+3])++bottom;
      log(R"({"event":"menu_alpha","visible_pixels":%u,"total_pixels":%u,"bottom_visible":%u,"alpha_target":true})",visible,d.dwWidth*d.dwHeight,bottom);
     }
    }catch(...){surface->Unlock(nullptr);throw;}
    hr(surface->Unlock(nullptr),"Unlock original menu");menuBridge.flush();menuCapture.restore(device);hr(device->BeginScene(),"Resume original menu frame");
    if(++menuCapturedFrames==1)log(R"({"event":"menu_texture","width":%u,"height":%u,"gpu_verified":true,"alpha":true,"runtime_overlay":%s})",menuBridge.width,menuBridge.height,startup.mode==3?"true":"false");
   }
   if(startup.mode==3&&menuBridge.textures[0]&&runtime.compositor->CanRenderScene())vrMenu.submit(runtime,menuBridge.textures[0],menuBridge.width,menuBridge.height,false,true);
   else if(startup.mode==3)vrMenu.hide();
  }else vrMenu.hide();
  if(menuCapture.active){auto device=mem<IDirect3DDevice7*>(renderer,0x9a4);hr(device->EndScene(),"Finish hidden menu");menuCapture.restore(device);hr(device->BeginScene(),"Restore hidden menu target");}
  renderMenuHint(show);
  if(show||!cinemaActive)cinemaOverlay.hide();
  if(show||(startup.mode==3&&(!runtime.compositor->CanRenderScene()||headBlocked)))vrHud.hide();
  if(hudCapture.active){auto device=mem<IDirect3DDevice7*>(renderer,0x9a4);hr(device->EndScene(),"Finish original HUD");if(!show)captureHud(device);hudCapture.restore(device);hr(device->BeginScene(),"Resume original target");}
  updateVendorOverlay();updateHealthOverlay();updateStatusOverlay();
  LONGLONG uiFinished=VrProfile::now();fpu.restore();originalUnlock(renderer,blit);LONGLONG desktopFinished=VrProfile::now();if(startup.mode==3)runtime.presentHandoff();LONGLONG handoffFinished=VrProfile::now();fpu.restore();
  if(vrFrames>5){static double uiMs=0,desktopMs=0,handoffMs=0;static unsigned count=0;uiMs+=vrProfile.ms(uiFinished-uiStarted);desktopMs+=vrProfile.ms(desktopFinished-uiFinished);handoffMs+=vrProfile.ms(handoffFinished-desktopFinished);
   if(++count==300){log("{\"event\":\"present_profile\",\"frames\":300,\"ui_ms\":%.6g,\"desktop_ms\":%.6g,\"handoff_ms\":%.6g}",uiMs/count,desktopMs/count,handoffMs/count);count=0;uiMs=desktopMs=handoffMs=0;}}

 }catch(const std::exception& e){log("{\"event\":\"fatal\",\"stage\":\"menu_render\",\"reason\":\"%s\"}",e.what());stopOwnProcess(135);}
}
