// Native Xbox 360 DirtySDK High-Level Buddy API providers.
//
// First bounded cohort recovered from BURNOUT_X360_ARTIST.XEX:
//   HLBApiRegisterBuddyChangeCallback      target 0x82B27DF8 (caller 0x8288B61C)
//   HLBApiRegisterBuddyPresenceCallback    0x82B27E50
//   HLBApiSetSortFunction                  Update caller 0x8288B65C; +0x30 sort field
//   HLBBudGetName                          callers 0x8287D600 / 0x8287DD68; name +0x08
//   HLBBudIsBlocked                        IsBuddyBlocked caller 0x8287D5CC -> 0x82B0F1B8
//   HLBBudGetXenonXUID                     0x82B27EA8
//
// This file is the X360 implementation. The PC backend in ../pc/hlbudapipc.cpp
// models an offline/empty list and is not used here.

// XDK size optimization matches ARTIST at 0x82B27F90 and 0x82B27E50.
#if defined(_XBOX) && defined(_MSC_VER)
#pragma optimize("s", on)
#endif

#include "hlbudapi.h"
#include "dirtymem.h"
#include "lobbydisplist.h"

#include <xtl.h>
#include <xonline.h>
#include "xnfriends_x360.h"
#include <stddef.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

// The X360 HLB handle is source-private. ARTIST HLBApiCreate allocates and zeros
// exactly 0x4c bytes. R1 field accesses pin every named offset below. The opaque
// ranges are present in that allocation and zeroed by the console constructor,
// but have no named or accessed role in the mapped X360 HLB routines; keep their
// bytes without inventing field names or initialization.
typedef void* (*HLBMemAllocFnT)(s32 iSize, s32 iMemModule, s32 iMemGroup);
typedef void  (*HLBMemFreeFnT)(void* pMemory, s32 iMemModule, s32 iMemGroup);
typedef void  (*HLBChangeCallbackFnT)(HLBApiRefT* pApi, s32 iArg2, s32 iArg3, void* pUserData);
typedef void  (*HLBPresenceCallbackFnT)(HLBApiRefT* pApi, HLBBudT* pBuddy, s32 iState, void* pUserData);
typedef s32   (*HLBSortCallbackFnT)(void* pSortRef, s32 iSortContext, void* pBuddy1, void* pBuddy2);
typedef void  (*HLBDebugCallbackFnT)(void* pContext, const char* pMessage);
typedef struct HasherRef HasherRef;

struct HLBApiRefT
{
    HLBMemAllocFnT          pMemAlloc;                 // +0x00 (DirtyMemAlloc)
    HLBMemFreeFnT           pMemFree;                  // +0x04 (DirtyMemFree)
    XnFriendsRef*           pXnFriends;                // +0x08 (opaque native XnFriends handle)
    s32                     iMemGroup;                 // +0x0c
    s32                     iConnectState;             // +0x10 (HLBApiConnect/Disconnect)
    void*                   pChangeUserData;            // +0x14
    HLBChangeCallbackFnT    pChangeCallback;            // +0x18
    u8                      maOpaque1CTo2B[0x10];       // +0x1c..+0x2b opaque, zeroed by ARTIST
    s32                     mbBuddyListDirty;           // +0x2c (XnFriends callback stores 1)
    HLBSortCallbackFnT      pSortCallback;              // +0x30
    HLBDebugCallbackFnT     pDebugCallback;             // +0x34
    u8                      maOpaque38To3B[4];          // +0x38..+0x3b opaque, zeroed by ARTIST
    HLBPresenceCallbackFnT  pPresenceCallback;          // +0x3c
    void*                   pPresenceUserData;          // +0x40
    HasherRef*              pHasher;                    // +0x44
    DispListRef*            pDispList;                  // +0x48
};

typedef char HLBApiRefTSizeIs0x4C[(sizeof(HLBApiRefT) == 0x4c) ? 1 : -1];
typedef char HLBApiRefTChangeDataAt14[(offsetof(HLBApiRefT, pChangeUserData) == 0x14) ? 1 : -1];
typedef char HLBApiRefTChangeCallbackAt18[(offsetof(HLBApiRefT, pChangeCallback) == 0x18) ? 1 : -1];
typedef char HLBApiRefTDirtyFlagAt2C[(offsetof(HLBApiRefT, mbBuddyListDirty) == 0x2c) ? 1 : -1];
typedef char HLBApiRefTSortAt30[(offsetof(HLBApiRefT, pSortCallback) == 0x30) ? 1 : -1];
typedef char HLBApiRefTDebugAt34[(offsetof(HLBApiRefT, pDebugCallback) == 0x34) ? 1 : -1];
typedef char HLBApiRefTPresenceAt3C[(offsetof(HLBApiRefT, pPresenceCallback) == 0x3c) ? 1 : -1];
typedef char HLBApiRefTPresenceDataAt40[(offsetof(HLBApiRefT, pPresenceUserData) == 0x40) ? 1 : -1];
typedef char HLBApiRefTHasherAt44[(offsetof(HLBApiRefT, pHasher) == 0x44) ? 1 : -1];
typedef char HLBApiRefTDispListAt48[(offsetof(HLBApiRefT, pDispList) == 0x48) ? 1 : -1];

typedef char HLBXOnlineFriendSizeIsC4[(sizeof(XONLINE_FRIEND) == 0xc4) ? 1 : -1];
typedef char HLBXOnlineFriendStateAt18[(offsetof(XONLINE_FRIEND, dwFriendState) == 0x18) ? 1 : -1];
typedef char HLBBudXuidAt00[(offsetof(HLBBudT, uXuid) == 0x00) ? 1 : -1];
typedef char HLBBudNameAt08[(offsetof(HLBBudT, strName) == 0x08) ? 1 : -1];
typedef char HLBBudFlagsAt18[(offsetof(HLBBudT, uFlags) == 0x18) ? 1 : -1];

static const s32 KI_HLB_ERROR_NONE = 0;
static const s32 KI_HLB_ERROR_ALREADY_SET = -5;

// ARTIST sub_82B27C00 sends diagnostics only when the optional debug callback
// (+0x34) is installed. HLBApiCreate's R1 memset zeros the handle and no provider
// in this family writes +0x34; keep the target callback path for a handle that
// arrives with this field populated. The callback's context argument is "": ARTIST 0x82B27C74
// passes 0x8324EAC0, the first of this module's one-byte .bss strings (_HLBApiInit's three ""
// follow at 0x8324EAC1..0x8324EAC3), which is where the C compiler puts an unpooled "".
static void HLBApiDebugPrintf(HLBApiRefT* pApi, const char* pFormat, ...)
{
    char lacMessage[4000];
    char lacFormatted[4096];
    va_list lArguments;

    if (pApi->pDebugCallback == 0)
    {
        return;
    }

    va_start(lArguments, pFormat);
    vsprintf(lacMessage, pFormat, lArguments);
    va_end(lArguments);
    sprintf(lacFormatted, "HLB: %s \n", lacMessage);
    pApi->pDebugCallback("", lacFormatted);
}

s32 HLBApiRegisterBuddyChangeCallback(HLBApiRefT* pApi, void* pCallback, void* pUserData)
{
    if ((pApi->pChangeCallback != 0) && (pCallback != 0))
    {
        HLBApiDebugPrintf(pApi, "ERROR: Cannot have more than one buddy change callback");
        return KI_HLB_ERROR_ALREADY_SET;
    }

    pApi->pChangeCallback = (HLBChangeCallbackFnT)pCallback;
    pApi->pChangeUserData = pUserData;
    return KI_HLB_ERROR_NONE;
}

s32 HLBApiRegisterBuddyPresenceCallback(HLBApiRefT* pApi, void* pCallback, void* pUserData)
{
    if ((pApi->pPresenceCallback != 0) && (pCallback != 0))
    {
        HLBApiDebugPrintf(pApi, "ERROR: Cannot have more than one Buddy Presence Callback");
        return KI_HLB_ERROR_ALREADY_SET;
    }

    pApi->pPresenceCallback = (HLBPresenceCallbackFnT)pCallback;
    pApi->pPresenceUserData = pUserData;
    return KI_HLB_ERROR_NONE;
}

void HLBApiSetSortFunction(HLBApiRefT* pApi, void* pSortCallback)
{
    pApi->pSortCallback = (HLBSortCallbackFnT)pSortCallback;
}

const char* HLBBudGetName(HLBBudT* pBuddy)
{
    // ARTIST GetBuddyName/GetAllBuddyNames call AptString::GetInternalString
    // with the HLB buddy pointer. That target is addi r3,r3,8; the current public
    // HLBBudT prefix names those bytes as strName[16].
    return pBuddy->strName;
}

s32 HLBBudIsBlocked(HLBBudT* pBuddy)
{
    // This is not the PC backend's empty-list answer: ARTIST IsBuddyBlocked at
    // 0x8287D5CC calls 0x82B0F1B8, whose body is exactly li r3,0; blr.
    return 0;
}

u64 HLBBudGetXenonXUID(HLBBudT* pBuddy)
{
    // ARTIST 0x82B27EA8 loads the full doubleword at buddy+0x00 into the PPC return pair.
    return pBuddy->uXuid;
}

s32 HLBBudGetState(HLBBudT* pBuddy)
{
    // ARTIST 0x82B27EC8 reads only uFlags at +0x18, preserving this precedence:
    // PASSIVE -> 5; otherwise ONLINE -> 1 or 2 for same-title; otherwise 0.
    if ((pBuddy->uFlags & HLB_BUDFLAG_PASSIVE) != 0)
    {
        return 5;
    }
    if ((pBuddy->uFlags & HLB_BUDFLAG_ONLINE) != 0)
    {
        return ((pBuddy->uFlags & HLB_BUDFLAG_TITLE_MASK) == HLB_BUDFLAG_SAME_TITLE) ? 2 : 1;
    }
    return 0;
}

s32 HLBBudIsJoinable(HLBBudT* pBuddy)
{
    // ARTIST 0x82B27F08 extracts bit 4 from uFlags at +0x18.
    return (pBuddy->uFlags & HLB_BUDFLAG_JOINABLE) != 0 ? 1 : 0;
}

s32 HLBBudIsRealBuddy(HLBBudT* pBuddy)
{
    // ARTIST 0x82B27EB0 returns true only when none of 0xC0000060 is set.
    return (pBuddy->uFlags & HLB_BUDFLAG_NOT_REAL) == 0 ? 1 : 0;
}

s32 HLBListSendChatMsg(HLBApiRefT* pApi, const void* pName, s32 iPayload,
                                      void* pCallback, void* pUserData)
{
    // This X360 HLB entry point is deliberately unsupported: ARTIST 0x82B27DF0 is
    // exactly `li r3,-6; blr`. No callback or list state is changed.
    return HLB_ERROR_UNSUPPORTED;
}

// XnFriends declarations are shared with its implementation in xnfriends_x360.h.
// Hasher remains a separate native owner.

HasherRef* HasherCreate(s32 iInitial, s32 iHashLength);
void HasherDestroy(HasherRef* pHasher);
void* HasherFlush(HasherRef* pHasher);
s32 HashStrAdd(HasherRef* pHasher, const char* pKey, void* pValue);
void* HashStrFind(HasherRef* pHasher, const char* pKey);
void* HashStrDel(HasherRef* pHasher, const char* pKey);
s32 ds_stricmp(const char* pString1, const char* pString2);
char* ds_strnzcpy(char* pDestination, const char* pSource, s32 iLength);

static const s32 KI_HLB_API_MEMID = 0x68627564;   // 'hbud'
static const s32 KI_HLB_BUDDY_MEMID = 0x62756464; // 'budd'
static const s32 KI_HLB_BUDDY_COPY_SIZE = 0xc4;
static const s32 KI_HLB_NAME_NOT_FOUND = -8;

// The roster helpers carry ARTIST's own symbol names (_RosterDestroy 0x82B27C98,
// _updateFriendInRoster 0x82B27FF0, _reloadFriends 0x82B280E0, _HLBListGetIndexByName
// 0x82B28288, _HLBApiInit 0x82B28518) as file-static C functions, and HLBApiConnect
// (0x82B284C0) is an external C function of the library. ARTIST keeps every one of them
// out of line even at single call sites (HLBApiCreate -> _HLBApiInit 0x82B28668,
// HLBApiDestroy -> _RosterDestroy 0x82B27DA0, HLBListGetBuddyByName ->
// _HLBListGetIndexByName); as internal-linkage C++ functions the XDK inliner folds the
// single-call ones into their callers, so they keep external linkage here.
// HLBApiConnect's server/port/key/resource arguments come from the shared DirtySDK API
// (DWARF hlbudapips3.c:373); the X360 body ignores them, _HLBApiInit passes "", 0, "", "".
s32 HLBApiConnect(HLBApiRefT* pApi, const char* strServer, s32 iPort,
                             const char* strKey, const char* strRsrc);

static void HLBApiFriendsChanged(XnFriendsRef* pFriendRef, s32 iChangeCount,
                          s32 iInitialised, const void* pChanges,
                          void* pUserData);
static s32 HLBRosterSort(void* pSortRef, s32 iSortContext,
                  void* pRecord1, void* pRecord2);

void* _updateFriendInRoster(HLBApiRefT* pApi, const void* pFriend);
void _reloadFriends(HLBApiRefT* pApi);
void _RosterDestroy(HLBApiRefT* pApi);
s32 _HLBListGetIndexByName(HLBApiRefT* pApi, const char* pName);

static s32 HLBStateFromFlags(u32 uFlags)
{
    if ((uFlags & HLB_BUDFLAG_PASSIVE) != 0)
    {
        return 5;
    }
    if ((uFlags & HLB_BUDFLAG_ONLINE) != 0)
    {
        return ((uFlags & HLB_BUDFLAG_TITLE_MASK) == HLB_BUDFLAG_SAME_TITLE) ? 2 : 1;
    }
    return 0;
}


s32 HLBApiConnect(HLBApiRefT* pApi, const char* strServer, s32 iPort,
                             const char* strKey, const char* strRsrc)
{
    if ((pApi == 0) || (pApi->pXnFriends == 0))
    {
        return -3;
    }

    pApi->iConnectState = 1;
    XnFriendsConnect(pApi->pXnFriends, 0, HLBApiFriendsChanged, pApi);
    return 0;
}

void* _updateFriendInRoster(HLBApiRefT* pApi, const void* pFriend)
{
    char lacName[32];   // ARTIST frame 0xB0 at 0x82B27FF8: a 32-byte name buffer
    DispListT* pItem;
    void* pBuddy;

    ds_strnzcpy(lacName, (const char*)pFriend + 8, sizeof(lacName));
    pItem = (DispListT*)(HashStrFind(pApi->pHasher, lacName));
    if (pItem == 0)
    {
        HLBApiDebugPrintf(pApi, "HLBApi::New, adding bud %s ", lacName);
        pBuddy = pApi->pMemAlloc(KI_HLB_BUDDY_COPY_SIZE, KI_HLB_API_MEMID, pApi->iMemGroup);
        memset(pBuddy, 0, KI_HLB_BUDDY_COPY_SIZE);
        memcpy(pBuddy, pFriend, KI_HLB_BUDDY_COPY_SIZE);
        pItem = DispListAdd(pApi->pDispList, pBuddy);
        HashStrAdd(pApi->pHasher, ((HLBBudT*)pBuddy)->strName, pItem);
    }
    else
    {
        pBuddy = DispListGet(pApi->pDispList, pItem);
        if (pBuddy != 0)
        {
            HLBApiDebugPrintf(pApi, "HLBApi::Updated bud %s ",
                              ((HLBBudT*)pBuddy)->strName);
            memcpy(pBuddy, pFriend, KI_HLB_BUDDY_COPY_SIZE);
        }
    }
    return pBuddy;
}

void _reloadFriends(HLBApiRefT* pApi)
{
    s32 iIndex;
    void* pFriend;
    s32 iBuddyIndex;
    HLBBudT* pBuddy;
    DispListT* pItem;

    for (iIndex = 0; ; ++iIndex)
    {
        pFriend = XnFriendsGetFriend(pApi->pXnFriends, iIndex);
        if (pFriend == 0)
        {
            break;
        }
        _updateFriendInRoster(pApi, pFriend);
    }

    for (iBuddyIndex = DispListCount(pApi->pDispList) - 1; iBuddyIndex >= 0; --iBuddyIndex)
    {
        pBuddy = HLBListGetBuddyByIndex(pApi, iBuddyIndex);
        if ((pBuddy != 0) &&
            (XnFriendsGetFriendByName(pApi->pXnFriends, pBuddy->strName) == 0))
        {
            HLBApiDebugPrintf(pApi, "HLBApi::Zap bud %s ", pBuddy->strName);
            pItem = (DispListT*)(HashStrFind(pApi->pHasher, pBuddy->strName));
            HashStrDel(pApi->pHasher, pBuddy->strName);
            DispListDel(pApi->pDispList, pItem);
            pApi->pMemFree(pBuddy, KI_HLB_BUDDY_MEMID, pApi->iMemGroup);
        }
    }
}

void _RosterDestroy(HLBApiRefT* pApi)
{
    if (pApi->pDispList != 0)
    {
        DispListT* pItem;
        void* pBuddy;
        while ((pItem = (DispListT*)(HasherFlush(pApi->pHasher))) != 0)
        {
            pBuddy = DispListDel(pApi->pDispList, pItem);
            pApi->pMemFree(pBuddy, KI_HLB_API_MEMID, pApi->iMemGroup);
        }
        HasherDestroy(pApi->pHasher);
        pApi->pHasher = 0;
        DispListDestroy(pApi->pDispList);
        pApi->pDispList = 0;
    }
}

HLBApiRefT* _HLBApiInit(HLBApiRefT* pApi)
{
    if (pApi == 0)
    {
        return 0;
    }
    pApi->pSortCallback = HLBRosterSort;
    if (pApi->pXnFriends == 0)
    {
        pApi->pXnFriends = XnFriendsCreate(0, 0);
    }
    if (pApi->pXnFriends == 0)
    {
        pApi->pMemFree(pApi, KI_HLB_API_MEMID, pApi->iMemGroup);
        return 0;
    }
    pApi->pHasher = HasherCreate(1000, 2000);
    pApi->pDispList = DispListCreate(100, 100, 0);
    // ARTIST 0x82B285B4..0x82B285CC passes three distinct "" literals (unpooled) and 0.
    HLBApiConnect(pApi, "", 0, "", "");
    return pApi;
}

s32 _HLBListGetIndexByName(HLBApiRefT* pApi, const char* pName)
{
    // ARTIST 0x82B282A0..0x82B282C8 is HLBListGetBuddyCount inlined: the count is the
    // display-list count of a live handle, else 0.
    s32 iIndex = 0;
    const s32 iCount = HLBListGetBuddyCount(pApi);

    for (; iIndex < iCount; ++iIndex)
    {
        HLBBudT* pBuddy = HLBListGetBuddyByIndex(pApi, iIndex);
        if ((pBuddy != 0) && (ds_stricmp(pName, pBuddy->strName) == 0))
        {
            break;
        }
    }
    if (iIndex >= iCount)
    {
        iIndex = KI_HLB_NAME_NOT_FOUND;
    }
    return iIndex;
}

static s32 HLBRosterSort(void* pSortRef, s32 iSortContext,
                  void* pRecord1, void* pRecord2)
{
    // ARTIST 0x82B281D0 reads pointer words at payload+0xec. The roster
    // allocates only 0xc4 bytes and DispList forwards that allocation directly:
    // this is the original out-of-bounds behavior, not an in-bounds layout.
    // Decode its two pointer words as bytes, without inventing a wrapper type.
    const XONLINE_FRIEND* pExtra1;
    const XONLINE_FRIEND* pExtra2;
    u32 uFlags1;
    u32 uFlags2;
    s32 iState1;
    s32 iState2;

    memcpy(&pExtra1, (const unsigned char*)pRecord1 + 0xec, sizeof(pExtra1));
    memcpy(&pExtra2, (const unsigned char*)pRecord2 + 0xec, sizeof(pExtra2));
    uFlags1 = pExtra1->dwFriendState;
    uFlags2 = pExtra2->dwFriendState;
    iState1 = HLBStateFromFlags(uFlags1);
    iState2 = HLBStateFromFlags(uFlags2);

    if (iState1 == iState2)
    {
        return ds_stricmp((const char*)pRecord1,
                          (const char*)pRecord2);
    }
    if (iState1 == 1)
    {
        return -1;
    }
    if (iState2 == 1)
    {
        return 1;
    }
    return (iState1 > iState2) ? -1 : 1;
}

static s32 HLBChangeAction(const u8* pChange)
{
    s32 iAction;
    memcpy(&iAction, pChange + 0x10, sizeof(iAction));
    return iAction;
}

static void HLBApiFriendsChanged(XnFriendsRef* pFriendRef, s32 iChangeCount,
                          s32 iInitialised, const void* pChanges,
                          void* pUserData)
{
    HLBApiRefT* pApi = (HLBApiRefT*)pUserData;
    s32 i;
    const u8* pChange = (const u8*)pChanges;
    void* pFriend;
    s32 iBuddy;
    HLBBudT* pOldBuddy;
    void* pUpdatedBuddy;
    s32 iOldState;

    if (pApi == 0)
    {
        return;
    }
    for (i = 0; i < iChangeCount; ++i, pChange += 0x18)
    {
        if (HLBChangeAction(pChange) == 0x43)
        {
            pFriend = XnFriendsGetFriendByName(pApi->pXnFriends,
                                               (const char*)pChange);
            iBuddy = _HLBListGetIndexByName(pApi,
                                           (const char*)pChange);
            pOldBuddy = HLBListGetBuddyByIndex(pApi, iBuddy);
            if ((pFriend == 0) || (pOldBuddy == 0))
            {
                return;
            }
            iOldState = HLBStateFromFlags(pOldBuddy->uFlags);
            pUpdatedBuddy = _updateFriendInRoster(pApi, pFriend);
            if (pApi->pPresenceCallback != 0)
            {
                pApi->pPresenceCallback(pApi, (HLBBudT*)pUpdatedBuddy,
                                        iOldState, pApi->pPresenceUserData);
            }
        }
    }

    _reloadFriends(pApi);
    if (iChangeCount > 0)
    {
        for (i = 0, pChange = (const u8*)pChanges;
             i < iChangeCount; ++i, pChange += 0x18)
        {
            HLBApiDebugPrintf(pApi,
                              "HLBApi::**Friend Changed CB, count = %d, name =%s, act=%c\n",
                              iChangeCount, pChange, HLBChangeAction(pChange));
        }
        pApi->mbBuddyListDirty = 1;
        if (pApi->pChangeCallback != 0)
        {
            pApi->pChangeCallback(pApi, 5, 0, pApi->pChangeUserData);
        }
    }
}

HLBApiRefT* HLBApiCreate(void* pConfigA, void* pConfigB, s32 iMemGroup)
{
    const s32 iCurrentGroup = DirtyMemGroupQuery();
    HLBApiRefT* pApi;

    if (iMemGroup == 0)
    {
        iMemGroup = iCurrentGroup;
    }
    pApi = (HLBApiRefT*)(DirtyMemAlloc(0x4c, KI_HLB_API_MEMID, iMemGroup));
    if (pApi != 0)
    {
        memset(pApi, 0, 0x4c);
        pApi->iMemGroup = iCurrentGroup;
        pApi->pMemAlloc = DirtyMemAlloc;
        pApi->pMemFree = DirtyMemFree;
    }

    return _HLBApiInit(pApi);

}

void HLBApiDestroy(HLBApiRefT* pApi)
{
    if (pApi != 0)
    {
        if (pApi->pXnFriends != 0)
        {
            XnFriendsDestroy(pApi->pXnFriends);
        }
        _RosterDestroy(pApi);
        pApi->pChangeUserData = 0;
        pApi->pChangeCallback = 0;
        // ARTIST 0x82B27DC0 frees the handle with a direct DirtyMemFree call.
        DirtyMemFree(pApi, KI_HLB_API_MEMID, pApi->iMemGroup);
    }
}

void HLBApiDisconnect(HLBApiRefT* pApi)
{
    if ((pApi != 0) && (pApi->pXnFriends != 0))
    {
        XnFriendsDisconnect(pApi->pXnFriends);
        pApi->iConnectState = 0;
    }
}

void HLBApiUpdate(HLBApiRefT* pApi)
{
    if (pApi != 0)
    {
        XnFriendsUpdate(pApi->pXnFriends);
    }
}

void HLBApiSetUserIndex(HLBApiRefT* pApi, s32 iUserIndex)
{
    // ARTIST 0x82B281C8 replaces r3 with pXnFriends and forwards r4 unchanged.
    XnFriendsSetUserIndex(pApi->pXnFriends, iUserIndex);
}

s32 HLBListGetBuddyCount(HLBApiRefT* pApi)
{
    if ((pApi != 0) && (pApi->pDispList != 0))
    {
        return DispListCount(pApi->pDispList);
    }
    return 0;
}

HLBBudT* HLBListGetBuddyByIndex(HLBApiRefT* pApi, s32 iIndex)
{
    if (pApi != 0)
    {
        XnFriendsGetFriend(pApi->pXnFriends, iIndex);
        return (HLBBudT*)(DispListIndex(pApi->pDispList, iIndex));
    }
    return 0;
}

HLBBudT* HLBListGetBuddyByName(HLBApiRefT* pApi, const char* pName)
{
    const s32 iIndex = _HLBListGetIndexByName(pApi, pName);
    return HLBListGetBuddyByIndex(pApi, iIndex);
}

s32 HLBListSort(HLBApiRefT* pApi)
{
    if (pApi->pDispList != 0)
    {
        DispListSort(pApi->pDispList, pApi, 0, pApi->pSortCallback);
        DispListChange(pApi->pDispList, 1);
        DispListOrder(pApi->pDispList);
    }
    return 0;
}