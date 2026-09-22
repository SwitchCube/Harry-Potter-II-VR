#pragma once
#include "vr_locale.h"
// An owned control placed over the stock native front end. The stock New/Load/Options
// pages and their save-slot selection remain owned by the original Game.exe.
static bool frontendMode=false,frontendVr=false,frontendSeen=false;
static HWND frontendToggle=nullptr,frontendOwner=nullptr;static bool frontendFound=false;static DWORD frontendPid=0;static HANDLE frontendProcess=nullptr;
static std::wstring frontendSettings;
static volatile LONG frontendSwitchRequested=0;
static const int kFrontendSwitchExit=42;
static LRESULT CALLBACK frontendProc(HWND w,UINT m,WPARAM a,LPARAM b){
 if(m==WM_LBUTTONUP||(m==WM_KEYUP&&a==VK_SPACE)){
  if(InterlockedCompareExchange(&frontendSwitchRequested,0,0))return 0;
  no_links(frontendSettings);
  const bool nextVr=!frontendVr;
  if(!WritePrivateProfileStringW(L"Launch",L"Mode",nextVr?L"VR":L"Flat",frontendSettings.c_str())){
   MessageBoxW(w,tr(L"Die Einstellung konnte nicht gespeichert werden.",L"The setting could not be saved."),L"Harry Potter II",MB_OK|MB_ICONERROR);return 0;
  }
  // A mode change needs a fresh original front end with its own save profile.
  // Close only the attached main menu, never a gameplay window.
  HWND root=frontendOwner;if(!IsWindow(root))return 0;EnableWindow(root,FALSE);
  InterlockedExchange(&frontendSwitchRequested,1);
  std::printf("{\"event\":\"frontend_switch_requested\",\"vr\":%s}\n",nextVr?"true":"false");
  if(!PostMessageW(root,WM_CLOSE,0,0)){
   InterlockedExchange(&frontendSwitchRequested,0);EnableWindow(root,TRUE);WritePrivateProfileStringW(L"Launch",L"Mode",frontendVr?L"VR":L"Flat",frontendSettings.c_str());return 0;
  }
  return 0;
 }
 if(m==WM_MOUSEACTIVATE)return MA_NOACTIVATE;
 if(m==WM_PAINT){PAINTSTRUCT p;HDC dc=BeginPaint(w,&p);RECT r;GetClientRect(w,&r);
  HBRUSH brush=CreateSolidBrush(frontendVr?RGB(56,36,108):RGB(31,36,60));FillRect(dc,&r,brush);DeleteObject(brush);
  SetBkMode(dc,TRANSPARENT);SetTextColor(dc,RGB(255,239,188));SelectObject(dc,GetStockObject(DEFAULT_GUI_FONT));
  DrawTextW(dc,frontendVr?tr(L"Modus: VR  >",L"Mode: VR  >"):tr(L"Modus: Flat  >",L"Mode: Flat  >"),-1,&r,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
  FrameRect(dc,&r,(HBRUSH)GetStockObject(GRAY_BRUSH));EndPaint(w,&p);return 0;
 }
 if(m==WM_GETTEXTLENGTH)return wcslen(frontendVr?tr(L"Modus: VR",L"Mode: VR"):tr(L"Modus: Flat",L"Mode: Flat"));
 if(m==WM_GETTEXT){const wchar_t* text=frontendVr?tr(L"Modus: VR",L"Mode: VR"):tr(L"Modus: Flat",L"Mode: Flat");if(a){wcsncpy((wchar_t*)b,text,a-1);((wchar_t*)b)[a-1]=0;}return wcslen(text);}
 return DefWindowProcW(w,m,a,b);
}
static BOOL CALLBACK frontendWindow(HWND w,LPARAM){
 DWORD pid=0;GetWindowThreadProcessId(w,&pid);if(pid!=frontendPid||!IsWindowVisible(w))return TRUE;
 wchar_t title[256]={};GetWindowTextW(w,title,256);
 if(wcscmp(title,L"Hauptmen\u00fc")&&wcscmp(title,L"Main Menu"))return TRUE;
 if(IsIconic(w))return TRUE;
 RECT r;GetClientRect(w,&r);if(r.right<350||r.bottom<400)return TRUE;
 frontendFound=true;frontendOwner=w;
 POINT point={r.right*58/100,r.bottom*44/100};ClientToScreen(w,&point);
 if(!frontendToggle||!IsWindow(frontendToggle)){
  WNDCLASSW c={};c.lpfnWndProc=frontendProc;c.hInstance=GetModuleHandleW(nullptr);c.hCursor=LoadCursorW(nullptr,IDC_HAND);c.lpszClassName=L"HP2VRModeSwitch";RegisterClassW(&c);
  // An owned popup has its own compositor surface. The original bitmap page
  // can otherwise overpaint a cross-process child while leaving it clickable.
  frontendToggle=CreateWindowExW(WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE,c.lpszClassName,L"Flat / VR",WS_POPUP,
   point.x,point.y,r.right*33/100,38,w,nullptr,c.hInstance,nullptr);
  require(frontendToggle!=nullptr,"Add visible mode switch to original menu");frontendSeen=true;
  std::puts("{\"event\":\"frontend_switch_added\",\"original_menu\":true,\"owned_surface\":true}");
 }
 RECT actual;GetWindowRect(frontendToggle,&actual);
 if(actual.left!=point.x||actual.top!=point.y||actual.right-actual.left!=r.right*33/100)
  SetWindowPos(frontendToggle,nullptr,point.x,point.y,r.right*33/100,38,SWP_NOZORDER|SWP_NOACTIVATE);
 if(!IsWindowVisible(frontendToggle))ShowWindow(frontendToggle,SW_SHOWNOACTIVATE);
 return FALSE;
}
static DWORD WINAPI frontendThread(void*){
 try{while(WaitForSingleObject(frontendProcess,30)==WAIT_TIMEOUT){
 frontendFound=false;EnumWindows(frontendWindow,0);
 if(!frontendFound&&frontendToggle)ShowWindow(frontendToggle,SW_HIDE);
 MSG m;while(PeekMessageW(&m,nullptr,0,0,PM_REMOVE)){TranslateMessage(&m);DispatchMessageW(&m);}
 }}catch(const std::exception& e){std::fprintf(stderr,"Original menu extension: %s\n",e.what());}return 0;
}
static void frontendPump(){if(!frontendMode)return;static bool started=false;if(!started){started=true;HANDLE thread=CreateThread(nullptr,0,frontendThread,nullptr,0,nullptr);require(thread!=nullptr,"Start original menu UI thread");CloseHandle(thread);}}
