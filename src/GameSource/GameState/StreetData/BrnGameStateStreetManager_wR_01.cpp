// ===========================================================================
// GameSource/GameState/StreetData/BrnGameStateStreetManager_wR_01.cpp
//   (road-rules wave partfile -- the per-frame pump and its score helpers)
//
//   BrnGameState::StreetManager::Update
//   BrnGameState::StreetManager::UpdateRoadRulesProfileScores
//   BrnGameState::StreetManager::GetHighScoreEntry
//   BrnGameState::StreetManager::GetNumberOfRoadsRulesByLocalPlayer
//
// UpdateBufferedHighScores, which Update also calls, lives in _wC_00 beside
// ProcessNewRoadScore: both post the same new-high-score record.
// ===========================================================================

#include "GameSource/GameState/StreetData/BrnGameStateStreetManager.h"

#include "GameSource/GameState/BrnGameStateModuleIO.h"                        // PreWorldInputBuffer::GetNetworkToGameStateInterface
#include "GameSource/Network/SharedIO/BrnNetworkModuleGameStateIOInterfaces.h" // NetworkToGameStateInterface queue accessors
#include "GameSource/GameState/Progression/BrnProgressionManager.h"           // SetRoadRuleNetworkHighScores / SetRoadRuleChallengeData
#include "GameSource/GameState/StreetData/BrnChallengeHighScoreEntry.h"       // ChallengeHighScoreEntry Construct / SetScore / GetScore / UpdateEntry
#include "SharedClasses/StreetData/BrnChallengeData.h"                        // ChallengeData::ContainsData / GetScore, ScoreType operator++
#include "GameSource/GameState/BrnCgsPlayerName.h"                            // CgsNetwork::PlayerName
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"     // CgsDev::PerfMonCpu::StartMonitor / StopMonitor
#include "GameShared/GameClasses/Core/CgsAssert.h"                            // CGS_ASSERT

#include <string.h>

// Case-insensitive bounded compare (console strnicmp -> MSVC _strnicmp), the same
// spelling as _wC_02.
#if defined(_MSC_VER)
#  define strnicmp _strnicmp
#endif

namespace BrnGameState
{

// ----------------------------------------------------------------------------
// The per-frame pump. The debug component ticks first; then, unless the current
// mode disables the upcoming-road signs, the wrong-way timer and the upcoming-street
// walk (under the "UpcomingRoads" monitor); then the downloaded friend records and
// the server's copy of the local scores are merged, the new-high-score buffer is
// drained and the live tables are copied into the profile.
// ----------------------------------------------------------------------------
void StreetManager::Update( bool lbIsAnythingPaused,
                            f32 lfSimTimeStep,
                            const GameStateModuleIO::PreWorldInputBuffer* lpInput,
                            GameStateModuleIO::OutputBuffer* lpOutput,
                            BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpActiveRaceCarInterface,
                            BrnAI::AIModuleIO::AICarOutputInterface* lpLastAICarOutputInterface,
                            bool lbCurrentGameModeHasRoute,
                            bool lbCurrentGameModeDisablesUpcoming,
                            f32 lfTimeGoingTheWrongWay )
{
    mStreetManagerDebugComponent.Update( lpOutput );

    CGS_ASSERT( lpInput, "lpInput" );
    CGS_ASSERT( lpInput->GetNetworkToGameStateInterface(), "lpInput->GetNetworkToGameStateInterface()" );

    if ( !lbCurrentGameModeDisablesUpcoming )
    {
        // A zero, negative or NaN wrong-way time resets the running total; the flag
        // raises once the total is no longer below ten seconds.
        if ( lfTimeGoingTheWrongWay > 0.0f )
        {
            mfTotalTimeGoinTheWrongWay += lfTimeGoingTheWrongWay;
            mbWrongWay = !( mfTotalTimeGoinTheWrongWay < 10.0f );
        }
        else
        {
            mfTotalTimeGoinTheWrongWay = 0.0f;
            mbWrongWay                 = false;
        }

        CgsDev::PerfMonCpu::StartMonitor( miUpcomingRoadsPM );
        UpdateUpcomingStreets( lpActiveRaceCarInterface, lpLastAICarOutputInterface, lpOutput,
                               lfSimTimeStep, lbCurrentGameModeHasRoute );
        CgsDev::PerfMonCpu::StopMonitor( miUpcomingRoadsPM );
    }

    UpdateFriendHighScores( lpInput->GetNetworkToGameStateInterface()->GetRoadRulesDownloadedQueue(), lpOutput );
    UpdateUserScoresFromServerRecords( lpInput->GetNetworkToGameStateInterface()->GetLocalRoadRulesDownloadedQueue(), lpOutput );
    UpdateBufferedHighScores( lfSimTimeStep, lbIsAnythingPaused, lpOutput );
    UpdateRoadRulesProfileScores();
}

// ----------------------------------------------------------------------------
// Copies the live network and local score tables into the profile, each copy under
// its own monitor.
// ----------------------------------------------------------------------------
void StreetManager::UpdateRoadRulesProfileScores()
{
    CGS_ASSERT( mpProgressionManager, "mpProgressionManager" );

    CgsDev::PerfMonCpu::StartMonitor( miSetRoadRuleNetworkHighScoresPM );
    mpProgressionManager->SetRoadRuleNetworkHighScores( maNetworkChallengeData );
    CgsDev::PerfMonCpu::StopMonitor( miSetRoadRuleNetworkHighScoresPM );

    CgsDev::PerfMonCpu::StartMonitor( miSetRoadRuleChallengeDataPM );
    mpProgressionManager->SetRoadRuleChallengeData( maChallengeData );
    CgsDev::PerfMonCpu::StopMonitor( miSetRoadRuleChallengeDataPM );
}

// ----------------------------------------------------------------------------
// The road's best score per type across the friend table and the local player: the
// friend record, updated with an entry holding the local player's scores under the
// local-player name.
// ----------------------------------------------------------------------------
void StreetManager::GetHighScoreEntry( BrnStreetData::ChallengeIndex liIndex,
                                       BrnStreetData::ChallengeHighScoreEntry* lpEntry,
                                       bool lbByRoadIndex )
{
    GetChallengeFriendHighScore( liIndex, lpEntry, lbByRoadIndex );

    BrnStreetData::ChallengeHighScoreEntry lLocalPlayerEntry;
    lLocalPlayerEntry.Construct();

    CgsNetwork::PlayerName lLocalPlayerName;
    lLocalPlayerName.Construct( KAC_LOCAL_PLAYER_NAME_TEXT );

    BrnStreetData::ChallengePlayerScoreEntry lUserScore;
    GetChallengeUserScore( liIndex, &lUserScore, lbByRoadIndex );

    for ( BrnStreetData::ScoreType leScoreType = BrnStreetData::E_SCORE_TYPE_START;
          leScoreType < BrnStreetData::E_SCORE_TYPE_COUNT;
          leScoreType++ )
    {
        if ( lUserScore.ContainsData( leScoreType ) )
        {
            lLocalPlayerEntry.SetScore( leScoreType, lUserScore.GetScore( leScoreType ), &lLocalPlayerName );
        }
    }

    lpEntry->UpdateEntry( &lLocalPlayerEntry, nullptr );
}

// ----------------------------------------------------------------------------
// Counts every (road, score type) whose high score is held by the local player.
// ----------------------------------------------------------------------------
s32 StreetManager::GetNumberOfRoadsRulesByLocalPlayer()
{
    s32 liNumberOfRoadsRuled = 0;

    for ( BrnStreetData::ChallengeIndex liIndex = 0; liIndex < KI_MAX_CHALLENGES; ++liIndex )
    {
        BrnStreetData::ChallengeHighScoreEntry lHighScoreEntry;
        GetHighScoreEntry( liIndex, &lHighScoreEntry, false );

        for ( BrnStreetData::ScoreType leScoreType = BrnStreetData::E_SCORE_TYPE_START;
              leScoreType < BrnStreetData::E_SCORE_TYPE_COUNT;
              leScoreType++ )
        {
            if ( lHighScoreEntry.ContainsData( leScoreType ) )
            {
                int32_t                liScore;
                CgsNetwork::PlayerName lHolderName;
                lHighScoreEntry.GetScore( leScoreType, &liScore, &lHolderName );

                if ( strnicmp( lHolderName.macName, KAC_LOCAL_PLAYER_NAME_TEXT, 16 ) == 0 )
                {
                    ++liNumberOfRoadsRuled;
                }
            }
        }
    }

    return liNumberOfRoadsRuled;
}

} // namespace BrnGameState
