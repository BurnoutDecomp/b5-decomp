// ===========================================================================
// GameSource/GameState/StreetData/BrnGameStateStreetManager_wB_03.cpp
//   (wave B partfile -- group 3 "set-scores":
//        SetChallengeUserScore / CopyUserChallengeData / ClearAllChallengeData)
//
// Faithful de-optimisation of the X360 BURNOUT_X360_ARTIST.XEX:
//   BrnGameState::StreetManager::SetChallengeUserScore  @ 0x82336070
//   BrnGameState::StreetManager::CopyUserChallengeData  @ 0x82317510
//   BrnGameState::StreetManager::ClearAllChallengeData  @ 0x82324CA8
// Every store / branch / early-out has an asm counterpart; members are the
// frozen-header named fields (BrnGameStateStreetManager.h), never raw-offset
// casts.
// ===========================================================================

#include "GameSource/GameState/StreetData/BrnGameStateStreetManager.h"
#include "GameSource/GameState/StreetData/BrnStreetManagerDebugComponent.h"
#include "GameSource/GameState/BrnGameEvents.h"
#include "GameSource/GameState/BrnGameActions.h"
#include "GameSource/GameState/Progression/BrnProgressionManager.h"
#include "GameSource/GameState/BrnGameStateModule.h"
#include "GameShared/GameClasses/RenderWare/Math/RwMathVectorTemplates.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

#include <cstring>   // memcpy

namespace BrnGameState
{
    // -----------------------------------------------------------------------
    // @ 0x82336070. Store one local-player challenge score entry into a slot.
    // When lbUpdateHighScore is set the passed index is a ROAD index: it is
    // resolved through the save-game challenge road-id table first (a miss
    // early-outs without writing). Once resolved, a bounds-checked slot in
    // maChallengeData receives the entry via ChallengePlayerScoreEntry::Copy.
    // -----------------------------------------------------------------------
    void StreetManager::SetChallengeUserScore( BrnStreetData::ChallengeIndex liRoadIndex,
                                               const BrnStreetData::ChallengePlayerScoreEntry* lpEntry,
                                               bool lbUpdateHighScore )
    {
        CGS_ASSERT( lpEntry, "lpData" );

        if ( lbUpdateHighScore )
        {
            const BrnStreetData::Road* lpRoad = GetStreetData()->GetRoad( liRoadIndex );

            BrnStreetData::ChallengeIndex liChallengeIndex = 0;
            const ::CgsID* lpTableEntry = KAA_SAVE_GAME_CHALLENGE_ROAD_IDS;
            while ( *lpTableEntry != lpRoad->GetId() )
            {
                ++lpTableEntry;
                ++liChallengeIndex;
                if ( lpTableEntry >= &KAA_SAVE_GAME_CHALLENGE_ROAD_IDS[KI_MAX_CHALLENGES] )
                {
                    return;
                }
            }
            liRoadIndex = liChallengeIndex;
        }

        if ( liRoadIndex >= 0 )
        {
            CGS_ASSERT( liRoadIndex < KI_MAX_CHALLENGES,
                        "liChallengeIndex < BrnGameState::KI_MAX_CHALLENGES" );
            maChallengeData[liRoadIndex].Copy( lpEntry );
        }
    }

    // -----------------------------------------------------------------------
    // @ 0x82317510. Bulk-copy the whole 64-entry local-player challenge table
    // out to lpDest, guarded on the destination buffer being large enough.
    // -----------------------------------------------------------------------
    void StreetManager::CopyUserChallengeData( BrnStreetData::ChallengePlayerScoreEntry* lpDest,
                                               s32 liSizeInBytes )
    {
        CGS_ASSERT( lpDest, "lpDest" );

        if ( liSizeInBytes >= static_cast<s32>( sizeof( maChallengeData ) ) )
        {
            memcpy( lpDest, maChallengeData, sizeof( maChallengeData ) );
        }
        else
        {
            // X360 composes this through a gpcMessageBuffer StrStream; per the
            // project convention the streamed message lowers to CGS_ASSERT.
            CGS_ASSERT( false, " not enough space to copy into " );
        }
    }

    // -----------------------------------------------------------------------
    // @ 0x82324CA8. Reset every challenge row: re-Construct the network
    // high-score entry (base ChallengeData::Construct + blank owner names) then
    // the local-player score entry, for all KI_MAX_CHALLENGES rows.
    // -----------------------------------------------------------------------
    void StreetManager::ClearAllChallengeData()
    {
        for ( BrnStreetData::ChallengeIndex liChallenge = 0;
              liChallenge < KI_MAX_CHALLENGES;
              ++liChallenge )
        {
            maNetworkChallengeData[liChallenge].Construct();
            maChallengeData[liChallenge].Construct();
        }
    }
}
