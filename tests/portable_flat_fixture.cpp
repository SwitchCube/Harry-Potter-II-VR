#define WIN32_LEAN_AND_MEAN
#include <windows.h>
static LRESULT CALLBACK proc(HWND w,UINT m,WPARAM a,LPARAM b){if(m==WM_DESTROY){PostQuitMessage(0);return 0;}return DefWindowProcW(w,m,a,b);}
int main(int argc,char**){if(argc!=1)return 8;WNDCLASSW c={};c.lpfnWndProc=proc;c.hInstance=GetModuleHandleW(nullptr);c.lpszClassName=L"PortableFlatFixture";c.hbrBackground=(HBRUSH)(COLOR_WINDOW+1);RegisterClassW(&c);
 HWND w=CreateWindowW(c.lpszClassName,L"Main Menu",WS_OVERLAPPEDWINDOW|WS_VISIBLE,100,100,500,600,nullptr,nullptr,c.hInstance,nullptr);if(!w)return 9;
 MSG msg;while(GetMessageW(&msg,nullptr,0,0)>0){TranslateMessage(&msg);DispatchMessageW(&msg);}return 0;}
