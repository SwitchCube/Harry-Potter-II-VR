#pragma once
static VrHudCapture vendorCapture;
static VrPanel vendorPanel;
static VrMenu vendorOverlay("vendor-world");
static bool vendorFresh=false,vendorAnchored=false;static unsigned vendorGeneration=~0u;
static Vec vendorRight={},vendorNormal={};static ULONGLONG vendorTextureAt=0;
static bool vendorDraw(void* self,void* frame,void* result){
 if(startup.mode<3||startup.mode>5||!vrPlayer||!frame||!vendorEngaged()||self!=objectField(vrPlayer,L"CurrVendorManager")||objectName(mem<void*>(frame,4))!=L"DrawVendorBar")return false;
 if(!vendorPrompt()||menuVisible||!currentViewport)return true;
 FpuState fpu;auto device=mem<IDirect3DDevice7*>(mem<void*>(currentViewport,0x5c),0x9a4);
 if(!beginCapture(vendorCapture,device))return true;
 void* locals=mem<void*>(frame,0x10);need(locals&&mem<WORD>(mem<void*>(frame,4),0x7a)==4,"Vendor DrawVendorBar signature");void* canvas=mem<void*>(locals,0);
 // Scale 2 preserves the original item, bean price and decorative frame at 732px.
 RestoreFields size;for(auto name:{L"SizeX",L"SizeY"})size.set(canvas,field(canvas,name,4,L"IntProperty"),1280);for(auto name:{L"ClipX",L"ClipY"})size.set(canvas,field(canvas,name,4,L"FloatProperty"),1280.0f);
 fpu.restore();originalInternal(self,frame,result);fpu.restore();hr(device->EndScene(),"Finish vendor bar");
 constexpr unsigned w=732,h=196;RECT region={0,0,w,h};DDSURFACEDESC2 d={};d.dwSize=sizeof(d);hr(vendorCapture.target->Lock(&region,&d,DDLOCK_READONLY|DDLOCK_WAIT|DDLOCK_NOSYSLOCK,nullptr),"Read vendor bar");
 try{vendorPanel.open(w,h);GdiFlush();for(unsigned y=0;y<h;++y)std::memcpy(vendorPanel.data+y*w*4,static_cast<BYTE*>(d.lpSurface)+y*d.lPitch,w*4);}catch(...){vendorCapture.target->Unlock(&region);throw;}
 hr(vendorCapture.target->Unlock(&region),"Unlock vendor bar");vendorCapture.restore(device);hr(device->BeginScene(),"Resume HUD after vendor");
 for(int i=0;i<2;++i){bool selected=vendorYes==(i==0);int y=i?108:40;
  vendorPanel.box(488,y-4,194,58,selected?RGB(255,247,189):RGB(123,108,80));vendorPanel.box(494,y+2,182,46,selected?RGB(255,218,87):RGB(27,34,51));
  vendorPanel.text(506,y-4,165,selected?(i?tr(L">  NEIN",L">  NO"):tr(L">  JA",L">  YES")):(i?tr(L"   Nein",L"   No"):tr(L"   Ja",L"   Yes")),27,selected?RGB(16,19,28):RGB(212,212,215));
 }
 vendorPanel.upload(startup.mode==3?runtime.adapter:0,&bridge);vendorFresh=true;vendorTextureAt=GetTickCount64();
 if(startup.mode==5){static unsigned proof=0;unsigned bit=vendorYes?1:2;if(!(proof&bit)){auto c=d;c.dwWidth=w;c.dwHeight=h;c.lpSurface=vendorPanel.data;c.lPitch=w*4;writeBmp(vendorYes?L"vendor-yes.bmp":L"vendor-no.bmp",c);log(R"({"event":"vendor_panel","yes":%s,"width":%u,"height":%u,"gpu_verified":true,"separate_from_subtitles":true})",vendorYes?"true":"false",w,h);proof|=bit;}}
 fpu.restore();return true;
}
static void updateVendorOverlay(){
 if(!vendorPrompt()||!vendorFresh||menuVisible||cinemaActive||levelLoadDepth||GetTickCount64()-vendorTextureAt>250){vendorOverlay.hide();vendorAnchored=false;return;}
 void* manager=objectField(vrPlayer,L"CurrVendorManager");void* npc=objectField(manager,L"Vendor");if(!npc||boolField(npc,L"bDeleteMe")){vendorOverlay.hide();vendorAnchored=false;return;}
 auto p=mem<Vec>(npc,0x11c);Vec up={0,0,1};
 if(!vendorAnchored||vendorGeneration!=levelGeneration){
  auto facing=hpvr::vendorFacing(hpvr::sub(playerViewAnchor(),p));vendorNormal=facing.normal;vendorRight=facing.right;vendorAnchored=true;vendorGeneration=levelGeneration;
  log(R"({"event":"vendor_anchor","npc":"%ls","head_attached":false,"side":"right","generation":%u})",objectName(npc).c_str(),levelGeneration);
 }
 // Centre above/right of the face, frozen orientation per offer; NPC translation follows.
 p=hpvr::add(p,hpvr::scale(vendorRight,.56f*vrSettings.units));p.z+=mem<float>(npc,field(npc,L"CollisionHeight",4,L"FloatProperty"))+.20f*vrSettings.units;
 auto plane=hpvr::worldOverlay(p,vendorRight,up,vendorNormal,trackingOrigin,playerViewAnchor(),vrSettings.units);
 float determinant=hpvr::dot(hpvr::healthCross(hpvr::axis(plane,0),hpvr::axis(plane,1)),hpvr::axis(plane,2));
 float front=hpvr::dot(hpvr::axis(plane,2),hpvr::sub(trackingOrigin.reference,hpvr::translation(plane)));
 if(startup.mode==3){if(runtime.compositor->CanRenderScene()&&!headBlocked){
  bool wasVisible=vendorOverlay.visible;vendorOverlay.width=.82f;vendorOverlay.submit(runtime,vendorPanel.texture.textures[0],vendorPanel.width,vendorPanel.height,false,false,&plane);
  if(!wasVisible){ETrackingUniverseOrigin space;HmdMatrix34_t actual={};auto error=vendorOverlay.api->GetOverlayTransformAbsolute(vendorOverlay.handle,&space,&actual);float difference=0;for(int i=0;i<3;++i)for(int j=0;j<4;++j)difference=std::max(difference,std::fabs(actual.m[i][j]-plane.m[i][j]));
   log(R"({"event":"vendor_overlay_runtime","visible":%s,"determinant":%.6g,"front_distance":%.6g,"transform_error":%d,"matrix_difference":%.6g})",vendorOverlay.api->IsOverlayVisible(vendorOverlay.handle)?"true":"false",determinant,front,int(error),difference);
  }
 }else vendorOverlay.hide();}
 if(startup.mode==5){static unsigned samples=0;if(++samples%60==1){auto back=trackingOrigin.position(plane,playerViewAnchor(),vrSettings.units);log(R"({"event":"vendor_anchor_sample","world":[%.6g,%.6g,%.6g],"roundtrip_error":%.6g,"base_yaw":%.6g,"determinant":%.6g,"front_distance":%.6g})",p.x,p.y,p.z,std::sqrt(hpvr::dot(hpvr::sub(back,p),hpvr::sub(back,p))),trackingOrigin.baseYaw,determinant,front);}}
}
static void closePresentation(){vendorOverlay.close();vendorPanel.close();vendorCapture.close();vendorFresh=vendorAnchored=false;menuCapture.close();}
