#pragma once
// Readable native text at a fixed texel density, independent of the old bitmap fonts.
struct VrPanel {
 HDC dc=nullptr;HBITMAP bitmap=nullptr;HGDIOBJ oldBitmap=nullptr;BYTE* data=nullptr;
 unsigned width=0,height=0;VrBridge texture;
 ~VrPanel(){close();}
 void close(){texture.close();if(dc){SelectObject(dc,oldBitmap);DeleteObject(bitmap);DeleteDC(dc);}dc=nullptr;bitmap=nullptr;data=nullptr;width=height=0;}
 void open(unsigned w,unsigned h){if(dc&&width==w&&height==h)return;close();width=w;height=h;
  dc=CreateCompatibleDC(nullptr);BITMAPINFO bi={};bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);bi.bmiHeader.biWidth=w;bi.bmiHeader.biHeight=-LONG(h);bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;bi.bmiHeader.biCompression=BI_RGB;
  bitmap=CreateDIBSection(dc,&bi,DIB_RGB_COLORS,reinterpret_cast<void**>(&data),nullptr,0);if(!dc||!bitmap)throw std::runtime_error("Create VR text panel");oldBitmap=SelectObject(dc,bitmap);SetBkMode(dc,TRANSPARENT);
 }
 void box(int x,int y,int w,int h,COLORREF color){RECT r={x,y,x+w,y+h};HBRUSH b=CreateSolidBrush(color);FillRect(dc,&r,b);DeleteObject(b);}
 void clear(){box(0,0,width,height,RGB(14,18,32));box(0,0,8,height,RGB(219,181,97));}
 void text(int x,int y,int w,const wchar_t* value,int size=32,COLORREF color=RGB(243,239,222)){
  HFONT font=CreateFontW(-size,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,DEFAULT_PITCH,L"Segoe UI");auto old=SelectObject(dc,font);SetTextColor(dc,color);RECT r={x,y,x+w,y+size*2};DrawTextW(dc,value,-1,&r,DT_SINGLELINE|DT_VCENTER|DT_END_ELLIPSIS|DT_NOPREFIX);SelectObject(dc,old);DeleteObject(font);
 }
 void upload(int adapter,VrBridge* parent){
  GdiFlush();for(unsigned i=0;i<width*height;++i)data[i*4+3]=255;
  texture.open(adapter,width,height,true,parent);DDSURFACEDESC2 d={};d.dwSize=sizeof(d);d.dwWidth=width;d.dwHeight=height;d.lpSurface=data;d.lPitch=width*4;
  d.ddpfPixelFormat.dwRGBBitCount=32;d.ddpfPixelFormat.dwRBitMask=0xff0000;d.ddpfPixelFormat.dwGBitMask=0xff00;d.ddpfPixelFormat.dwBBitMask=0xff;d.ddpfPixelFormat.dwRGBAlphaBitMask=0xff000000;
  texture.uploadArgb(d,!(texture.verifiedMask&1));texture.flush();
 }
};
