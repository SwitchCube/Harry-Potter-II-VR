#pragma once
// Device-side OpenVR geometry; HP2 projection and tracking origin adapters below.
static Vec tutorialPoint(const hpvr::Mat& matrix,Vec p){return hpvr::add(hpvr::translation(matrix),hpvr::add(hpvr::scale(hpvr::axis(matrix,0),p.x),hpvr::add(hpvr::scale(hpvr::axis(matrix,1),p.y),hpvr::scale(hpvr::axis(matrix,2),p.z))));}
struct Hp2TutorialModels {
 struct Mesh{RenderModel_t* model=nullptr;bool failed=false;};
 struct Part{std::string component,model;};
 struct Device{unsigned index=64;VRInputValueHandle_t path=0;std::string model;std::vector<Part> parts;std::vector<std::string> highlights;std::wstring title,label;};
 VR_IVRRenderModels_FnTable* api=nullptr;std::map<std::string,Mesh> meshes;std::vector<Device> devices;
 VrPanel panels[2];VrMenu labels[2]={VrMenu("tutorial-left"),VrMenu("tutorial-right")};
 unsigned serial=0,reportedSerial=0;ULONGLONG refresh=0;float alpha=0;
 unsigned drawnMask=0;bool fallback=false;
 std::wstring paintedLabels[2];
 static std::wstring wide(const char* value){int n=MultiByteToWideChar(CP_UTF8,0,value,-1,nullptr,0);if(n<=1)return L"";std::vector<wchar_t> buffer(n);MultiByteToWideChar(CP_UTF8,0,value,-1,buffer.data(),n);return buffer.data();}
 void hide(){alpha=0;for(auto& label:labels)label.hide();}
 void close(){hide();for(auto& label:labels)label.close();for(auto& panel:panels)panel.close();if(api)for(auto& item:meshes)if(item.second.model)api->FreeRenderModel(item.second.model);meshes.clear();devices.clear();api=nullptr;refresh=0;serial=0;}
 RenderModel_t* mesh(const std::string& key){
  if(key.empty()||meshes.size()>128)return nullptr;auto& entry=meshes[key];if(entry.failed)return nullptr;
  if(!entry.model){auto error=api->LoadRenderModel_Async(const_cast<char*>(key.c_str()),&entry.model);
   if(error!=EVRRenderModelError_VRRenderModelError_None&&error!=EVRRenderModelError_VRRenderModelError_Loading){entry.failed=true;log("{\"event\":\"hp2_tutorial_model_unavailable\",\"code\":%d}",int(error));}
   if(entry.model&&(!entry.model->rVertexData||!entry.model->rIndexData||entry.model->unVertexCount>65535||entry.model->unTriangleCount>100000)){api->FreeRenderModel(entry.model);entry.model=nullptr;entry.failed=true;}
  }return entry.model;
 }
 void resolve(){
  devices.clear();if(!tutorialLine||!runtime.input||!vrInput.set)return;
  for(const char* key:tutorialLine->actions)if(*key){
   auto handle=vrInput.mappedHandle(key);if(!handle)continue;
   VRInputValueHandle_t origins[16]={};
   if(runtime.input->GetActionOrigins(vrInput.set,handle,origins,16)!=EVRInputError_VRInputError_None)continue;
   for(auto origin:origins)if(origin){
    InputOriginInfo_t info={};if(runtime.input->GetOriginTrackedDeviceInfo(origin,&info,sizeof(info))!=EVRInputError_VRInputError_None||info.trackedDeviceIndex>=64)continue;
    unsigned index=info.trackedDeviceIndex;auto it=std::find_if(devices.begin(),devices.end(),[&](const Device& d){return d.index==index;});
    if(it==devices.end()){
     if(devices.size()>=2)continue;char model[1024]={};ETrackedPropertyError error;
     auto n=runtime.system->GetStringTrackedDeviceProperty(index,ETrackedDeviceProperty_Prop_RenderModelName_String,model,sizeof(model),&error);
     if(error||n<=1||n>=sizeof(model))continue;
     Device d;d.index=index;d.path=info.devicePath;d.model=model;
     unsigned count=std::min(64u,api->GetComponentCount(model));
     for(unsigned i=0;i<count;++i){char component[128]={},part[1024]={};
      auto componentSize=api->GetComponentName(model,i,component,sizeof(component));
      if(componentSize<=1||componentSize>=sizeof(component))continue;
      auto partSize=api->GetComponentRenderModelName(model,component,part,sizeof(part));
      if(partSize>1&&partSize<sizeof(part))d.parts.push_back({component,part});
     }
     if(d.parts.empty())d.parts.push_back({"",model});devices.push_back(d);it=devices.end()-1;
    }
    std::string component=info.rchRenderModelComponentName;
    if(!component.empty()&&std::find(it->highlights.begin(),it->highlights.end(),component)==it->highlights.end())it->highlights.push_back(component);
    char text[256]={};runtime.input->GetOriginLocalizedName(origin,text,sizeof(text),EVRInputStringBits_VRInputString_Hand|EVRInputStringBits_VRInputString_InputSource);
    auto label=wide(text);if(!label.empty()&&it->label.find(label)==std::wstring::npos){if(!it->label.empty())it->label+=L" / ";it->label+=label;}
    std::wstring id=tutorialLine->id;bool flight=id.find(L"Quid")!=std::wstring::npos;bool lesson=*tutorialLine->group;
    const wchar_t* title=!std::strcmp(key,"show_health")?(flight?tr(L"Bremsen",L"Brake"):tr(L"Inventar anzeigen",L"Show inventory")):!std::strcmp(key,"jump")?(flight?tr(L"Greifen / Schubsen",L"Catch / bump"):tr(L"Springen",L"Jump")):!std::strcmp(key,"move")?(flight?tr(L"Besen lenken",L"Steer broom"):lesson?tr(L"Pfeilrichtung eingeben",L"Enter arrow direction"):tr(L"Bewegen",L"Move")):!std::strcmp(key,"turn")?tr(L"Drehen",L"Turn"):!std::strcmp(key,"menu")?tr(L"Men\u00fc \u00f6ffnen",L"Open menu"):!std::strcmp(key,"potion")?tr(L"Trank / Duellzauber",L"Potion / duel spell"):!std::strcmp(key,"boost")?tr(L"Beschleunigen",L"Accelerate"):tr(L"Zielen und zaubern",L"Aim and cast");
    if(!it->title.empty()&&it->title!=title)it->title+=L" / ";if(it->title.find(title)==std::wstring::npos)it->title+=title;
   }
  }
  for(auto& d:devices)for(auto& p:d.parts)mesh(p.model);
  if(reportedSerial!=tutorialSerial&&!devices.empty()){
   reportedSerial=tutorialSerial;log("{\"event\":\"hp2_tutorial_models_resolved\",\"serial\":%u,\"devices\":%u,\"actual_binding_origins\":true}",tutorialSerial,unsigned(devices.size()));
  }
 }
 void update(bool canAct,bool withLabels=true){
  if(vrInput.confirm&&cinemaActive){resetTutorial();hide();return;}
  bool ready=tutorialLine&&runtime.compositor&&runtime.compositor->CanRenderScene()&&!menuVisible;
  alpha=tutorialAlpha(ready,canAct);if(alpha<=0){hide();return;}
  if(!api)api=static_cast<VR_IVRRenderModels_FnTable*>(runtime.table(IVRRenderModels_Version));
  if(serial!=tutorialSerial){serial=tutorialSerial;refresh=0;fallback=false;drawnMask=0;for(auto& p:panels)p.close();}
  if(GetTickCount64()>=refresh){refresh=GetTickCount64()+500;resolve();}
  if(!withLabels)return;
  for(unsigned i=0;i<2;++i){
   if(i>=devices.size()||!runtime.poses[devices[i].index].bPoseIsValid||!runtime.poses[devices[i].index].bDeviceIsConnected){labels[i].hide();continue;}
   auto& d=devices[i];auto caption=d.title+L"\n"+d.label;if(!panels[i].dc||paintedLabels[i]!=caption){panels[i].open(760,120);panels[i].clear();panels[i].text(16,0,728,d.title.c_str(),23);panels[i].text(16,55,728,d.label.c_str(),28,RGB(255,216,81));panels[i].upload(runtime.adapter,&bridge);paintedLabels[i]=caption;}
   hpvr::Mat pose,head;std::memcpy(&pose,&runtime.poses[d.index].mDeviceToAbsoluteTracking,sizeof(pose));std::memcpy(&head,&runtime.poses[0].mDeviceToAbsoluteTracking,sizeof(head));
   auto plane=hpvr::healthPlane(pose,head);plane.m[1][3]+=.06f;labels[i].width=.24f;
   labels[i].submit(runtime,panels[i].texture.textures[0],760,120,false,false,&plane);labels[i].api->SetOverlayAlpha(labels[i].handle,alpha);
  }
 }
 void draw(IDirect3DDevice7* device,void* frame,const hpvr::Mat& trackingToWorld,unsigned eye){
  if(alpha<=0||!api)return;
  struct Vertex{float x,y,z,w;DWORD color;};std::vector<Vertex> vertices;
  Vec origin=mem<Vec>(frame,0x34),right=mem<Vec>(frame,0x40),down=mem<Vec>(frame,0x4c),forward=mem<Vec>(frame,0x58);
  float cx=mem<DWORD>(frame,0xa8)*.5f,cy=mem<DWORD>(frame,0xac)*.5f,f=mem<float>(frame,0xdc);
  bool highlighted=false;
  for(auto& d:devices){
   const auto& pose=runtime.poses[d.index];if(!pose.bPoseIsValid||!pose.bDeviceIsConnected)continue;
   hpvr::Mat tracked;std::memcpy(&tracked,&pose.mDeviceToAbsoluteTracking,sizeof(tracked));
   for(auto& part:d.parts){
    auto model=mesh(part.model);if(!model)continue;hpvr::Mat local=hpvr::identity();
    if(!part.component.empty()){
     RenderModel_ComponentState_t component={};RenderModel_ControllerMode_State_t mode={};
     if(!api->GetComponentStateForDevicePath(const_cast<char*>(d.model.c_str()),const_cast<char*>(part.component.c_str()),d.path,&mode,&component)||!(component.uProperties&EVRComponentProperty_VRComponentProperty_IsVisible))continue;
     std::memcpy(&local,&component.mTrackingToComponentRenderModel,sizeof(local));
    }
    auto transform=hpvr::multiply(trackingToWorld,hpvr::multiply(tracked,local));
    bool mark=std::find(d.highlights.begin(),d.highlights.end(),part.component)!=d.highlights.end();highlighted|=mark;
    float pulse=.82f+.18f*std::sin(float(GetTickCount64()%1500)*2*hpvr::pi/1500);
    DWORD opacity=DWORD(alpha*255)<<24;
    for(unsigned tri=0;tri<model->unTriangleCount;++tri){Vertex v[3];bool valid=true;
     for(unsigned k=0;k<3;++k){unsigned idx=model->rIndexData[tri*3+k];if(idx>=model->unVertexCount){valid=false;break;}
      auto& in=model->rVertexData[idx];Vec localPoint={in.vPosition.v[0],in.vPosition.v[1],in.vPosition.v[2]};
      Vec world=tutorialPoint(transform,localPoint),p=hpvr::sub(world,origin);float z=hpvr::dot(p,forward);if(!std::isfinite(z)||z<=1){valid=false;break;}
      float shade=.60f+.40f*std::fabs(in.vNormal.v[1]);unsigned c=unsigned(195*shade);
      DWORD color=mark?((DWORD(255*pulse)<<16)|(DWORD(212*pulse)<<8)|35):(c<<16)|(c<<8)|(c+20);
      float w=1/z;v[k]={cx+f*hpvr::dot(p,right)*w,cy+f*hpvr::dot(p,down)*w,1-w,w,opacity|color};
     }if(valid)for(auto q:v)vertices.push_back(q);
    }
   }
  }
  if(!highlighted&&!fallback&&GetTickCount64()>tutorialStarted+1500){
   // Drivers without component geometry can still show SteamVR's own action hint.
   for(const char* key:tutorialLine->actions)if(*key)runtime.input->ShowActionOrigins(vrInput.set,vrInput.mappedHandle(key));fallback=true;
   log("{\"event\":\"hp2_tutorial_driver_hint_fallback\",\"component_geometry_missing\":true}");
  }
  if(vertices.empty())return;DWORD saved=0;hr(device->CreateStateBlock(D3DSBT_ALL,&saved),"Save tutorial controller state");
  try{
   hr(device->CaptureStateBlock(saved),"Capture tutorial controller state");device->SetTexture(0,nullptr);
   device->SetTextureStageState(0,D3DTSS_COLOROP,D3DTOP_SELECTARG1);device->SetTextureStageState(0,D3DTSS_COLORARG1,D3DTA_DIFFUSE);
   device->SetTextureStageState(0,D3DTSS_ALPHAOP,D3DTOP_SELECTARG1);device->SetTextureStageState(0,D3DTSS_ALPHAARG1,D3DTA_DIFFUSE);device->SetTextureStageState(1,D3DTSS_COLOROP,D3DTOP_DISABLE);
   for(auto s:{std::pair<D3DRENDERSTATETYPE,DWORD>{D3DRENDERSTATE_ZENABLE,TRUE},{D3DRENDERSTATE_ZWRITEENABLE,TRUE},{D3DRENDERSTATE_ZFUNC,D3DCMP_LESSEQUAL},{D3DRENDERSTATE_ALPHABLENDENABLE,TRUE},{D3DRENDERSTATE_SRCBLEND,D3DBLEND_SRCALPHA},{D3DRENDERSTATE_DESTBLEND,D3DBLEND_INVSRCALPHA},{D3DRENDERSTATE_ALPHATESTENABLE,FALSE},{D3DRENDERSTATE_FOGENABLE,FALSE},{D3DRENDERSTATE_LIGHTING,FALSE},{D3DRENDERSTATE_CULLMODE,D3DCULL_NONE},{D3DRENDERSTATE_STENCILENABLE,FALSE},{D3DRENDERSTATE_FILLMODE,D3DFILL_SOLID}})hr(device->SetRenderState(s.first,s.second),"Tutorial controller state");
   hr(device->DrawPrimitive(D3DPT_TRIANGLELIST,D3DFVF_XYZRHW|D3DFVF_DIFFUSE,vertices.data(),vertices.size(),0),"Draw tracked tutorial controller");
   hr(device->ApplyStateBlock(saved),"Restore tutorial controller state");hr(device->DeleteStateBlock(saved),"Delete tutorial state");saved=0;
   if(!(drawnMask&(1u<<eye))){drawnMask|=1u<<eye;log("{\"event\":\"hp2_tutorial_model_draw\",\"eye\":%u,\"vertices\":%u,\"highlighted\":%s}",eye,unsigned(vertices.size()),highlighted?"true":"false");}
  }catch(...){if(saved){device->ApplyStateBlock(saved);device->DeleteStateBlock(saved);}throw;}
 }
};
static Hp2TutorialModels tutorialModels;
static bool tutorialVisualFailed=false;
static void tutorialVisualFailure(const char* reason){tutorialVisualFailed=true;tutorialModels.close();log("{\"event\":\"hp2_tutorial_visual_disabled\",\"reason\":\"%s\",\"game_continues\":true}",reason);}

static hpvr::Mat tutorialWorldTransform(const hpvr::Origin& origin,Vec anchor){
 auto matrix=hpvr::identity();Vec base=origin.position(hpvr::identity(),anchor,vrSettings.units);matrix.m[0][3]=base.x;matrix.m[1][3]=base.y;matrix.m[2][3]=base.z;
 for(int i=0;i<3;++i){auto p=hpvr::identity();p.m[i][3]=1;auto v=hpvr::sub(origin.position(p,anchor,vrSettings.units),base);matrix.m[0][i]=v.x;matrix.m[1][i]=v.y;matrix.m[2][i]=v.z;}return matrix;
}
static void updateTutorial(){if(startup.mode!=3||tutorialVisualFailed)return;try{tutorialModels.update(!cinemaActive&&!worldSceneActive);}catch(const std::exception& e){tutorialVisualFailure(e.what());}}
static void drawTutorial(IDirect3DDevice7* device,void* frame,unsigned eye){
 if(tutorialVisualFailed)return;try{auto matrix=tutorialWorldTransform(hallRendering?hallOrigin:trackingOrigin,hallRendering?hallAnchor:tutorialViewAnchor);tutorialModels.draw(device,frame,matrix,eye);}catch(const std::exception& e){tutorialVisualFailure(e.what());}
}
static void closeTutorial(){tutorialModels.close();closeTutorialSounds();}
