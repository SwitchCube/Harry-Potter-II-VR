#pragma once
// The original game menu is captured after its own PostRender, while gameplay
// continues to use the two independent world views.
struct VrMenu {
 std::string tag;explicit VrMenu(const char* name="menu"):tag(name){}
 VR_IVROverlay_FnTable* api=nullptr;VROverlayHandle_t handle=0;
 bool visible=false,placed=false,alphaMode=false;hpvr::Mat plane=hpvr::identity();float width=1.35f,height=1.35f,distance=1.35f;
 float cursorX=.5f,cursorY=.5f;
 static void check(EVROverlayError e,const char* what){if(e!=EVROverlayError_VROverlayError_None)throw std::runtime_error(std::string(what)+": "+std::to_string(e));}
 void hide(){if(api&&handle)api->HideOverlay(handle);visible=placed=false;}
 void close(){hide();if(api&&handle){api->ClearOverlayTexture(handle);api->DestroyOverlay(handle);}handle=0;api=nullptr;}
 void submit(VrRuntime& runtime,ID3D11Texture2D* image,unsigned w,unsigned h,bool followHead=false,bool premultiplied=false,const hpvr::Mat* trackedPlane=nullptr){
  if(!api){
   api=static_cast<VR_IVROverlay_FnTable*>(runtime.table(IVROverlay_Version));
   std::string key="hp2vr."+tag+"."+std::to_string(GetCurrentProcessId());char name[]="Harry Potter II - Menue";
   check(api->CreateOverlay(&key[0],name,&handle),"Create game menu overlay");
   check(api->SetOverlayWidthInMeters(handle,width),"Menu width");
  }
  height=width*float(h)/w;
  if(!placed){
   hpvr::Mat head;std::memcpy(&head,&runtime.poses[0].mDeviceToAbsoluteTracking,sizeof(head));
   auto offset=hpvr::identity();offset.m[2][3]=-distance;plane=hpvr::multiply(head,offset);
   HmdMatrix34_t pose;std::memcpy(&pose,&plane,sizeof(pose));
   if(followHead){std::memcpy(&pose,&offset,sizeof(pose));check(api->SetOverlayTransformTrackedDeviceRelative(handle,0,&pose),"HUD placement");}
   else check(api->SetOverlayTransformAbsolute(handle,ETrackingUniverseOrigin_TrackingUniverseStanding,&pose),"Menu placement");placed=true;
   check(api->SetOverlayFlag(handle,VROverlayFlags_IsPremultiplied,premultiplied),"Overlay alpha mode");alphaMode=premultiplied;
  }
  if(alphaMode!=premultiplied){check(api->SetOverlayFlag(handle,VROverlayFlags_IsPremultiplied,premultiplied),"Change menu alpha mode");alphaMode=premultiplied;}
  if(trackedPlane){plane=*trackedPlane;HmdMatrix34_t pose;std::memcpy(&pose,&plane,sizeof(pose));
   check(api->SetOverlayTransformAbsolute(handle,ETrackingUniverseOrigin_TrackingUniverseStanding,&pose),"Hand health placement");
   check(api->SetOverlayWidthInMeters(handle,width),"Hand health width");}
  Texture_t texture={image,ETextureType_TextureType_DirectX,EColorSpace_ColorSpace_Gamma};
  check(api->SetOverlayTexture(handle,&texture),"Original menu texture");
  check(api->ShowOverlay(handle),"Show game menu");visible=true;
 }
 bool point(const hpvr::Mat& hand){
  if(!placed)return false;
  auto origin=hpvr::translation(hand),direction=hpvr::scale(hpvr::axis(hand,2),-1);
  auto normal=hpvr::axis(plane,2),centre=hpvr::translation(plane);
  float d=hpvr::dot(direction,normal);if(!std::isfinite(d)||d>=-.0001f)return false;
  float distance=hpvr::dot(hpvr::sub(centre,origin),normal)/d;if(distance<0||distance>10)return false;
  auto hit=hpvr::sub(hpvr::add(origin,hpvr::scale(direction,distance)),centre);
  float u=.5f+hpvr::dot(hit,hpvr::axis(plane,0))/width,v=.5f-hpvr::dot(hit,hpvr::axis(plane,1))/height;
  if(!std::isfinite(u)||!std::isfinite(v)||u<0||u>1||v<0||v>1)return false;
  cursorX=u;cursorY=v;return true;
 }
 void moveCursor(float x,float y,float delta){cursorX=std::max(0.0f,std::min(1.0f,cursorX+x*delta*.65f));cursorY=std::max(0.0f,std::min(1.0f,cursorY-y*delta*.65f));}
};
