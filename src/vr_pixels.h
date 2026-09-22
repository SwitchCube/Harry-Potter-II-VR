#pragma once
#include <cstddef>
#include <cstring>
#include <emmintrin.h>
// D3D7 BGRX -> opaque D3D11 BGRA. Read each source pixel once; no padding reads.
// SSE2 is available on the verified Ryzen target. Unaligned rows are supported.
inline void copyOpaqueBGRA(unsigned char* destination,const unsigned char* source,
                           unsigned width,unsigned height,std::size_t sourcePitch){
 const __m128i opaque=_mm_set1_epi32(-16777216); // 0xff000000
 for(unsigned y=0;y<height;++y){
  const unsigned char* src=source+std::size_t(y)*sourcePitch;
  unsigned char* dst=destination+std::size_t(y)*width*4;
  unsigned x=0;
  for(;width-x>=4;x+=4){
   const __m128i bgra=_mm_loadu_si128(reinterpret_cast<const __m128i*>(src+std::size_t(x)*4));
   _mm_storeu_si128(reinterpret_cast<__m128i*>(dst+std::size_t(x)*4),_mm_or_si128(bgra,opaque));
  }
  for(;x<width;++x){const unsigned char* p=src+std::size_t(x)*4;unsigned char* q=dst+std::size_t(x)*4;q[0]=p[0];q[1]=p[1];q[2]=p[2];q[3]=255;}
 }
}

// Preserve RGB; normalize fixed-function blended alpha for a premultiplied overlay.
// Load each uncached D3D7 pixel once, including odd-width row tails.
inline unsigned copyHudBGRA(unsigned char* destination,const unsigned char* source,
                            unsigned width,unsigned height,std::size_t sourcePitch,bool sourceAlpha=true){
 unsigned visible=0;const __m128i rgb=_mm_set1_epi32(0x00ffffff),zero=_mm_setzero_si128();
 const unsigned bits[16]={0,1,1,2,1,2,2,3,1,2,2,3,2,3,3,4};
 for(unsigned y=0;y<height;++y){
  const auto src=source+std::size_t(y)*sourcePitch;auto dst=destination+std::size_t(y)*width*4;unsigned x=0;
  for(;width-x>=4;x+=4){
   __m128i value=_mm_loadu_si128(reinterpret_cast<const __m128i*>(src+x*4));if(!sourceAlpha)value=_mm_and_si128(value,rgb);
   __m128i maximum=_mm_max_epu8(value,_mm_srli_epi32(value,8));maximum=_mm_max_epu8(maximum,_mm_srli_epi32(maximum,16));
   __m128i alpha=_mm_slli_epi32(maximum,24),out=_mm_or_si128(_mm_and_si128(value,rgb),alpha);
   _mm_storeu_si128(reinterpret_cast<__m128i*>(dst+x*4),out);
   unsigned mask=15u^unsigned(_mm_movemask_ps(_mm_castsi128_ps(_mm_cmpeq_epi32(alpha,zero))));visible+=bits[mask];
  }
  for(;x<width;++x){unsigned value;std::memcpy(&value,src+x*4,4);if(!sourceAlpha)value&=0x00ffffff;unsigned a=value>>24;
   for(unsigned shift=0;shift<24;shift+=8){unsigned c=(value>>shift)&255;if(c>a)a=c;}
   value=(value&0x00ffffff)|(a<<24);std::memcpy(dst+x*4,&value,4);if(a)++visible;
  }
 }
 return visible;
}
