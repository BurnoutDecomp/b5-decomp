// DirtySDK 5.5.3 lobby settings provider, recovered from ARTIST 82B1F968..82B1FC64.
#include "lobbysetting.h"
#include "lobbytagfield.h"
#include "dirtymem.h"
#include "dirtylib.h"
#include <stddef.h>
#include <string.h>

namespace CgsNetwork { namespace DirtySock {
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
} }
using CgsNetwork::DirtySock::LobbySettingRefT;
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

static void _LobbySettingSaveCallback(LobbyApiRefT*, LobbyApiMsgT* pMsg, void* pUserData)
{
    LobbySettingRefT* pSettings = static_cast<LobbySettingRefT*>(pUserData);
    if (pMsg->code)
        NetPrintfCode("_LobbySettingSaveCallback: Error response to mesg\n");
    _LobbySettingCmdDone(pSettings, pMsg->code);
}

static void _LobbySettingLoadCallback(LobbyApiRefT*, LobbyApiMsgT* pMsg, void* pUserData)
{
    LobbySettingRefT* pSettings = static_cast<LobbySettingRefT*>(pUserData);
    if (pMsg->code)
    {
        NetPrintfCode("_LobbySettingLoadCallback: Error response to mesg\n");
        pSettings->strSettings[0] = 0;
    }
    else
        TagFieldDupl(pSettings->strSettings, sizeof(pSettings->strSettings), pMsg->pData);
    _LobbySettingCmdDone(pSettings, pMsg->code);
}

extern "C" LobbySettingRefT* LobbySettingCreate(LobbyApiRefT* pApiRef)
{
    const s32 iMemGroup = DirtyMemGroupQuery();
    LobbySettingRefT* pSettings = static_cast<LobbySettingRefT*>(DirtyMemAlloc(sizeof(LobbySettingRefT), 0x6c736574, iMemGroup));
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

extern "C" s32 LobbySettingLoad(LobbySettingRefT* pSettings, LobbySettingCallbackT* pCallback, void* pUserData)
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

extern "C" s32 LobbySettingSave(LobbySettingRefT* pSettings, LobbySettingCallbackT* pCallback, void* pUserData)
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

extern "C" s32 LobbySettingGetNumber(LobbySettingRefT* pSettings, const char* pName, s32 iDefault)
{
    return TagFieldGetNumber(TagFieldFind(pSettings->strSettings, pName), iDefault);
}

extern "C" void LobbySettingSetNumber(LobbySettingRefT* pSettings, const char* pName, s32 iValue)
{
    TagFieldSetNumber(pSettings->strSettings, sizeof(pSettings->strSettings), pName, iValue);
}

extern "C" void LobbySettingDestroy(LobbySettingRefT* pSettings)
{
    DirtyMemFree(pSettings, 0x6c736574, pSettings->iMemGroup);
}
