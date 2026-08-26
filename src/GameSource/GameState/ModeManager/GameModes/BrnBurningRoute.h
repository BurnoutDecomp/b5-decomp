#pragma once

#include "types.hpp"
#include "GameSource/GameState/ModeManager/GameModes/BrnOfflineGameMode.h"

namespace BrnGameState
{
// BurningRouteMode is a concrete offline game mode. Bases (OfflineGameMode -> GameMode) are #included
// from their owning headers, never forked. Only GetName and GetOutroTimeout are owned by this
// TU (DWARF-attested); the rest of the mode belongs to BrnBurningRoute.cpp.
//
// NOT-YET-RECONSTRUCTED OVERRIDES (vtable 0x820D06B8, checked 2026-08-26): BurningRouteMode also
// overrides slot 2 PreWorldUpdate (0x82331D98), slot 5 Start (0x82331A80) and slot 15
// FillInGameModeSpecificResults -- the DWARF declares that set. No body exists in the tree for any
// of them, so none is declared here (a declaration with no definition is an unresolved external as
// soon as the vtable is emitted); the mode inherits the GameMode base for all three.
class BurningRouteMode : public OfflineGameMode
{
public:
    virtual const char* GetName() const;                               // slot 6,  X360 0x827E2508
    virtual f32         GetOutroTimeout() const;                       // slot 16, X360 0x827E2518

    // Slot 23 (vtbl+92). Folded leaf 0x827E2F38 (`li r3,0; blr`); the base is 0x82C296C8
    // (`li r3,1`). DWARF BrnBurningRoute.h:81 declares this override. ADDED 2026-08-26 with the
    // 26-slot base: without it, SetupGameMode would put a burning route through WaitForStreaming.
    virtual bool RequiresStreaming() const;

private:
    // DWARF: BrnBurningRoute.cpp:27. The mode's fixed outro timeout; the X360 GetOutroTimeout body
    // returns 0.0, so this constant is 0.0f for this build (same shape as RaceMode).
    static const f32 KF_OUTRO_TIME_SECONDS;
};

// ---- VTABLE-BINDING TRIPWIRE (see the explanation in BrnOfflineGameMode.h) ----------------------
static_assert(sizeof(static_cast<const char* (BurningRouteMode::*)() const>(&BurningRouteMode::GetName)) != 0,
              "BurningRouteMode::GetName must bind GameMode vtable slot 6");
static_assert(sizeof(static_cast<f32 (BurningRouteMode::*)() const>(&BurningRouteMode::GetOutroTimeout)) != 0,
              "BurningRouteMode::GetOutroTimeout must bind GameMode vtable slot 16");
static_assert(sizeof(static_cast<bool (BurningRouteMode::*)() const>(&BurningRouteMode::RequiresStreaming)) != 0,
              "BurningRouteMode::RequiresStreaming must bind GameMode vtable slot 23");
}
