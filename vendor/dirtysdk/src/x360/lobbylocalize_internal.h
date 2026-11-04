#pragma once
#include "types.hpp"

enum DateFormat_E { _DF_YMD = 0, _DF_DMY = 1, _DF_MDY = 2 };
struct LangMapT
{
    s32 iLang;
    char cDecSymbol;
    char cThouSep;
    DateFormat_E eDateFormat;
    char cDateSep;
    char cTimeSep;
    s32 bUse24Hour;
    s32 bSignEnd;
    char strAM[6];
    char strPM[6];
    s32 bCurrSpace;
    s32 bPercentInFront;
    s32 bPercentSpace;
    s32 bCurrEnd;
};
typedef s32 LobbyLocalizeFuncT(s32, char*, s32, const char*, void*);
struct LobbyLocalizeT
{
    s32 iMemGroup;
    s16 iCountry;
    s16 iLanguage;
    s32 iCurrency;
    LangMapT LangRules;
    LobbyLocalizeFuncT* pCallback;
    void* pUserData;
};
extern "C" LobbyLocalizeT* LobbyLocalizeCreate(u32 uLocality, s32 iCurrency);
extern "C" LobbyLocalizeT* LobbyLocalizeDestroy(LobbyLocalizeT* pLocalize);
extern "C" s32 LobbyLocalizeData(LobbyLocalizeT* pLocalize, s32 iType, char* pBuffer, s32 iBufLen, const char* pSource);
