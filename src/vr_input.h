#pragma once
struct VrInput {
 VRActionSetHandle_t set=0;std::map<std::string,VRActionHandle_t> actions;
 InputAnalogActionData_t move={},turn={};InputPoseActionData_t left={},right={},leftGrip={},rightGrip={};
 bool raw[VrBindings::Count]={};
 bool jump=false,cast=false,menu=false,recenter=false,confirm=false,potion=false,walk=false,map=false,boost=false,quicksave=false,showHealth=false;
 static void check(EVRInputError r,const char* s){if(r!=EVRInputError_VRInputError_None)throw std::runtime_error(std::string(s)+": "+std::to_string(r));}
 void start(VrRuntime& r,const std::wstring& manifest){
  int n=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,manifest.c_str(),-1,nullptr,0,nullptr,nullptr);if(n<2)throw std::runtime_error("Invalid action manifest path");
  std::vector<char> p(n);WideCharToMultiByte(CP_UTF8,0,manifest.c_str(),-1,p.data(),n,nullptr,nullptr);
  check(r.input->SetActionManifestPath(p.data()),"Action manifest");std::string path="/actions/game";check(r.input->GetActionSetHandle(&path[0],&set),"Action set");
  for(auto name:{"move","turn","jump","cast","menu","recenter","confirm","potion","walk","map","boost","quicksave","show_health","left_hand","right_hand","left_grip","right_grip"}){path=std::string("/actions/game/in/")+name;VRActionHandle_t h=0;check(r.input->GetActionHandle(&path[0],&h),name);actions[name]=h;}
  path="/actions/game/out/haptic";VRActionHandle_t h=0;check(r.input->GetActionHandle(&path[0],&h),"haptic");actions["haptic"]=h;
  for(auto side:{"haptic_left","haptic_right"}){path=std::string("/actions/game/out/")+side;check(r.input->GetActionHandle(&path[0],&h),side);actions[side]=h;}
 }
 EVRInputError pulse(VrRuntime& r,float amplitude,float seconds){return r.initialized&&amplitude>0?r.input->TriggerHapticVibrationAction(actions.at("haptic"),0,seconds,90,amplitude,0):EVRInputError_VRInputError_None;}
 EVRInputError pulseSide(VrRuntime& r,bool leftSide,float amplitude,float seconds){return r.initialized&&amplitude>0?r.input->TriggerHapticVibrationAction(actions.at(leftSide?"haptic_left":"haptic_right"),0,seconds,70,amplitude,0):EVRInputError_VRInputError_None;}
 bool button(VrRuntime& r,const char* name){InputDigitalActionData_t d={};check(r.input->GetDigitalActionData(actions.at(name),&d,sizeof(d),0),name);return d.bActive&&d.bState;}
 void clear(){std::fill(raw,raw+VrBindings::Count,false);move={};turn={};left={};right={};leftGrip={};rightGrip={};jump=cast=menu=recenter=confirm=potion=walk=map=boost=quicksave=showHealth=false;}
 bool poll(VrRuntime& r){
  clear();if(!r.initialized||!r.compositor->CanRenderScene())return false;
  VRActiveActionSet_t s={};s.ulActionSet=set;check(r.input->UpdateActionState(&s,sizeof(s),1),"Update actions");
  check(r.input->GetAnalogActionData(actions.at("move"),&move,sizeof(move),0),"Move");check(r.input->GetAnalogActionData(actions.at("turn"),&turn,sizeof(turn),0),"Turn");
  check(r.input->GetPoseActionDataRelativeToNow(actions.at("left_hand"),ETrackingUniverseOrigin_TrackingUniverseStanding,0,&left,sizeof(left),0),"Left hand");
  check(r.input->GetPoseActionDataRelativeToNow(actions.at("right_hand"),ETrackingUniverseOrigin_TrackingUniverseStanding,0,&right,sizeof(right),0),"Right hand");
  check(r.input->GetPoseActionDataRelativeToNow(actions.at("left_grip"),ETrackingUniverseOrigin_TrackingUniverseStanding,0,&leftGrip,sizeof(leftGrip),0),"Left grip");
  check(r.input->GetPoseActionDataRelativeToNow(actions.at("right_grip"),ETrackingUniverseOrigin_TrackingUniverseStanding,0,&rightGrip,sizeof(rightGrip),0),"Right grip");
  const char* rawNames[]={"jump","cast","menu","recenter","confirm","potion","map","boost","quicksave","show_health"};
  for(int i=0;i<VrBindings::Count;++i)raw[i]=button(r,rawNames[i]);
  bool* destinations[]={&jump,&cast,&menu,&recenter,&confirm,&potion,&map,&boost,&quicksave,&showHealth};
  for(int i=0;i<VrBindings::Count;++i)*destinations[i]=raw[vrBindings.source[i]];
  walk=button(r,"walk");if(vrBindings.swapSticks)std::swap(move,turn);
  return true;
 }
};
