#pragma once
#include "vr_controls.h"
namespace hpvr {
struct HealthRect {unsigned width,height,icons;};
inline HealthRect healthRect(int potential,int units,int iconWidth,int textureWidth,int textureHeight,unsigned viewportWidth,unsigned viewportHeight){
 if(potential<=0||potential>600||units<=0||units>600||iconWidth<=0||textureWidth<=0||textureHeight<=0)throw std::runtime_error("Invalid original health geometry");
 unsigned icons=unsigned((potential+units-1)/units);if(icons>6)throw std::runtime_error("Original health exceeds six reviewed containers");
 float scale=float(viewportWidth)/640;
 unsigned w=std::max(64u,unsigned(std::ceil((5+icons*iconWidth)*scale))+4);
 unsigned h=std::max(64u,unsigned(std::ceil((4+textureHeight)*scale))+4);
 if(w>viewportWidth||h>viewportHeight)throw std::runtime_error("Health region exceeds viewport");
 return {w,h,icons};
}
inline Vec healthCross(Vec a,Vec b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
inline Vec unit(Vec a){float n=std::sqrt(dot(a,a));if(!std::isfinite(n)||n<.0001f)throw std::runtime_error("Invalid health plane direction");return scale(a,1/n);}
// Position follows the physical left fist; orientation faces the viewer without
// changing the attachment position when the viewer turns or the hand tilts.
inline Mat healthPlane(const Mat& leftGrip,const Mat& head){
 Vec centre=translation(leftGrip);centre.y+=.10f;
 Vec normal=unit(sub(translation(head),centre));Vec right=healthCross(Vec{0,1,0},normal);
 if(dot(right,right)<.0001f)right=axis(head,0);right=unit(right);
 Vec up=unit(healthCross(normal,right));right=unit(healthCross(up,normal));Mat p={};
 Vec axes[4]={right,up,normal,centre};for(int col=0;col<4;++col){p.m[0][col]=axes[col].x;p.m[1][col]=axes[col].y;p.m[2][col]=axes[col].z;}return p;
}
}
