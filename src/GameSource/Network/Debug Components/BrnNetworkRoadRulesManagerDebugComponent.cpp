// Bodies for the online road-rules debug component, reconstructed from BURNOUT_X360_ARTIST.XEX:
//   Construct                     @ 0x82586350
//   Destruct                      (folded; see the header)
//   GetPath                       (folded; see the header)
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
#include "GameSource/Network/BrnNetworkInEventTypeDefs.h"             // BrnNetworkModuleIO::NetworkInRoadRulesPBEvent (TriggerPersonalBest)
#include "GameShared/GameClasses/Core/CgsAssert.h"                    // CGS_ASSERT

namespace BrnNetwork
{
    void RoadRulesManagerDebugComponent::Construct( NetworkRoadRulesManager* lpRoadRulesManager )
    {
        mpRoadRulesManager = lpRoadRulesManager;
        Register();
    }

    // Folded on the console onto the identical body of a sibling debug component: drop the
    // back-pointer, then the base teardown.
    void RoadRulesManagerDebugComponent::Destruct()
    {
        mpRoadRulesManager = nullptr;
        CgsDev::DebugComponent::Destruct();
    }

    const char* RoadRulesManagerDebugComponent::GetName() const
    {
        return "Road Rules";
    }

    // The vtable's GetPath slot is the shared network-component path getter.
    const char* RoadRulesManagerDebugComponent::GetPath() const
    {
        return "Network";
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
    // ARTIST checks the TIME bounds before setting valid then dirty and calling SetScore.
    // Its fixed payload10 is inside the immutable0..600000 bounds, so this bounded
    // reconstruction retains the same event and stores for that original constant.
    void RoadRulesManagerDebugComponent::TriggerPersonalBest( void* lpData )
    {
        CGS_ASSERT( lpData != nullptr, "lpData" );
        RoadRulesManagerDebugComponent* lpThis = static_cast<RoadRulesManagerDebugComponent*>( lpData );
        CGS_ASSERT( lpThis != nullptr, "lpRoadRulesManagerDebugCmpt" );

        // The record is a whole personal-best event: the score entry, then challenge 0 and the
        // not-a-lobby-score flag (the two stores just before the call).
        BrnNetworkModuleIO::NetworkInRoadRulesPBEvent lPBEvent;
        BrnStreetData::ChallengePlayerScoreEntry& lScoreEntry = lPBEvent.mPersonalBestScore;
        lScoreEntry.Construct();

        lScoreEntry.mValidScores.SetBit( 0 );    // v5 |= 1 at +0x08: mark the TIME score valid
        lScoreEntry.mDirty.SetBit( 0 );          // v4 |= 1 at +0x00: mark the TIME score dirty
        lScoreEntry.mScoreList.SetScore( BrnStreetData::E_SCORE_TYPE_TIME, 10 );

        lPBEvent.mChallengeIndex     = 0;
        lPBEvent.mbLobbyPersonalBest = false;

        lpThis->mpRoadRulesManager->HandleNewPersonalBest( &lPBEvent );
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
