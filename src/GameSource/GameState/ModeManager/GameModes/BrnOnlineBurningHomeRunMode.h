#pragma once

#include "GameSource/GameState/ModeManager/GameModes/BrnOnlineGameMode.h"
#include "GameSource/GameState/BrnGameStateSharedIO.h"   // GameStateModuleIO::GameActionQueue (PickNewBurningHomeRunRunner)

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

    // The runner (blue team) left or lost the host: the nearest connected red-team car
    // becomes the runner (action 167). Called by ModeManager::SetPlayerDisconnected / HandleNewHostEvent.
    void PickNewBurningHomeRunRunner(const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpActiveCarInterface,
                                     GameStateModuleIO::GameActionQueue* lpActionQueue);

    // Debug info BrnOnlineBurningHomeRunMode.cpp. Hand the runner (blue team) from the old car to
    // the new one: teams, HUD team-change messages, boost, invulnerability for a local new runner,
    // and the runner crash/time counters. Body in BrnOnlineBurningHomeRunMode_wBT_05.cpp.
    void SwitchBurningHomeRunRunner(::EActiveRaceCarIndex leOldRunnerRaceCarIndex,
                                    ::EActiveRaceCarIndex leNewRunnerRaceCarIndex,
                                    GameStateModuleIO::OutputBuffer* lpOutputBuffer);

private:
    // The runner counters (console +0xF0..+0xFC). The three words are the debug info's, in its
    // order; PreWorldUpdate compares the runner's crash count minus the first against the other
    // two. The float has no debug-info name: PreWorldUpdate adds the frame time to it and
    // SwitchBurningHomeRunRunner ignores a switch until it reaches 1 s, then clears it.
    s32 miNumCrashesWhenPlayerBecameRunner;   // +0xF0
    s32 miNumCrashesAsRunner;                 // +0xF4
    s32 miMaxCrashesBeforeChange;             // +0xF8
    f32 mfTimeSinceRunnerSwitch;              // +0xFC
};

// ---- VTABLE-BINDING TRIPWIRE (see the explanation in BrnOfflineGameMode.h) ----------------------
static_assert(sizeof(static_cast<const char* (OnlineBurningHomeRunMode::*)() const>(&OnlineBurningHomeRunMode::GetName)) != 0,
              "OnlineBurningHomeRunMode::GetName must bind GameMode vtable slot 6");
static_assert(sizeof(static_cast<bool (OnlineBurningHomeRunMode::*)() const>(&OnlineBurningHomeRunMode::HasLoadingScreen)) != 0,
              "OnlineBurningHomeRunMode::HasLoadingScreen must bind GameMode vtable slot 24");
}
