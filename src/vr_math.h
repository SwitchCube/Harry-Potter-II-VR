#pragma once
#include <cmath>
#include <algorithm>
#include <stdexcept>
namespace hpvr {
constexpr float pi=3.14159265358979323846f;
struct Vec {float x,y,z;};
struct Rot {int pitch,yaw,roll;};
struct Mat {float m[3][4];};
struct Bounds {float uMin,vMin,uMax,vMax;};
struct Projection {float tanH,tanV,fov;Bounds eye[2];};
inline Vec add(Vec a,Vec b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
inline Vec sub(Vec a,Vec b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
inline Vec scale(Vec a,float s){return {a.x*s,a.y*s,a.z*s};}
inline Vec convert(Vec p){return {-p.z,p.x,p.y};} // Tracking right/up/back -> game forward/right/up.
inline Vec yaw(Vec p,float a){float c=std::cos(a),s=std::sin(a);return {p.x*c-p.y*s,p.x*s+p.y*c,p.z};}
inline Vec translation(const Mat& m){return {m.m[0][3],m.m[1][3],m.m[2][3]};}
inline Mat identity(){Mat r={};for(int i=0;i<3;++i)r.m[i][i]=1;return r;}
inline Mat multiply(const Mat& a,const Mat& b){Mat r={};for(int i=0;i<3;++i){for(int j=0;j<4;++j){for(int k=0;k<3;++k)r.m[i][j]+=a.m[i][k]*b.m[k][j];}r.m[i][3]+=a.m[i][3];}return r;}
inline Vec axis(const Mat& m,int c){return {m.m[0][c],m.m[1][c],m.m[2][c]};}
inline float heading(const Mat& m){Vec f=convert(scale(axis(m,2),-1));return std::atan2(f.y,f.x);}
inline Rot rotation(const Mat& m,float baseYaw){
 Vec f=yaw(convert(scale(axis(m,2),-1)),baseYaw),r=yaw(convert(axis(m,0)),baseYaw),u=yaw(convert(axis(m,1)),baseYaw);
 float k=32768.0f/pi;return {int(std::lround(std::atan2(f.z,std::hypot(f.x,f.y))*k)),int(std::lround(std::atan2(f.y,f.x)*k)),int(std::lround(std::atan2(-r.z,u.z)*k))};
}
struct Origin {
 Vec reference={};float baseYaw=0;bool ready=false;
 void recenter(const Mat& h,float playerYaw){reference=translation(h);baseYaw=playerYaw-heading(h);ready=true;}
 Vec position(const Mat& pose,Vec playerEye,float unitsPerMetre)const{return add(playerEye,scale(yaw(convert(sub(translation(pose),reference)),baseYaw),unitsPerMetre));}
};
inline Projection projection(const float (&t)[2][4],float aspect){
 if(!std::isfinite(aspect)||aspect<=0)throw std::runtime_error("Invalid render aspect");
 Projection p={};float h=0,v=0;
 for(int e=0;e<2;++e){for(int j=0;j<4;++j)if(!std::isfinite(t[e][j]))throw std::runtime_error("Nonfinite projection");
  if(!(t[e][0]<0&&t[e][1]>0&&t[e][2]<0&&t[e][3]>0))throw std::runtime_error("Invalid projection signs");
  h=std::max(h,std::max(-t[e][0],t[e][1]));v=std::max(v,std::max(-t[e][2],t[e][3]));}
 p.tanH=std::max(h,v*aspect);p.tanV=p.tanH/aspect;p.fov=2*std::atan(p.tanH)*180/pi;
 if(p.fov>=170||p.fov<=1)throw std::runtime_error("Projection exceeds reviewed renderer range");
 // OpenVR raw "top" is the negative Y/bottom plane; "bottom" is positive Y/top.
 // D3D texture V grows downward, so negate and swap the vertical endpoints.
 for(int e=0;e<2;++e)p.eye[e]={(1+t[e][0]/p.tanH)*.5f,(1-t[e][3]/p.tanV)*.5f,(1+t[e][1]/p.tanH)*.5f,(1-t[e][2]/p.tanV)*.5f};
 return p;
}
// Independently compare cropped game rays against OpenVR's actual projection matrix.
inline float projectionMatrixError(const Projection& p,unsigned eye,const float (&m)[4][4]){
 if(eye>1)throw std::runtime_error("Invalid projection eye");
 const auto b=p.eye[eye];float error=0;
 for(float x:{-.6f,0.0f,.6f})for(float y:{-.6f,0.0f,.6f}){
  float ray[4]={x,y,-1,1},clip[4]={};
  for(int i=0;i<4;++i)for(int j=0;j<4;++j)clip[i]+=m[i][j]*ray[j];
  if(!std::isfinite(clip[3])||std::fabs(clip[3])<.00001f)throw std::runtime_error("Invalid projection clip W");
  float u=(.5f+x/(2*p.tanH)-b.uMin)/(b.uMax-b.uMin);
  float v=(.5f-y/(2*p.tanV)-b.vMin)/(b.vMax-b.vMin);
  float du=std::fabs(u-(clip[0]/clip[3]+1)*.5f),dv=std::fabs(v-(1-clip[1]/clip[3])*.5f);
  if(!std::isfinite(du)||!std::isfinite(dv))throw std::runtime_error("Nonfinite projection check");
  error=std::max(error,std::max(du,dv));
 }
 return error;
}
}
