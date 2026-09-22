#pragma once
#include "vr_math.h"
namespace hpvr {
// Controller-local right/up/forward translation; pitch up, yaw right, roll right.
// The same calibrated pose drives both the mesh and original spell integration.
inline Mat wandPose(const Mat& hand,Vec offset,Vec anglesDegrees){
 Mat pitch=identity(),yaw=identity(),roll=identity(),local=identity();
 float p=anglesDegrees.x*pi/180,y=-anglesDegrees.y*pi/180,r=-anglesDegrees.z*pi/180;
 pitch.m[1][1]=pitch.m[2][2]=std::cos(p);pitch.m[1][2]=-std::sin(p);pitch.m[2][1]=std::sin(p);
 yaw.m[0][0]=yaw.m[2][2]=std::cos(y);yaw.m[0][2]=std::sin(y);yaw.m[2][0]=-std::sin(y);
 roll.m[0][0]=roll.m[1][1]=std::cos(r);roll.m[0][1]=-std::sin(r);roll.m[1][0]=std::sin(r);
 local=multiply(yaw,multiply(pitch,roll));local.m[0][3]=offset.x;local.m[1][3]=offset.y;local.m[2][3]=-offset.z;
 return multiply(hand,local);
}
// Grip gives both the fist centre and the held-object direction. The handle occupies
// the first 30% of the wand, so its midpoint (15%) belongs exactly at the grip.
inline Mat heldWandPose(const Mat& grip,Vec offset,Vec angles,float lengthMetres){
 Mat pose=wandPose(grip,Vec{0,0,0},angles);
 auto centre=wandPose(grip,offset,Vec{0,0,0});
 Vec origin=add(translation(centre),scale(axis(pose,2),lengthMetres*.15f));
 pose.m[0][3]=origin.x;pose.m[1][3]=origin.y;pose.m[2][3]=origin.z;
 return pose;
}

}
