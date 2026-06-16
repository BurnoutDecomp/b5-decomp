#include "GameSource/GameState/TakedownManager/BrnTakedownManagerDebugComponent.h"
#include "GameSource/GameState/TakedownManager/BrnTakedownManager.h"   // TakedownManager + RaceCarData

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

    // @ 0x823597F8 - clear the player's active-car takedown state then arm a standard takedown
    // (aggressor car 0 -> victim car 1). The X360 reaches the car via the takedown manager (whose
    // first member is maRaceCarData[]), then writes the fields below; reconstructed as named
    // member access. The takedown-index/type values are written as casts because the local
    // EActiveRaceCarIndex / ETakedownType enums are still minimal (see BrnTakedownManagerTypes.h
    // - it duplicates BurnoutConstants' enums and lacks the _0/_1/_STANDARD names; that drift is a
    // separate cleanup). The named meaning is in the trailing comments.
    void TakedownManagerDebugComponent::ForceTakedownCallback(void* lpContext)
    {
        TakedownManagerDebugComponent* lpThis = static_cast<TakedownManagerDebugComponent*>(lpContext);
        TakedownManager::RaceCarData& lrRaceCar = lpThis->mpTakedownManager->maRaceCarData[0];

        lrRaceCar.Clear();
        lrRaceCar.mfTimeSinceVictimCrashed = 0.0f;
        lrRaceCar.mbWaitingOnTakedown = true;
        lrRaceCar.mPendingTakedownEvent.meAggressorIndex = static_cast<EActiveRaceCarIndex>(0);  // E_ACTIVE_RACE_CAR_INDEX_0
        lrRaceCar.mPendingTakedownEvent.meVictimIndex    = static_cast<EActiveRaceCarIndex>(1);  // E_ACTIVE_RACE_CAR_INDEX_1
        lrRaceCar.mPendingTakedownEvent.meType           = static_cast<ETakedownType>(0);        // E_TAKEDOWN_STANDARD
    }
}
