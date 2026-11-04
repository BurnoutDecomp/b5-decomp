// DirtySDK 5.5.3 lobby find-user provider: ARTIST 82B23D88..82B24220.
#include "lobbyfinduser.h"
#include "lobbytagfield.h"
#include "lobbyname.h"
#include "dirtymem.h"
#include <stddef.h>
#include <string.h>

extern "C" LobbyApiUserT* LobbyApiGetUserInfo(LobbyApiRefT*, const char*);
extern "C" void LobbyApiExtractUserRecord(LobbyApiUserT*, const char*);
extern "C" char* ds_strnzcpy(char*, const char*, s32);

struct LobbyFindUserT
{
    LobbyApiRefT* pLobby;
    s32 eState;
    s32 iMemGroup;
    char strUser[16];
    u32 uReqTime;
    s32 iReqId;
    s32 bOnlineOnly;
    LobbyFindUserCallbackT* pCallback;
    void* pCalldata;
    LobbyApiUserT aCache[8];
    u32 aCacheExpire[8];
    u32 uCacheExpire;
    u32 uRate;
};
typedef char FindUserSizeCheck[sizeof(LobbyFindUserT) == 4600 ? 1 : -1];
typedef char FindUserCacheOffsetCheck[offsetof(LobbyFindUserT, aCache) == 48 ? 1 : -1];
typedef char FindUserRateOffsetCheck[offsetof(LobbyFindUserT, uRate) == 4596 ? 1 : -1];

static s32 _LobbyFindUserCache(LobbyFindUserT* pRef, const char* pData)
{
    const u32 uNow = static_cast<u32>(LobbyApiStatus(pRef->pLobby, 0x7469636b, 0, 0));
    s32 iOldest = 0;
    s32 iFree = -1;
    for (s32 i = 0; i < 8; ++i)
    {
        if (pRef->aCache[i].name[0] && pRef->aCacheExpire[i] < uNow)
        {
            memset(&pRef->aCache[i], 0, sizeof(pRef->aCache[i]));
            iFree = i;
            pRef->aCacheExpire[i] = 0;
        }
        if (!pRef->aCache[i].name[0])
            iFree = i;
        if (pRef->aCacheExpire[i] && pRef->aCacheExpire[i] < pRef->aCacheExpire[iOldest])
            iOldest = i;
    }
    if (iFree == -1)
        iFree = iOldest;
    pRef->aCacheExpire[iFree] = pRef->uCacheExpire + uNow;
    LobbyApiExtractUserRecord(&pRef->aCache[iFree], pData);
    return iFree;
}

static void _LobbyFindUserCallback(LobbyApiRefT*, LobbyApiMsgT* pMsg, void* pUserData)
{
    LobbyFindUserT* pRef = static_cast<LobbyFindUserT*>(pUserData);
    if (pRef->eState != 1 || pRef->iReqId != pMsg->id)
        return;
    if (pMsg->code)
        pRef->pCallback(pRef->pCalldata, pRef->strUser, 0);
    else
    {
        char strName[64];
        TagFieldGetString(TagFieldFind(pMsg->pData, "N"), strName, 16, "");
        if (LobbyNameCmp(pRef->strUser, strName))
            return;
        const s32 iCache = _LobbyFindUserCache(pRef, pMsg->pData);
        pRef->pCallback(pRef->pCalldata, pRef->strUser, &pRef->aCache[iCache]);
    }
    pRef->strUser[0] = 0;
    pRef->pCalldata = 0;
    pRef->pCallback = 0;
    pRef->eState = 0;
}

extern "C" LobbyFindUserT* LobbyFindUserCreate(LobbyApiRefT* pLobby)
{
    const s32 iMemGroup = DirtyMemGroupQuery();
    LobbyFindUserT* pRef = static_cast<LobbyFindUserT*>(DirtyMemAlloc(sizeof(LobbyFindUserT), 0x6c667573, iMemGroup));
    if (!pRef)
        return 0;
    memset(pRef, 0, sizeof(*pRef));
    pRef->iMemGroup = iMemGroup;
    pRef->pLobby = pLobby;
    pRef->eState = 0;
    pRef->uRate = 1000;
    pRef->uCacheExpire = 20000;
    return pRef;
}

extern "C" void LobbyFindUserSetParams(LobbyFindUserT* pRef, u32 uRate, s32 iCacheExpire)
{
    pRef->uRate = uRate;
    if (uRate < 250)
        pRef->uRate = 250;
    pRef->uCacheExpire = iCacheExpire;
}

extern "C" void LobbyFindUserCancel(LobbyFindUserT* pRef)
{
    if (pRef->eState == 1)
        LobbyApiCancelCB(pRef->pLobby, pRef->iReqId);
    pRef->eState = 0;
    pRef->strUser[0] = 0;
    pRef->pCallback = 0;
    pRef->pCalldata = 0;
}

extern "C" void LobbyFindUserDestroy(LobbyFindUserT* pRef)
{
    DirtyMemFree(pRef, 0x6c667573, pRef->iMemGroup);
}

extern "C" s32 LobbyFindUser(LobbyFindUserT* pRef, const char* pName, LobbyFindUserCallbackT* pCallback,
                             void* pUserData, s32 bOnlineOnly, s32 bUseCache)
{
    if (!pRef || !pName || !*pName || !pCallback)
        return -3;
    if (pRef->eState)
        return -1;
    const u32 uNow = static_cast<u32>(LobbyApiStatus(pRef->pLobby, 0x7469636b, 0, 0));
    LobbyApiUserT* pUser = LobbyApiGetUserInfo(pRef->pLobby, pName);
    if (!pUser && bUseCache)
    {
        for (u32 i = 0; i < 8; ++i)
            if (!LobbyNameCmp(pName, pRef->aCache[i].name) && uNow < pRef->aCacheExpire[i])
            {
                pUser = &pRef->aCache[i];
                break;
            }
    }
    if (pUser)
    {
        pCallback(pUserData, pName, pUser);
        return 0;
    }
    if (uNow - pRef->uReqTime < pRef->uRate)
        return -2;
    pRef->pCallback = pCallback;
    pRef->pCalldata = pUserData;
    pRef->uReqTime = uNow;
    pRef->bOnlineOnly = bOnlineOnly;
    pRef->eState = 1;
    ds_strnzcpy(pRef->strUser, pName, 16);
    char strRequest[64];
    TagFieldPrintf(strRequest, sizeof(strRequest), "PERS=%s OFFLINE=%d", pName, bOnlineOnly == 0);
    pRef->iReqId = LobbyApiRequestCB(pRef->pLobby, 0x6f6e6c6e, strRequest, _LobbyFindUserCallback, pRef);
    return pRef->iReqId < 0 ? -1 : 0;
}
