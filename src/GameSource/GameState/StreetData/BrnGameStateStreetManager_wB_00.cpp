// ===========================================================================
// GameSource/GameState/StreetData/BrnGameStateStreetManager_wB_00.cpp
//   (wave B partfile -- group 0 "lifecycle": Construct / Destruct / Prepare)
//
// Faithful de-optimisation of the X360 BURNOUT_X360_ARTIST.XEX:
//   BrnGameState::StreetManager::Construct  @ 0x82335978
//   BrnGameState::StreetManager::Destruct   @ 0x82335D40
//   BrnGameState::StreetManager::Prepare    @ 0x82350900
// Every store / branch has an asm counterpart; members are the frozen-header
// named fields (BrnGameStateStreetManager.h), never raw-offset casts.
// ===========================================================================

#include "GameSource/GameState/StreetData/BrnGameStateStreetManager.h"
#include "GameSource/GameState/StreetData/BrnStreetManagerDebugComponent.h"
#include "GameSource/GameState/BrnGameEvents.h"
#include "GameSource/GameState/BrnGameActions.h"
#include "GameSource/GameState/Progression/BrnProgressionManager.h"
#include "GameSource/GameState/BrnGameStateModule.h"
#include "GameShared/GameClasses/RenderWare/Math/RwMathVectorTemplates.h"

#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"  // CgsDev::PerfMonCpu::AddMonitor
#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT

#include <cstring>   // memset

namespace BrnGameState
{
    // -----------------------------------------------------------------------
    // @ 0x82335978. Wire the three owner pointers, zero the score tables and the
    // whole walk / upcoming-roads state, construct the embedded debug component,
    // and register the five perf monitors.
    // -----------------------------------------------------------------------
    void StreetManager::Construct( GameStateModule* lpGameStateModule,
                                   BrnProgression::ProgressionManager* lpProgression,
                                   RoadRulesManager* lpRoadRulesManager )
    {
        // Embedded debug component: base Construct, miNumberOfRoadRulesToWin = 0,
        // mpStreetManager = this (the COMDAT-folded BaseCollisionGenerator::Destruct
        // at this+0x1DF0 is the DebugComponent base Construct).
        mStreetManagerDebugComponent.Construct( this );

        CGS_ASSERT( lpProgression, "lpProgression" );
        mpProgressionManager = lpProgression;

        CGS_ASSERT( lpGameStateModule, "lpGameStateModule" );
        mpGameStateModule = lpGameStateModule;

        CGS_ASSERT( lpRoadRulesManager, "lpRoadRulesManager" );
        mpRoadRulesManager = lpRoadRulesManager;

        meLoadStage            = E_LOAD_NOT_STARTED;
        meAILoadStage          = E_AI_DATA_LOAD_NOT_STARTED;
        meDistrictMapLoadStage = E_DISTRICT_MAP_LOAD_REQUEST;

        // Zero both score tables (X360 order: player table then network table).
        memset( maChallengeData, 0, sizeof( maChallengeData ) );
        memset( maNetworkChallengeData, 0, sizeof( maNetworkChallengeData ) );

        mNewHighScoreBuffer.Clear();
        mbTooManyHighScoresToBuffer = false;
        miNumScoresLost             = 0;
        mfUnbufferTimer             = 0.0f;
        meActiveRoadRuleType        = BrnStreetData::E_SCORE_TYPE_COUNT;
        mbFirstDownload             = true;

        for ( BrnStreetData::ScoreType leEnumIndex = BrnStreetData::E_SCORE_TYPE_START;
              leEnumIndex < BrnStreetData::E_SCORE_TYPE_COUNT;
              leEnumIndex++ )
        {
            maRoadRulesChangedBitArrays[leEnumIndex].UnSetAll();
        }

        miCurrentPlayerRoadIndex       = BrnStreetData::KI_INVALID_ROAD_INDEX;
        miLastPlayerRoadIndex          = BrnStreetData::KI_INVALID_ROAD_INDEX;
        miOriginalInterStateRoadIndex  = BrnStreetData::KI_INVALID_ROAD_INDEX;
        mCurrentPlayerSectionExitRight.SetZero();
        mFirstJunctionPosition.SetZero();
        miNewestForwardPlayerRoadIndex = BrnStreetData::KI_INVALID_ROAD_INDEX;
        miNewestLeftPlayerRoadIndex    = BrnStreetData::KI_INVALID_ROAD_INDEX;
        miNextFreeSectionSlot          = 0;
        miNewestRightPlayerRoadIndex   = BrnStreetData::KI_INVALID_ROAD_INDEX;
        mLeftRoadIndex                 = BrnStreetData::KI_INVALID_ROAD_INDEX;
        mRightRoadIndex                = BrnStreetData::KI_INVALID_ROAD_INDEX;
        mfHopefulRoadTimer             = 0.0f;
        mfGuidanceLockTime             = 0.0f;
        mbRightRoadHighlighted         = false;
        mfTimeInJunction               = 0.0f;
        mbLeftRoadHighlighted          = false;
        mfTotalTimeGoinTheWrongWay     = 0.0f;
        mbNeedToUpdateUpcomingRoads    = true;
        mbWrongWay                     = false;
        miHopefulForwardPlayerRoadIndex = BrnStreetData::KI_INVALID_ROAD_INDEX;
        miHopefulLeftPlayerRoadIndex   = BrnStreetData::KI_INVALID_ROAD_INDEX;
        miHopefulRightPlayerRoadIndex  = BrnStreetData::KI_INVALID_ROAD_INDEX;
        mLastNode.SetZero();
        mSecondLastNode.SetZero();
        miNewRoadNodeIndex             = 0;
        miLastRoadNodeIndex            = 0;
        mbNewRoadIsInterStateExit      = false;
        mbPlayerOnInterstateExit       = false;
        mbLockRightSign                = false;
        mbLockLeftSign                 = false;
        mbJunctionPosSet               = false;
        mbPlayerIsInAShortcut          = false;

        // Register the five perf monitors (page 5, no minimum, 1.0ms budget, lib-perf
        // tagged). The X360 uses the 5-parameter AddMonitor (the float budget reserves
        // the r6 GPR slot, so only r4/r5/f1/r7 are set at each call site).
        miSetRoadRuleChallengeDataPM =
            CgsDev::PerfMonCpu::AddMonitor( "SetRoadRuleChallengeData",
                                            static_cast<CgsDev::PerfMonCpuPage>( 5 ), false, 1.0f, true );
        miSetRoadRuleNetworkHighScoresPM =
            CgsDev::PerfMonCpu::AddMonitor( "SetRoadRuleNetworkHighScores",
                                            static_cast<CgsDev::PerfMonCpuPage>( 5 ), false, 1.0f, true );
        miUpcomingRoadsPM =
            CgsDev::PerfMonCpu::AddMonitor( "UpcomingRoads",
                                            static_cast<CgsDev::PerfMonCpuPage>( 5 ), false, 1.0f, true );
        miUpcomingRoadsSentMessagePM =
            CgsDev::PerfMonCpu::AddMonitor( "UpcomingRoadsSendMessage",
                                            static_cast<CgsDev::PerfMonCpuPage>( 5 ), false, 1.0f, true );
        miUpcomingRoadsQSortPM =
            CgsDev::PerfMonCpu::AddMonitor( "UpcomingRoadsQSort",
                                            static_cast<CgsDev::PerfMonCpuPage>( 5 ), false, 1.0f, true );

        CGS_ASSERT( miSetRoadRuleChallengeDataPM >= 0,     "miSetRoadRuleChallengeDataPM >= 0" );
        CGS_ASSERT( miSetRoadRuleNetworkHighScoresPM >= 0, "miSetRoadRuleNetworkHighScoresPM >= 0" );
        CGS_ASSERT( miUpcomingRoadsPM >= 0,                "miUpcomingRoadsPM >= 0" );
        CGS_ASSERT( miUpcomingRoadsSentMessagePM >= 0,     "miUpcomingRoadsSentMessagePM >= 0" );
        CGS_ASSERT( miUpcomingRoadsQSortPM >= 0,           "miUpcomingRoadsQSortPM >= 0" );
    }

    // -----------------------------------------------------------------------
    // @ 0x82335D40. Clear the changed-bit arrays and the buffered-score state,
    // zero both score tables, drop the game-state / progression owner pointers,
    // destruct the embedded debug component.
    // -----------------------------------------------------------------------
    void StreetManager::Destruct()
    {
        for ( BrnStreetData::ScoreType leEnumIndex = BrnStreetData::E_SCORE_TYPE_START;
              leEnumIndex < BrnStreetData::E_SCORE_TYPE_COUNT;
              leEnumIndex++ )
        {
            maRoadRulesChangedBitArrays[leEnumIndex].UnSetAll();
        }

        miNumScoresLost             = 0;
        mbTooManyHighScoresToBuffer = false;
        mNewHighScoreBuffer.Clear();
        mfUnbufferTimer             = 0.0f;
        meActiveRoadRuleType        = BrnStreetData::E_SCORE_TYPE_COUNT;
        mbFirstDownload             = true;

        memset( maChallengeData, 0, sizeof( maChallengeData ) );
        memset( maNetworkChallengeData, 0, sizeof( maNetworkChallengeData ) );

        mpGameStateModule    = NULL;
        mpProgressionManager = NULL;

        mStreetManagerDebugComponent.Destruct();
    }

    // -----------------------------------------------------------------------
    // @ 0x82350900. Streamed prepare: pump LoadAIData then LoadDistrictMap; once
    // both complete, (re)construct + register the debug component and report done.
    // -----------------------------------------------------------------------
    bool StreetManager::Prepare( GameStateModuleIO::OutputBuffer* lpOutput,
                                 CgsModule::EventReceiverQueue<3072,16>* lpReceiverQueue )
    {
        CGS_ASSERT( lpOutput, "lpOutput" );
        CGS_ASSERT( lpReceiverQueue, "lpReceiverQueue" );

        if ( LoadAIData( lpOutput, lpReceiverQueue ) && LoadDistrictMap( lpOutput, lpReceiverQueue ) )
        {
            mStreetManagerDebugComponent.Construct( this );
            mStreetManagerDebugComponent.Register();
            return true;
        }

        return false;
    }
}
