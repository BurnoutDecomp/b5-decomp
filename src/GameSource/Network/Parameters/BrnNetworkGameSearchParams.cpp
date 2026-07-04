#include "BrnNetworkGameSearchParams.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
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
    // Free helper the LiveRevengeManager TU owns (maps a table index to a
    // LiveRevengeRelationship* inside the profile). Declared here so AreRivalsInSameGame
    // can name it; the definition is resolved at link time.
    LiveRevengeRelationship* LiveRevengeRelations(LiveRevengeProfile* lpProfile, s32 liTableIndex);

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

        // 2. Bulk game-side block: memcpy(this+0x6C, rhs+0x6C, 0x2A0).
        memcpy(maGameSearchPayload, lrhs.maGameSearchPayload, sizeof(maGameSearchPayload));

        // 3. Trailing block: 0x83 bytes copied one at a time (asm lbzx/stb loop).
        for (s32 li = 0; li < static_cast<s32>(sizeof(maGameSearchTail)); ++li)
            maGameSearchTail[li] = lrhs.maGameSearchTail[li];

        return *this;
    }

    // ========================================================================
    // GameSearchParams::AreRivalsInSameGame  @ 0x82590FC0
    //
    // For every stored live-revenge rival with a non-zero takedown history, ask
    // Xbox LIVE presence whether that rival is currently present in a Burnout
    // title, and write the yes/no answer into lpbRivalInSameGame in relationship
    // order. Returns the current revenge-relationship count (the value left in r3
    // by the final GetNumberOfRelationships call).
    // ========================================================================
    s32 GameSearchParams::AreRivalsInSameGame(bool* lpbRivalInSameGame,
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
                BrnNetwork::LiveRevengeRelations(lpProfile, liIndex);

            CGS_ASSERT(lpRelationship->GetTotalTakedowns() >= 0,
                       "mOverallStats.mPlayerStats.miTakedowns + mOverallStats.mRivalStats.miTakedowns >= 0");
            if (lpRelationship->GetTotalTakedowns() > 0)
            {
                CGS_ASSERT(lpProfile, "mpLiveRevengeProfile");
                laXUIDs[liNumPeers] =
                    BrnNetwork::LiveRevengeRelations(lpProfile, liIndex)->GetRivalXUID();
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
                BrnNetwork::LiveRevengeRelations(lpProfile, liIndex);

            CGS_ASSERT(lpRelationship->GetTotalTakedowns() >= 0,
                       "mOverallStats.mPlayerStats.miTakedowns + mOverallStats.mRivalStats.miTakedowns >= 0");
            if (lpRelationship->GetTotalTakedowns() > 0)
            {
                CGS_ASSERT(lpProfile, "mpLiveRevengeProfile");
                CGS_ASSERT(
                    lpPresence->GetXuid() ==
                        BrnNetwork::LiveRevengeRelations(lpProfile, liIndex)->GetRivalXUID(),
                    "laPresence[ liNumPeers ].xuid == lpNetworkManager->GetLiveRevengeManager()->GetRevengeRelationshipByIndex( liIndex )->GetRivalXUID()");

                *lpbOut = (lpPresence->GetTitleId() == KU_BURNOUT_TITLE_ID);
                ++lpbOut;
                ++lpPresence;
            }

            liNumRelationships = lpLiveRevengeManager->GetNumberOfRelationships();
        }

        return liNumRelationships;
    }
}
