// ============================================================================
// b5-decomp/src/GameSource/GameState/StreetData/BrnGameStateStreetManager_wB_05.cpp
// ============================================================================
// BrnGameState::StreetManager -- wave B partfile (group: online-small handlers).
//
// SOURCE-OF-TRUTH: the X360 ARTIST asm is the spine (stores/branches/constants);
// the frozen StreetManager layout header + BrnGameEvents.h event layouts + the
// committed BrnProgression::Profile members supply the named vocabulary. No raw
// `*(p+off)` casts: every offset the asm folds (GetProfile() == pm+0x170,
// muLastRoadRulesResetTime, the road-rules id split, etc.) is mapped to its
// committed named member.
//
//   ProcessUploadEvent            @ 0x8233F3E8  -- mark the uploaded index range clean
//   ProcessDownloadEvent          @ 0x82324B40  -- null-assert only (retail body empty)
//   ProcessOnlineGameLaunchedEvent@ 0x82324B88  -- clear buffer / bits / lost-score count
//
// ProcessConnectedOnlineEvent (@ 0x8234A148) is BLOCKED and left for a follow-up:
// its body reads/writes four BrnProgression::Profile road-rules members
// (muLastRoadRulesResetTime, muRoadRulesIDLowBits, muRoadRulesIDHighBits,
// muTimeStampOfLastRoadRulesDownload) that are private in BrnProfile.h with no
// public accessor, and StreetManager is not a friend of Profile -- unreachable
// without editing the frozen header.
// ============================================================================

#include "GameSource/GameState/StreetData/BrnGameStateStreetManager.h"

#include "GameSource/GameState/StreetData/BrnStreetManagerDebugComponent.h"  // embedded debug component
#include "GameSource/GameState/BrnGameEvents.h"                              // OnlineRoadRules* event layouts
#include "GameSource/GameState/BrnGameActions.h"                             // road-rules game-action family
#include "GameSource/GameState/BrnGameStateModule.h"                         // GameStateModule
#include "GameShared/GameClasses/RenderWare/Math/RwMathVectorTemplates.h"    // rw::math::fpu::Vector3Template<f32>

namespace BrnGameState
{

// ----------------------------------------------------------------------------
// ProcessUploadEvent  (X360 0x8233F3E8)
//
// For every challenge index in [mStartUploadIndex, mEndUploadIndex): read the
// player's current entry, clear its dirty bits (the score was just uploaded, so
// it no longer needs re-upload), and store it back WITHOUT a high-score update
// (lbUpdateHighScore == false). The X360 folds the clean into a single 8-byte
// zero store over the entry's leading BitArray<2u> mDirty field.
// ----------------------------------------------------------------------------
void StreetManager::ProcessUploadEvent(
    const GameStateModuleIO::OnlineRoadRulesUploadedEvent* lpUploadedEvent )
{
    CGS_ASSERT( lpUploadedEvent, "lpUploadedEvent" );

    for ( BrnStreetData::ChallengeIndex liChallengeIndex = lpUploadedEvent->mStartUploadIndex;
          liChallengeIndex < lpUploadedEvent->mEndUploadIndex;
          ++liChallengeIndex )
    {
        BrnStreetData::ChallengePlayerScoreEntry lEntry;
        GetChallengeUserScore( liChallengeIndex, &lEntry, false );
        lEntry.mDirty.UnSetAll();
        SetChallengeUserScore( liChallengeIndex, &lEntry, false );
    }
}

// ----------------------------------------------------------------------------
// ProcessDownloadEvent  (X360 0x82324B40)
//
// Retail body is the null-assert only -- the timestamp store the source once
// carried compiled away, so there is nothing else to reconstruct.
// ----------------------------------------------------------------------------
void StreetManager::ProcessDownloadEvent(
    const GameStateModuleIO::OnlineRoadRulesDownloadedEvent* lpDownloadedEvent )
{
    CGS_ASSERT( lpDownloadedEvent, "lpDownloadedEvent" );
}

// ----------------------------------------------------------------------------
// ProcessOnlineGameLaunchedEvent  (X360 0x82324B88)
//
// Reset the new-high-score buffering state at online-game launch: empty the
// buffer, drop the "too many to buffer" flag and the lost-score count, then clear
// every road-rules-changed bit array (one per score type).
// ----------------------------------------------------------------------------
void StreetManager::ProcessOnlineGameLaunchedEvent()
{
    mNewHighScoreBuffer.Clear();
    mbTooManyHighScoresToBuffer = false;
    miNumScoresLost             = 0;

    for ( BrnStreetData::ScoreType leScoreType = BrnStreetData::E_SCORE_TYPE_START;
          leScoreType < BrnStreetData::E_SCORE_TYPE_COUNT;
          leScoreType++ )
    {
        maRoadRulesChangedBitArrays[leScoreType].UnSetAll();
    }
}

}   // namespace BrnGameState
