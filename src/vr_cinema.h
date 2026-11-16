#pragma once
static VrBridge cinemaBridge;static VrMenu cinemaOverlay("cinema");static unsigned cinemaFrames=0;
static bool detectCinema(void* actor){
 void* lesson=objectField(actor,L"CurrSpellLesson");
 // Exact UObject::GetStateFrame / execGetStateName fields verified in Core.dll.
 void* stateFrame=lesson?mem<void*>(lesson,0x0c):nullptr;void* state=stateFrame?mem<void*>(stateFrame,0x1c):nullptr;
 bool interactive=state&&objectName(state)==L"PlayGame";
 if(interactive!=lessonWorldActive){lessonWorldActive=interactive;trackingOrigin.ready=false;controlsArmed=false;
  log("{\"event\":\"lesson_presentation\",\"interactive_world\":%s,\"state\":\"%ls\",\"original_timing\":true}",interactive?"true":"false",state?objectName(state).c_str():L"None");}
 void* cam=objectField(actor,L"Cam");void* hud=objectField(actor,L"myHUD");
 bool full=cam&&mem<BYTE>(cam,field(cam,L"CameraMode",1,L"ByteProperty"))==7;
 if(hud){full=full||boolField(hud,L"bCutSceneMode");void* manager=objectField(hud,L"managerCutScene");if(manager)full=full||boolField(manager,L"bBothBordersActive");}
 bool reward=actorState(actor)==L"CelebrateCardSet";
 if(reward&&!cardRewardActive){rewardRainStarted=GetTickCount64();rewardRainCentre=mem<Vec>(actor,0x11c);rewardRainBronze=boolField(actor,L"bCelebrateBronze");log("{\"event\":\"card_reward_vr\",\"bronze\":%s,\"first_person\":true,\"origin_unchanged\":true}",rewardRainBronze?"true":"false");}
 cardRewardActive=reward;
 // CutRelease clears CurrVendorManager before the original camera/borders settle.
 // Keep the trade in first person through that release, then relinquish ownership.
 static unsigned tradeGeneration=~0u;if(tradeGeneration!=levelGeneration){vendorWorldActive=false;tradeGeneration=levelGeneration;}
 if(vendorEngaged())vendorWorldActive=true;else if(!full)vendorWorldActive=false;
 bool trade=vendorWorldActive;bool skip=full&&skippableScene(actor);bool world=full&&!interactive&&!reward&&!trade&&vrPresentation.spatialScenes;
 if(world!=worldSceneActive){worldSceneActive=world;controlsArmed=false;log("{\"event\":\"world_cutscene\",\"active\":%s,\"stereo\":true,\"skippable\":%s,\"original_camera\":true,\"option\":%s}",world?"true":"false",skip?"true":"false",vrPresentation.spatialScenes?"true":"false");}
 if(interactive||world||reward||trade)full=false;
 if(full!=cinemaActive){log("{\"event\":\"cinema_mode\",\"active\":%s,\"original_camera\":true,\"world_fixed_screen\":true}",full?"true":"false");cinemaOverlay.hide();if(!full){trackingOrigin.ready=false;controlsArmed=false;}}
 return full;
}
static void closeCinema(){cinemaOverlay.close();cinemaBridge.close();closeHall();cinemaActive=false;lessonWorldActive=false;worldSceneActive=false;cardRewardActive=false;vendorWorldActive=false;rewardRainStarted=0;}
static void renderCinema(IDirect3DDevice7* device){
 if(startup.mode==3&&!runtime.renderReady)return; // Loading/menu unlocks need not follow a new WaitGetPoses.
 hr(device->EndScene(),"Finish original cinematic frame");IDirectDrawSurface7* surface=nullptr;hr(device->GetRenderTarget(&surface),"Get cinematic target");
 DDSURFACEDESC2 d={};d.dwSize=sizeof(d);hr(surface->Lock(nullptr,&d,DDLOCK_READONLY|DDLOCK_WAIT|DDLOCK_NOSYSLOCK,nullptr),"Read original cinematic frame");
 try{bridge.open(startup.mode==3?runtime.adapter:0,d.dwWidth,d.dwHeight);cinemaBridge.open(startup.mode==3?runtime.adapter:0,d.dwWidth,d.dwHeight,false,&bridge);cinemaBridge.upload(0,d,!(cinemaBridge.verifiedMask&1),nativeCapturesEnabled()&&(cinemaFrames==0||lessonCaptureRequested));
  if(!cinemaFrames&&nativeCapturesEnabled()){auto cpu=d;cpu.lpSurface=cinemaBridge.pixels.data();cpu.lPitch=cinemaBridge.width*4;writeBmp(L"native-cinema.bmp",cpu);log("{\"event\":\"cinema_texture\",\"width\":%lu,\"height\":%lu,\"gpu_verified\":true,\"subtitles_included\":true}",d.dwWidth,d.dwHeight);}
  if(lessonCaptureRequested){auto cpu=d;cpu.lpSurface=cinemaBridge.pixels.data();cpu.lPitch=cinemaBridge.width*4;writeBmp(L"native-lesson.bmp",cpu);lessonCaptureRequested=false;}
  cinemaBridge.releaseCpu();
 }catch(...){surface->Unlock(nullptr);surface->Release();throw;}
 hr(surface->Unlock(nullptr),"Unlock cinema");surface->Release();cinemaBridge.flush();
 if(startup.mode==3&&runtime.compositor->CanRenderScene()){
  auto plane=hallScreenPose(2.4f);cinemaOverlay.width=2.3f;cinemaOverlay.submit(runtime,cinemaBridge.textures[0],d.dwWidth,d.dwHeight,false,false,&plane);
 }else cinemaOverlay.hide();
 ++cinemaFrames;hr(device->BeginScene(),"Resume original cinematic target");
}
