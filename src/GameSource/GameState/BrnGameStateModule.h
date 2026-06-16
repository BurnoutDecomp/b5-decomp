#pragma once

#include "types.hpp"
#include "GameSource/BurnoutConstants.h"                            // EActiveRaceCarIndex
#include "GameShared/GameClasses/Module/CgsModuleSingleBuffered.h"  // CgsModule::ModuleSingleBuffered base
#include "GameShared/GameClasses/Core/CgsAssert.h"                  // CgsDev::Assert Begin/Fire/EndAssert
#include "GameSource/GameState/ModeManager/BrnModeManager.h"        // BrnGameState::ModeManager (mModeManager, by value)

namespace BrnGameState
{
// Minimal slice (BrnPursuitMode.h GetName-only precedent): the full GameStateModule layout (~290KB,
// ~190 methods) is owned by the BrnGameStateModule.cpp TU. Only the members touched by the three
// reconstructed functions of this TU are declared here; exact member offsets are NOT modelled (the
// X360 attests mbIsUpdating at this+292289, mePlayerActiveRaceCarIndex at this+208304, mModeManager at
// the +0x1DB8 region) -- inter-member padding for the full class is out of scope. The base
// (CgsModule::ModuleSingleBuffered) is #included, not forked.
class GameStateModule : public CgsModule::ModuleSingleBuffered
{
public:
    // X360 @ 0x82311570 (BrnGameStateModule.h:949). Inline accessor for the player's active-race-car
    // slot index. Asserts the module is mid-update (mbIsUpdating) before handing back the cached index.
    // The X360 baked the assert file/line (BrnGameStateModule.h:971) into the binary, so the explicit
    // BeginAssert/FireAssert/EndAssert sequence preserves the strings verbatim.
    EActiveRaceCarIndex GetPlayerActiveRaceCarIndex()
    {
        if (!mbIsUpdating)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(
                "Can not use this function unless module is updating\n",
                "..\\..\\..\\GameSource\\GameState/BrnGameStateModule.h",
                971);
            CgsDev::Assert::EndAssert();
        }
        return mePlayerActiveRaceCarIndex;
    }

    // X360 @ 0x823116D0 (BrnGameStateModule.h:982). Out-of-line; defined in BrnGameStateModule.cpp.
    bool IsOnlineGameMode();

    // DWARF BrnGameStateModule.h:1300/651. X360 inlines it (sets mbToggleShowtimeBehaviour=true at
    // offset 284512); declared-only here, used by GameStateDebugComponent::ToggleShowtimeCallback.
    void ToggleShowtimeBehaviour();

private:
    // DWARF BrnGameStateModule.h:771. The by-value ModeManager that owns the current game mode.
    ModeManager         mModeManager;
    // DWARF BrnGameStateModule.h:794 (X360 this+208304).
    EActiveRaceCarIndex mePlayerActiveRaceCarIndex;
    // DWARF BrnGameStateModule.h:882 (X360 this+292289) -- set true only while the module is updating.
    bool                mbIsUpdating;
};
}
