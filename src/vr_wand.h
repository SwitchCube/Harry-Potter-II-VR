#pragma once
// Small procedural tapered wand in the original stereo scene. No game assets
// are replaced and no simulation actor is moved to draw it.
static unsigned wandDraws=0;
static Vec cross(Vec a,Vec b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
static void drawWand(IDirect3DDevice7* device,void* frame,unsigned eye){
 if(headBlocked||menuVisible)return;
 bool carry=carriedActor()!=nullptr,broom=isBroom(vrPlayer),sword=usingSword();
 bool rain=rewardRainStarted&&GetTickCount64()-rewardRainStarted<5200;
 if(!wandValid&&!rain&&!broom)return;
 struct Vertex{float x,y,z,rhw;DWORD color;};std::vector<Vertex> vertices;
 Vec origin=mem<Vec>(frame,0x34),right=mem<Vec>(frame,0x40),down=mem<Vec>(frame,0x4c),forward=mem<Vec>(frame,0x58);
 float halfW=mem<DWORD>(frame,0xa8)*.5f,halfH=mem<DWORD>(frame,0xac)*.5f,focal=mem<float>(frame,0xdc);
 Vec side=cross(wandUp,wandForward);
 auto vertex=[&](Vec world,DWORD colour,Vertex& out){
  Vec p=hpvr::sub(world,origin);float z=hpvr::dot(p,forward);if(!std::isfinite(z)||z<=1)return false;
  float rhw=1/z;out={halfW+focal*hpvr::dot(p,right)*rhw,halfH+focal*hpvr::dot(p,down)*rhw,1-rhw,rhw,colour};return true;
 };
 auto triangle=[&](Vec a,Vec b,Vec c,DWORD color){Vertex va,vb,vc;if(vertex(a,color,va)&&vertex(b,color,vb)&&vertex(c,color,vc)){vertices.push_back(va);vertices.push_back(vb);vertices.push_back(vc);}};
 auto ring=[&](float distance,float radius,int i){float angle=i*hpvr::pi/4;return hpvr::add(hpvr::add(wandBase,hpvr::scale(wandForward,distance)),hpvr::scale(hpvr::add(hpvr::scale(side,std::cos(angle)),hpvr::scale(wandUp,std::sin(angle))),radius));};
 float length=vrSettings.wandMetres*vrSettings.units;
 if(wandValid&&!carry&&!broom&&!sword&&!worldSceneActive&&!cardRewardActive)for(int band=0;band<2;++band){float start=band?length*.3f:0,end=band?length:length*.3f,r0=(band?.0045f:.007f)*vrSettings.units,r1=(band?.0018f:.006f)*vrSettings.units;
  for(int i=0;i<8;++i){DWORD shade=DWORD(18*(i%4)),color=0xff000000|((96+shade)<<16)|((48+shade/2)<<8)|(22+shade/3);
   Vec a=ring(start,r0,i),b=ring(start,r0,i+1),c=ring(end,r1,i+1),d=ring(end,r1,i);
   triangle(a,b,c,color);triangle(a,c,d,color);
  }
 }
 if(!carry&&!broom&&!sword&&!worldSceneActive&&syntheticHeld[129]){float radius=.005f*vrSettings.units;for(int i=0;i<8;++i)triangle(wandTip,ring(length,radius,i),ring(length,radius,i+1),0xff8edbff);}
 auto quad=[&](Vec a,Vec b,Vec c,Vec d,DWORD colour){triangle(a,b,c,colour);triangle(a,c,d,colour);};
 auto line=[&](Vec a,Vec b,float width,DWORD colour){Vec dir=hpvr::sub(b,a);Vec normal=cross(dir,forward);float n=std::sqrt(hpvr::dot(normal,normal));if(n<.001f)return;normal=hpvr::scale(normal,width/n);quad(hpvr::add(a,normal),hpvr::sub(a,normal),hpvr::sub(b,normal),hpvr::add(b,normal),colour);};
 if(carry&&throwAimValid&&wandValid&&!worldSceneActive){
  float distance=std::sqrt(hpvr::dot(hpvr::sub(throwPoint,origin),hpvr::sub(throwPoint,origin)));float size=std::max(.4f,std::min(3.5f,distance*.007f));
  for(Vec axis:{right,down}){line(hpvr::sub(throwPoint,hpvr::scale(axis,size)),hpvr::add(throwPoint,hpvr::scale(axis,size)),size*.18f,0xff15202b);line(hpvr::sub(throwPoint,hpvr::scale(axis,size*.85f)),hpvr::add(throwPoint,hpvr::scale(axis,size*.85f)),size*.07f,0xfff8eeac);}
  static unsigned count=0;if(++count<=2)log("{\"event\":\"throw_crosshair\",\"eye\":%u,\"stereo\":true,\"wand_hidden\":true}",eye);
 }
 if(broom&&!worldSceneActive&&!cinemaActive){
  for(int i=0;i<2;++i)if(nearbyFlight[i]){
   Vec p=hpvr::add(origin,hpvr::add(hpvr::scale(forward,55),hpvr::scale(right,i?28:-28)));
   Vec tip=hpvr::add(p,hpvr::scale(right,i?2:-2));
   triangle(tip,hpvr::add(p,hpvr::scale(down,1.6f)),hpvr::sub(p,hpvr::scale(down,1.6f)),0xffffcc55);
  }
 }
 if(rain){
  float elapsed=(GetTickCount64()-rewardRainStarted)*.001f;Vec centre=rewardRainCentre;centre.z+=mem<float>(vrPlayer,eyeHeightOffset);
  for(int i=0;i<28;++i){float birth=i*.045f,age=elapsed-birth;if(age<0||age>3.6f)continue;
   float angle=i*2.39996323f,radius=28+(i%5)*9.0f;Vec p=hpvr::add(centre,Vec{std::cos(angle)*radius,std::sin(angle)*radius,100-age*48});
   Vec u={0,0,1},r={-std::sin(angle),std::cos(angle),0};auto point=[&](float x,float y){return hpvr::add(p,hpvr::add(hpvr::scale(r,x),hpvr::scale(u,y)));};
   DWORD colour=rewardRainBronze?0xffffc43d:0xffffeea1;
   if(rewardRainBronze){triangle(point(0,7),point(-4,0),point(1,1),colour);triangle(point(-1,-1),point(4,0),point(0,-7),colour);quad(point(-1,1),point(1,1),point(1,-1),point(-1,-1),colour);}
   else {for(int n=0;n<12;++n){float a=n*hpvr::pi/6,b=(n+1)*hpvr::pi/6;line(point(std::cos(a)*2.5f,4+std::sin(a)*2.5f),point(std::cos(b)*2.5f,4+std::sin(b)*2.5f),.45f,colour);}line(point(0,1.5f),point(0,-6),.55f,colour);line(point(0,-5.4f),point(2,-5.4f),.5f,colour);line(point(0,-3.5f),point(2,-3.5f),.5f,colour);}
  }
 }
 if(vertices.empty())return;
 DWORD state=0;hr(device->CreateStateBlock(D3DSBT_ALL,&state),"Save wand render state");
 try{
  hr(device->CaptureStateBlock(state),"Capture wand render state");
  hr(device->SetTexture(0,nullptr),"Wand texture");
  hr(device->SetTextureStageState(0,D3DTSS_COLOROP,D3DTOP_SELECTARG1),"Wand color op");
  hr(device->SetTextureStageState(0,D3DTSS_COLORARG1,D3DTA_DIFFUSE),"Wand color");
  hr(device->SetTextureStageState(0,D3DTSS_ALPHAOP,D3DTOP_SELECTARG1),"Wand alpha op");
  hr(device->SetTextureStageState(0,D3DTSS_ALPHAARG1,D3DTA_DIFFUSE),"Wand alpha");
  hr(device->SetTextureStageState(1,D3DTSS_COLOROP,D3DTOP_DISABLE),"Wand texture stage");
  for(auto setting:{std::pair<D3DRENDERSTATETYPE,DWORD>{D3DRENDERSTATE_ZWRITEENABLE,FALSE},{D3DRENDERSTATE_ZFUNC,D3DCMP_LESSEQUAL},{D3DRENDERSTATE_ALPHABLENDENABLE,FALSE},{D3DRENDERSTATE_ALPHATESTENABLE,FALSE},{D3DRENDERSTATE_FOGENABLE,FALSE},{D3DRENDERSTATE_LIGHTING,FALSE},{D3DRENDERSTATE_CULLMODE,D3DCULL_NONE},{D3DRENDERSTATE_FILLMODE,D3DFILL_SOLID},{D3DRENDERSTATE_STENCILENABLE,FALSE}})hr(device->SetRenderState(setting.first,setting.second),"Wand render state");
  hr(device->DrawPrimitive(D3DPT_TRIANGLELIST,D3DFVF_XYZRHW|D3DFVF_DIFFUSE,vertices.data(),DWORD(vertices.size()),0),"Draw tracked wand");
  hr(device->ApplyStateBlock(state),"Restore wand render state");hr(device->DeleteStateBlock(state),"Release wand state");state=0;
  ++wandDraws;
  if(wandDraws<=2)log("{\"event\":\"wand_draw\",\"eye\":%u,\"vertices\":%u,\"base\":[%.5f,%.5f,%.5f],\"tip\":[%.5f,%.5f,%.5f]}",eye,unsigned(vertices.size()),wandBase.x,wandBase.y,wandBase.z,wandTip.x,wandTip.y,wandTip.z);
 }catch(...){if(state){device->ApplyStateBlock(state);device->DeleteStateBlock(state);}throw;}
}
