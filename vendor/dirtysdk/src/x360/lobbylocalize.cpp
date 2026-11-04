#include "lobbylocalize_internal.h"
#include "dirtymem.h"
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <ctime>
#include "platform.h"

// ARTIST 0x82F836F8, 16 native LangMapT records, stride 52.
static const LangMapT _LangMap[] = {
    {0x656e,'.',',',_DF_MDY,'/',':',0,0,"AM","PM",0,0,0,0},
    {0x6465,',','.',_DF_DMY,'.',':',1,0,"","",0,0,0,0},
    {0x6672,',',' ',_DF_DMY,'-',':',1,0,"","",1,0,1,1},
    {0x6669,',',' ',_DF_DMY,'-','.',1,0,"","",0,0,0,0},
    {0x6461,',',' ',_DF_DMY,'-','.',1,0,"","",0,0,0,0},
    {0x7376,',',' ',_DF_YMD,'-',':',1,0,"","",0,0,0,0},
    {0x6e6f,'.','.',_DF_MDY,'.',':',1,0,"","",0,0,0,0},
    {0x6573,',','.',_DF_DMY,'/',':',1,0,"","",0,0,0,0},
    {0x6974,',','.',_DF_DMY,'/',':',1,0,"","",0,0,0,0},
    {0x7074,',','.',_DF_DMY,'/',':',1,0,"","",0,0,0,0},
    {0x6875,',','.',_DF_DMY,'-',':',1,0,"","",0,0,0,0},
    {0x7275,',','.',_DF_DMY,'-',':',1,0,"","",0,0,0,0},
    {0x6373,',','.',_DF_DMY,'-',':',1,0,"","",0,0,0,0},
    {0x706c,',','.',_DF_DMY,'-',':',1,0,"","",0,0,0,0},
    {0x6e6c,',','.',_DF_DMY,'-',':',1,0,"","",0,0,0,0},
    {0x7a7a,'.',',',_DF_MDY,'/',':',1,0,"AM","PM",0,0,0,0}
};
struct CurrMapT { s32 iCountry; u16 uCurr; };
// ARTIST 0x82F83A38, native 8-byte records and ZZ fallback.
static const CurrMapT _CurrMap[] = {
    {0x5553,0x0024}, {0x4742,0x00a3}, {0x4341,0x0024},
    {0x4b52,0x20a9}, {0x434e,0x570e}, {0x4d58,0x20b1}, {0x5a5a,0x0024}
};

// ARTIST 0x82B34C50.
static u16 _LobbyLocalizePickCurrency(s32 iCountry, s32* pResult)
{
    *pResult = 0;
    switch (iCountry)
    {
    case 0x4652: case 0x4445: case 0x4e4c: case 0x4752:
    case 0x4649: case 0x4945: case 0x4954: case 0x5054:
    case 0x4553: case 0x4154: case 0x4c55: case 0x4245:
        return 0x20ac;
    }
    const CurrMapT* pEntry = _CurrMap;
    while (pEntry->iCountry != iCountry && pEntry->iCountry != 0x5a5a) ++pEntry;
    if (pEntry->iCountry == 0x5a5a) *pResult = -2;
    return pEntry->uCurr;
}

// ARTIST 0x82B351C0, called by LobbyLocalizeCreate.
static void _LobbyLocalizeSetLocale(LobbyLocalizeT* pLocalize, u32 uLocality, s32 iCurrency)
{
    if (!uLocality) uLocality = 0x656e5553; // enUS
    pLocalize->iCountry = static_cast<s16>(uLocality);
    pLocalize->iLanguage = static_cast<s16>(uLocality >> 16);
    if (!iCurrency)
    {
        s32 iResult;
        iCurrency = _LobbyLocalizePickCurrency(pLocalize->iCountry, &iResult);
    }
    pLocalize->iCurrency = iCurrency;
    const LangMapT* pRules = _LangMap;
    while (pRules->iLang != pLocalize->iLanguage && pRules->iLang != 0x7a7a) ++pRules;
    std::memcpy(&pLocalize->LangRules, pRules, sizeof(LangMapT));
    if (pLocalize->iCountry == 0x4742 && pLocalize->iLanguage == 0x656e)
        pLocalize->LangRules.eDateFormat = _DF_DMY;
}

extern "C" LobbyLocalizeT* LobbyLocalizeCreate(u32 uLocality, s32 iCurrency)
{
    const s32 iMemGroup = DirtyMemGroupQuery();
    LobbyLocalizeT* pLocalize = static_cast<LobbyLocalizeT*>(DirtyMemAlloc(sizeof(LobbyLocalizeT), 0x6c6c6f63, iMemGroup));
    if (pLocalize)
    {
        std::memset(pLocalize, 0, sizeof(LobbyLocalizeT));
        pLocalize->iMemGroup = iMemGroup;
        _LobbyLocalizeSetLocale(pLocalize, uLocality, iCurrency);
    }
    return pLocalize;
}

extern "C" LobbyLocalizeT* LobbyLocalizeDestroy(LobbyLocalizeT* pLocalize)
{
    if (pLocalize) DirtyMemFree(pLocalize, 0x6c6c6f63, pLocalize->iMemGroup);
    return 0;
}

// ARTIST 0x82B34D10; the 36-byte tm argument is passed by value on X360.
static s32 _LobbyLocalizeFormatTime(const LangMapT* pRule, const char* pInBuf,
                                    struct tm inDate, s32 iBufLen, char* pOutBuf)
{
    s32 iResult = 0;
    s32 iHour, iMinute, iSecond;
    if (pInBuf)
    {
        const s32 iSecs = std::atoi(pInBuf);
        if (iSecs < 0 || iSecs >= 86400) iResult = -7;
        iSecond = iSecs % 60;
        iHour = iSecs / 60 / 60;
        iMinute = (iSecs / 60) % 60;
    }
    else
    {
        iHour = inDate.tm_hour;
        iMinute = inDate.tm_min;
        iSecond = inDate.tm_sec;
    }
    const char cSep = pRule->cTimeSep;
    if (pRule->bUse24Hour)
        _snprintf_c(pOutBuf, iBufLen, "%02d%c%02d%c%02d", iHour, cSep, iMinute, cSep, iSecond);
    else
    {
        char strAmPm[6];
        std::strncpy(strAmPm, iHour < 12 ? "AM" : "PM", sizeof(strAmPm));
        if (iHour > 12) iHour -= 12;
        else if (!iHour) iHour = 12;
        _snprintf_c(pOutBuf, iBufLen, "%02d%c%02d%c%02d %s", iHour, cSep, iMinute, cSep, iSecond, strAmPm);
    }
    if (std::strlen(pOutBuf) >= static_cast<u32>(iBufLen))
    {
        iResult = -5;
        pOutBuf[iBufLen - 1] = 0;
    }
    return iResult;
}

// ARTIST 0x82B34EA0.
static s32 _LobbyLocalizeFormatDate(const LangMapT* pRule, const char* pInBuf,
                                    s32 iBufLen, char* pOutBuf, s32 bTimeToo)
{
    s32 iResult = 0;
    struct tm Date;
    char strDate[32];
    char strTime[32];
    ds_secstotime(&Date, static_cast<u32>(std::atoi(pInBuf)));
    const char cSep = pRule->cDateSep;
    if (pRule->eDateFormat == _DF_YMD)
        _snprintf_c(strDate, sizeof(strDate), "%04d%c%02d%c%02d", Date.tm_year + 1900, cSep, Date.tm_mon + 1, cSep, Date.tm_mday);
    else if (pRule->eDateFormat == _DF_MDY)
        _snprintf_c(strDate, sizeof(strDate), "%02d%c%02d%c%04d", Date.tm_mon + 1, cSep, Date.tm_mday, cSep, Date.tm_year + 1900);
    else
    {
        if (pRule->eDateFormat != _DF_DMY) iResult = -6;
        _snprintf_c(strDate, sizeof(strDate), "%02d%c%02d%c%04d", Date.tm_mday, cSep, Date.tm_mon + 1, cSep, Date.tm_year + 1900);
    }
    if (bTimeToo)
    {
        iResult = _LobbyLocalizeFormatTime(pRule, 0, Date, sizeof(strTime), strTime);
        _snprintf_c(pOutBuf, iBufLen, "%s%c%s", strDate, ' ', strTime);
    }
    else
        std::strncpy(pOutBuf, strDate, iBufLen);
    if (std::strlen(pOutBuf) >= static_cast<u32>(iBufLen) && !iResult)
    {
        iResult = -5;
        pOutBuf[iBufLen - 1] = 0;
    }
    return iResult;
}

// ARTIST 0x82B35038.
static s32 _LobbyLocalizeFormatNum(const LangMapT* pRule, s32 iType, const char* pInBuf,
                                   s32 iBufLen, char* pOutBuf)
{
    s32 iDecPos = -1, iMinPos = -1, bOverflow = 0, iOutPos = 0;
    const s32 iInStrLen = static_cast<s32>(std::strlen(pInBuf));
    for (s32 i = 0; i < iInStrLen; ++i)
    {
        if (iDecPos == -1 && pInBuf[i] == '.') iDecPos = i;
        if (iMinPos == -1 && pInBuf[i] == '-') iMinPos = i;
    }
    if (iDecPos == -1) iDecPos = iInStrLen;
    s32 iRemaining = iDecPos;
    for (s32 i = 0; i < iInStrLen; ++i, --iRemaining)
    {
        if (iOutPos >= iBufLen - 1) { bOverflow = 1; break; }
        char cValue;
        if (i == iDecPos) cValue = pRule->cDecSymbol;
        else
        {
            if (i == iMinPos && pRule->bSignEnd) continue;
            if (iType == 0x7e736570 && iRemaining > 1 && !(iRemaining % 3) && iOutPos)
                pOutBuf[iOutPos++] = pRule->cThouSep;
            cValue = pInBuf[i];
        }
        pOutBuf[iOutPos++] = cValue;
    }
    if (iMinPos != -1 && pRule->bSignEnd && !bOverflow)
    {
        pOutBuf[iOutPos++] = '-';
        if (iOutPos >= iBufLen) { --iOutPos; bOverflow = 1; }
    }
    pOutBuf[iOutPos] = 0;
    return bOverflow ? -5 : 0;
}

// Native DirtySDK UTF-8 provider at 0x82B302C8.
extern "C" s32 Utf8EncodeFromUCS2CodePt(char*, u16);

// ARTIST 0x82B35328.
extern "C" s32 LobbyLocalizeData(LobbyLocalizeT* pLocalize, s32 iType, char* pBuffer,
                                  s32 iBufLen, const char* pSource)
{
    if (!pLocalize) return -8;
    if (iBufLen <= 0 || !pBuffer || !pSource) return -6;
    s32 iResult = 0;
    switch (iType)
    {
    case 0x7e637572: // ~cur
    {
        char strCurrency[32];
        const s32 iLength = Utf8EncodeFromUCS2CodePt(strCurrency, static_cast<u16>(pLocalize->iCurrency));
        if (iLength < 32) strCurrency[iLength] = 0;
        const char* pFormat = pLocalize->LangRules.bCurrSpace ? "%s %s" : "%s%s";
        if (pLocalize->LangRules.bCurrEnd)
            _snprintf_c(pBuffer, iBufLen, pFormat, pSource, strCurrency);
        else if (*pSource == '-')
        {
            *pBuffer = '-';
            _snprintf_c(pBuffer + 1, iBufLen - 1, pFormat, strCurrency, pSource + 1);
        }
        else
            _snprintf_c(pBuffer, iBufLen, pFormat, strCurrency, pSource);
        break;
    }
    case 0x7e646174: // ~dat
        iResult = _LobbyLocalizeFormatDate(&pLocalize->LangRules, pSource, iBufLen, pBuffer, 0);
        break;
    case 0x7e64746d: // ~dtm
        iResult = _LobbyLocalizeFormatDate(&pLocalize->LangRules, pSource, iBufLen, pBuffer, 1);
        break;
    case 0x7e6e756d: // ~num
    case 0x7e736570: // ~sep
        iResult = _LobbyLocalizeFormatNum(&pLocalize->LangRules, iType, pSource, iBufLen, pBuffer);
        break;
    case 0x7e706374: // ~pct
        if (std::strcmp(pSource, "-") == 0 || std::strcmp(pSource, "") == 0)
            _snprintf_c(pBuffer, iBufLen, "%s", pSource);
        else if (pLocalize->LangRules.bPercentInFront)
            _snprintf_c(pBuffer, iBufLen, pLocalize->LangRules.bPercentSpace ? "%% %s" : "%%%s", pSource);
        else
            _snprintf_c(pBuffer, iBufLen, pLocalize->LangRules.bPercentSpace ? "%s %%" : "%s%%", pSource);
        break;
    case 0x7e726177: // ~raw
    case 0x7e737472: // ~str
        std::strncpy(pBuffer, pSource, iBufLen);
        break;
    case 0x7e74696d: // ~tim
    {
        struct tm DummyDate = {0};
        iResult = _LobbyLocalizeFormatTime(&pLocalize->LangRules, pSource, DummyDate, iBufLen, pBuffer);
        break;
    }
    default:
        if (pLocalize->pCallback)
            iResult = pLocalize->pCallback(iType, pBuffer, iBufLen, pSource, pLocalize->pUserData);
        else
        {
            std::strncpy(pBuffer, pSource, iBufLen);
            return -4;
        }
        break;
    }
    if (!iResult && std::strlen(pBuffer) >= static_cast<u32>(iBufLen))
    {
        iResult = -5;
        pBuffer[iBufLen - 1] = 0;
    }
    return iResult;
}
