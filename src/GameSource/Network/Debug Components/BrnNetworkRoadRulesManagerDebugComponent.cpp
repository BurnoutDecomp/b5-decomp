// Bodies for the online road-rules debug component, reconstructed from BURNOUT_X360_ARTIST.XEX:
//   Construct                     @ 0x82586350
//   GetName                       @ 0x82586398
//   OnActivate                    @ 0x8258AF90
//   TriggerPersonalBest           @ 0x8258AEB0
//   RequestRoadRulesHighScores    @ 0x825863A8
//
// The component registers two manual menu actions against the real CgsDev::DebugComponent menu API.
// The X360's `sub_8282F720(this, cb, this, name)` is DebugComponent::RegisterFunction(cb, this, name).
// The leading `BaseCollisionGenerator::Destruct(this)` the decompiler shows at the head of Construct
// is the base CgsDev::DebugComponent::Construct() two-phase init - the X360 image folds every trivial
// body (the base init, several empty Destructs) onto one shared address, which IDA names after one
// arbitrary winner. It is the base init here by construction (Construct -> set member -> Register).

#include "GameSource/Network/Debug Components/BrnNetworkRoadRulesManagerDebugComponent.h"
#include "GameSource/Network/Managers/BrnNetworkRoadRulesManager.h"   // NetworkRoadRulesManager (method calls)
#include "SharedClasses/StreetData/BrnChallengeData.h"                // ChallengePlayerScoreEntry / ScoreList / ScoreType
#include "GameShared/GameClasses/Core/CgsAssert.h"                    // CGS_ASSERT

namespace BrnNetwork
{
    void RoadRulesManagerDebugComponent::Construct( NetworkRoadRulesManager* lpRoadRulesManager )
    {
        CgsDev::DebugComponent::Construct();   // base two-phase init
        mpRoadRulesManager = lpRoadRulesManager;
        Register();
    }

    const char* RoadRulesManagerDebugComponent::GetName() const
    {
        return "Road Rules";
    }

    // Called when the debug menu opens this component: wire up the two manual road-rule actions.
    void RoadRulesManagerDebugComponent::OnActivate()
    {
        RegisterFunction( &TriggerPersonalBest,        this, "Trigger Personal Best" );
        RegisterFunction( &RequestRoadRulesHighScores, this, "Get Road Rules High Scores" );
    }

    // @ 0x8258AEB0. Build a synthetic personal-best record (score type TIME = 10) on the stack and push
    // it through the manager's normal new-personal-best path - a developer shortcut for testing the
    // online road-rules score flow. The decompiler's raw `v4 |= 1 / v5 |= 1` are the dirty/valid bit-0
    // sets on the two CgsContainers::BitArray<2u> sub-objects; reconstructed as named SetBit calls.
    void RoadRulesManagerDebugComponent::TriggerPersonalBest( void* lpData )
    {
        CGS_ASSERT( lpData != nullptr, "lpData" );
        RoadRulesManagerDebugComponent* lpThis = static_cast<RoadRulesManagerDebugComponent*>( lpData );
        CGS_ASSERT( lpThis != nullptr, "lpRoadRulesManagerDebugCmpt" );

        BrnStreetData::ChallengePlayerScoreEntry lScoreEntry;
        lScoreEntry.Construct();
        lScoreEntry.mDirty.SetBit( 0 );          // v4 |= 1: mark the TIME score dirty
        lScoreEntry.mValidScores.SetBit( 0 );    // v5 |= 1: mark the TIME score valid
        lScoreEntry.mScoreList.SetScore( BrnStreetData::E_SCORE_TYPE_TIME, 10 );

        lpThis->mpRoadRulesManager->HandleNewPersonalBest( &lScoreEntry );
    }

    // @ 0x825863A8. Request the latest road-rules high scores from the server.
    void RoadRulesManagerDebugComponent::RequestRoadRulesHighScores( void* lpData )
    {
        CGS_ASSERT( lpData != nullptr, "lpData" );
        RoadRulesManagerDebugComponent* lpThis = static_cast<RoadRulesManagerDebugComponent*>( lpData );
        CGS_ASSERT( lpThis != nullptr, "lpRoadRulesManagerDebugCmpt" );

        lpThis->mpRoadRulesManager->StartDownloadingRoadRulesScoresFromServer();
    }
}
