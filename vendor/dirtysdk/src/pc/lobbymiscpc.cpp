// ============================================================================
// lobbymiscpc.cpp -- LobbyRank, LobbyStatbook, LobbySetting and LobbyFindUser for the PC build.
//
// [PC platform layer] These lobby modules are thin clients of the online lobby service (rank
// lists, stat books, account settings, user lookup). The PC build has no such service, in
// either mode: offline there is no network, and the LAN transport has no counterpart for any
// of them. Each module answers exactly as the console module does when its lobby request
// cannot be served:
//   - LobbyRank: no category tree and no rank list are ever downloaded; fetches fail with
//     LOBBYRANK_ERROR_LOBBY_INVALID (or the not-selected / bad-view codes checked first),
//     counts answer "not downloaded", names and cells are NULL.
//   - LobbyStatbook: view / view-list requests complete at once with LOBBYSTATBOOK_ERROR_MISC;
//     a row fetch fails with LOBBYSTATBOOK_ERROR_VIEW_NOT_SELECTED (no view is ever present).
//   - LobbySetting: Get / SetNumber work on the local record. Offline, Load / Save are not
//     accepted (return 0, no callback). With the LAN transport active they complete at once
//     with result 0: a load leaves the record empty (every setting reads its default), a save
//     keeps the local record and sends nothing.
//   - LobbyFindUser: every lookup fails to send (-1); no callback.
// All state lives in the refs the Create functions return (DirtyMemAlloc / DirtyMemFree).
// ============================================================================

#include "lobbyrank.h"
#include "lobbystatbook.h"
#include "lobbysetting.h"
#include "lobbyfinduser.h"
#include "lobbytagfield.h"   // TagFieldFind / TagFieldGetNumber / TagFieldSetNumber
#include "dirtymem.h"        // DirtyMemAlloc / DirtyMemFree / DirtyMemGroupQuery

#include "lan/pclan.h"       // PcLanActive

#include <cstring>           // memset / strlen

namespace
{
    // DirtyMem module ids of the four modules.
    const s32 KI_MEMID_LOBBYRANK     = ('l' << 24) | ('r' << 16) | ('n' << 8) | 'k';
    const s32 KI_MEMID_LOBBYSTATBOOK = ('l' << 24) | ('s' << 16) | ('t' << 8) | 'a';
    const s32 KI_MEMID_LOBBYSETTING  = ('l' << 24) | ('s' << 16) | ('e' << 8) | 't';
    const s32 KI_MEMID_LOBBYFINDUSER = ('l' << 24) | ('f' << 16) | ('u' << 8) | 's';

    // Longest rank view name LobbyRankCategoryFetch accepts.
    const size_t KU_RANK_VIEW_MAX = 11;

    // Statbook slot count bounds and the fallback for an out-of-range request.
    const u32 KU_STATBOOK_SLOTS_MAX     = 1024;
    const u32 KU_STATBOOK_SLOTS_DEFAULT = 10;

    // Size of the settings tagfield record.
    const s32 KI_SETTINGS_RECORD_SIZE = 1001;

    // Find-user request pacing defaults and floor (ms).
    const u32 KU_FINDUSER_RATE_DEFAULT         = 1000;
    const u32 KU_FINDUSER_RATE_MIN             = 250;
    const s32 KI_FINDUSER_CACHE_EXPIRE_DEFAULT = 20000;
}

// ---- module state ------------------------------------------------------------------------

struct LobbyRankT
{
    LobbyApiRefT*       pLobby;
    s32                 iMemGroup;
    bool                bSelected;   // LobbyRankSelect succeeded (direct selection only: no tree)
};

struct LobbyStatbookT
{
    LobbyApiRefT* pLobby;
    s32           iMemGroup;
    u32           uSlots;
    s32           iStatus;       // result of the last view / view-list request
};

namespace CgsNetwork
{
    namespace DirtySock
    {
        struct LobbySettingRefT
        {
            LobbyApiRefT* pLobby;
            s32           iMemGroup;
            char          strSettings[KI_SETTINGS_RECORD_SIZE];
        };
    }
}

struct LobbyFindUserT
{
    LobbyApiRefT* pLobby;
    s32           iMemGroup;
    u32           uRate;
    s32           iCacheExpire;
};

using CgsNetwork::DirtySock::LobbySettingRefT;

// ============================================================================
// LobbyRank
// ============================================================================

extern "C" LobbyRankT* LobbyRankCreate(LobbyApiRefT* pLobby, s32 /*iCache*/)
{
    const s32 iMemGroup = DirtyMemGroupQuery();
    LobbyRankT* pRanker = static_cast<LobbyRankT*>(DirtyMemAlloc(sizeof(LobbyRankT), KI_MEMID_LOBBYRANK, iMemGroup));
    if (pRanker == NULL)
    {
        return NULL;
    }
    memset(pRanker, 0, sizeof(*pRanker));
    pRanker->pLobby    = pLobby;
    pRanker->iMemGroup = iMemGroup;
    return pRanker;
}

extern "C" void LobbyRankDestroy(LobbyRankT* pRanker)
{
    DirtyMemFree(pRanker, KI_MEMID_LOBBYRANK, pRanker->iMemGroup);
}

extern "C" s32 LobbyRankCategoryFetch(LobbyRankT* /*pRanker*/, const char* pView, LobbyRankCallbackT* /*pCallback*/,
                                      void* /*pUserData*/)
{
    if (strlen(pView) > KU_RANK_VIEW_MAX)
    {
        return LOBBYRANK_ERROR_INVALID_VIEW;
    }
    return LOBBYRANK_ERROR_LOBBY_INVALID;
}

extern "C" s32 LobbyRankCategoryCount(LobbyRankT* /*pRanker*/)
{
    return LOBBYRANK_ERROR_LIST_NOT_DOWNLOADED;
}

extern "C" const char* LobbyRankCategoryName(LobbyRankT* /*pRanker*/, s32 /*iCategory*/, char* /*pBuffer*/,
                                             s32 /*iBuflen*/)
{
    return NULL;
}

extern "C" s32 LobbyRankIndexCount(LobbyRankT* /*pRanker*/, s32 /*iCategory*/)
{
    return LOBBYRANK_ERROR_LIST_NOT_DOWNLOADED;
}

extern "C" const char* LobbyRankIndexName(LobbyRankT* /*pRanker*/, s32 /*iCategory*/, s32 /*iIndex*/,
                                          char* /*pBuffer*/, s32 /*iBuflen*/)
{
    return NULL;
}

extern "C" s32 LobbyRankVariationCount(LobbyRankT* /*pRanker*/, s32 /*iCategory*/, s32 /*iIndex*/)
{
    return LOBBYRANK_ERROR_LIST_NOT_DOWNLOADED;
}

extern "C" const char* LobbyRankVariationName(LobbyRankT* /*pRanker*/, s32 /*iCategory*/, s32 /*iIndex*/,
                                              s32 /*iVariation*/, char* /*pBuffer*/, s32 /*iBuflen*/)
{
    return NULL;
}

extern "C" s32 LobbyRankVariationUserType(LobbyRankT* /*pRanker*/, s32 /*iCategory*/, s32 /*iIndex*/,
                                          s32 /*iVariation*/)
{
    return 0;
}

extern "C" s32 LobbyRankSelect(LobbyRankT* pRanker, s32 iCategory, s32 iIndex, s32 iVariation)
{
    if (iCategory != LOBBYRANK_SELECT_DIRECT)
    {
        // Selecting through the category tree needs the tree.
        return LOBBYRANK_ERROR_INVALID_CATEGORY;
    }
    if (iIndex < 0)
    {
        return LOBBYRANK_ERROR_INVALID_INDEX;
    }
    if (iVariation < 0)
    {
        return LOBBYRANK_ERROR_INVALID_VARIATION;
    }
    pRanker->bSelected = true;
    return 0;
}

extern "C" void LobbyRankFlush(LobbyRankT* /*pRanker*/, s32 /*bAll*/)
{
}

extern "C" s32 LobbyRankParm(LobbyRankT* pRanker, s32 /*iIndex*/, s32* /*pData*/)
{
    // A direct selection carries no parameter table.
    return pRanker->bSelected ? LOBBYRANK_ERROR_INVALID_INDEX : LOBBYRANK_ERROR_RANKINGS_NOT_DOWNLOADED;
}

extern "C" s32 LobbyRankFetch(LobbyRankT* pRanker, s32 /*iTimeout*/, s32 /*iOffset*/, s32 /*iRows*/,
                              const char** /*pBuddyList*/, s32 /*iBuddyCount*/, LobbyRankCallbackT* /*pCallback*/,
                              void* /*pUserData*/)
{
    if (!pRanker->bSelected)
    {
        return LOBBYRANK_ERROR_RANKINGS_NOT_DOWNLOADED;
    }
    return LOBBYRANK_ERROR_LOBBY_INVALID;
}

extern "C" s32 LobbyRankColumnCount(LobbyRankT* pRanker)
{
    return pRanker->bSelected ? 0 : LOBBYRANK_ERROR_RANKINGS_NOT_DOWNLOADED;
}

extern "C" s32 LobbyRankColumnWidth(LobbyRankT* pRanker, s32 /*iColumn*/)
{
    return pRanker->bSelected ? LOBBYRANK_ERROR_INVALID_COL : LOBBYRANK_ERROR_RANKINGS_NOT_DOWNLOADED;
}

extern "C" s32 LobbyRankColumnType(LobbyRankT* /*pRanker*/, s32 /*iColumn*/)
{
    return 0;
}

extern "C" s32 LobbyRankColumnStyle(LobbyRankT* /*pRanker*/, s32 /*iColumn*/)
{
    return 0;
}

extern "C" const char* LobbyRankColumnName(LobbyRankT* /*pRanker*/, s32 /*iColumn*/, char* /*pBuffer*/,
                                           s32 /*iBuflen*/)
{
    return NULL;
}

extern "C" s32 LobbyRankRowAttrib(LobbyRankT* pRanker, s32 /*iRow*/, s32* /*pAttrib*/)
{
    return pRanker->bSelected ? LOBBYRANK_ERROR_INVALID_ROW : LOBBYRANK_ERROR_RANKINGS_NOT_DOWNLOADED;
}

extern "C" s32 LobbyRankRowCount(LobbyRankT* pRanker)
{
    return pRanker->bSelected ? 0 : LOBBYRANK_ERROR_RANKINGS_NOT_DOWNLOADED;
}

extern "C" const char* LobbyRankData(LobbyRankT* /*pRanker*/, s32 /*iColumn*/, s32 /*iRow*/, char* /*pBuffer*/,
                                     s32 /*iBuflen*/)
{
    return NULL;
}

extern "C" s32 LobbyRankCancelCB(LobbyRankT* /*pRanker*/)
{
    // No request is ever outstanding, so nothing is cancelled.
    return 0;
}

// ============================================================================
// LobbyStatbook
// ============================================================================

extern "C" LobbyStatbookT* LobbyStatbookCreate(LobbyApiRefT* pLobbyApi, u32 uSlots)
{
    const s32 iMemGroup = DirtyMemGroupQuery();
    if ((uSlots == 0) || (uSlots > KU_STATBOOK_SLOTS_MAX))
    {
        uSlots = KU_STATBOOK_SLOTS_DEFAULT;
    }

    LobbyStatbookT* pStatbook =
        static_cast<LobbyStatbookT*>(DirtyMemAlloc(sizeof(LobbyStatbookT), KI_MEMID_LOBBYSTATBOOK, iMemGroup));
    if (pStatbook == NULL)
    {
        return NULL;
    }
    memset(pStatbook, 0, sizeof(*pStatbook));
    pStatbook->pLobby    = pLobbyApi;
    pStatbook->iMemGroup = iMemGroup;
    pStatbook->uSlots    = uSlots;
    return pStatbook;
}

extern "C" void LobbyStatbookDestroy(LobbyStatbookT* pStatbook)
{
    DirtyMemFree(pStatbook, KI_MEMID_LOBBYSTATBOOK, pStatbook->iMemGroup);
}

extern "C" s32 LobbyStatbookFetch(LobbyStatbookT* /*pStatbook*/, u32 /*uSlot*/, const u8* /*pPersona*/)
{
    return LOBBYSTATBOOK_ERROR_VIEW_NOT_SELECTED;
}

extern "C" s32 LobbyStatbookFetchView(LobbyStatbookT* pStatbook, const u8* /*pView*/)
{
    // The view request is answered at once: it could not be served.
    pStatbook->iStatus = LOBBYSTATBOOK_ERROR_MISC;
    return 0;
}

extern "C" s32 LobbyStatbookFetchViewInfo(LobbyStatbookT* pStatbook)
{
    pStatbook->iStatus = LOBBYSTATBOOK_ERROR_MISC;
    return 0;
}

extern "C" s32 LobbyStatbookStatus(LobbyStatbookT* pStatbook, u32 uSlot)
{
    if (uSlot >= pStatbook->uSlots)
    {
        return LOBBYSTATBOOK_ERROR_BAD_SLOT;
    }
    return pStatbook->iStatus;
}

extern "C" s32 LobbyStatbookViewStatus(LobbyStatbookT* pStatbook)
{
    return pStatbook->iStatus;
}

extern "C" s32 LobbyStatbookViewInfoStatus(LobbyStatbookT* pStatbook)
{
    return pStatbook->iStatus;
}

extern "C" s32 LobbyStatbookViewCount(LobbyStatbookT* /*pStatbook*/)
{
    return 0;
}

extern "C" const char* LobbyStatbookRowData(LobbyStatbookT* /*pStatbook*/, u32 /*uSlot*/, u32 /*uRow*/,
                                            char* /*pBuffer*/, s32 /*iBuflen*/)
{
    return NULL;
}

extern "C" LobbyStatbookRowInfoT* LobbyStatbookRowInfo(LobbyStatbookT* /*pStatbook*/, u32 /*uRow*/)
{
    return NULL;
}

// ============================================================================
// LobbySetting
// ============================================================================

extern "C" LobbySettingRefT* LobbySettingCreate(LobbyApiRefT* pLobbyApi)
{
    const s32 iMemGroup = DirtyMemGroupQuery();
    LobbySettingRefT* pRef =
        static_cast<LobbySettingRefT*>(DirtyMemAlloc(sizeof(LobbySettingRefT), KI_MEMID_LOBBYSETTING, iMemGroup));
    if (pRef == NULL)
    {
        return NULL;
    }
    memset(pRef, 0, sizeof(*pRef));
    pRef->pLobby    = pLobbyApi;
    pRef->iMemGroup = iMemGroup;
    return pRef;
}

extern "C" void LobbySettingDestroy(LobbySettingRefT* pRef)
{
    DirtyMemFree(pRef, KI_MEMID_LOBBYSETTING, pRef->iMemGroup);
}

extern "C" s32 LobbySettingGetNumber(LobbySettingRefT* pRef, const char* pcKey, s32 iDefault)
{
    return TagFieldGetNumber(TagFieldFind(pRef->strSettings, pcKey), iDefault);
}

extern "C" void LobbySettingSetNumber(LobbySettingRefT* pRef, const char* pcKey, s32 iValue)
{
    TagFieldSetNumber(pRef->strSettings, KI_SETTINGS_RECORD_SIZE, pcKey, iValue);
}

extern "C" s32 LobbySettingLoad(LobbySettingRefT* pRef, LobbySettingCallbackT* pCallback, void* pUserData)
{
    if (!PcLanActive())
    {
        return 0;
    }

    // No settings service: the load completes with nothing stored, so every key reads its default.
    pRef->strSettings[0] = 0;
    if (pCallback != NULL)
    {
        pCallback(pRef, 0, pUserData);
    }
    return 1;
}

extern "C" s32 LobbySettingSave(LobbySettingRefT* pRef, LobbySettingCallbackT* pCallback, void* pUserData)
{
    if (!PcLanActive())
    {
        return 0;
    }

    // No settings service: the save completes and the local record is kept as it is.
    if (pCallback != NULL)
    {
        pCallback(pRef, 0, pUserData);
    }
    return 1;
}

// ============================================================================
// LobbyFindUser
// ============================================================================

extern "C" LobbyFindUserT* LobbyFindUserCreate(LobbyApiRefT* pLobbyApi)
{
    const s32 iMemGroup = DirtyMemGroupQuery();
    LobbyFindUserT* pFindUser =
        static_cast<LobbyFindUserT*>(DirtyMemAlloc(sizeof(LobbyFindUserT), KI_MEMID_LOBBYFINDUSER, iMemGroup));
    if (pFindUser == NULL)
    {
        return NULL;
    }
    memset(pFindUser, 0, sizeof(*pFindUser));
    pFindUser->pLobby       = pLobbyApi;
    pFindUser->iMemGroup    = iMemGroup;
    pFindUser->uRate        = KU_FINDUSER_RATE_DEFAULT;
    pFindUser->iCacheExpire = KI_FINDUSER_CACHE_EXPIRE_DEFAULT;
    return pFindUser;
}

extern "C" void LobbyFindUserDestroy(LobbyFindUserT* pFindUser)
{
    DirtyMemFree(pFindUser, KI_MEMID_LOBBYFINDUSER, pFindUser->iMemGroup);
}

extern "C" void LobbyFindUserSetParams(LobbyFindUserT* pFindUser, u32 uRate, s32 iCacheExpire)
{
    pFindUser->uRate        = (uRate < KU_FINDUSER_RATE_MIN) ? KU_FINDUSER_RATE_MIN : uRate;
    pFindUser->iCacheExpire = iCacheExpire;
}

extern "C" void LobbyFindUserCancel(LobbyFindUserT* /*pFindUser*/)
{
    // No lookup is ever in flight.
}

extern "C" s32 LobbyFindUser(LobbyFindUserT* pFindUser, const char* pcName, LobbyFindUserCallbackT* pCallback,
                             void* /*pUserData*/, s32 /*bOnlineOnly*/, s32 /*bUseCache*/)
{
    if ((pFindUser == NULL) || (pcName == NULL) || (pcName[0] == 0) || (pCallback == NULL))
    {
        return -3;
    }
    // No user directory to ask: the lookup request cannot be sent.
    return -1;
}
