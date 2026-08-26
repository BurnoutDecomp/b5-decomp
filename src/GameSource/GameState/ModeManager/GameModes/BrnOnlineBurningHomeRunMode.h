#pragma once

#include "GameSource/GameState/ModeManager/GameModes/BrnOnlineGameMode.h"

namespace BrnGameState
{
// OnlineBurningHomeRunMode is a concrete game mode. The base types (OnlineGameMode -> GameMode) are #included from
// their own owning headers rather than forked locally. Only GetName is owned by this TU;
// OnlineBurningHomeRunMode's remaining members/methods belong to the BrnOnlineBurningHomeRunMode.cpp TU.
//
// NOT-YET-RECONSTRUCTED OVERRIDES (vtable 0x820D0960, checked 2026-08-26): OnlineBurningHomeRunMode
// also overrides slot 5 Start (0x82339968) and slot 2 PreWorldUpdate (0x8234CFD0). Neither has a
// body in the tree, so neither is declared here -- a declaration with no definition is an
// unresolved external as soon as the vtable is emitted.
class OnlineBurningHomeRunMode : public OnlineGameMode
{
public:
    virtual const char* GetName() const;                               // slot 6, X360 0x827E25C0

    // Slot 24 (vtbl+96). 0x82C296C8 (`li r3,1; blr`) at slot 24 of vtable 0x820D0960, against the
    // GameMode base's 0x827E2F38 (`li r3,0`). DWARF BrnOnlineBurningHomeRunMode.h:33 declares this
    // override; ADDED 2026-08-26 with the 26-slot base.
    virtual bool HasLoadingScreen() const;
};

// ---- VTABLE-BINDING TRIPWIRE (see the explanation in BrnOfflineGameMode.h) ----------------------
static_assert(sizeof(static_cast<const char* (OnlineBurningHomeRunMode::*)() const>(&OnlineBurningHomeRunMode::GetName)) != 0,
              "OnlineBurningHomeRunMode::GetName must bind GameMode vtable slot 6");
static_assert(sizeof(static_cast<bool (OnlineBurningHomeRunMode::*)() const>(&OnlineBurningHomeRunMode::HasLoadingScreen)) != 0,
              "OnlineBurningHomeRunMode::HasLoadingScreen must bind GameMode vtable slot 24");
}
