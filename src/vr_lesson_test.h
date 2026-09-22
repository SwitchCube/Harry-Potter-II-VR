#pragma once
// Isolated original-map integration fixture, never active in normal VR.
static bool lessonFixtureActivated=false;
static void prepareLessonLevel(){
 if(startup.mode!=5||lessonFixtureActivated||!currentViewport||!profileFlag(L"replay-lesson.flag"))return;
 void* actor=mem<void*>(currentViewport,0x30);if(!actor)return;
 auto array=GetProcAddress(GetModuleHandleW(L"Core.dll"),"?GObjObjects@UObject@@0V?$TArray@PAVUObject@@@@A");need(array!=nullptr,"Missing lesson object array");
 int count=mem<int>(reinterpret_cast<void*>(array),4);need(count>0&&count<1000000,"Lesson object count invalid");using Indexed=void*(__cdecl*)(int);
 unsigned stopped=0;for(int i=0;i<count;++i){void* o=reinterpret_cast<Indexed>(targets.at("indexedobject"))(i);if(!o||objectName(mem<void*>(o,0x24))!=L"CutScene"||mem<void*>(o,0x94)!=mem<void*>(actor,0x94)||!boolField(o,L"bPlaying"))continue;originalDispatch(o,climbFunction(o,L"ForceFinish",0),nullptr,nullptr);++stopped;}
 if(stopped)log("{\"event\":\"lesson_fixture_clear_story\",\"original_force_finish\":%u}",stopped);
}
static void replayLesson(){
 if(startup.mode!=5||!vrPlayer||!profileFlag(L"replay-lesson.flag")||controlsFrames<125)return;
 static ULONGLONG clearedAt=0;
 static void* target=nullptr;static bool activated=false;static unsigned seen=0,last=~0u;static Vec body={};
 if(clearedAt&&GetTickCount64()-clearedAt<1000)return;
 if(!activated){
  auto array=GetProcAddress(GetModuleHandleW(L"Core.dll"),"?GObjObjects@UObject@@0V?$TArray@PAVUObject@@@@A");need(array!=nullptr,"Missing lesson object array");
  int count=mem<int>(reinterpret_cast<void*>(array),4);need(count>0&&count<1000000,"Lesson object count invalid");using Indexed=void*(__cdecl*)(int);
  if(!clearedAt){unsigned stopped=0;for(int i=0;i<count;++i){void* o=reinterpret_cast<Indexed>(targets.at("indexedobject"))(i);if(!o||objectName(mem<void*>(o,0x24))!=L"CutScene"||mem<void*>(o,0x94)!=mem<void*>(vrPlayer,0x94)||!boolField(o,L"bPlaying"))continue;originalDispatch(o,climbFunction(o,L"ForceFinish",0),nullptr,nullptr);++stopped;}
   clearedAt=GetTickCount64();log("{\"event\":\"lesson_fixture_clear_story\",\"original_force_finish\":%u}",stopped);return;
  }
  for(int i=0;i<count;++i){void* o=reinterpret_cast<Indexed>(targets.at("indexedobject"))(i);if(!o||objectName(mem<void*>(o,0x24))!=L"SpellLessonTrigger"||mem<void*>(o,0x94)!=mem<void*>(vrPlayer,0x94))continue;
   BYTE shape=mem<BYTE>(o,field(o,L"LessonShape",1,L"ByteProperty"));log("{\"event\":\"lesson_fixture_candidate\",\"name\":\"%ls\",\"shape\":%u}",objectName(o).c_str(),shape);if(!target&&shape==0)target=o;
  }
  need(target!=nullptr,"No original Rictusempra lesson actor in diagnostic map");
  activated=true;lessonFixtureActivated=true;void* args[2]={vrPlayer,vrPlayer};originalDispatch(target,climbFunction(target,L"Activate",8),args,nullptr);body=mem<Vec>(vrPlayer,0x11c);
  log("{\"event\":\"lesson_fixture_started\",\"original_activate\":true,\"body\":[%.6g,%.6g,%.6g]}",body.x,body.y,body.z);return;
 }
 if(objectField(vrPlayer,L"CurrSpellLesson")!=target)return;
 unsigned offset=field(target,L"nKeyNeedsReset",4,L"IntProperty"),mask=0;
 for(unsigned i=0;i<4;++i)if(mem<int>(target,offset+4*i))mask|=1u<<i;
 if(mask!=last||controlsFrames%300==0){last=mask;Vec p=mem<Vec>(vrPlayer,0x11c);Vec d=hpvr::sub(p,body);float distance=std::sqrt(d.x*d.x+d.y*d.y+d.z*d.z);
  unsigned buttons=0;const wchar_t* names[]={L"bSpellLessonUp",L"bSpellLessonDown",L"bSpellLessonLeft",L"bSpellLessonRight"};for(unsigned i=0;i<4;++i)if(mem<BYTE>(vrPlayer,field(vrPlayer,names[i],1,L"ByteProperty")))buttons|=1u<<i;
  if(controlsFrames%300==0)log("{\"event\":\"lesson_button_bytes\",\"mask\":%u,\"expected\":%u}",buttons,lessonExpected);
  bool matches=mask==lessonExpected&&lessonWorldActive;if(mask&&matches){seen|=1u<<lessonTestPhase;static bool captured=false;if(!captured){lessonCaptureRequested=true;captured=true;}}
  log("{\"event\":\"lesson_original_input\",\"mask\":%u,\"expected\":%u,\"hand\":%u,\"cinema\":%s,\"body_distance\":%.6g,\"coverage\":%u}",mask,lessonExpected,lessonTestPhase/4,cinemaActive?"true":"false",distance,seen);
 }
}
