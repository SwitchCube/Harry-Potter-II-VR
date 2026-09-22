#pragma once
// Only a native VR game viewport is isolated from desktop WM_SIZE. Stock Flat
// startup does not load this hook. Explicit engine setres remains authoritative.
using ViewportMessage=LRESULT(__thiscall*)(void*,UINT,WPARAM,LPARAM);
static ViewportMessage originalViewportMessage=nullptr;
static void* protectedViewport=nullptr;
static bool allowViewportResize=false;
static LRESULT __fastcall viewportMessageHook(void* self,void*,UINT message,WPARAM wparam,LPARAM lparam){
 if(self==protectedViewport&&!allowViewportResize&&message==WM_SIZE){
  log("{\"event\":\"desktop_resize_isolated\",\"state\":%u,\"desktop_width\":%u,\"desktop_height\":%u,\"vr_width\":%u,\"vr_height\":%u}",unsigned(wparam),unsigned(LOWORD(lparam)),unsigned(HIWORD(lparam)),vrSettings.renderSize,vrSettings.renderSize);
  return 0;
 }
 return originalViewportMessage(self,message,wparam,lparam);
}
static void protectVrResolution(void* viewport,unsigned width,unsigned height){
 static bool enabled=startup.mode==3||profileFlag(L"replay-window.flag");
 if(enabled&&width==vrSettings.renderSize&&height==vrSettings.renderSize&&protectedViewport!=viewport){
  protectedViewport=viewport;log("{\"event\":\"vr_resolution_protected\",\"width\":%u,\"height\":%u}",width,height);
 }
}
static void replayDesktopWindow(){
 if(startup.mode!=4||!profileFlag(L"replay-window.flag")||!protectedViewport)return;
 static unsigned phase=0;if(phase>=3||vrFrames<300+phase*400)return;
 using GetWindow=HWND(__thiscall*)(void*);
 HWND window=reinterpret_cast<GetWindow>(targets.at("viewportwindow"))(protectedViewport);
 need(window&&IsWindow(window),"Missing own diagnostic viewport window");DWORD process=0;GetWindowThreadProcessId(window,&process);need(process==GetCurrentProcessId(),"Diagnostic window is not owned by game");
 if(phase==0)ShowWindow(window,SW_MAXIMIZE);
 else if(phase==1){ShowWindow(window,SW_RESTORE);SetWindowPos(window,nullptr,0,0,920,650,SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);}
 else SetWindowPos(window,nullptr,0,0,1440,900,SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);
 RECT r={};GetClientRect(window,&r);
 log("{\"event\":\"desktop_window_fixture\",\"phase\":%u,\"client_width\":%ld,\"client_height\":%ld}",++phase,r.right,r.bottom);
}
