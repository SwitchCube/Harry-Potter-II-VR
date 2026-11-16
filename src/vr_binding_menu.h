#pragma once
#include "vr_script.h"
static VrPanel bindingPanel,menuHintPanel;static VrMenu menuHint("controls-hint");
static int bindingPage=0,bindingWaiting=-1,settingsPage=0;static bool bindingNeutral=false;
static bool bindingOld[VrBindings::Count]={};static std::wstring bindingMessage;
static void* settingsButton=nullptr;static void* settingsOwner=nullptr;static unsigned settingsLanguage=~0u;
static VrPanel settingsEntry;static unsigned settingsEntryPaints=0;
static void settingsNativeButton(){
 if(!menuVisible||!gameConsole||levelLoadDepth||startup.mode<3)return;
 auto book=objectField(gameConsole,L"menuBook");auto page=book?objectField(book,L"curPage"):nullptr;
 if(!page||objectName(mem<void*>(page,0x24))!=L"FEInGamePage")return;
 if(settingsOwner!=page){settingsOwner=page;settingsButton=nullptr;
  for(void* child=objectField(page,L"FirstChildWindow");child;child=objectField(child,L"NextSiblingWindow"))if(hasField(child,L"HelpText")&&vrString(child,field(child,L"HelpText",12,L"StrProperty"))==L"HP2VR.Settings"){settingsButton=child;break;}
  // Use a regular icon-button class with an explicit, full-height hit area.
  void* reference=objectField(page,L"InputButton");need(reference,"HP2 native button class missing");
  if(!settingsButton){auto fn=vrScriptFunction(page,L"CreateControl");BYTE args[512]={};mem<void*>(args,vrParameter(fn,L"ControlClass",4))=mem<void*>(reference,0x24);
   mem<float>(args,vrParameter(fn,L"X",4))=180;mem<float>(args,vrParameter(fn,L"Y",4))=338;mem<float>(args,vrParameter(fn,L"W",4))=280;mem<float>(args,vrParameter(fn,L"H",4))=46;
   vrScriptCall(page,L"CreateControl",args);settingsButton=mem<void*>(args,vrParameter(fn,L"ReturnValue",4));need(settingsButton,"Create HP2 VR settings button");vrAssignString(static_cast<BYTE*>(settingsButton)+field(settingsButton,L"HelpText",12,L"StrProperty"),L"HP2VR.Settings");
  }
  mem<int>(settingsButton,field(settingsButton,L"Font",4,L"IntProperty"))=mem<int>(reference,field(reference,L"Font",4,L"IntProperty"));mem<BYTE>(settingsButton,field(settingsButton,L"Align",1,L"ByteProperty"))=2;mem<DWORD>(settingsButton,field(settingsButton,L"TextColor",4,L"StructProperty"))=0xff86ddff;settingsLanguage=~0u;
  log("{\"event\":\"hp2_settings_button\",\"native_page\":\"FEInGamePage\",\"original_controls_preserved\":true}");
 }
 if(settingsButton&&settingsLanguage!=vrLocaleRevision){vrScriptString(settingsButton,L"SetText",tr(L"VR-Einstellungen",L"VR Settings"));vrAssignString(static_cast<BYTE*>(settingsButton)+field(settingsButton,L"ToolTipString",12,L"StrProperty"),tr(L"VR-Einstellungen: Controller, Zwischensequenzen, Aufl\u00f6sung und Sprache",L"VR Settings: controllers, cinematics, resolution and language"));settingsLanguage=vrLocaleRevision;}
}
// Rasterize the one native menu entry at the actual capture resolution. Its
// original UWindowButton continues to own hit testing, focus and click events.
static bool settingsEntryBounds(float& x,float& y,float& w,float& h){
 if(!settingsButton||!settingsOwner||!gameConsole||bindingPanelActive||!menuVisible)return false;
 auto book=objectField(gameConsole,L"menuBook");auto root=objectField(gameConsole,L"Root");
 if(!book||!root||objectField(book,L"curPage")!=settingsOwner||!boolField(settingsButton,L"bWindowVisible"))return false;
 x=y=0;void* window=settingsButton;unsigned depth=0;
 for(;window&&window!=root&&depth<32;++depth){x+=mem<float>(window,field(window,L"WinLeft",4,L"FloatProperty"));y+=mem<float>(window,field(window,L"WinTop",4,L"FloatProperty"));window=objectField(window,L"ParentWindow");}
 if(window!=root)return false;
 float rw=mem<float>(root,field(root,L"WinWidth",4,L"FloatProperty")),rh=mem<float>(root,field(root,L"WinHeight",4,L"FloatProperty"));
 if(!(rw>0&&rh>0))return false;
 w=mem<float>(settingsButton,field(settingsButton,L"WinWidth",4,L"FloatProperty"))/rw;h=mem<float>(settingsButton,field(settingsButton,L"WinHeight",4,L"FloatProperty"))/rh;x/=rw;y/=rh;
 return std::isfinite(x)&&std::isfinite(y)&&w>0&&h>0&&x>=0&&y>=0&&x+w<=1.001f&&y+h<=1.001f;
}
static void paintSettingsEntry(BYTE* pixels,unsigned width,unsigned height){
 float u,v,uw,vh;if(!settingsEntryBounds(u,v,uw,vh))return;
 int x=int(std::round(u*width)),y=int(std::round(v*height)),w=int(std::round(uw*width)),h=int(std::round(vh*height));
 if(x<0||y<0||w<16||h<16||x+w>int(width)||y+h>int(height))return;
 bool hover=vrMenu.cursorX>=u&&vrMenu.cursorX<u+uw&&vrMenu.cursorY>=v&&vrMenu.cursorY<v+vh;
 auto& p=settingsEntry;p.open(w,h);int border=std::max(2,h/18);
 p.box(0,0,w,h,hover?RGB(255,227,143):RGB(209,170,83));p.box(border,border,w-2*border,h-2*border,hover?RGB(62,65,97):RGB(21,29,54));
 int fontSize=std::max(14,h*44/100);HFONT font=CreateFontW(-fontSize,0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,DEFAULT_PITCH,L"Segoe UI");
 auto old=SelectObject(p.dc,font);SetTextColor(p.dc,RGB(255,235,184));RECT rect={border*3,0,w-border*3,h};DrawTextW(p.dc,tr(L"VR-Einstellungen",L"VR Settings"),-1,&rect,DT_SINGLELINE|DT_CENTER|DT_VCENTER|DT_NOPREFIX);SelectObject(p.dc,old);DeleteObject(font);
 if(hover){int cx=int((vrMenu.cursorX-u)*width),cy=int((vrMenu.cursorY-v)*height),arm=std::max(4,h/10);p.box(cx-arm,cy-1,arm*2,3,RGB(255,255,255));p.box(cx-1,cy-arm,3,arm*2,RGB(255,255,255));}
 GdiFlush();for(unsigned i=0;i<p.width*p.height;++i)p.data[i*4+3]=255;
 for(int row=0;row<h;++row)std::memcpy(pixels+((y+row)*width+x)*4,p.data+row*w*4,w*4);
 if(++settingsEntryPaints==1)log("{\"event\":\"hp2_settings_entry_visible\",\"rect\":[%d,%d,%d,%d],\"font_pixels\":%d,\"native_hit_target\":true}",x,y,w,h,fontSize);
}
static void settingsClick(float u,float v){
 if(!bindingPanelActive||u<.035f||u>.97f)return;int x=int(u*1100),y=int(v*960);
 try{
  if(bindingWaiting>=0){if(y>=775&&y<850){bindingWaiting=-1;bindingNeutral=false;}return;}
  if(settingsPage==0){
   if(y>=180&&y<280)settingsPage=1;
   else if(y>=300&&y<400)vrPresentation.setSpatial(!vrPresentation.spatialScenes);
   else if(y>=420&&y<520){const unsigned sizes[]={1024,1280,1536,2048};unsigned i=0;while(i<3&&sizes[i]!=vrModPreferences.renderSize)++i;vrModPreferences.renderSize=sizes[(i+1)%4];vrModPreferences.save();}
   else if(y>=540&&y<640){vrLanguageChoice=(vrLanguageChoice+1)%3;vrModPreferences.save();}
   else if(y>=775&&y<850)bindingPanelActive=false;
  }else{
   if(y>=180&&y<630){bindingWaiting=bindingPage*5+(y-180)/90;bindingNeutral=false;bindingMessage.clear();}
   else if(y>=655&&y<735){if(x<520)bindingPage=1-bindingPage;else{vrBindings.swapSticks=!vrBindings.swapSticks;vrBindings.save();}}
   else if(y>=775&&y<850){if(x<520){vrBindings.defaults();vrBindings.save();}else settingsPage=0;}
  }
 }catch(const std::exception& e){bindingMessage=tr(L"Speichern fehlgeschlagen. Bitte erneut versuchen.",L"Could not save. Please try again.");log("{\"event\":\"settings_save_error\",\"reason\":\"%s\"}",e.what());}
}
static bool settingsInternal(void* self,void* frame,void* result){
 if(!frame||startup.mode<3||levelLoadDepth)return false;auto fn=objectName(mem<void*>(frame,4));auto args=mem<void*>(frame,0x10);
 if(settingsButton&&self==settingsOwner&&fn==L"Notify"&&args&&mem<void*>(args,0)==settingsButton&&mem<BYTE>(args,4)==2){bindingPanelActive=bindingPanelConsumed=true;settingsPage=0;bindingWaiting=-1;bindingMessage.clear();log("{\"event\":\"hp2_settings_open\"}");return true;}
 if(bindingPanelActive&&gameConsole&&self==objectField(gameConsole,L"menuBook")&&fn==L"KeyEvent"&&args){BYTE key=mem<BYTE>(args,0),action=mem<BYTE>(args,1);
  if(action==1&&key==27){if(bindingWaiting>=0)bindingWaiting=-1;else if(settingsPage)settingsPage=0;else bindingPanelActive=false;}
  if(action==1&&key==1){auto root=objectField(gameConsole,L"Root");settingsClick(mem<float>(gameConsole,field(gameConsole,L"MouseX",4,L"FloatProperty"))/mem<float>(root,field(root,L"WinWidth",4,L"FloatProperty")),mem<float>(gameConsole,field(gameConsole,L"MouseY",4,L"FloatProperty"))/mem<float>(root,field(root,L"WinHeight",4,L"FloatProperty")));}
  if(result)*static_cast<DWORD*>(result)=1;return true;
 }return false;
}
static void updateBindingPanel(bool usable){
 menuPointerConsumed=false;bindingPanelConsumed=bindingPanelActive;
 if(!menuVisible){bindingPanelActive=false;bindingWaiting=-1;std::fill(bindingOld,bindingOld+VrBindings::Count,false);return;}
 settingsNativeButton();
 if(!usable)return;bool rising[VrBindings::Count]={};for(int i=0;i<VrBindings::Count;++i)rising[i]=vrInput.raw[i]&&!bindingOld[i];
 if(bindingPanelActive){
  bindingPanelConsumed=true;const auto& hand=vrSettings.left?vrInput.left:vrInput.right;
  if(hand.bActive&&hand.pose.bPoseIsValid&&hand.pose.bDeviceIsConnected)vrMenu.point(copyMatrix(hand.pose.mDeviceToAbsoluteTracking));
  if(bindingWaiting>=0){bool any=false;for(bool down:vrInput.raw)any|=down;if(!any)bindingNeutral=true;
   if(bindingNeutral)for(int i=0;i<VrBindings::Count;++i)if(rising[i]){try{vrBindings.assign(bindingWaiting,i);bindingMessage=tr(L"Gespeichert. Doppelte Belegungen wurden getauscht.",L"Saved. Duplicate assignments were swapped.");}catch(const std::exception& e){bindingMessage=tr(L"Speichern fehlgeschlagen.",L"Could not save.");log("{\"event\":\"binding_error\",\"reason\":\"%s\"}",e.what());}bindingWaiting=-1;controlsArmed=false;break;}
  }else if(rising[VrBindings::Menu]){if(settingsPage)settingsPage=0;else bindingPanelActive=false;}
  else if(rising[VrBindings::Cast]||rising[VrBindings::Confirm])settingsClick(vrMenu.cursorX,vrMenu.cursorY);
 }
 std::copy(vrInput.raw,vrInput.raw+VrBindings::Count,bindingOld);
}
static std::wstring bindingSource(int source){
 const char* names[]={"jump","cast","menu","recenter","confirm","potion","map","boost","quicksave","show_health"};
 if(runtime.input&&vrInput.set){VRInputValueHandle_t origins[16]={};auto found=vrInput.actions.find(names[source]);
  if(found!=vrInput.actions.end()&&runtime.input->GetActionOrigins(vrInput.set,found->second,origins,16)==EVRInputError_VRInputError_None)for(auto origin:origins)if(origin){char label[256]={};if(runtime.input->GetOriginLocalizedName(origin,label,256,7)==EVRInputError_VRInputError_None){wchar_t text[256]={};MultiByteToWideChar(CP_UTF8,0,label,-1,text,256);return text;}}
 }return VrBindings::physical(source,vrSettings.left);
}
static void paintSettings(){
 auto& p=bindingPanel;p.open(1100,960);p.clear();p.text(40,24,1020,tr(L"VR-Einstellungen",L"VR Settings"),48,RGB(236,198,116));
 auto row=[&](int y,int h,const wchar_t* title,const std::wstring& value){bool hover=vrMenu.cursorY*960>=y&&vrMenu.cursorY*960<y+h;p.box(40,y,1020,h,hover?RGB(64,65,77):RGB(26,34,53));p.text(62,y-3,980,title,30);if(!value.empty())p.text(62,y+39,980,value.c_str(),26,RGB(247,213,142));};
 if(settingsPage==0){
  row(180,100,tr(L"Controller-Tastenbelegung",L"Controller bindings"),tr(L"Aktion ausw\u00e4hlen, dann die neue Taste dr\u00fccken",L"Choose an action, then press its new button"));
  row(300,100,tr(L"Zwischensequenzen",L"Cinematics"),vrPresentation.spatialScenes?L"3D VR":tr(L"Kino / Flat",L"Cinema / Flat"));
  row(420,100,tr(L"VR-Aufl\u00f6sung (ab n\u00e4chstem Spielstart)",L"VR resolution (next game launch)"),std::to_wstring(vrModPreferences.renderSize)+L" x "+std::to_wstring(vrModPreferences.renderSize));
  row(540,100,tr(L"Sprache der VR-Mod",L"VR mod language"),vrLanguageChoice==0?tr(L"Automatisch (Spielsprache)",L"Automatic (game language)"):vrLanguageChoice==1?L"Deutsch":L"English");
  p.text(40,675,1020,tr(L"Originale Maus-/Tastaturoptionen bleiben im Spielmen\u00fc.",L"Original mouse/keyboard options remain in the game menu."),25);
  row(775,75,tr(L"Zur\u00fcck zum Spielmen\u00fc",L"Back to game menu"),L"");
 }else{
  p.text(40,105,1020,bindingWaiting>=0?tr(L"Alle Tasten loslassen, dann neue Taste dr\u00fccken.",L"Release all buttons, then press the new button."):tr(L"Zeigen und Trigger dr\u00fccken.",L"Point and pull the trigger."),28);
  for(int i=0;i<5;++i){int action=bindingPage*5+i;row(180+i*90,80,VrBindings::label(action),bindingWaiting==action?tr(L"Taste dr\u00fccken ...",L"Press a button ..."):bindingSource(vrBindings.source[action]));}
  p.box(40,655,460,80,RGB(37,49,71));p.text(62,664,420,bindingPage?tr(L"< Seite 1 / 2",L"< Page 1 / 2"):tr(L"Seite 2 / 2 >",L"Page 2 / 2 >"),31);
  p.box(540,655,520,80,RGB(37,49,71));p.text(562,664,470,vrBindings.swapSticks?tr(L"Sticks: vertauscht",L"Sticks: swapped"):tr(L"Sticks: Standard",L"Sticks: default"),31);
  p.box(40,775,460,75,RGB(37,49,71));p.text(62,780,420,bindingWaiting>=0?tr(L"Abbrechen",L"Cancel"):tr(L"Standard wiederherstellen",L"Restore defaults"),28);
  p.box(540,775,520,75,RGB(37,49,71));p.text(562,780,470,tr(L"Zur\u00fcck",L"Back"),30);
 }
 p.text(40,875,1020,bindingMessage.empty()?tr(L"Sprachausgaben beschreiben die Standardbelegung.",L"Spoken hints describe the default bindings."):bindingMessage.c_str(),25);
 int x=int(vrMenu.cursorX*1100),y=int(vrMenu.cursorY*960);p.box(x-9,y-2,18,4,RGB(255,240,195));p.box(x-2,y-9,4,18,RGB(255,240,195));
}
static void renderBindingPanel(){paintSettings();bindingPanel.upload(startup.mode==3?runtime.adapter:0,&bridge);if(startup.mode==3)vrMenu.submit(runtime,bindingPanel.texture.textures[0],1100,960);}
static void renderMenuHint(bool){menuHint.hide();} // Original menu now has a single native entry.
