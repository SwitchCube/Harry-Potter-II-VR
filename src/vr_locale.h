#pragma once
// Mod language only; native game resources and locale remain unchanged.
static bool vrEnglish=false,vrAutoEnglish=false;
static int vrLanguageChoice=0;static unsigned vrLocaleRevision=0;
static void vrLoadLocale(const std::wstring& root=L""){
 wchar_t value[32]={};GetEnvironmentVariableW(L"HP2VR_LANGUAGE",value,32);
 if(!*value&&!root.empty())GetPrivateProfileStringW(L"Engine.Engine",L"Language",L"int",value,32,(root+L"\\system\\Default.ini").c_str());
 vrAutoEnglish=_wcsicmp(value,L"de")&&_wcsicmp(value,L"ger");
 vrLanguageChoice=root.empty()?0:int(GetPrivateProfileIntW(L"VR",L"Language",0,(root+L"\\config\\vr-preferences.ini").c_str()));
 if(vrLanguageChoice<0||vrLanguageChoice>2)vrLanguageChoice=0;
 vrEnglish=vrLanguageChoice==2||(vrLanguageChoice==0&&vrAutoEnglish);++vrLocaleRevision;
}
static const wchar_t* tr(const wchar_t* de,const wchar_t* en){return vrEnglish?en:de;}
