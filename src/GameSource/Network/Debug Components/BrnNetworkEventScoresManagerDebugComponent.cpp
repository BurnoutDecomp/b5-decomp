// Bodies for the online event-scores debug component, reconstructed from BURNOUT_X360_ARTIST.XEX:
//   Construct                     @ 0x82586530
//   Destruct                      @ 0x82586578  (folded base-destruct shared body)
//   GetName                       @ 0x82586588
//   OnActivate                    @ 0x82591E98
//   SetCalvalryBurningRouteBest   @ 0x82591D00
//
// The component registers one manual menu action against the real CgsDev::DebugComponent menu
// API. The X360's `sub_8282F720(this, cb, this, name)` is DebugComponent::RegisterFunction(cb,
// this, name). The leading `BaseCollisionGenerator::Destruct(this)` the decompiler shows at the
// head of Construct is the base CgsDev::DebugComponent::Construct() two-phase init - the X360
// image folds every trivial body (the base init, several empty Destructs) onto one shared address
// that IDA names after an arbitrary winner. It is the base init here by construction
// (Construct -> set member -> Register).

#include "GameSource/Network/Debug Components/BrnNetworkEventScoresManagerDebugComponent.h"
#include "GameSource/Network/Managers/BrnEventScoresManager.h"      // EventScoresManager (back-pointer + GetServerInterface)
#include "GameSource/Network/BrnServerInterfaceBase.h"              // BrnServerInterfaceBase::GetCustomCommandsComponent
#include "GameSource/Network/Components/BrnServerInterfaceCustomCommands.h" // UploadEventScoreData
#include "GameSource/Network/Parameters/BrnNetworkEventScoreData.h" // EventScoreData
#include "GameShared/GameClasses/Core/CgsAssert.h"                  // CGS_ASSERT

namespace BrnNetwork
{
    void EventScoresManagerDebugComponent::Construct( EventScoresManager* lpEventScoresManager )
    {
        CgsDev::DebugComponent::Construct();   // base two-phase init
        mpEventScoresManager = lpEventScoresManager;
        Register();
    }

    const char* EventScoresManagerDebugComponent::GetName() const
    {
        return "Event Scores";
    }

    // Called when the debug menu opens this component: wire up the manual event-score action.
    void EventScoresManagerDebugComponent::OnActivate()
    {
        RegisterFunction( &SetCalvalryBurningRouteBest, this, "Set Cavalry Burning Route Score" );
    }

    // @ 0x82591D00. Build a synthetic single-entry event-score batch on the stack and push it
    // through the manager's normal upload path - a developer shortcut for testing the online
    // event-score upload flow. The three element values (type 6, score 50000, sub-type 5) are
    // X360-authoritative (li r8,6 / 0xC350 / li r4,5).
    void EventScoresManagerDebugComponent::SetCalvalryBurningRouteBest( void* lpData )
    {
        CGS_ASSERT( lpData != nullptr, "lpData" );
        EventScoresManagerDebugComponent* lpThis = static_cast<EventScoresManagerDebugComponent*>( lpData );
        CGS_ASSERT( lpThis != nullptr, "lpEventScoresManagerDebugCmpt" );

        EventScoreData lEventScoreData;
        lEventScoreData.AddEventScore( 6, 50000, 5 );

        EventScoresManager* lpManager = lpThis->mpEventScoresManager;
        BrnServerInterfaceBase* lpServerInterface = lpManager->GetServerInterface();
        CGS_ASSERT( lpServerInterface != nullptr,
                    "lpEventScoresManagerDebugCmpt->mpEventScoresManager->mpServerInterface" );
        CGS_ASSERT( lpServerInterface->GetCustomCommandsComponent() != nullptr,
                    "lpEventScoresManagerDebugCmpt->mpEventScoresManager->mpServerInterface->GetCustomCommandsComponent()" );

        lpServerInterface->GetCustomCommandsComponent()->UploadEventScoreData(
            &lEventScoreData,
            &EventScoresManager::_UploadEventScoreCallback,
            lpManager );
    }
}
