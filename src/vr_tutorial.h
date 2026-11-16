#pragma once
#include "vr_tutorial_catalogue.h"
#include "vr_tutorial_catalogue_en.h"
// HP2 uses independent Localize + DynamicLoadObject calls in actor/lesson scripts.
static const Hp2TutorialLine* tutorialLine=nullptr;
static ULONGLONG tutorialStarted=0,tutorialEnds=0,tutorialFade=0;
static unsigned tutorialSerial=0,tutorialReplacements=0;
using TutorialDuration=float(__thiscall*)(void*);
static TutorialDuration originalTutorialDuration=nullptr;
static bool tutorialArmed[2]={};
static std::map<void*,const Hp2TutorialLine*> tutorialSounds;
static std::map<std::string,unsigned> tutorialLessonSeen;
static void resetTutorial(){tutorialLine=nullptr;tutorialStarted=tutorialEnds=tutorialFade=0;tutorialArmed[0]=tutorialArmed[1]=false;}
static void beginTutorial(const Hp2TutorialLine& line,float seconds){
 if(*line.group){auto it=tutorialLessonSeen.find(line.group);if(it!=tutorialLessonSeen.end()&&it->second==levelGeneration)return;tutorialLessonSeen[line.group]=levelGeneration;}
 tutorialLine=&line;tutorialStarted=GetTickCount64();tutorialEnds=tutorialStarted+ULONGLONG(std::max(7.f,std::min(35.f,seconds+6))*1000);tutorialFade=0;tutorialArmed[0]=tutorialArmed[1]=false;++tutorialSerial;
 log("{\"event\":\"hp2_tutorial_hint\",\"id\":\"%ls\",\"serial\":%u,\"action\":\"%s\",\"second_action\":\"%s\"}",line.id,tutorialSerial,line.actions[0],line.actions[1]);
}
static bool tutorialActionDown(const char* key){
 if(!std::strcmp(key,"move"))return std::hypot(vrInput.move.x,vrInput.move.y)>.35f;
 if(!std::strcmp(key,"turn"))return std::hypot(vrInput.turn.x,vrInput.turn.y)>.35f;
 const char* keys[]={"jump","cast","menu","recenter","confirm","potion","map","boost","quicksave","show_health"};
 const bool values[]={vrInput.jump,vrInput.cast,vrInput.menu,vrInput.recenter,vrInput.confirm,vrInput.potion,vrInput.map,vrInput.boost,vrInput.quicksave,vrInput.showHealth};
 for(unsigned i=0;i<10;++i)if(!std::strcmp(key,keys[i]))return values[i];return false;
}
static float tutorialAlpha(bool ready,bool canAct){
 if(!tutorialLine)return 0;auto now=GetTickCount64();if(now>=tutorialEnds){resetTutorial();return 0;}if(!ready)return 0;
 for(unsigned i=0;i<2;++i)if(*tutorialLine->actions[i]){bool down=tutorialActionDown(tutorialLine->actions[i]);if(!canAct)tutorialArmed[i]=false;else if(!down)tutorialArmed[i]=true;
  if(canAct&&tutorialArmed[i]&&down&&now>tutorialStarted+1800&&!tutorialFade)tutorialFade=now;
 }
 float alpha=std::min(1.f,float(now-tutorialStarted)/250);if(tutorialFade)alpha*=std::max(0.f,1-float(now-tutorialFade)/550);else alpha*=std::min(1.f,float(tutorialEnds-now)/550);
 if(tutorialFade&&now>=tutorialFade+550){resetTutorial();return 0;}return alpha;
}
static void* tutorialSound(const Hp2TutorialLine& line){
 for(auto& pair:tutorialSounds)if(pair.second==&line)return pair.first;
 using Class=void*(__cdecl*)();using Load=void*(__cdecl*)(void*,void*,const wchar_t*,const wchar_t*,DWORD,void*);using Root=void(__thiscall*)(void*);
 auto core=GetModuleHandleW(L"Core.dll");auto cls=reinterpret_cast<Class>(GetProcAddress(GetModuleHandleW(L"Engine.dll"),"?StaticClass@USound@@SAPAVUClass@@XZ"));auto load=reinterpret_cast<Load>(GetProcAddress(core,"?StaticLoadObject@UObject@@SAPAV1@PAVUClass@@PAV1@PBG2KPAVUPackageMap@@@Z"));auto root=reinterpret_cast<Root>(GetProcAddress(core,"?AddToRoot@UObject@@QAEXXZ"));if(!cls||!load||!root)return nullptr;
 auto path=std::wstring(startup.root)+L"\\assets\\tutorial\\"+(std::wcsncmp(line.sound,L"HP2VRTutorialEN.",16)==0?L"HP2VRTutorialEN.uax":L"HP2VRTutorial.uax");if(GetFileAttributesW(path.c_str())==INVALID_FILE_ATTRIBUTES)return nullptr;
 FpuState fpu;fpu.restore();auto sound=load(cls(),nullptr,line.sound,path.c_str(),0,nullptr);if(sound){using Duration=float(__thiscall*)(void*);float seconds=(originalTutorialDuration?originalTutorialDuration:reinterpret_cast<Duration>(targets.at("soundduration")))(sound);need(std::isfinite(seconds)&&std::fabs(seconds-line.seconds)<.08f,"HP2 tutorial duration differs from PCM");root(sound);tutorialSounds[sound]=&line;}fpu.restore();return sound;
}
static bool tutorialIsPcm(void* sound){return tutorialSounds.find(sound)!=tutorialSounds.end();}
static void tutorialPlayed(void* sound){auto it=tutorialSounds.find(sound);if(it!=tutorialSounds.end())beginTutorial(*it->second,it->second->seconds);}
static const Hp2TutorialLine* tutorialFind(const wchar_t* id){
 if(!id)return nullptr;auto catalogue=vrEnglish?tutorialLinesEn:tutorialLines;for(unsigned i=0;i<sizeof(tutorialLines)/sizeof(tutorialLines[0]);++i)if(!_wcsicmp(id,catalogue[i].id))return &catalogue[i];return nullptr;
}
// Cutscene commands also hold preloaded Sound references. Resolve the same exact
// catalogue for playback AND GetDuration so the stock scene scheduler waits for
// the complete replacement; no shared Sound metadata is mutated.
static void* tutorialResolveSound(void* sound){
 if(!sound||startup.mode<3||startup.mode>5)return sound;
 bool original=false;for(void* o=mem<void*>(sound,0x18);o;o=mem<void*>(o,0x18))if(objectName(o)==L"AllDialog"){original=true;break;}
 if(!original)return sound;auto name=objectName(sound);auto line=tutorialFind(name.c_str());if(!line)return sound;
 try{auto replacement=tutorialSound(*line);if(replacement)return replacement;}catch(const std::exception& e){log("{\"event\":\"hp2_tutorial_cached_fallback\",\"reason\":\"%s\"}",e.what());}return sound;
}
static float __fastcall tutorialDurationHook(void* sound,void*){FpuState fpu;auto replacement=tutorialResolveSound(sound);fpu.restore();return originalTutorialDuration(replacement);}
using TutorialLoad=void*(__cdecl*)(void*,void*,const wchar_t*,const wchar_t*,DWORD,void*);
using TutorialLocalize=const wchar_t*(__cdecl*)(const wchar_t*,const wchar_t*,const wchar_t*,const wchar_t*,int);
static TutorialLoad originalTutorialLoad=nullptr;static TutorialLocalize originalTutorialLocalize=nullptr;
static void* __cdecl tutorialLoadHook(void* cls,void* outer,const wchar_t* name,const wchar_t* file,DWORD flags,void* map){
 // Restrict replacement to the exact original dialog package and catalogue IDs.
 if(startup.mode>=3&&startup.mode<=5&&name&&!_wcsnicmp(name,L"AllDialog.",10)&&cls&&objectName(cls)==L"Sound"){
  auto line=tutorialFind(name+10);if(line){try{auto sound=tutorialSound(*line);if(sound){++tutorialReplacements;log("{\"event\":\"hp2_tutorial_replaced\",\"id\":\"%ls\",\"english\":%s,\"seconds\":%.6g,\"native_sound\":true}",line->id,vrEnglish?"true":"false",line->seconds);return sound;}}catch(const std::exception& e){log("{\"event\":\"hp2_tutorial_original_fallback\",\"reason\":\"%s\"}",e.what());}}
 }
 return originalTutorialLoad(cls,outer,name,file,flags,map);
}
static const wchar_t* __cdecl tutorialLocalizeHook(const wchar_t* section,const wchar_t* key,const wchar_t* package,const wchar_t* language,int quiet){
 if(startup.mode>=3&&startup.mode<=5&&section&&package&&!_wcsicmp(section,L"all")&&!_wcsicmp(package,L"HPdialog")){
  auto line=tutorialFind(key);if(line){try{if(tutorialSound(*line))return line->text;}catch(const std::exception& e){log("{\"event\":\"hp2_tutorial_text_fallback\",\"reason\":\"%s\"}",e.what());}}
 }
 return originalTutorialLocalize(section,key,package,language,quiet);
}
static void closeTutorialSounds(){using Root=void(__thiscall*)(void*);auto remove=reinterpret_cast<Root>(GetProcAddress(GetModuleHandleW(L"Core.dll"),"?RemoveFromRoot@UObject@@QAEXXZ"));if(remove)for(auto& pair:tutorialSounds)remove(pair.first);tutorialSounds.clear();tutorialLessonSeen.clear();resetTutorial();}
