#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <algorithm>
#include <vector>
#include <string>
#include <stdexcept>
#include <cstring>
#include <cstdio>
#include "vr_math.h"
#include "vr_bindings.h"
#include "vr_settings.h"
static VrSettings vrSettings;
#include "vr_preferences.h"
static void require(bool ok,const char* reason){if(!ok)throw std::runtime_error(reason);}
int wmain(int argc,wchar_t** argv){try{
 require(argc==2,"Private fixture directory required");std::wstring root=argv[1];CreateDirectoryW(root.c_str(),nullptr);CreateDirectoryW((root+L"\\config").c_str(),nullptr);CreateDirectoryW((root+L"\\system").c_str(),nullptr);
 SetEnvironmentVariableW(L"HP2VR_LANGUAGE",nullptr);auto game=root+L"\\system\\Default.ini";
 WritePrivateProfileStringW(L"Engine.Engine",L"Language",L"ger",game.c_str());vrLoadLocale(root);require(!vrEnglish,"Installed German language");
 WritePrivateProfileStringW(L"Engine.Engine",L"Language",L"int",game.c_str());vrLoadLocale(root);require(vrEnglish,"Installed English language");
 vrModPreferences.load(root);vrLanguageChoice=1;vrModPreferences.renderSize=1024;vrModPreferences.save();vrLoadLocale(root);vrModPreferences.load(root);require(!vrEnglish&&vrLanguageChoice==1&&vrSettings.renderSize==1024,"German override and next-launch resolution");
 vrLanguageChoice=2;vrModPreferences.renderSize=1536;vrModPreferences.save();vrLoadLocale(root);vrModPreferences.load(root);require(vrEnglish&&vrSettings.renderSize==1536,"English override and resolution");
 vrLanguageChoice=0;vrModPreferences.save();SetEnvironmentVariableW(L"HP2VR_LANGUAGE",L"de");vrLoadLocale(root);require(!vrEnglish,"Auto installed language from portable launcher");
 std::puts("HP2 preferences: automatic DE/EN, manual overrides and next-launch resolution passed");return 0;
 }catch(const std::exception& e){std::fprintf(stderr,"%s\n",e.what());return 1;}}
