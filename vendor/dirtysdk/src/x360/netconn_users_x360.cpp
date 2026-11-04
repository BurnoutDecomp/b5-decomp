// ARTIST NetConnXenon address status and asynchronous user-info helpers.
#include "netconn_x360_internal.h"
#include "dirtymem.h"
#include "dirtylib.h"
#include "dirtyaddr.h"
#include <string.h>

extern "C" const char* DirtyErrGetName(u32 uError);
extern "C" char* ds_strnzcpy(char*, const char*, s32);

// ARTIST 0x82B11CE0.
s32 NetConnX360AddressStatus(NetConnX360State* pState, u32 uStatus)
{
    if ((pState->bSyslink != 0) && (XNetGetEthernetLinkStatus() != 0))
    {
        pState->bAddressReady = 1;
        return 0x2b697370;
    }
    if (uStatus == 0) return 0x7e696e69;
    if ((uStatus & 1) != 0) return 0x2d696e69;
    if ((uStatus & 0x8000) != 0) return 0x2d747262;
    if ((uStatus & 0x60) != 0)
    {
        pState->bAddressReady = 1;
        return 0x2b697370;
    }
    if ((uStatus & 2) != 0) return 0x7e657468;
    if ((uStatus & 0x40) != 0) return 0x7e646e73;
    if ((uStatus & 4) != 0) return 0x7e737461;
    if ((uStatus & 8) != 0) return 0x7e646870;
    if ((uStatus & 0x10) != 0) return 0x7e707070;
    return 0x3f3f2d3f;
}

// ARTIST 0x82B121C8: identity changes start the asynchronous account lookup.
void NetConnX360RefreshUsers(NetConnX360State* pState)
{
    for (s32 i = 0; i < 4; ++i)
    {
        NetConnX360User* pUser = &pState->aUsers[i];
        if (XUserGetSigninState(i) == eXUserSigninState_SignedInToLive)
        {
            XUID xuid;
            char strName[16];
            XUserGetXUID(i, &xuid);
            XUserGetName(i, strName, sizeof(strName));
            if ((pUser->xuid != xuid) || (strcmp(pUser->strName, strName) != 0))
            {
                NetPrintfCode("netconnxenon: user index %d has changed\n", i);
                pUser->xuid = xuid;
                ds_strnzcpy(pUser->strName, strName, 16);
                pUser->iInfoState = 1;
                DirtyAddrT address;
                DirtyAddrFromHostAddr(&address, &pUser->xuid);
                NetPrintfCode("netconnxenon: %s=%s\n", pUser->strName, &address);
            }
        }
        else
        {
            if (pUser->pInfo != 0)
            {
                DirtyMemFree(pUser->pInfo, 0x6e636f6e, pState->iMemGroup);
            }
            memset(pUser, 0, sizeof(*pUser));
        }
    }
}

// ARTIST 0x82B11F18: complete the active account lookup, retaining its data.
static void NetConnX360FinishUserInfo(NetConnX360State* pState, NetConnX360User* pUser, s32 iUser)
{
    if (!XHasOverlappedIoCompleted(&pState->AccountOverlapped))
    {
        return;
    }
    pState->bAccountBusy = 0;
    DWORD uError = XGetOverlappedExtendedError(&pState->AccountOverlapped);
    if (uError != 0)
    {
        NetPrintfCode("netconnxenon: _XAccountGetUserInfo() failed: error=%s\n", DirtyErrGetName(uError));
        DirtyMemFree(pUser->pInfo, 0x6e636f6e, pState->iMemGroup);
        pUser->pInfo = 0;
        pUser->iInfoState = -1;
        return;
    }
    GET_USER_INFO_RESPONSE* pInfo = pUser->pInfo;
    NetPrintfCode("netconnxenon: acquired user info for user [%d] %s\n", iUser, pUser->strName);
    NetPrintfCode("   firstName:    %S\n", pInfo->wszFirstName);
    NetPrintfCode("   lastName:     %S\n", pInfo->wszLastName);
    NetPrintfCode("   street1:      %S\n", pInfo->addressInfo.wszStreet1);
    NetPrintfCode("   city:         %S\n", pInfo->addressInfo.wszCity);
    NetPrintfCode("   state:        %S\n", pInfo->addressInfo.wszState);
    NetPrintfCode("   postalCode:   %S\n", pInfo->addressInfo.wszPostalCode);
    NetPrintfCode("   email:        %S\n", pInfo->wszEmail);
    NetPrintfCode("   wLanguageId:  %d\n", pInfo->wLanguageId);
    NetPrintfCode("   bCountryId:   %d\n", pInfo->bCountryId);
    NetPrintfCode("   bParterOptIn: %d\n", pInfo->bParterOptIn);
    NetPrintfCode("   bAge:         %d\n", pInfo->bAge);
    pUser->iInfoState = 3;
}

// ARTIST 0x82B12068: one in-flight request, no synthetic success.
s32 NetConnX360UpdateUserInfo(NetConnX360State* pState)
{
    s32 i;
    for (i = 0; i < 4; ++i)
    {
        NetConnX360User* pUser = &pState->aUsers[i];
        if ((pUser->iInfoState == 1) && (pState->bAccountBusy == 0))
        {
            if (pUser->pInfo == 0)
            {
                pUser->pInfo = static_cast<GET_USER_INFO_RESPONSE*>(DirtyMemAlloc(0x5de, 0x6e636f6e, pState->iMemGroup));
            }
            if (pUser->pInfo == 0)
            {
                NetPrintfCode("netconnxenon: could not allocate user info buffer\n");
                pUser->iInfoState = -1;
                break;
            }
            memset(&pState->AccountOverlapped, 0, sizeof(pState->AccountOverlapped));
            memset(pUser->pInfo, 0, 0x5de);
            if (_XAccountGetUserInfo(i, 0x5de, pUser->pInfo, &pState->AccountOverlapped) == ERROR_IO_PENDING)
            {
                pUser->iInfoState = 2;
                pState->bAccountBusy = 1;
            }
            else
            {
                DWORD uError = XGetOverlappedExtendedError(&pState->AccountOverlapped);
                NetPrintfCode("netconnxenon: _XAccountGetUserInfo() failed for user %d: error=%s\n", i, DirtyErrGetName(uError));
                DirtyMemFree(pUser->pInfo, 0x6e636f6e, pState->iMemGroup);
                pUser->pInfo = 0;
                pUser->iInfoState = -1;
                pState->bAccountBusy = 0;
            }
            break;
        }
        if (pUser->iInfoState == 2)
        {
            NetConnX360FinishUserInfo(pState, pUser, i);
            break;
        }
    }
    return i == 4;
}
