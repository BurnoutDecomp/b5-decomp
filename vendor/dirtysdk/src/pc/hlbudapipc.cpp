// ============================================================================
// hlbudapipc.cpp -- the high-level buddy API for the PC build.
//
// [PC platform layer] The buddy list is the platform friends list, which the PC build does
// not have (offline or on the LAN): an API ref is created and keeps its callbacks, sort
// function and user index, but the list is always empty, so the change / presence callbacks
// never fire. Chat and invite messages answer HLB_ERROR_UNSUPPORTED, as every message entry
// point of the console module does. The per-buddy accessors read the HLBBudT fields the same
// way the console accessors do; with an empty list the game never reaches them.
// State lives in the ref HLBApiCreate returns (DirtyMemAlloc / DirtyMemFree).
// ============================================================================

#include "hlbudapi.h"
#include "dirtymem.h"   // DirtyMemAlloc / DirtyMemFree / DirtyMemGroupQuery

#include <cstring>      // memset

namespace
{
    // DirtyMem module id.
    const s32 KI_MEMID_HLBUD = ('h' << 24) | ('b' << 16) | ('u' << 8) | 'd';

    // HLBBudGetState answers.
    const s32 KI_HLB_STATE_OFFLINE       = 0;
    const s32 KI_HLB_STATE_ONLINE        = 1;
    const s32 KI_HLB_STATE_IN_SAME_TITLE = 2;
    const s32 KI_HLB_STATE_PASSIVE       = 5;
}

struct HLBApiRefT
{
    s32   iMemGroup;
    void* pChangeCallback;
    void* pChangeUserData;
    void* pPresenceCallback;
    void* pPresenceUserData;
    void* pSortCallback;
    s32   iUserIndex;
};

// ---- API lifecycle -----------------------------------------------------------------------

extern "C" HLBApiRefT* HLBApiCreate(void* /*pConfigA*/, void* /*pConfigB*/, s32 iMemGroup)
{
    if (iMemGroup == 0)
    {
        iMemGroup = DirtyMemGroupQuery();
    }

    HLBApiRefT* pApi = static_cast<HLBApiRefT*>(DirtyMemAlloc(sizeof(HLBApiRefT), KI_MEMID_HLBUD, iMemGroup));
    if (pApi == NULL)
    {
        return NULL;
    }
    memset(pApi, 0, sizeof(*pApi));
    pApi->iMemGroup = iMemGroup;
    return pApi;
}

extern "C" void HLBApiDestroy(HLBApiRefT* pApi)
{
    if (pApi != NULL)
    {
        DirtyMemFree(pApi, KI_MEMID_HLBUD, pApi->iMemGroup);
    }
}

extern "C" void HLBApiDisconnect(HLBApiRefT* /*pApi*/)
{
    // No friends service connection.
}

extern "C" void HLBApiUpdate(HLBApiRefT* /*pApi*/)
{
    // The list never changes.
}

extern "C" void HLBApiSetUserIndex(HLBApiRefT* pApi, s32 iUserIndex)
{
    pApi->iUserIndex = iUserIndex;
}

// ---- callbacks ---------------------------------------------------------------------------

extern "C" s32 HLBApiRegisterBuddyChangeCallback(HLBApiRefT* pApi, void* pCallback, void* pUserData)
{
    if ((pApi->pChangeCallback != NULL) && (pCallback != NULL))
    {
        return HLB_ERROR_ALREADY_SET;
    }
    pApi->pChangeCallback = pCallback;
    pApi->pChangeUserData = pUserData;
    return HLB_ERROR_NONE;
}

extern "C" s32 HLBApiRegisterBuddyPresenceCallback(HLBApiRefT* pApi, void* pCallback, void* pUserData)
{
    if ((pApi->pPresenceCallback != NULL) && (pCallback != NULL))
    {
        return HLB_ERROR_ALREADY_SET;
    }
    pApi->pPresenceCallback = pCallback;
    pApi->pPresenceUserData = pUserData;
    return HLB_ERROR_NONE;
}

extern "C" void HLBApiSetSortFunction(HLBApiRefT* pApi, void* pSortCallback)
{
    pApi->pSortCallback = pSortCallback;
}

// ---- list --------------------------------------------------------------------------------

extern "C" s32 HLBListGetBuddyCount(HLBApiRefT* /*pApi*/)
{
    return 0;
}

extern "C" HLBBudT* HLBListGetBuddyByIndex(HLBApiRefT* /*pApi*/, s32 /*iIndex*/)
{
    return NULL;
}

extern "C" HLBBudT* HLBListGetBuddyByName(HLBApiRefT* /*pApi*/, const char* /*pName*/)
{
    return NULL;
}

extern "C" void HLBListSort(HLBApiRefT* /*pApi*/)
{
    // An empty list is already sorted.
}

extern "C" s32 HLBListSendChatMsg(HLBApiRefT* /*pApi*/, const void* /*pName*/, s32 /*iPayload*/, void* /*pCallback*/,
                                  void* /*pUserData*/)
{
    return HLB_ERROR_UNSUPPORTED;
}

// ---- one buddy ---------------------------------------------------------------------------

extern "C" const char* HLBBudGetName(HLBBudT* pBuddy)
{
    return pBuddy->strName;
}

extern "C" u64 HLBBudGetXenonXUID(HLBBudT* pBuddy)
{
    return pBuddy->uXuid;
}

extern "C" s32 HLBBudIsRealBuddy(HLBBudT* pBuddy)
{
    return ((pBuddy->uFlags & HLB_BUDFLAG_NOT_REAL) == 0) ? 1 : 0;
}

extern "C" s32 HLBBudIsJoinable(HLBBudT* pBuddy)
{
    return ((pBuddy->uFlags & HLB_BUDFLAG_JOINABLE) != 0) ? 1 : 0;
}

extern "C" s32 HLBBudIsBlocked(HLBBudT* /*pBuddy*/)
{
    // The platform list carries no blocked buddies.
    return 0;
}

extern "C" s32 HLBBudGetState(HLBBudT* pBuddy)
{
    if ((pBuddy->uFlags & HLB_BUDFLAG_PASSIVE) != 0)
    {
        return KI_HLB_STATE_PASSIVE;
    }
    if ((pBuddy->uFlags & HLB_BUDFLAG_ONLINE) != 0)
    {
        return ((pBuddy->uFlags & HLB_BUDFLAG_TITLE_MASK) == HLB_BUDFLAG_SAME_TITLE) ? KI_HLB_STATE_IN_SAME_TITLE
                                                                                       : KI_HLB_STATE_ONLINE;
    }
    return KI_HLB_STATE_OFFLINE;
}
