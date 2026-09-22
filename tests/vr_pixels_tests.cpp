#include "vr_pixels.h"
#include <windows.h>
#include <vector>
#include <cstdio>
#include <cstdlib>
static void check(bool ok,const char* why){if(!ok){std::fprintf(stderr,"FAIL: %s\n",why);std::exit(1);}}
int main(){
 const unsigned widths[]={1,2,3,4,5,7,8,9,15,16,17,31,32,33,65,127,800,1280,2048};
 unsigned cases=0;
 for(unsigned w:widths)for(unsigned padding:{0u,1u,7u,64u})for(unsigned offset:{0u,1u,3u,15u}){
  const unsigned height=3,pitch=w*4+padding;
  std::vector<unsigned char> source(pitch*height+32,0x5a),destination(w*4*height+32,0xa5);
  for(unsigned y=0;y<height;++y)for(unsigned x=0;x<w;++x)for(unsigned c=0;c<4;++c)
   source[offset+y*pitch+x*4+c]=static_cast<unsigned char>((x*37+y*19+c*71)%256);
  const auto before=source;
  copyOpaqueBGRA(destination.data()+offset,source.data()+offset,w,height,pitch);
  check(source==before,"source modified");
  for(unsigned i=0;i<destination.size();++i){
   unsigned char expected=0xa5;
   if(i>=offset&&i<offset+w*4*height){unsigned a=i-offset,y=a/(w*4),x=(a/4)%w,c=a%4;expected=c==3?255:static_cast<unsigned char>((x*37+y*19+c*71)%256);}
   check(destination[i]==expected,"color, alpha or destination canary mismatch");
  }
  std::fill(destination.begin(),destination.end(),0xa5);
  unsigned visible=copyHudBGRA(destination.data()+offset,source.data()+offset,w,height,pitch),expectedVisible=0;
  check(source==before,"HUD source modified");
  for(unsigned y=0;y<height;++y)for(unsigned x=0;x<w;++x){
   const auto in=source.data()+offset+y*pitch+x*4;const auto out=destination.data()+offset+(y*w+x)*4;
   unsigned a=0;for(unsigned c=0;c<4;++c)if(in[c]>a)a=in[c];
   check(out[0]==in[0]&&out[1]==in[1]&&out[2]==in[2]&&out[3]==a,"HUD premultiplied alpha/color mismatch");if(a)++expectedVisible;
  }
  check(visible==expectedVisible,"HUD pixel count");
  for(unsigned i=0;i<offset;++i)check(destination[i]==0xa5,"HUD leading canary");
  for(unsigned i=offset+w*4*height;i<destination.size();++i)check(destination[i]==0xa5,"HUD trailing canary");
  visible=copyHudBGRA(destination.data()+offset,source.data()+offset,w,height,pitch,false);expectedVisible=0;
  for(unsigned y=0;y<height;++y)for(unsigned x=0;x<w;++x){
   const auto in=source.data()+offset+y*pitch+x*4;const auto out=destination.data()+offset+(y*w+x)*4;
   unsigned a=0;for(unsigned c=0;c<3;++c)if(in[c]>a)a=in[c];
   check(out[0]==in[0]&&out[1]==in[1]&&out[2]==in[2]&&out[3]==a,"Borrowed BGRX alpha/color mismatch");if(a)++expectedVisible;
  }
  check(visible==expectedVisible&&source==before,"Borrowed BGRX pixel count/source");
  ++cases;
 }
 // A readable source row ending immediately at an inaccessible page catches
 // SIMD reads past the last pixel, including the scalar tail on odd widths.
 SYSTEM_INFO info;GetSystemInfo(&info);const unsigned page=info.dwPageSize;
 auto* allocation=static_cast<unsigned char*>(VirtualAlloc(nullptr,page*2,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
 check(allocation!=nullptr,"guard allocation");DWORD old=0;
 check(VirtualProtect(allocation+page,page,PAGE_NOACCESS,&old)!=0,"guard protection");
 for(unsigned w=1;w<=65;++w){unsigned char* source=allocation+page-w*4;for(unsigned n=0;n<w*4;++n)source[n]=static_cast<unsigned char>(n);std::vector<unsigned char> destination(w*4);copyOpaqueBGRA(destination.data(),source,w,1,w*4);copyHudBGRA(destination.data(),source,w,1,w*4);}
 unsigned char clear[20]={},clearOut[20];check(copyHudBGRA(clearOut,clear,5,1,20)==0,"Transparent HUD counted visible");
 for(unsigned i=0;i<5;++i)clear[i*4+3]=255;
 check(copyHudBGRA(clearOut,clear,5,1,20,false)==0,"Unused X byte made background opaque");
 VirtualFree(allocation,0,MEM_RELEASE);
 std::printf("VR pixels: %u padded/unaligned color and alpha fixtures plus 65 guarded row ends passed\n",cases);
}
