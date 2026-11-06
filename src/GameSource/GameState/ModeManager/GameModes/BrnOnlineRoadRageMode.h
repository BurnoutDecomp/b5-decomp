#pragma once

#include "GameSource/GameState/ModeManager/GameModes/BrnOnlineGameMode.h"
#include "GameShared/GameClasses/Numeric/CgsRandom.h"                      // CgsNumeric::Random (mRandom, by value)

namespace BrnGameState
{
// OnlineRoadRageMode is a concrete game mode. The base types (OnlineGameMode -> GameMode) are #included from
// their own owning headers rather than forked locally. Only GetName is owned by this TU;
// OnlineRoadRageMode's remaining members/methods belong to the BrnOnlineRoadRageMode.cpp TU.
class OnlineRoadRageMode : public OnlineGameMode
{
public:
    virtual const char* GetName() const;                               // slot 6, X360 0x827E25A0

    // Slot 24 (vtbl+96). 0x82C296C8 (`li r3,1; blr`) at slot 24 of vtable 0x820D0860, against the
    // GameMode base's 0x827E2F38 (`li r3,0`). DWARF BrnOnlineRoadRageMode.h:15 declares this
    // override; ADDED 2026-08-26 with the 26-slot base.
    virtual bool HasLoadingScreen() const;

    // X360 0x823394A0, vtable 0x820D0860 slot 5 (vtbl+20). Fills the online road rage's
    // GameModeParams from the NetworkRoundManager's start events.
    virtual void Start(const StartGameModeParams* lpStartGameModeParams,
                       GameModeParams*            lpGameModeParams,
                       ScoringSystem*             lpScoringSystem);

private:
    // DWARF BrnOnlineRoadRageMode.h:66, console this+0xC0 (Start's `addi r6, r30, 0xC0`). The console
    // object is 240 bytes, 0xC0 + sizeof(Random). Primed on the first round only.
    CgsNumeric::Random mRandom;
};

// ---- VTABLE-BINDING TRIPWIRE (see the explanation in BrnOfflineGameMode.h) ----------------------
static_assert(sizeof(static_cast<const char* (OnlineRoadRageMode::*)() const>(&OnlineRoadRageMode::GetName)) != 0,
              "OnlineRoadRageMode::GetName must bind GameMode vtable slot 6");
static_assert(sizeof(static_cast<bool (OnlineRoadRageMode::*)() const>(&OnlineRoadRageMode::HasLoadingScreen)) != 0,
              "OnlineRoadRageMode::HasLoadingScreen must bind GameMode vtable slot 24");
static_assert(sizeof(static_cast<void (OnlineRoadRageMode::*)(const StartGameModeParams*, GameModeParams*, ScoringSystem*)>(&OnlineRoadRageMode::Start)) != 0,
              "OnlineRoadRageMode::Start must bind GameMode vtable slot 5");
}
