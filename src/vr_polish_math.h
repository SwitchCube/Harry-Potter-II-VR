#pragma once
#include "vr_controls.h"
#include <algorithm>
#include <cmath>
namespace hpvr {
struct VendorFacing {Vec right,normal;};
inline VendorFacing vendorFacing(Vec towardPlayer){
 float length=std::hypot(towardPlayer.x,towardPlayer.y);Vec normal=length<1?Vec{1,0,0}:Vec{towardPlayer.x/length,towardPlayer.y/length,0};
 // Game coordinates and OpenVR have opposite handedness. The screen-right
 // axis is normal cross world-up, so conversion yields a proper rotation (+1).
 return {Vec{normal.y,-normal.x,0},normal};
}

inline Mat worldOverlay(Vec centre,Vec right,Vec up,Vec normal,const Origin& origin,Vec eye,float units){
 Mat result={};Vec axes[4]={inverseConvert(yaw(right,-origin.baseYaw)),inverseConvert(yaw(up,-origin.baseYaw)),inverseConvert(yaw(normal,-origin.baseYaw)),add(origin.reference,inverseConvert(yaw(scale(sub(centre,eye),1/units),-origin.baseYaw)))};
 for(int c=0;c<4;++c){result.m[0][c]=axes[c].x;result.m[1][c]=axes[c].y;result.m[2][c]=axes[c].z;}return result;
}

struct StepSmoother {
 bool ready=false;float shown=0,last=0;
 float update(float z,float dt,bool grounded,bool discontinuity=false){
  if(!ready||!grounded||discontinuity||std::fabs(z-last)>40||dt<=0||dt>.2f){shown=z;ready=true;}
  else {shown+=(z-shown)*(1-std::exp(-dt/.065f));shown=z-std::max(-32.0f,std::min(32.0f,z-shown));}
  last=z;return shown-z;
 }
};
}
