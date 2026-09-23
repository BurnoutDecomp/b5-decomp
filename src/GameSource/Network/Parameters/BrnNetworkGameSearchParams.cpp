#include "BrnNetworkGameSearchParams.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Core/CgsStringUtils.h"                          // CgsCore::SPrintf
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceGameFlags.h" // KU_GAME_FLAGS_RANKED
#include "GameSource/Network/Parameters/BrnNetworkParameterData.h"             // SetUpContexts, MatchmakingContext
// Include the FULL LiveRevengeManager header FIRST: it defines the real
// BrnNetwork::LiveRevengeManager (struct) and sets BRNETWORK_LIVEREVENGEMANAGER_DEFINED,
// which suppresses BrnNetworkManager.h's minimal `class LiveRevengeManager` slice (a
// struct/class tag mismatch otherwise).
#include "GameSource/Network/Managers/BrnNetworkLiveRevengeManager.h"     // LiveRevengeManager / LiveRevengeProfile
#include "GameSource/Network/Managers/BrnNetworkLiveRevengeRelationship.h"// LiveRevengeRelationship::GetTotalTakedowns / GetRivalXUID
#include "GameSource/Network/BrnNetworkManager.h"                         // BrnNetworkManager::GetLiveRevengeManager
#include <string.h>

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnNetwork::GameSearchParamsBase::operator=              @ 0x8255EA80
//   BrnNetwork::GameSearchParamsX360::AreRivalsInSameGame    @ 0x82590FC0
//   BrnNetwork::GameSearchParamsBase::Prepare

// ---- XDK / Xbox-LIVE presence entry points -----------------------------------
// Real prototypes live in the Xbox 360 XDK (<xonline.h>/<xapi.h>); declared here as
// file-scope free functions, mirroring the System/X360 glue precedent
// (CgsServerInterfaceGames.cpp declares XMemSet the same way; CgsXOverlappedX360.cpp
// declares the X* APIs as extern "C"). Argument shapes are taken from the X360 call
// sites (register usage), not the PPC Hex-Rays.
extern "C"
{
    // XMemSet(pDest, c, count) -- X360 fast memset (asm: r3=pDest, r4=c, r5=count).
    void* XMemSet(void* lpDest, s32 liValue, u32 luCount);

    // XPresenceCreateEnumerator(user, cPeers, pPeers, startIndex, peersToReturn,
    //   pcbBuffer, phEnum) -- 0 == ERROR_SUCCESS. pPeers is an array of XUID (u64).
    u32 XPresenceCreateEnumerator(u32 luUserIndex, u32 luPeers, const u64* lpaPeers,
                                  u32 luStartingIndex, u32 luPeersToReturn,
                                  u32* lpcbBuffer, void** lphEnum);

    // XEnumerate(hEnum, pvBuffer, cbBuffer, pcItemsReturned, pOverlapped).
    u32 XEnumerate(void* lhEnum, void* lpvBuffer, u32 lcbBuffer,
                   u32* lpcItemsReturned, void* lpOverlapped);
}

namespace BrnNetwork
{
    // The XDK XPRESENCE_INFO record. On X360 the presence array is strided by 0xA4 == 164
    // bytes (asm: XMemSet count 0xA028 == 250 * 164; XEnumerate cbBuffer == 164 * count;
    // second-loop stride addi r29,r29,0xA4). Only two fields are read by this TU, both by
    // raw offset off the record base, so the whole record is modelled as a 164-byte opaque
    // span (KI_STRIDE) with typed accessors at the attested offsets rather than a field
    // struct -- a named u64 member would force 8-byte alignment and round sizeof up to 168,
    // breaking the 164-byte stride. The two attested fields:
    //   +0x00  peer XUID   (u64)   asm ld r11,-0x14(r29) with r29 = base+0x14
    //   +0x14  title id    (u32)   asm lwz r11,0(r29)     with r29 = base+0x14
    struct XPresenceInfoView
    {
        static const u32 KI_STRIDE = 0xA4;   // 164 bytes, X360-attested presence stride

        u8 maRecord[KI_STRIDE];

        u64 GetXuid() const
        {
            u64 lRet;
            for (u32 li = 0; li < sizeof(u64); ++li)
                reinterpret_cast<u8*>(&lRet)[li] = maRecord[li];   // xuid @ +0x00
            return lRet;
        }
        u32 GetTitleId() const
        {
            u32 lRet;
            for (u32 li = 0; li < sizeof(u32); ++li)
                reinterpret_cast<u8*>(&lRet)[li] = maRecord[0x14 + li]; // title id @ +0x14
            return lRet;
        }
    };

    // The Burnout title id a peer must be present in for it to count as "in the same
    // game" (asm cmplw against 0x45410806).
    static const u32 KU_BURNOUT_TITLE_ID = 0x45410806u;

    // Enumerate at most 100 peers per XPresenceCreateEnumerator call (asm clamps
    // cPeers to 0x64).
    static const s32 KI_MAX_PEERS_PER_ENUMERATION = 100;


    // operator= (asm @ 0x8255EA80):
    //   bl  CgsNetwork__ServerInterfaceGameSearchParamsX360   -> base copy-assign
    //   memcpy(this+0x6C, rhs+0x6C, 0x2A0)                     -> bulk payload
    //   0x83-byte lbzx/stb loop over this+0x30C from rhs+0x30C -> trailing block
    GameSearchParamsBase& GameSearchParamsBase::operator=(const GameSearchParamsBase& lrhs)
    {
        // 1. Base subobject copy-assignment (the X360 search-params block).
        static_cast<CgsNetwork::ServerInterfaceGameSearchParams&>(*this) =
            static_cast<const CgsNetwork::ServerInterfaceGameSearchParams&>(lrhs);

        // 2. The search payload, copied whole.
        memcpy(&mSearchData, &lrhs.mSearchData, sizeof(mSearchData));

        // 3. The pattern string, copied one byte at a time.
        for (s32 li = 0; li < KI_PATTERN_SIZE; ++li)
            macPattern[li] = lrhs.macPattern[li];

        return *this;
    }

    // Seed the platform search block, build the pattern (forty 16-char rival names, then the
    // search fields), store the search fields, ask for ranked games only when lbIsRanked, fill
    // in the rival names and publish the matchmaking contexts. The ranked byte of the payload
    // is not written here.
    bool GameSearchParamsBase::Prepare(s32 leGameSearchGameMode, EBrnGameState leGameState, u32 luSkillLevel,
                                       s32 leOpponentType, bool lbIsRanked, bool lbIsFreeburn,
                                       u32 luRequiredSlots, CgsNetwork::EFirewallSettings leFirewallSettings,
                                       BrnNetworkManager* lpNetworkManager, u32 luField29C)
    {
        CGS_ASSERT(lpNetworkManager, "lpNetworkManager");

        if (!CgsNetwork::ServerInterfaceGameSearchParams::Prepare())
        {
            return false;
        }

        macPattern[0] = 0;
        for (s32 liIndex = 0; liIndex < SearchData::KI_MAX_PLAYERS_UPLOAD; ++liIndex)
        {
            char lacField[10];
            CgsCore::SPrintf(lacField, sizeof(lacField), "%ds", SearchData::KI_MAX_PLAYER_NAME_LENGTH);
            strncat(macPattern, lacField, KI_PATTERN_SIZE);
            mSearchData.macPlayerNames[liIndex][0] = 0;
        }
        strncat(macPattern, "llllllbbwl", KI_PATTERN_SIZE);

        mSearchData.meGameSearchGameMode = leGameSearchGameMode;
        mSearchData.meGameState          = leGameState;
        mSearchData.muSkillLevel         = luSkillLevel;
        mSearchData.meOpponentType       = leOpponentType;
        mSearchData.muRequiredSlots      = luRequiredSlots;
        mSearchData.mbIsFreeburn         = lbIsFreeburn;
        mSearchData.mu16Field29A         = 0;
        mSearchData.muField29C           = luField29C;

        muGameFlagsMask  = CgsNetwork::KU_GAME_FLAGS_RANKED;
        muGameFlagsValue = lbIsRanked ? CgsNetwork::KU_GAME_FLAGS_RANKED : 0;

        mSearchData.meFirewallSettings = leFirewallSettings;
        FillInRivals(lpNetworkManager);

        // The platform context list is { id, value } pairs with the count after it; the
        // context helper takes it as a MatchmakingContext array and an s32 count.
        SetUpContexts(leGameSearchGameMode, static_cast<char>(lbIsRanked),
                      reinterpret_cast<s32*>(&muX360Field_68),
                      reinterpret_cast<MatchmakingContext*>(maX360Payload));
        return true;
    }

    // The pattern Prepare built.
    const char* GameSearchParamsBase::GetPattern() const
    {
        return macPattern;
    }

    // The pattern buffer's length.
    s32 GameSearchParamsBase::GetPatternLength() const
    {
        return KI_PATTERN_SIZE;
    }

    // The replicated payload's size (0x2A0).
    u32 GameSearchParamsBase::GetDataSize() const
    {
        return sizeof(mSearchData);
    }

    // The replicated payload. The const overload shares the body.
    void* GameSearchParamsBase::GetData()
    {
        return &mSearchData;
    }

    const void* GameSearchParamsBase::GetData() const
    {
        return &mSearchData;
    }

    // No custom-flag filtering: the console body of both slots is a bare `return 0`
    // (shared by identical code folding with other such functions).
    u32 GameSearchParamsBase::GetCustomFlagsMask() const
    {
        return 0;
    }

    u32 GameSearchParamsBase::GetCustomFlagsValue() const
    {
        return 0;
    }

    // ========================================================================
    // GameSearchParams::AreRivalsInSameGame  @ 0x82590FC0
    //
    // For every stored live-revenge rival with a non-zero takedown history, ask
    // Xbox LIVE presence whether that rival is currently present in a Burnout
    // title, and write the yes/no answer into lpbRivalInSameGame in relationship
    // order.
    // ========================================================================
    void GameSearchParams::AreRivalsInSameGame(bool* lpbRivalInSameGame,
                                               BrnNetworkManager* lpNetworkManager)
    {
        CGS_ASSERT(lpbRivalInSameGame, "lpbRivalInSameGame");
        CGS_ASSERT(lpNetworkManager, "lpNetworkManager");

        LiveRevengeManager* lpLiveRevengeManager = lpNetworkManager->GetLiveRevengeManager();

        // Local scratch, matching the X360 stack frame.
        u64 laXUIDs[LiveRevengeProfile::KI_MAX_REVENGE_HISTORY];      // one per relationship
        XPresenceInfoView laPresence[LiveRevengeProfile::KI_MAX_REVENGE_HISTORY];

        // ---- Pass 1: collect the XUID of every rival with a takedown history. ----
        s32 liNumPeers = 0;
        for (s32 liIndex = 0;
             liIndex < lpLiveRevengeManager->GetNumberOfRelationships();
             ++liIndex)
        {
            LiveRevengeProfile* lpProfile = lpLiveRevengeManager->GetProfile();
            CGS_ASSERT(lpProfile, "mpLiveRevengeProfile");
            LiveRevengeRelationship* lpRelationship =
                &lpProfile->maRelationshipTable[static_cast<u32>(liIndex)];

            CGS_ASSERT(lpRelationship->GetTotalTakedowns() >= 0,
                       "mOverallStats.mPlayerStats.miTakedowns + mOverallStats.mRivalStats.miTakedowns >= 0");
            if (lpRelationship->GetTotalTakedowns() > 0)
            {
                CGS_ASSERT(lpProfile, "mpLiveRevengeProfile");
                laXUIDs[liNumPeers] =
                    lpProfile->maRelationshipTable[static_cast<u32>(liIndex)].GetRivalXUID();
                ++liNumPeers;
            }
        }

        // ---- Pass 2: batch-query LIVE presence for those peers. ----
        XMemSet(laPresence, 0, sizeof(laPresence));   // 250 * 164 == 0xA028
        for (s32 liNumPeersRetrieved = 0; liNumPeers > 0; )
        {
            s32 liNumPeersToRetrieve = liNumPeers;
            if (liNumPeersToRetrieve >= KI_MAX_PEERS_PER_ENUMERATION)
                liNumPeersToRetrieve = KI_MAX_PEERS_PER_ENUMERATION;

            u32   lBufferSize = 0;
            void* lEnumerationHandle = 0;
            CGS_ASSERT(
                XPresenceCreateEnumerator(0, static_cast<u32>(liNumPeers), laXUIDs,
                                          static_cast<u32>(liNumPeersRetrieved),
                                          static_cast<u32>(liNumPeersToRetrieve),
                                          &lBufferSize, &lEnumerationHandle) == 0,
                "XPresenceCreateEnumerator( 0, liNumPeers, laXUIDs, liNumPeersRetrieved, liNumPeersToRetrieve, &lBufferSize, &lEnumerationHandle ) == ERROR_SUCCESS");

            u32 liItemsReturned = 0;
            XEnumerate(lEnumerationHandle, &laPresence[liNumPeersRetrieved],
                       static_cast<u32>(XPresenceInfoView::KI_STRIDE * liNumPeersToRetrieve),
                       &liItemsReturned, 0);

            liNumPeers          -= liNumPeersToRetrieve;
            liNumPeersRetrieved += liNumPeersToRetrieve;
        }

        // ---- Pass 3: fold the presence results back into lpbRivalInSameGame. ----
        bool* lpbOut = lpbRivalInSameGame;
        const XPresenceInfoView* lpPresence = laPresence;
        s32 liNumRelationships = lpLiveRevengeManager->GetNumberOfRelationships();
        for (s32 liIndex = 0; liIndex < liNumRelationships; ++liIndex)
        {
            LiveRevengeProfile* lpProfile = lpLiveRevengeManager->GetProfile();
            CGS_ASSERT(lpProfile, "mpLiveRevengeProfile");
            LiveRevengeRelationship* lpRelationship =
                &lpProfile->maRelationshipTable[static_cast<u32>(liIndex)];

            CGS_ASSERT(lpRelationship->GetTotalTakedowns() >= 0,
                       "mOverallStats.mPlayerStats.miTakedowns + mOverallStats.mRivalStats.miTakedowns >= 0");
            if (lpRelationship->GetTotalTakedowns() > 0)
            {
                CGS_ASSERT(lpProfile, "mpLiveRevengeProfile");
                CGS_ASSERT(
                    lpPresence->GetXuid() ==
                        lpProfile->maRelationshipTable[static_cast<u32>(liIndex)].GetRivalXUID(),
                    "laPresence[ liNumPeers ].xuid == lpNetworkManager->GetLiveRevengeManager()->GetRevengeRelationshipByIndex( liIndex )->GetRivalXUID()");

                *lpbOut = (lpPresence->GetTitleId() == KU_BURNOUT_TITLE_ID);
                ++lpbOut;
                ++lpPresence;
            }

            liNumRelationships = lpLiveRevengeManager->GetNumberOfRelationships();
        }
    }

    // Put the rivals the search should look for into the payload's name slots: for a rivals
    // search (with or without friends), every rival with a takedown history who is in the same
    // title right now, up to the slot count. Other searches leave the slots empty.
    void GameSearchParamsBase::FillInRivals(BrnNetworkManager* lpNetworkManager)
    {
        CGS_ASSERT(lpNetworkManager, "lpNetworkManager");

        s32  liPlayerNameIndex = 0;
        bool lbRivalInSameGame[LiveRevengeProfile::KI_MAX_REVENGE_HISTORY];
        AreRivalsInSameGame(lbRivalInSameGame, lpNetworkManager);

        switch (mSearchData.meOpponentType)
        {
        case E_SEARCH_OPPONENT_TYPES_FRIENDS_AND_RIVALS:
        case E_SEARCH_OPPONENT_TYPES_RIVALS:
        {
            LiveRevengeManager* lpLiveRevengeManager = lpNetworkManager->GetLiveRevengeManager();
            s32 liValidRivalIndex = 0;
            for (s32 liRivalIndex = 0;
                 liRivalIndex < lpLiveRevengeManager->GetNumberOfRelationships()
                     && liPlayerNameIndex < SearchData::KI_MAX_PLAYERS_UPLOAD;
                 ++liRivalIndex)
            {
                if (lpLiveRevengeManager->GetRevengeRelationshipByIndex(liRivalIndex)->GetTotalTakedowns() > 0)
                {
                    if (lbRivalInSameGame[liValidRivalIndex])
                    {
                        const char* lpcRivalName =
                            lpLiveRevengeManager->GetRevengeRelationshipByIndex(liRivalIndex)->GetRivalName()->GetPlayerName();
                        CGS_ASSERT(strlen(lpcRivalName) < static_cast<u32>(SearchData::KI_MAX_PLAYER_NAME_LENGTH),
                                   "String too long");
                        strncpy(mSearchData.macPlayerNames[liPlayerNameIndex], lpcRivalName,
                                SearchData::KI_MAX_PLAYER_NAME_LENGTH);
                        ++liPlayerNameIndex;
                    }
                    ++liValidRivalIndex;
                }
            }
            break;
        }

        case E_SEARCH_OPPONENT_TYPES_ANY:
        case E_SEARCH_OPPONENT_TYPES_FRIENDS:
            break;

        default:
            CGS_ASSERT(false, "Invalid opponent type");
            break;
        }
    }
}
