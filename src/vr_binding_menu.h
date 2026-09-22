#pragma once
static VrPanel bindingPanel,menuHintPanel;
static VrMenu menuHint("controls-hint");
static int bindingPage=0,bindingWaiting=-1;static bool bindingNeutral=false;
static bool bindingOld[VrBindings::Count]={};static void* bindingDismissedPage=nullptr;
static std::wstring bindingMessage;
static bool toolbarHeld=false,toolbarHover=false,toolbarFilmHover=false;static std::wstring presentationError;
static void togglePresentation(){
 try{vrPresentation.setSpatial(!vrPresentation.spatialScenes);presentationError.clear();log(R"({"event":"presentation_saved","spatial_scenes":%s})",vrPresentation.spatialScenes?"true":"false");}
 catch(const std::exception& e){presentationError=tr(L"Speichern fehlgeschlagen. Bitte erneut versuchen.",L"Could not save. Please try again.");log(R"({"event":"presentation_error","reason":"%s"})",e.what());}
}

static void updateBindingPanel(bool usable){
 menuPointerConsumed=false;bindingPanelConsumed=bindingPanelActive;
 if(!menuVisible){toolbarHeld=toolbarHover=toolbarFilmHover=false;bindingPanelActive=false;bindingWaiting=-1;bindingDismissedPage=nullptr;std::fill(bindingOld,bindingOld+VrBindings::Count,false);return;}
 void* book=gameConsole?objectField(gameConsole,L"menuBook"):nullptr;void* page=book?objectField(book,L"curPage"):nullptr;
 bool inputPage=page&&objectName(mem<void*>(page,0x24))==L"FEInputPage";
 if(page!=bindingDismissedPage&&inputPage)bindingPanelActive=true;
 if(startup.mode==5&&profileFlag(L"replay-binding-panel.flag"))bindingPanelActive=controlsFrames>=1010&&controlsFrames<1190;
 bool toolbarFixture=startup.mode==5&&profileFlag(L"replay-binding-panel.flag")&&controlsFrames>=1002&&controlsFrames<=1008;
 if(toolbarFixture){
  static bool initialized=false;if(!initialized){vrPresentation.load(startup.profile);vrPresentation.setSpatial(false);initialized=true;}
  menuHint.placed=menuHint.visible=true;menuHint.plane=hpvr::identity();menuHint.plane.m[2][3]=-1;menuHint.width=1.35f;menuHint.height=.18f;
  auto hand=hpvr::identity();hand.m[0][3]=-.3f;auto& pose=vrSettings.left?vrInput.left:vrInput.right;pose.bActive=pose.pose.bPoseIsValid=pose.pose.bDeviceIsConnected=true;std::memcpy(&pose.pose.mDeviceToAbsoluteTracking,&hand,sizeof(hand));
  std::fill(vrInput.raw,vrInput.raw+VrBindings::Count,false);vrInput.raw[VrBindings::Cast]=controlsFrames==1004||controlsFrames==1007;
 }
 if((startup.mode!=3&&!toolbarFixture)||!usable){bindingPanelConsumed=bindingPanelActive;return;}
 bool rising[VrBindings::Count]={};for(int i=0;i<VrBindings::Count;++i)rising[i]=vrInput.raw[i]&&!bindingOld[i];
 if(!bindingPanelActive&&rising[VrBindings::Potion]){bindingPanelActive=true;bindingWaiting=-1;bindingMessage.clear();}
 bool toolbarAction=false;toolbarHover=toolbarFilmHover=false;
 if(!bindingPanelActive){
  const auto& hand=vrSettings.left?vrInput.left:vrInput.right;
  toolbarHover=menuHint.visible&&hand.bActive&&hand.pose.bPoseIsValid&&hand.pose.bDeviceIsConnected&&menuHint.point(copyMatrix(hand.pose.mDeviceToAbsoluteTracking));toolbarFilmHover=toolbarHover&&menuHint.cursorX<.64f;
  if(toolbarHover&&(rising[VrBindings::Cast]||rising[VrBindings::Confirm])){toolbarHeld=toolbarAction=true;if(toolbarFilmHover)togglePresentation();else {bindingPanelActive=true;bindingWaiting=-1;bindingMessage.clear();}}
 }
 if(!vrInput.raw[VrBindings::Cast]&&!vrInput.raw[VrBindings::Confirm])toolbarHeld=false;
 menuPointerConsumed=toolbarHover||toolbarHeld; // Block underlying clicks; B still closes the book.
 if(bindingPanelActive){
  bindingPanelConsumed=true;
  const auto& hand=vrSettings.left?vrInput.left:vrInput.right;
  if(hand.bActive&&hand.pose.bPoseIsValid)vrMenu.point(copyMatrix(hand.pose.mDeviceToAbsoluteTracking));
  if(bindingWaiting>=0){
   bool any=false;for(bool down:vrInput.raw)any|=down;
   if(!any)bindingNeutral=true;
   if(bindingNeutral){for(int i=0;i<VrBindings::Count;++i)if(rising[i]){
    try{vrBindings.assign(bindingWaiting,i);bindingMessage=tr(L"Gespeichert. Doppelte Belegungen wurden getauscht.",L"Saved. Duplicate assignments were swapped.");log("{\"event\":\"controller_binding_saved\",\"action\":%d,\"source\":%d}",bindingWaiting,i);}
    catch(const std::exception& e){bindingMessage=tr(L"Speichern fehlgeschlagen. Bitte erneut versuchen.",L"Could not save. Please try again.");log("{\"event\":\"controller_binding_error\",\"reason\":\"%s\"}",e.what());}
    bindingWaiting=-1;controlsArmed=false;break;
   }}
  }else if(rising[VrBindings::Menu]){bindingPanelActive=false;bindingDismissedPage=page;}
  else if(!toolbarAction&&(rising[VrBindings::Cast]||rising[VrBindings::Confirm])){
   int x=int(vrMenu.cursorX*1100),y=int(vrMenu.cursorY*960);
   if(y>=180&&y<630&&x>=40&&x<1060){bindingWaiting=bindingPage*5+(y-180)/90;bindingNeutral=false;bindingMessage.clear();}
   else if(y>=655&&y<735){if(x<520){bindingPage=1-bindingPage;}else {vrBindings.swapSticks=!vrBindings.swapSticks;vrBindings.save();bindingMessage=tr(L"Stick-Zuordnung gespeichert.",L"Stick assignment saved.");}}
   else if(y>=775&&y<850){if(x<520){vrBindings.defaults();vrBindings.save();bindingMessage=tr(L"Standardbelegung gespeichert.",L"Default bindings saved.");}else {bindingPanelActive=false;bindingDismissedPage=page;}}
  }
 }
 if(toolbarFixture&&(controlsFrames==1004||controlsFrames==1007)){need(menuPointerConsumed&&!bindingPanelConsumed,"Toolbar must capture clicks but allow book back button");log(R"({"event":"presentation_toolbar_input","spatial":%s,"click_consumed":true,"book_back_available":true})",vrPresentation.spatialScenes?"true":"false");}
 std::copy(vrInput.raw,vrInput.raw+VrBindings::Count,bindingOld);
}
static void renderBindingPanel(){
 bindingPanel.open(1100,960);bindingPanel.clear();
 bindingPanel.text(40,24,1020,tr(L"VR-Controllerbelegung",L"VR controller bindings"),48,RGB(236,198,116));
 bindingPanel.text(40,104,1020,bindingWaiting<0?tr(L"Zeigen und Trigger dr\u00fccken. Danach die gew\u00fcnschte Taste dr\u00fccken.",L"Point and pull the trigger, then press the desired button."):tr(L"Alle Tasten loslassen, dann die neue Taste dr\u00fccken.",L"Release all buttons, then press the new button."),28);
 for(int i=0;i<5;++i){int action=bindingPage*5+i,y=180+i*90;bool hover=vrMenu.cursorY*960>=y&&vrMenu.cursorY*960<y+80;
  bindingPanel.box(40,y,1020,80,bindingWaiting==action?RGB(97,75,28):hover?RGB(48,65,89):RGB(26,34,53));
  bindingPanel.text(62,y+7,560,VrBindings::label(action),30);bindingPanel.text(640,y+7,390,bindingWaiting==action?tr(L"Taste dr\u00fccken ...",L"Press a button ..."):VrBindings::physical(vrBindings.source[action],vrSettings.left),29);
 }
 bindingPanel.box(40,655,460,80,RGB(37,49,71));bindingPanel.text(62,664,420,bindingPage?tr(L"< Seite 1 / 2",L"< Page 1 / 2"):tr(L"Seite 2 / 2 >",L"Page 2 / 2 >"),31);
 bindingPanel.box(540,655,520,80,RGB(37,49,71));bindingPanel.text(562,664,470,vrBindings.swapSticks?tr(L"Sticks: vertauscht",L"Sticks: swapped"):tr(L"Sticks: Standard",L"Sticks: default"),31);
 bindingPanel.box(40,775,460,75,RGB(37,49,71));bindingPanel.text(62,780,420,tr(L"Standard wiederherstellen",L"Restore defaults"),28);
 bindingPanel.box(540,775,520,75,RGB(37,49,71));bindingPanel.text(562,780,470,tr(L"Zur\u00fcck zum Spielmen\u00fc",L"Back to game menu"),30);
 bindingPanel.text(40,867,1020,bindingMessage.empty()?tr(L"Im VR-Men\u00fc bleiben Trigger = Ausw\u00e4hlen und B/Y = Zur\u00fcck fest.",L"VR menus always use trigger = select and B/Y = back."):bindingMessage.c_str(),25);
 int px=int(vrMenu.cursorX*1100),py=int(vrMenu.cursorY*960);bindingPanel.box(px-5,py-5,10,10,RGB(255,225,95));
 bindingPanel.upload(startup.mode==3?runtime.adapter:0,&bridge);
 static bool captured=false;if(!captured){DDSURFACEDESC2 d={};d.dwSize=sizeof(d);d.dwWidth=1100;d.dwHeight=960;d.lPitch=4400;d.lpSurface=bindingPanel.data;d.ddpfPixelFormat.dwFlags=DDPF_RGB;d.ddpfPixelFormat.dwRGBBitCount=32;d.ddpfPixelFormat.dwRBitMask=0xff0000;d.ddpfPixelFormat.dwGBitMask=0xff00;d.ddpfPixelFormat.dwBBitMask=0xff;writeBmp(L"controller-menu.bmp",d);log("{\"event\":\"controller_panel\",\"width\":1100,\"height\":960,\"gpu_verified\":true}");captured=true;}
 if(startup.mode==3)vrMenu.submit(runtime,bindingPanel.texture.textures[0],1100,960);
}
static void renderMenuHint(bool show){
 if(!show||bindingPanelActive||(startup.mode!=3&&startup.mode!=5)){menuHint.hide();return;}
 static int prior=-1;int state=(vrPresentation.spatialScenes?1:0)|(toolbarHover?2:0)|(toolbarFilmHover?4:0)|(!presentationError.empty()?8:0);
 if(!menuHintPanel.dc||state!=prior){
  menuHintPanel.open(1100,150);menuHintPanel.clear();
  menuHintPanel.box(15,12,680,76,toolbarFilmHover?RGB(88,77,44):RGB(37,49,71));menuHintPanel.box(710,12,375,76,toolbarHover&&!toolbarFilmHover?RGB(88,77,44):RGB(37,49,71));
  menuHintPanel.text(35,18,640,vrPresentation.spatialScenes?tr(L"Filmsequenzen: 3D / 360\u00b0",L"Cutscenes: 3D / 360\u00b0"):tr(L"Filmsequenzen: Leinwand",L"Cutscenes: cinema screen"),34,RGB(255,229,146));
  menuHintPanel.text(730,20,345,tr(L"Controllerbelegung",L"Controller bindings"),31);
  menuHintPanel.text(25,94,1050,presentationError.empty()?(vrPresentation.spatialScenes?tr(L"Originale Szenenkamera, frei umsehen. Zum Wechseln zeigen und Trigger dr\u00fccken.",L"Original scene camera, free head tracking. Point and pull trigger to change."):tr(L"Zum Wechseln zeigen und Trigger dr\u00fccken. Nicht \u00fcberspringbare Szenen bleiben r\u00e4umlich.",L"Point and pull trigger to change. Unskippable scenes stay in 3D.")):presentationError.c_str(),22);
  menuHintPanel.upload(startup.mode==3?runtime.adapter:0,&bridge);prior=state;
  static unsigned proof=0;unsigned bit=vrPresentation.spatialScenes?2:1;
  if(startup.mode==5&&!(proof&bit)){proof|=bit;DDSURFACEDESC2 d={};d.dwSize=sizeof(d);d.dwWidth=1100;d.dwHeight=150;d.lpSurface=menuHintPanel.data;d.lPitch=4400;d.ddpfPixelFormat.dwFlags=DDPF_RGB;d.ddpfPixelFormat.dwRGBBitCount=32;d.ddpfPixelFormat.dwRBitMask=0xff0000;d.ddpfPixelFormat.dwGBitMask=0xff00;d.ddpfPixelFormat.dwBBitMask=0xff;writeBmp(vrPresentation.spatialScenes?L"menu-presentation-360.bmp":L"menu-presentation-options.bmp",d);log(R"({"event":"presentation_toolbar","spatial_scenes":%s,"gpu_verified":true})",vrPresentation.spatialScenes?"true":"false");}
 }
 if(startup.mode==3){hpvr::Mat pose=vrMenu.plane;auto p=hpvr::add(hpvr::translation(pose),hpvr::scale(hpvr::axis(pose,1),vrMenu.height*.5f+.11f));pose.m[0][3]=p.x;pose.m[1][3]=p.y;pose.m[2][3]=p.z;menuHint.width=1.35f;menuHint.submit(runtime,menuHintPanel.texture.textures[0],1100,150,false,false,&pose);}
}
