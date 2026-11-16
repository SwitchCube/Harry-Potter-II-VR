#pragma once
// Only explicit private profiles use this fixture. No production preferences/saves.
static void settingsTestSavePanel(const wchar_t* filename){
 paintSettings();GdiFlush();DDSURFACEDESC2 d={};d.dwSize=sizeof(d);d.dwWidth=bindingPanel.width;d.dwHeight=bindingPanel.height;d.lPitch=bindingPanel.width*4;d.lpSurface=bindingPanel.data;d.ddpfPixelFormat.dwFlags=DDPF_RGB;d.ddpfPixelFormat.dwRGBBitCount=32;d.ddpfPixelFormat.dwRBitMask=0xff0000;d.ddpfPixelFormat.dwGBitMask=0xff00;d.ddpfPixelFormat.dwBBitMask=0xff;writeBmp(filename,d);
}
static void settingsTestSaveMenu(const wchar_t* filename){
 need(settingsEntryPaints>0&&menuBridge.verifiedMask&& !menuBridge.pixels.empty(),"Settings entry not rendered to verified menu texture");
 DDSURFACEDESC2 d={};d.dwSize=sizeof(d);d.dwWidth=menuBridge.width;d.dwHeight=menuBridge.height;d.lPitch=menuBridge.width*4;d.lpSurface=menuBridge.pixels.data();d.ddpfPixelFormat.dwFlags=DDPF_RGB;d.ddpfPixelFormat.dwRGBBitCount=32;d.ddpfPixelFormat.dwRBitMask=0xff0000;d.ddpfPixelFormat.dwGBitMask=0xff00;d.ddpfPixelFormat.dwBBitMask=0xff;writeBmp(filename,d);
}
static void settingsTestPointerClick(){
 float x,y,w,h;need(settingsEntryBounds(x,y,w,h),"Settings entry has no visible native bounds");
 auto root=objectField(gameConsole,L"Root");float rw=mem<float>(root,field(root,L"WinWidth",4,L"FloatProperty")),rh=mem<float>(root,field(root,L"WinHeight",4,L"FloatProperty"));
 // Include the lower edge: a text-only label must not leave most of the button unclickable.
 for(float fraction:{.2f,.8f}){BYTE args[512]={};auto fn=vrScriptFunction(root,L"FindWindowUnder");mem<float>(args,vrParameter(fn,L"X",4))=(x+w*.5f)*rw;mem<float>(args,vrParameter(fn,L"Y",4))=(y+h*fraction)*rh;vrScriptCall(root,L"FindWindowUnder",args);need(mem<void*>(args,vrParameter(fn,L"ReturnValue",4))==settingsButton,"Visible VR entry is not the native pointer target");}
 vrMenu.cursorX=x+w*.5f;vrMenu.cursorY=y+h*.8f;BYTE mouse[512]={};auto move=vrScriptFunction(root,L"MoveMouse");mem<float>(mouse,vrParameter(move,L"X",4))=vrMenu.cursorX*rw;mem<float>(mouse,vrParameter(move,L"Y",4))=vrMenu.cursorY*rh;vrScriptCall(root,L"MoveMouse",mouse);
 mem<float>(gameConsole,field(gameConsole,L"MouseX",4,L"FloatProperty"))=vrMenu.cursorX*rw;mem<float>(gameConsole,field(gameConsole,L"MouseY",4,L"FloatProperty"))=vrMenu.cursorY*rh;
 setGameKey(currentInput,currentViewport,1,true);setGameKey(currentInput,currentViewport,1,false);need(bindingPanelActive,"Pointer click did not open native VR settings entry");
 log("{\"event\":\"hp2_settings_pointer_verified\",\"native_hit_test\":true,\"engine_mouse_click\":true}");
}
static void settingsTestTick(){
 if(startup.mode!=5||!profileFlag(L"replay-vr-settings.flag")||!vrPlayer||!gameConsole||vrFrames<20)return;
 static unsigned phase=0;static ULONGLONG at=0;auto now=GetTickCount64();if(now-at<1000)return;
 auto book=objectField(gameConsole,L"menuBook");if(!book)return;
 if(phase==0){vrScriptString(book,L"OpenBook",L"INGAME");}
 if(phase==1){need(menuVisible&&settingsButton&&objectField(book,L"curPage")==settingsOwner,"Native VR entry missing");
  unsigned buttons=0;for(auto child=objectField(settingsOwner,L"FirstChildWindow");child;child=objectField(child,L"NextSiblingWindow"))++buttons;need(buttons>=16,"Original main menu controls missing");
  settingsTestSaveMenu(L"settings-entry-de.bmp");settingsTestPointerClick();settingsTestSavePanel(L"settings-de.bmp");
 }
 if(phase==2){settingsClick(.5f,590.f/960);need(vrEnglish,"Manual English override failed");settingsTestSavePanel(L"settings-en.bmp");
  settingsClick(.5f,470.f/960);need(vrModPreferences.renderSize==1024,"Resolution choice failed");unsigned old=vrSettings.renderSize;VrModPreferences saved;saved.load(startup.profile);need(saved.renderSize==1024&&vrLanguageChoice==2,"Preferences failed round trip");vrSettings.renderSize=old;
  settingsClick(.5f,350.f/960);VrPresentation presentation;presentation.load(startup.profile);need(presentation.spatialScenes==vrPresentation.spatialScenes,"Cinema option persistence");
  settingsClick(.5f,230.f/960);settingsClick(.5f,220.f/960);need(bindingWaiting==0,"Jump remap selection");vrInput.clear();updateBindingPanel(true);vrInput.raw[VrBindings::Cast]=true;updateBindingPanel(true);vrInput.clear();need(vrBindings.source[VrBindings::Jump]==VrBindings::Cast&&vrBindings.source[VrBindings::Cast]==VrBindings::Jump,"Conflict swap not applied");
  settingsClick(.7f,690.f/960);VrBindings stored;stored.load(startup.profile);need(stored.swapSticks&&stored.source[0]==1,"Bindings did not persist");settingsTestSavePanel(L"settings-bindings-en.bmp");
  bindingPanelActive=false;vrScriptString(book,L"ChangePageNamed",L"INPUT");
 }
 if(phase==3){auto page=objectField(book,L"curPage");need(objectName(mem<void*>(page,0x24))==L"FEInputPage"&&!bindingPanelActive,"Original input page replaced");
  mem<int>(page,field(page,L"Selection",4,L"IntProperty"))=0;int key=84;vrScriptCall(page,L"ProcessKey",&key);vrScriptCall(page,L"LoadExistingKeys",nullptr);auto primary=mem<int>(page,field(page,L"BoundKey1",4,L"IntProperty"));need(primary==84||mem<int>(page,field(page,L"BoundKey2",4,L"IntProperty"))==84,"Original keyboard remap failed");
  log("{\"event\":\"hp2_original_keyboard_verified\",\"key\":\"T\",\"original_page\":true}");
 }
 if(phase==4){float x,y,w,h;need(!settingsEntryBounds(x,y,w,h),"VR entry leaked onto original keyboard page");vrScriptString(book,L"ChangePageNamed",L"INGAME");}
 if(phase==5){settingsTestSaveMenu(L"settings-entry-en.bmp");settingsTestPointerClick();bindingPanelActive=false;vrScriptCall(book,L"CloseBook",nullptr);}
 if(phase==6){vrScriptString(book,L"OpenBook",L"INGAME");}
 if(phase==7){unsigned count=0;for(void* child=objectField(settingsOwner,L"FirstChildWindow");child;child=objectField(child,L"NextSiblingWindow"))if(hasField(child,L"HelpText")&&vrString(child,field(child,L"HelpText",12,L"StrProperty"))==L"HP2VR.Settings")++count;need(count==1,"Duplicate settings entry after reopening");settingsTestPointerClick();bindingPanelActive=false;vrScriptString(book,L"ChangePageNamed",L"MAIN");}
 if(phase==8){need(hallActive&&hallFrames>0,"Main menu did not enter independent hall");
  auto screen=hallScreenPose(2.4f);auto world=hallOrigin.position(screen,hallAnchor,vrSettings.units);auto expected=hpvr::add(hallAnchor,hpvr::yaw(Vec{2.4f*vrSettings.units,0,0},hallHeading));need(std::sqrt(hpvr::dot(hpvr::sub(world,expected),hpvr::sub(world,expected)))<.01f,"Screen moved with entry gaze");
  vrScriptCall(book,L"CloseBook",nullptr);log("{\"event\":\"hp2_settings_test_complete\",\"native_button\":true,\"visible_entry\":true,\"pointer_click\":true,\"reopen\":true,\"original_keyboard\":true,\"english\":true,\"persistence\":true,\"hand_binding_swap\":true,\"fixed_hall_screen\":true,\"passed\":true}");
 }
 if(phase<=8){++phase;at=now;}
}
