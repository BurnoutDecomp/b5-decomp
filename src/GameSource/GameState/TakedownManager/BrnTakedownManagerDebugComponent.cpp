#include "GameSource/GameState/TakedownManager/BrnTakedownManagerDebugComponent.h"
#include "GameSource/GameState/TakedownManager/BrnTakedownManager.h"   // TakedownManager + RaceCarData
#include "GameSource/GameState/BrnGameStateModule.h"                          // GameStateModule::GetLastActiveRaceCarInterface (RecordTakedown)
#include "GameSource/GameState/ModeManager/BrnModeManager.h"                  // ModeManager::GetGameStateModule
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarEntityModuleOutputInterface.h" // RCEntityActiveRaceCarOutputInterface
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.h"      // RaceCarState (mi8LastContactedRaceCar / mfTimeSinceLastRaceCarContact)

// Reconstructed from BURNOUT_X360_ARTIST.XEX. The takedown debug menu registers three display
// toggles + a "force takedown" action with the real CgsDev::DebugComponent debug-menu API. The
// X360 issued these through the (unnamed) DebugComponent registration helpers (now the real
// base-class RegisterVariable / RegisterFunction, called by name). ForceTakedownCallback forces a
// takedown on the player's active race car (maRaceCarData[0]).

namespace BrnGameState
{
    const char* TakedownManagerDebugComponent::GetName() const
    {
        return "TakedownManager";
    }

    void TakedownManagerDebugComponent::OnActivate()
    {
        RegisterVariable(&mbShowLastTakedownInfo, "Show last takedown info");
        RegisterVariable(&mbShowVulnerability, "Show vulnerability");
        RegisterVariable(&mbShowRevengeTakedownInfo, "Show revenge takedown info");
        RegisterFunction(&ForceTakedownCallback, this, "Force takedown");
    }

    // Inlined into GameStateModule::Construct @0x82380404..0x82380434 (r3 = &gsm.mTakedownManager
    // .mTakedownManagerDebugComponent, r23 = &gsm.mTakedownManager):
    //   0x82380418  stfs -1.0, +0x18   mfLastTakedownContactTime
    //   0x8238041C  stw  r23,  +0x0C   mpTakedownManager
    //   0x82380420  stb  0,    +0x1D   mbShowLastTakedownInfo
    //   0x82380424  stb  0,    +0x1E   mbShowRevengeTakedownInfo
    //   0x82380428  stb  0,    +0x1C   mbShowVulnerability
    //   0x8238042C  stw  -1,   +0x10   meLastAggressorIndex
    //   0x82380430  stw  -1,   +0x14   meLastVictimIndex
    //   0x82380434  bl   CgsDev::DebugComponent::Register
    // (the base Construct() is NOT called on the console; only Register()).
    void TakedownManagerDebugComponent::Construct(TakedownManager* lpTakedownManager)
    {
        mfLastTakedownContactTime = -1.0f;
        mpTakedownManager         = lpTakedownManager;
        mbShowLastTakedownInfo    = false;
        mbShowRevengeTakedownInfo = false;
        mbShowVulnerability       = false;
        meLastAggressorIndex      = E_ACTIVE_RACE_CAR_INDEX_INVALID;
        meLastVictimIndex         = E_ACTIVE_RACE_CAR_INDEX_INVALID;

        Register();
    }

    // @ 0x823597F8 - clear the player's active-car takedown state then arm a standard takedown
    // (aggressor car 0 -> victim car 1). The X360 reaches the car via the takedown manager (whose
    // first member is maRaceCarData[]), then writes the fields below; reconstructed as named
    // member access with named enumerators (the takedown enums are now full/deduped).
    void TakedownManagerDebugComponent::ForceTakedownCallback(void* lpContext)
    {
        TakedownManagerDebugComponent* lpThis = static_cast<TakedownManagerDebugComponent*>(lpContext);
        TakedownManager::RaceCarData& lrRaceCar = lpThis->mpTakedownManager->maRaceCarData[0];

        lrRaceCar.Clear();
        lrRaceCar.mfTimeSinceVictimCrashed = 0.0f;
        lrRaceCar.mbWaitingOnTakedown = true;
        lrRaceCar.mPendingTakedownEvent.meAggressorIndex = E_ACTIVE_RACE_CAR_INDEX_0;
        lrRaceCar.mPendingTakedownEvent.meVictimIndex    = E_ACTIVE_RACE_CAR_INDEX_1;
        lrRaceCar.mPendingTakedownEvent.meType           = E_TAKEDOWN_STANDARD;
    }

    // @ 0x823663F8 -- DWARF BrnTakedownManagerDebugComponent.cpp:216. Reached from
    // DetectStandardTakedown (`bl RecordTakedown(this+676, aggressor, victim)` @0x8237A5E8).
    //   0x82366414  lwz r9, 0xC(this)         mpTakedownManager
    //   0x82366420  lwz r10, 0x28C(r9)        ->mpModeManager
    //   0x82366424  lwz r10, 0x6D58(r10)      ->mpGameStateModule
    //   0x82366430/38 r29 = gsm + 0x397E0     the module's cached active-race-car interface
    //   0x82366428  stfs -1.0, 0x18(this)     mfLastTakedownContactTime = -1
    //   0x8236642C  stw  aggressor, 0x10      meLastAggressorIndex
    //   0x82366434  stw  victim, 0x14         meLastVictimIndex
    //   IsRaceCarActive(victim) -> GetRaceCarState(victim): `lbz 0x445` (mi8LastContactedRaceCar)
    //   != 0xFF -> `lfs 0x430` (mfTimeSinceLastRaceCarContact) -> mfLastTakedownContactTime.
    // The console reads the three pointers raw; GetGameStateModule() adds its own non-null assert
    // (assert-is-not-a-guard: the read still happens either way).
    void TakedownManagerDebugComponent::RecordTakedown(EActiveRaceCarIndex leAggressorIndex,
                                                       EActiveRaceCarIndex leVictimIndex)
    {
        const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpActiveCarInterface =
            mpTakedownManager->mpModeManager->GetGameStateModule()->GetLastActiveRaceCarInterface();

        mfLastTakedownContactTime = -1.0f;
        meLastAggressorIndex      = leAggressorIndex;
        meLastVictimIndex         = leVictimIndex;

        if (lpActiveCarInterface->IsRaceCarActive(static_cast<::EActiveRaceCarIndex>(leVictimIndex)))
        {
            const BrnPhysics::Vehicle::RaceCarState* lpVictimRaceCarState =
                lpActiveCarInterface->GetRaceCarState(static_cast<::EActiveRaceCarIndex>(leVictimIndex));
            if (lpVictimRaceCarState->mi8LastContactedRaceCar != -1)
            {
                mfLastTakedownContactTime = lpVictimRaceCarState->mfTimeSinceLastRaceCarContact;
            }
        }
    }
}
