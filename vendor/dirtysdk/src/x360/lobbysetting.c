// DirtySDK 5.5.3 lobby settings provider, recovered from ARTIST 82B1F968..82B1FC64.
// XDK size optimization matches ARTIST at 0x82B1FAF0 and 0x82B1FB80.
// DirtySDK is C (DWARF home: SDKs/Packages/DirtySDK/5.5.3/core/source/lobby/lobbysetting.c), and
// the C compiler is what places its literals as ARTIST has them: without string pooling a C
// module keeps its literals in .data and an all-zero "" in .bss, beside the module's statics
// (LobbySettingLoad passes "" from 0x8324EA8B, between GameManagerLeaveCb's .bss byte 0x8324EA88
// and the next lobby module's; its NetPrintf text is .data at 0x82F7DF64).
#if defined(_XBOX) && defined(_MSC_VER)
#pragma optimize("s", on)
#endif

#include "lobbysetting.h"
#include "lobbytagfield.h"
#include "dirtymem.h"
#include "dirtylib.h"
#include <stddef.h>
#include <string.h>

struct LobbySettingRefT
{
    LobbyApiRefT* pApiRef;
    s32 iCallbackID;
    s32 iMemGroup;
    LobbySettingCallbackT* pCallback;
    void* pUserData;
    s32 eState;
    char strSettings[1001];
};
typedef char SettingSizeCheck[sizeof(LobbySettingRefT) == 1028 ? 1 : -1];
typedef char SettingBufferOffsetCheck[offsetof(LobbySettingRefT, strSettings) == 24 ? 1 : -1];

static void _LobbySettingCmdDone(LobbySettingRefT* pSettings, s32 iCode)
{
    if (pSettings->pCallback)
        pSettings->pCallback(pSettings, iCode, pSettings->pUserData);
    pSettings->eState = 0;
    pSettings->pCallback = 0;
    pSettings->pUserData = 0;
    pSettings->iCallbackID = -1;
}

static void _LobbySettingSaveCallback(LobbyApiRefT* pApiRef, LobbyApiMsgT* pMsg, void* pUserData)
{
    LobbySettingRefT* pSettings = (LobbySettingRefT*)pUserData;
    if (pMsg->code)
        NetPrintfCode("_LobbySettingSaveCallback: Error response to mesg\n");
    _LobbySettingCmdDone(pSettings, pMsg->code);
}

static void _LobbySettingLoadCallback(LobbyApiRefT* pApiRef, LobbyApiMsgT* pMsg, void* pUserData)
{
    LobbySettingRefT* pSettings = (LobbySettingRefT*)pUserData;
    if (pMsg->code)
    {
        NetPrintfCode("_LobbySettingLoadCallback: Error response to mesg\n");
        pSettings->strSettings[0] = 0;
    }
    else
        TagFieldDupl(pSettings->strSettings, sizeof(pSettings->strSettings), pMsg->pData);
    _LobbySettingCmdDone(pSettings, pMsg->code);
}

LobbySettingRefT* LobbySettingCreate(LobbyApiRefT* pApiRef)
{
    LobbySettingRefT* pSettings;
    const s32 iMemGroup = DirtyMemGroupQuery();
    pSettings = (LobbySettingRefT*)DirtyMemAlloc(sizeof(LobbySettingRefT), 0x6c736574, iMemGroup);
    if (!pSettings)
        return 0;
    memset(pSettings, 0, sizeof(*pSettings));
    pSettings->iMemGroup = iMemGroup;
    pSettings->pApiRef = pApiRef;
    pSettings->eState = 0;
    pSettings->iCallbackID = -1;
    pSettings->strSettings[0] = 0;
    return pSettings;
}

s32 LobbySettingLoad(LobbySettingRefT* pSettings, LobbySettingCallbackT* pCallback, void* pUserData)
{
    if (pSettings->eState)
    {
        NetPrintfCode("LobbySettingLoad: Load already in progress\n");
        return 0;
    }
    pSettings->pCallback = pCallback;
    pSettings->pUserData = pUserData;
    pSettings->eState = 1;
    pSettings->iCallbackID = LobbyApiRequestCB(pSettings->pApiRef, 0x75736c64, "", _LobbySettingLoadCallback, pSettings);
    return 1;
}

s32 LobbySettingSave(LobbySettingRefT* pSettings, LobbySettingCallbackT* pCallback, void* pUserData)
{
    if (pSettings->eState)
    {
        NetPrintfCode("LobbySettingSave: Save already in progress\n");
        return 0;
    }
    pSettings->pCallback = pCallback;
    pSettings->pUserData = pUserData;
    pSettings->eState = 1;
    pSettings->iCallbackID = LobbyApiRequestCB(pSettings->pApiRef, 0x75737376, pSettings->strSettings, _LobbySettingSaveCallback, pSettings);
    return 1;
}

s32 LobbySettingGetNumber(LobbySettingRefT* pSettings, const char* pName, s32 iDefault)
{
    return TagFieldGetNumber(TagFieldFind(pSettings->strSettings, pName), iDefault);
}

void LobbySettingSetNumber(LobbySettingRefT* pSettings, const char* pName, s32 iValue)
{
    TagFieldSetNumber(pSettings->strSettings, sizeof(pSettings->strSettings), pName, iValue);
}

void LobbySettingDestroy(LobbySettingRefT* pSettings)
{
    DirtyMemFree(pSettings, 0x6c736574, pSettings->iMemGroup);
}
