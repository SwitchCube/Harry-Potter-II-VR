#pragma once
// Fingerprinted original ALAudio interfaces. PCM stream chunks are doubled
// consistently at the seven reviewed sites: about 4.5s instead of 2.2s reserve
// for 22kHz mono voices. Still three buffers, same decoder, pitch and cue timing.
static void configureAudioBuffers(){
 struct Patch {unsigned rva,size;BYTE before[7],after[7];};
 static const Patch patches[]={
  {0x3810,6,{0x81,0xe2,0xff,0x7f,0,0},{0x81,0xe2,0xff,0xff,0,0}},
  {0x3818,3,{0xc1,0xf8,0x0f},{0xc1,0xf8,0x10}},
  {0x3851,3,{0xc1,0xe0,0x0f},{0xc1,0xe0,0x10}},
  {0x385c,7,{0xc7,0x45,0xd8,0,0x80,0,0},{0xc7,0x45,0xd8,0,0,1,0}},
  {0x387f,5,{0x68,0,0x80,0,0},{0x68,0,0,1,0}},
  {0x38db,5,{0x68,0,0x80,0,0},{0x68,0,0,1,0}},
  {0x3a68,7,{0xc7,0x47,0x30,0,0x80,0,0},{0xc7,0x47,0x30,0,0,1,0}}
 };
 BYTE* module=reinterpret_cast<BYTE*>(GetModuleHandleW(L"ALAudio.dll"));need(module!=nullptr,"Missing pinned ALAudio");
 for(const auto& p:patches)need(!std::memcmp(module+p.rva,p.before,p.size),"Audio buffer instruction differs");
 bool stock=startup.mode==5&&profileFlag(L"replay-stock-audio.flag");
 if(!stock)for(const auto& p:patches){DWORD old=0,unused=0;need(VirtualProtect(module+p.rva,p.size,PAGE_EXECUTE_READWRITE,&old)!=0,"Audio patch protection");std::memcpy(module+p.rva,p.after,p.size);need(VirtualProtect(module+p.rva,p.size,old,&unused)!=0,"Restore audio instruction protection");need(FlushInstructionCache(GetCurrentProcess(),module+p.rva,p.size)!=0,"Flush audio instructions");}
 log("{\"event\":\"audio_buffer_size\",\"pcm_chunk_bytes\":%u,\"buffers\":3,\"original_decoder\":true}",stock?32768u:65536u);
}
using AudioPlay=int(__thiscall*)(void*,void*,int,void*,Vec,float,float,float,int,float,float);
using AudioStop=void(__thiscall*)(void*,int);
using AudioUpdate=void(__thiscall*)(void*,void*);
static AudioPlay originalAudioPlay=nullptr;static AudioStop originalAudioStop=nullptr;static AudioUpdate originalAudioUpdate=nullptr;
struct DialogueVoice {std::wstring name;ULONGLONG started;float duration;bool observed=false,underrun=false;};
static std::map<void*,DialogueVoice> dialogueVoices;
static bool dialogueSound(void* sound){
 for(void* o=sound;o;o=mem<void*>(o,0x18))if(objectName(o)==L"AllDialog")return true;
 return false;
}
static int __fastcall audioPlayHook(void* self,void*,void* actor,int id,void* sound,Vec position,float volume,float radius,float pitch,int flags,float fadeIn,float fadeOut){
 FpuState fpu;bool dialogue=sound&&dialogueSound(sound);float duration=0;
 if(dialogue){using Duration=float(__thiscall*)(void*);fpu.restore();duration=reinterpret_cast<Duration>(targets.at("soundduration"))(sound);}
 fpu.restore();int result=originalAudioPlay(self,actor,id,sound,position,volume,radius,pitch,flags,fadeIn,fadeOut);
 if(dialogue&&result){DialogueVoice v;v.name=objectName(sound);v.started=GetTickCount64();v.duration=duration;dialogueVoices[sound]=v;log("{\"event\":\"dialogue_start\",\"sound\":\"%ls\",\"time_ms\":%llu,\"duration\":%.6g,\"pitch\":%.6g,\"accepted\":%s}",objectName(sound).c_str(),GetTickCount64(),duration,pitch,result?"true":"false");}
 fpu.restore();return result;
}
static void __fastcall audioStopHook(void* self,void*,int index){
 FpuState fpu;int count=mem<int>(self,0x74);void* sources=mem<void*>(self,0x70);
 if(sources&&index>=0&&index<count&&count<1024){void* sound=mem<void*>(sources,index*92);auto it=dialogueVoices.find(sound);
  if(it!=dialogueVoices.end()){auto& v=it->second;log("{\"event\":\"dialogue_stop\",\"sound\":\"%ls\",\"elapsed_ms\":%llu,\"duration\":%.6g,\"loading\":%s}",v.name.c_str(),GetTickCount64()-v.started,v.duration,levelLoadDepth?"true":"false");dialogueVoices.erase(it);}
 }
 fpu.restore();originalAudioStop(self,index);
}
static void __fastcall audioUpdateHook(void* self,void*,void* frame){
 FpuState fpu;
 using GetSource=void(__cdecl*)(unsigned,int,int*);static GetSource getSource=reinterpret_cast<GetSource>(GetProcAddress(GetModuleHandleW(L"OpenAL32.dll"),"alGetSourcei"));
 if(startup.mode==5&&profileFlag(L"replay-audio-stall.flag")){static bool stalled=false;for(const auto& pair:dialogueVoices){const auto& voice=pair.second;if(!stalled&&voice.observed&&voice.duration>5&&GetTickCount64()-voice.started>1000){stalled=true;log("{\"event\":\"audio_stall_fixture\",\"ms\":2500,\"sound\":\"%ls\"}",voice.name.c_str());Sleep(2500);break;}}}
 if(getSource){int count=mem<int>(self,0x74);void* sources=mem<void*>(self,0x70);if(sources&&count>=0&&count<=1024)for(int i=0;i<count;++i){auto it=dialogueVoices.find(mem<void*>(sources,i*92));if(it==dialogueVoices.end())continue;
  unsigned source=mem<unsigned>(sources,i*92+4);if(!source)continue;int state=0,queued=0,processed=0;
  getSource(source,0x1010,&state);getSource(source,0x1015,&queued);getSource(source,0x1016,&processed);
  auto& v=it->second;bool early=GetTickCount64()-v.started+100<ULONGLONG(v.duration*1000);
  if(state==0x1014&&queued>0&&processed==queued&&early&&!v.underrun){v.underrun=true;log("{\"event\":\"dialogue_buffer_underrun\",\"sound\":\"%ls\",\"queued\":%d,\"processed\":%d,\"elapsed_ms\":%llu}",v.name.c_str(),queued,processed,GetTickCount64()-v.started);}
 }}
 static ULONGLONG previous=0;ULONGLONG now=GetTickCount64();
 if(previous&&now-previous>150&&!dialogueVoices.empty())log("{\"event\":\"dialogue_update_gap\",\"ms\":%llu,\"loading\":%s}",now-previous,levelLoadDepth?"true":"false");previous=now;
 fpu.restore();originalAudioUpdate(self,frame);
 using GetFloat=void(__cdecl*)(unsigned,int,float*);using GetListener=void(__cdecl*)(int,float*);
 static GetFloat sourceFloat=reinterpret_cast<GetFloat>(GetProcAddress(GetModuleHandleW(L"OpenAL32.dll"),"alGetSourcef"));
 static GetFloat sourceVector=reinterpret_cast<GetFloat>(GetProcAddress(GetModuleHandleW(L"OpenAL32.dll"),"alGetSourcefv"));
 static GetListener listenerVector=reinterpret_cast<GetListener>(GetProcAddress(GetModuleHandleW(L"OpenAL32.dll"),"alGetListenerfv"));
 static ULONGLONG sampled=0;bool sample=nativeCapturesEnabled()&&sourceFloat&&sourceVector&&listenerVector&&GetTickCount64()-sampled>=200;
 if(sample)sampled=GetTickCount64();
 int count=mem<int>(self,0x74);void* sources=mem<void*>(self,0x70);
 if(!sources||count<0||count>1024){fpu.restore();return;}
 for(auto it=dialogueVoices.begin();it!=dialogueVoices.end();){bool found=false;unsigned sourceId=0;
  for(int i=0;i<count;++i)if(mem<void*>(sources,i*92)==it->first){found=true;sourceId=mem<unsigned>(sources,i*92+4);break;}
  if(found&&sample){float pitch=0,gain=0,listener[3]={},velocity[3]={};sourceFloat(sourceId,0x1003,&pitch);sourceFloat(sourceId,0x100a,&gain);sourceVector(sourceId,0x1006,velocity);listenerVector(0x1006,listener);
   log("{\"event\":\"dialogue_sample\",\"sound\":\"%ls\",\"elapsed_ms\":%llu,\"pitch\":%.6g,\"gain\":%.6g,\"listener_velocity\":[%.6g,%.6g,%.6g],\"source_velocity\":[%.6g,%.6g,%.6g]}",it->second.name.c_str(),GetTickCount64()-it->second.started,pitch,gain,listener[0],listener[1],listener[2],velocity[0],velocity[1],velocity[2]);}
  if(found&&!it->second.observed){it->second.observed=true;log("{\"event\":\"dialogue_source\",\"sound\":\"%ls\",\"source\":%u}",it->second.name.c_str(),sourceId);}
  if(!found&&it->second.observed){log("{\"event\":\"dialogue_finished\",\"sound\":\"%ls\",\"elapsed_ms\":%llu,\"duration\":%.6g}",it->second.name.c_str(),GetTickCount64()-it->second.started,it->second.duration);it=dialogueVoices.erase(it);}else ++it;
 }

 fpu.restore();
}
