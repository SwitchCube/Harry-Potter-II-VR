#pragma once
// Language is selected from the installed game, never from a developer's PC.
// Keep identifiers and the original game's localized resources untouched.
static bool vrEnglish=false;
static void vrLoadLocale(){wchar_t value[16]={};GetEnvironmentVariableW(L"HP2VR_LANGUAGE",value,16);vrEnglish=!_wcsicmp(value,L"en");}
static const wchar_t* tr(const wchar_t* de,const wchar_t* en){return vrEnglish?en:de;}
