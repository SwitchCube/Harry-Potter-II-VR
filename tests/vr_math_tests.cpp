#include "vr_compat.h"
#include "vr_polish_math.h"
#include "vr_gameplay_math.h"
#include "vr_math.h"
#include "vr_controls.h"
#include "vr_wand_pose.h"
#include "vr_health_math.h"
#include <cstdio>
#include <cstdlib>
using namespace hpvr;
void check(bool b,const char* m){if(!b){std::fprintf(stderr,"FAIL: %s\n",m);std::exit(1);}}
void near(float a,float b){check(std::fabs(a-b)<.0001f,"numerical reference");}
int main(){
 check(vrMemoryCap(2048,0)==1280,"unknown or integrated GPU conservative fallback");
 check(vrMemoryCap(2048,1024ull*1024*1024)==1280,"low VRAM cap");
 check(vrMemoryCap(2048,2ull*1024*1024*1024)==1536,"mid VRAM cap");
 check(vrMemoryCap(2048,8ull*1024*1024*1024)==2048,"high VRAM preserves requested quality");
 check(vrMemoryCap(1280,8ull*1024*1024*1024)==1280,"never increase requested quality");
 for(Vec target:{Vec{400,0,0},Vec{250,120,100},Vec{-50,-200,-150}}){Vec start={10,20,30},g={0,0,-950};auto v=velocityTo(start,target,g,.8f);auto hit=trajectory(start,v,g,.8f);near(hit.x,target.x);near(hit.y,target.y);near(hit.z,target.z);}
 near(yawDelta(65530,4),10*pi/32768);near(yawDelta(4,65530),-10*pi/32768);
 auto nose=pitchedHead(Rot{0,0,0},0,pi/6);check(std::abs(nose.pitch-5461)<2&&nose.yaw==0,"flight nose follows pitch");
 auto side=forwardRot(pitchedHead(Rot{0,16384,0},0,pi/6));near(side.y,1);near(side.z,0);

 LessonStick lesson;
 check(lesson.update({0,1},{0,0},true)==0,"lesson ignores walking stick carried into lesson");
 check(lesson.update({0,0},{0,0},true)==0,"lesson arms after neutral");
 check(lesson.update({0,1},{0,0},true)==1,"lesson left-stick up");
 check(lesson.update({0,.4f},{0,0},true)==1,"lesson hysteresis retains hold");
 check(lesson.update({0,.2f},{0,0},true)==0,"lesson neutral releases");
 check(lesson.update({0,0},{-1,0},true)==4,"lesson right-stick left");
 check(lesson.update({0,0},{0,-1},true)==2,"lesson cardinal change");
 check(lesson.update({1,0},{0,0},true)==8,"lesson right");
 check(lesson.update({1,0},{0,0},false)==0,"lesson focus loss releases");
 check(lesson.update({1,1},{0,0},true)==0,"lesson ambiguous diagonal has no double arrow");
 check(lesson.update({0,0},{0,0},true)==0,"lesson rearms after focus loss");
 check(lesson.update({0,1},{1,0},true)==1,"lesson conflicting sticks select one arrow");

 auto id=identity();auto r=rotation(id,0);check(!r.pitch&&!r.yaw&&!r.roll,"identity forward");
 r=rotation(id,pi/2);check(r.yaw==16384&&!r.pitch&&!r.roll,"world yaw quarter turn");
 Mat pitch={{{1,0,0,0},{0,0,-1,0},{0,1,0,0}}};r=rotation(pitch,0);check(r.pitch==16384,"look up");
 Mat roll={{{0,-1,0,0},{1,0,0,0},{0,0,1,0}}};r=rotation(roll,0);check(r.roll==-16384,"roll sign");
 Mat h=id;h.m[0][3]=2;h.m[1][3]=1.7f;h.m[2][3]=3;Origin o;o.recenter(h,0);
 Vec eye={100,200,140};auto p=o.position(h,eye,50);near(p.x,100);near(p.y,200);near(p.z,140);
 Mat left=id,right=id;left.m[0][3]=-.032f;right.m[0][3]=.032f;
 auto l=o.position(multiply(h,left),eye,50),q=o.position(multiply(h,right),eye,50);near(l.y,198.4f);near(q.y,201.6f);near(q.y-l.y,3.2f);
 h.m[2][3]-=.2f;p=o.position(h,eye,50);near(p.x,110);near(p.z,140);
 float t[2][4]={{-1.2f,.9f,-1.1f,1},{-.9f,1.2f,-1.1f,1}};auto v=projection(t,4.0f/3);near(v.tanV,1.1f);check(v.fov>100&&v.fov<120,"overscan FOV");
 for(int e=0;e<2;++e){auto b=v.eye[e];near((2*b.uMin-1)*v.tanH,t[e][0]);near((2*b.uMax-1)*v.tanH,t[e][1]);near((1-2*b.vMin)*v.tanV,t[e][3]);near((1-2*b.vMax)*v.tanV,t[e][2]);check(b.uMin>=0&&b.vMin>=0&&b.uMax<=1&&b.vMax<=1,"bounded crop");}
 // Asymmetric frustum reference: RH eye coordinates, -Z forward, clip +Y up.
 // This fixture catches the old vertical crop sign/endpoint error.
 float matrix[4][4]={{.952380952f,0,-.142857143f,0},{0,.952380952f,-.047619048f,0},{0,0,-1.001001f,-.1001001f},{0,0,-1,0}};
 near(projectionMatrixError(v,0,matrix),0);near(v.eye[0].vMin,1.0f/22);near(v.eye[0].vMax,1);
 auto bad=v;bad.eye[0].vMin=0;bad.eye[0].vMax=21.0f/22;check(projectionMatrixError(bad,0,matrix)>.04f,"reject mirrored vertical offset");
 matrix[0][2]=.142857143f;near(projectionMatrixError(v,1,matrix),0);
 bool caught=false;try{t[0][0]=1;projection(t,1);}catch(...){caught=true;}check(caught,"reject malformed runtime projection");
 auto stick=radialStick(1,1,.2f);near(std::hypot(stick.x,stick.y),1);near(stick.x,stick.y);
 stick=radialStick(.1f,-.1f,.2f);near(stick.x,0);near(stick.y,0);
 stick=radialStick(-1,0,.2f);near(stick.x,-1);near(stick.y,0);
 TurnControl turn;near(turn.update(1,.01f,false,30,90),pi/6);near(turn.update(1,.01f,false,30,90),0);
 near(turn.update(-1,.01f,false,30,90),0);turn.update(0,.01f,false,30,90);near(turn.update(-1,.01f,false,30,90),-pi/6);
 near(turn.update(1,.02f,true,30,90),pi/100);near(turn.update(1,1,true,30,90),pi/40);
 h=identity();o.recenter(h,0);h.m[0][3]=.3f;h.m[2][3]=-.5f;auto before=o.position(h,eye,50);
 turnAtHead(o,h,pi/2);auto after=o.position(h,eye,50);near(before.x,after.x);near(before.y,after.y);near(before.z,after.z);
 // Calibration is local to the hand, preserves a 28 cm wand, and does not
 // change direction when only its grip origin is moved.
 auto grip=wandPose(identity(),Vec{.02f,-.04f,.03f},Vec{0,0,0});
 near(grip.m[0][3],.02f);near(grip.m[1][3],-.04f);near(grip.m[2][3],-.03f);
 auto direction=scale(axis(grip,2),-1);near(direction.x,0);near(direction.y,0);near(direction.z,-1);
 auto rotatedGrip=wandPose(roll,Vec{0,-.04f,0},Vec{0,0,0});
 near(rotatedGrip.m[0][3],.04f);near(rotatedGrip.m[1][3],0);
 auto aimed=wandPose(identity(),Vec{0,0,0},Vec{90,0,0});direction=scale(axis(aimed,2),-1);near(direction.y,1);near(direction.z,0);
 aimed=wandPose(identity(),Vec{0,0,0},Vec{0,90,0});direction=scale(axis(aimed,2),-1);near(direction.x,1);near(direction.z,0);
 aimed=wandPose(identity(),Vec{0,0,0},Vec{20,35,10});direction=scale(axis(aimed,2),-.28f);
 near(std::sqrt(direction.x*direction.x+direction.y*direction.y+direction.z*direction.z),.28f);
 // Different aim/grip origins must still put the handle centre in the fist,
 // even with head-independent controller rotation and a calibrated angle.
 auto aim=identity(),fist=identity();aim.m[1][3]=.3f;fist.m[0][3]=.2f;fist.m[1][3]=-.15f;fist.m[2][3]=-.4f;
 for(float yawAngle:{0.0f,90.0f,-50.0f}){
  auto held=heldWandPose(fist,Vec{0,0,0},Vec{25,yawAngle,0},.28f);
  auto middle=add(translation(held),scale(axis(held,2),-.28f*.15f));
  near(middle.x,.2f);near(middle.y,-.15f);near(middle.z,-.4f);
 }
 auto tiltedFist=wandPose(fist,Vec{0,0,0},Vec{35,20,0});
 auto held=heldWandPose(tiltedFist,Vec{0,0,0},Vec{0,0,0},.28f);
 for(int row=0;row<3;++row)for(int col=0;col<3;++col)near(held.m[row][col],tiltedFist.m[row][col]);
 // Use independent actor-rotator reconstruction to verify the original sword,
 // including its grip pivot, blade direction and calibration under combined rotations.
 for(Vec angles:{Vec{-15.563f,0,0},Vec{35,90,25},Vec{-70,-40,80}}){
  auto hand=heldWandPose(fist,Vec{},angles,.28f);Vec f=convert(scale(axis(hand,2),-1)),u=convert(axis(hand,1)),centre={110,-20,42};
  auto rot=swordRot(f,u);Vec loc=swordPoint(Vec{},centre,f,u,50);
  auto actual=add(loc,rotateModel(swordGripPoint(),rot));
  check(std::sqrt(dot(sub(actual,centre),sub(actual,centre)))<.001f,"actual sword actor grip stays in fist");
  Vec tip=swordPoint({-.430183f,-.31240f,-55.82732f},centre,f,u,50);
  near(dot(sub(tip,centre),f),57.542944f);
  auto meshTip=add(loc,rotateModel({-.430183f,-.31240f,-55.82732f},rot));
  check(std::sqrt(dot(sub(meshTip,tip),sub(meshTip,tip)))<.01f,"rotated original blade follows accepted wand direction");
  for(float charge:{0.0f,1.0f,2.0f}){float offset=55*charge/2;Vec weaponLoc=add(tip,scale(u,offset));
   Vec stockSpawn=sub(weaponLoc,rotateModel({0,0,offset},rotation(hand,0)));
   check(std::sqrt(dot(sub(stockSpawn,tip),sub(stockSpawn,tip)))<.01f,"original charge-dependent sword spawn stays at tip");
  }
 }
 // Health containers must retain their entire last icon, including a partial
 // potential. The attachment stays at the fist independently of head rotation.
 for(unsigned view:{800u,1280u,2048u})for(int containers=1;containers<=6;++containers){
  auto rect=healthRect(containers*100-1,100,21,32,64,view,view);
  check(rect.icons==unsigned(containers),"partial potential still has final bolt");
  check(rect.width>=(5+containers*21)*float(view)/640,"last actual health icon is not clipped");
  check(rect.height>=68*float(view)/640,"health full height");
 }
 auto fistPose=identity(),headPose=identity();fistPose.m[0][3]=-.25f;fistPose.m[1][3]=1.0f;fistPose.m[2][3]=-.4f;headPose.m[1][3]=1.65f;
 auto health=healthPlane(fistPose,headPose);near(health.m[0][3],-.25f);near(health.m[1][3],1.1f);near(health.m[2][3],-.4f);
 auto tilted=wandPose(fistPose,Vec{0,0,0},Vec{75,30,90});auto still=healthPlane(tilted,headPose);
 for(int row=0;row<3;++row)for(int col=0;col<4;++col)near(health.m[row][col],still.m[row][col]);
 headPose.m[0][3]=.4f;auto movedHead=healthPlane(fistPose,headPose);
 for(int row=0;row<3;++row)near(health.m[row][3],movedHead.m[row][3]);
 auto toward=unit(sub(translation(headPose),translation(movedHead)));near(dot(axis(movedHead,2),toward),1);
 near(dot(axis(movedHead,0),axis(movedHead,1)),0);near(dot(axis(movedHead,1),axis(movedHead,2)),0);
 // The actual NPC-facing basis must become a rotation, not a reflection, in OpenVR.
 for(float angle:{0.0f,.73f,pi/2,2.68f,pi,-1.99f})for(float yawAngle:{-2.0f,0.0f,1.5f}){
  Vec npc={140,80,30},viewer=add(npc,Vec{80*std::cos(angle),80*std::sin(angle),35});auto facing=vendorFacing(sub(viewer,npc));
  Origin origin;origin.ready=true;origin.baseYaw=yawAngle;origin.reference={.1f,1.6f,-.2f};
  Vec centre=add(npc,add(scale(facing.right,28),Vec{0,0,45}));auto panel=worldOverlay(centre,facing.right,Vec{0,0,1},facing.normal,origin,viewer,50);
  float determinant=dot(healthCross(axis(panel,0),axis(panel,1)),axis(panel,2));
  check(std::fabs(determinant-1)<.0001f,"vendor overlay must have determinant +1 (front-facing rotation)");
  near(dot(axis(panel,0),axis(panel,1)),0);near(dot(axis(panel,0),axis(panel,2)),0);near(dot(axis(panel,1),axis(panel,2)),0);
  auto trackingViewer=identity();trackingViewer.m[0][3]=origin.reference.x;trackingViewer.m[1][3]=origin.reference.y;trackingViewer.m[2][3]=origin.reference.z;
  check(dot(axis(panel,2),sub(translation(trackingViewer),translation(panel)))>0,"vendor panel front must face viewer");
  auto stockRight=Vec{std::sin(angle),-std::cos(angle),0};near(dot(facing.right,stockRight),1);
 }
 // Only virtual walking steps are smoothed; real head motion is added later.
 for(float yawAngle:{-2.0f,0.0f,1.5f})for(Vec playerEye:{Vec{20,30,50},Vec{-10,8,80}}){
  Origin o;o.ready=true;o.reference={.2f,1.6f,.1f};o.baseYaw=yawAngle;Vec centre={150,70,100},right={0,1,0},normal={-1,0,0};auto panel=worldOverlay(centre,right,Vec{0,0,1},normal,o,playerEye,50);auto back=o.position(panel,playerEye,50);
  near(back.x,centre.x);near(back.y,centre.y);near(back.z,centre.z);auto worldRight=yaw(convert(axis(panel,0)),o.baseYaw);near(worldRight.x,right.x);near(worldRight.y,right.y);near(worldRight.z,right.z);
  auto head=identity();head.m[0][3]=.6f;head.m[1][3]=1.7f;auto headInWorld=o.position(head,playerEye,50);auto deltaTracking=sub(translation(panel),translation(head));auto deltaWorld=scale(yaw(convert(deltaTracking),o.baseYaw),50);auto expected=sub(centre,headInWorld);near(deltaWorld.x,expected.x);near(deltaWorld.y,expected.y);near(deltaWorld.z,expected.z);
 }
 StepSmoother stair;near(stair.update(0,.01f,true),0);
 float offset=stair.update(16,.01f,true);check(offset<0&&offset>-16,"walking step must ramp");
 for(int i=0;i<100;++i)offset=stair.update(16,.01f,true);
 check(std::fabs(offset)<.001f,"step settles without permanent height loss");
 near(stair.update(70,.01f,false),0);near(stair.update(700,.01f,true,true),0);
 StepSmoother fast,slow;fast.update(0,.01f,true);slow.update(0,.02f,true);
 for(int i=0;i<10;++i)fast.update(16,.01f,true);
 for(int i=0;i<5;++i)slow.update(16,.02f,true);
 near(fast.shown,slow.shown);
 std::puts("VR math: orientation, recenter, physical IPD, translation and asymmetric crop reference checks passed");
}
