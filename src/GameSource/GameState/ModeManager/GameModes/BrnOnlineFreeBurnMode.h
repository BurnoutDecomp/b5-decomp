#pragma once

#include "GameSource/GameState/ModeManager/GameModes/BrnOnlineGameMode.h"
#include "GameSource/GameState/ModeManager/GameModes/BrnGameModeParams.h"  // StartGameModeParams/GameModeParams/ScoringSystem

namespace BrnGameState
{
// OnlineFreeBurnMode is a concrete online game mode. Bases (OnlineGameMode -> GameMode) are
// #included from their owning headers, never forked. GetName / Start / SendEvent are owned here.
class OnlineFreeBurnMode : public OnlineGameMode
{
public:
    virtual const char* GetName() const;                               // slot 6,  X360 0x827E25D0
    // X360 0x823160C8 -- an unconditional "should never be called" assert stub on this build
    // (the mode is brought up another way). Third param unused; kept for DWARF-shape parity.
    virtual void        Start(const StartGameModeParams* lpStartGameModeParams,
                              GameModeParams* lpGameModeParams,
                              ScoringSystem* lpScoringSystem);         // slot 5
    virtual void        SendEvent(EGameModeEvent leEvent);             // slot 12, X360 0x82331460

    // Slot 23 (vtbl+92). Folded leaf 0x827E2F38 (`li r3,0; blr`) at slot 23 of vtable 0x820D09E8;
    // the GameMode base is 0x82C296C8 (`li r3,1`). DWARF BrnOnlineFreeBurnMode.h:14 declares this
    // override; ADDED 2026-08-26 with the 26-slot base.
    virtual bool        RequiresStreaming() const;
};

// ---- VTABLE-BINDING TRIPWIRE (see the explanation in BrnOfflineGameMode.h) ----------------------
static_assert(sizeof(static_cast<const char* (OnlineFreeBurnMode::*)() const>(&OnlineFreeBurnMode::GetName)) != 0,
              "OnlineFreeBurnMode::GetName must bind GameMode vtable slot 6");
static_assert(sizeof(static_cast<void (OnlineFreeBurnMode::*)(const StartGameModeParams*, GameModeParams*, ScoringSystem*)>(&OnlineFreeBurnMode::Start)) != 0,
              "OnlineFreeBurnMode::Start must bind GameMode vtable slot 5");
static_assert(sizeof(static_cast<void (OnlineFreeBurnMode::*)(EGameModeEvent)>(&OnlineFreeBurnMode::SendEvent)) != 0,
              "OnlineFreeBurnMode::SendEvent must bind GameMode vtable slot 12");
static_assert(sizeof(static_cast<bool (OnlineFreeBurnMode::*)() const>(&OnlineFreeBurnMode::RequiresStreaming)) != 0,
              "OnlineFreeBurnMode::RequiresStreaming must bind GameMode vtable slot 23");
}
