#pragma once
// HP2's reflected script ABI; function arguments may start at offset zero.
static void* vrScriptFunction(void* object,const wchar_t* key){
 using Make=void(__thiscall*)(void*,const wchar_t*,int);using Find=void*(__thiscall*)(void*,void*,int);
 void* name=nullptr;reinterpret_cast<Make>(targets.at("name"))(&name,key,0);void* fn=reinterpret_cast<Find>(targets.at("field"))(object,name,0);
 need(fn&&objectName(mem<void*>(fn,0x24))==L"Function","HP2 script function missing");return fn;
}
static unsigned vrParameter(void* fn,const wchar_t* key,unsigned size=0){
 using Find=void*(__cdecl*)(void*,const wchar_t*);void* p=reinterpret_cast<Find>(targets.at("property"))(fn,key);
 need(p&&(!size||mem<DWORD>(p,0x38)==size),"HP2 script parameter type/size differs");unsigned offset=mem<DWORD>(p,0x48);need(offset<500,"HP2 script parameter offset");return offset;
}
static void vrScriptCall(void* object,const wchar_t* key,void* params){FpuState fpu;fpu.restore();originalDispatch(object,vrScriptFunction(object,key),params,nullptr);fpu.restore();}
static std::wstring vrString(void* object,unsigned offset){auto p=mem<wchar_t*>(object,offset);int n=mem<int>(object,offset+4),capacity=mem<int>(object,offset+8);need(n>=0&&n<4096&&capacity>=n,"HP2 string extent");if(!n)return L"";need(p&&p[n-1]==0,"HP2 string terminator");return std::wstring(p,n-1);}
static void vrAssignString(void* value,const wchar_t* text){using Assign=void*(__thiscall*)(void*,const wchar_t*);auto f=reinterpret_cast<Assign>(GetProcAddress(GetModuleHandleW(L"Core.dll"),"??4FString@@QAEAAV0@PBG@Z"));need(f,"HP2 string assignment missing");f(value,text);}
static void vrScriptString(void* object,const wchar_t* key,const wchar_t* text){
 using Make=void(__thiscall*)(void*,const wchar_t*);using Delete=void(__thiscall*)(void*);auto core=GetModuleHandleW(L"Core.dll");auto make=reinterpret_cast<Make>(GetProcAddress(core,"??0FString@@QAE@PBG@Z"));auto destroy=reinterpret_cast<Delete>(GetProcAddress(core,"??1FString@@QAE@XZ"));need(make&&destroy,"HP2 string ABI missing");DWORD arg[3]={};make(arg,text);vrScriptCall(object,key,arg);destroy(arg);
}
