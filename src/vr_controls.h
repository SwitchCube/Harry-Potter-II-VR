#pragma once
#include "vr_math.h"
namespace hpvr {
struct Stick2 {float x,y;};
inline Stick2 radialStick(float x,float y,float deadzone){
 if(!std::isfinite(x)||!std::isfinite(y))return {0,0};
 float length=std::hypot(x,y);if(length<=deadzone)return {0,0};
 float scale=(std::min(length,1.0f)-deadzone)/((1-deadzone)*length);
 return {x*scale,y*scale};
}
// One cardinal direction at a time; release hysteresis avoids noisy repeated arrows.
// Either thumbstick can operate spell lessons; they never turn the world here.
struct LessonStick {
 int held=-1;bool ready=false;
 unsigned update(Stick2 left,Stick2 right,bool enabled){
  if(!enabled){held=-1;ready=false;return 0;}
  if(!std::isfinite(left.x)||!std::isfinite(left.y))left={0,0};
  if(!std::isfinite(right.x)||!std::isfinite(right.y))right={0,0};
  float a=std::max(std::fabs(left.x),std::fabs(left.y)),b=std::max(std::fabs(right.x),std::fabs(right.y));
  if(!ready){ready=a<.30f&&b<.30f;return 0;}
  Stick2 s=b>a?right:left;float x=std::fabs(s.x),y=std::fabs(s.y),m=std::max(x,y);
  if(m<.30f)held=-1;
  else if(m>.55f&&std::fabs(x-y)>.05f)held=y>x?(s.y>0?0:1):(s.x<0?2:3);
  return held<0?0:1u<<held;
 }
};
struct TurnControl {
 bool held=false;
 float update(float x,float delta,bool smooth,float degrees,float speed){
  if(!std::isfinite(x))x=0;
  if(smooth){held=false;return radialStick(x,0,.2f).x*speed*pi/180*std::max(0.0f,std::min(delta,.05f));}
  if(std::fabs(x)<.25f)held=false;
  if(std::fabs(x)>.55f&&!held){held=true;return std::copysign(degrees*pi/180,x);}
  return 0;
 }
};
inline float dot(Vec a,Vec b){return a.x*b.x+a.y*b.y+a.z*b.z;}
inline Vec inverseConvert(Vec p){return {p.y,p.z,-p.x};}
// Rotate about the tracked head, including a room-scale offset from the body.
inline void turnAtHead(Origin& origin,const Mat& head,float angle){
 Vec offset=yaw(convert(sub(translation(head),origin.reference)),origin.baseYaw);
 origin.baseYaw=std::remainder(origin.baseYaw+angle,2*pi);
 origin.reference=sub(translation(head),inverseConvert(yaw(offset,-origin.baseYaw)));
}
}
