#pragma once
#include "vr_controls.h"
namespace hpvr {
inline Vec trajectory(Vec start,Vec velocity,Vec gravity,float time){return add(start,add(scale(velocity,time),scale(gravity,.5f*time*time)));}
inline Vec velocityTo(Vec start,Vec target,Vec gravity,float seconds){
 if(!std::isfinite(seconds)||seconds<.05f)throw std::runtime_error("Invalid throw time");
 return sub(scale(sub(target,start),1/seconds),scale(gravity,.5f*seconds));
}
inline float yawDelta(int previous,int next){return std::remainder(float(next-previous),65536.0f)*pi/32768;}
// Pitch about the broom's lateral axis. Roll stays level; physical head roll remains.
inline Vec flightPitch(Vec v,float yawAngle,float pitch){
 Vec p=yaw(v,-yawAngle);float c=std::cos(pitch),s=std::sin(pitch);
 return yaw({p.x*c-p.z*s,p.y,p.x*s+p.z*c},yawAngle);
}
inline Vec forwardRot(Rot r){float y=r.yaw*pi/32768,p=r.pitch*pi/32768;return {std::cos(y)*std::cos(p),std::sin(y)*std::cos(p),std::sin(p)};}
inline Rot pitchedHead(Rot head,float yawAngle,float pitch){
 auto f=flightPitch(forwardRot(head),yawAngle,pitch);
 // Transform the real head's right/up axes as well, including its own roll.
 float y=head.yaw*pi/32768,p=head.pitch*pi/32768,r=head.roll*pi/32768;
 Vec right={-std::sin(y),std::cos(y),0},up={-std::cos(y)*std::sin(p),-std::sin(y)*std::sin(p),std::cos(p)};
 Vec rr=flightPitch(sub(scale(right,std::cos(r)),scale(up,std::sin(r))),yawAngle,pitch);
 Vec uu=flightPitch(add(scale(right,std::sin(r)),scale(up,std::cos(r))),yawAngle,pitch);
 return {int(std::atan2(f.z,std::hypot(f.x,f.y))*32768/pi),int(std::atan2(f.y,f.x)*32768/pi),int(std::atan2(-rr.z,uu.z)*32768/pi)};
}
// The stock Gryffindor mesh points down local -Z; its measured handle centre
// must remain at the calibrated physical fist through pitch, yaw and roll.
inline Vec swordGripPoint(){return {-.430183f,-.31240f,1.715624f};}
inline Vec swordVector(Vec p,Vec forward,Vec up){
 Vec side={up.y*forward.z-up.z*forward.y,up.z*forward.x-up.x*forward.z,up.x*forward.y-up.y*forward.x};
 return add(add(scale(up,p.x),scale(side,p.y)),scale(forward,-p.z));
}
inline Vec swordPoint(Vec model,Vec fist,Vec forward,Vec up,float units){return add(fist,scale(swordVector(sub(model,swordGripPoint()),forward,up),units/50));}
inline Rot swordRot(Vec forward,Vec up){
 Vec x=up,y=swordVector({0,1,0},forward,up),z=scale(forward,-1);float k=32768/pi;
 return {int(std::lround(std::atan2(x.z,std::hypot(x.x,x.y))*k)),int(std::lround(std::atan2(x.y,x.x)*k)),int(std::lround(std::atan2(-y.z,z.z)*k))};
}
// Independent Unreal rotator reconstruction, also used to audit actual actor fields.
inline Vec rotateModel(Vec v,Rot rot){
 float p=rot.pitch*pi/32768,y=rot.yaw*pi/32768,r=rot.roll*pi/32768;
 float cp=std::cos(p),sp=std::sin(p),cy=std::cos(y),sy=std::sin(y),cr=std::cos(r),sr=std::sin(r);
 return add(add(scale(Vec{cp*cy,cp*sy,sp},v.x),scale(Vec{-sy*cr+cy*sp*sr,cy*cr+sy*sp*sr,-cp*sr},v.y)),scale(Vec{-cy*sp*cr-sy*sr,-sy*sp*cr+cy*sr,cp*cr},v.z));
}

}
