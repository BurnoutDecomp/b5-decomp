#pragma once

#include "types.hpp"
#include "GameSource/GameState/ModeManager/GameModes/BrnGameMode.h"

namespace BrnGameState
{
// Owning header for the OnlineGameMode base of the online game-mode hierarchy.
// Reconstructed from the DecFIGS DWARF (BrnOnlineGameMode.h: struct OnlineGameMode :
// public GameMode) and the X360 ledger. It derives directly from GameMode (NOT
// OfflineGameMode) and overrides the two GameMode virtuals the X360 build attests for it:
//   - Construct(ModeManager*)  (identity.json 0x8232FE98)
//   - SendEvent(EGameModeEvent) (identity.json 0x8232FED0)
// Virtual/return-type/param shapes are taken from the GameMode base declaration, not the
// Hex-Rays pseudocode (which renders these as int-returning functions).
//
// [!] GetFrameRateType RESTORED 2026-08-26 (wave-B fix round). This header previously gated it
// out as "present in the PS3 DWARF but absent from the X360 ledger, so PS3-only drift". The image
// refutes that: vtable slot 7 (vtbl+28) is the folded leaf 0x827DF718 (`li r3,2; blr`) in ALL
// SEVEN online mode vtables (0x820D07F0/0860/08E0/0960/09E8/0A68/0AE8) and 0x82C296C8
// (`li r3,1; blr`) in all eight offline ones -- a split only possible if BOTH intermediate bases
// override the slot. It is missing from the ledger because COMDAT identical-code folding gives a
// two-instruction leaf no unique address to attest. ModeManager::StartGameMode @0x8234FCE8 calls
// it directly: `v17 = (*(**(a1+3480)+28))(*(a1+3480))`.
//
// Deliberately omitted:
//   - KF_ONLINE_TRAFFIC_DENSITY: a file-scope const in the original, not a member, and not
//     used by the reconstructed functions; left out of this minimal header.
//
// OnlineGameMode adds no data members of its own that the X360 build attests: the state it
// touches (meCurrentState, mpModeManager, mbIsOnline @+172, mbShowResultsRequested @+175) is all
// GameMode base state (see the +160..+179 table in BrnGameMode.h).
class OnlineGameMode : public GameMode
{
public:
    virtual void Construct(ModeManager* lpModeManager);                // slot 0,  X360 0x8232FE98

    // Vtable slot 7 (vtbl+28). Folded leaf 0x827DF718 == `li r3,2; blr` in all seven online
    // vtables -> CgsSystem::E_FRAMERATEMANAGER_MULTIPLE_UNCAPPED.
    virtual CgsSystem::EFrameRateManagerType GetFrameRateType() const;

    virtual void SendEvent(EGameModeEvent leEvent);                    // slot 12, X360 0x8232FED0
};

// ---- VTABLE-BINDING TRIPWIRE (see the explanation in BrnOfflineGameMode.h) ----------------------
static_assert(sizeof(static_cast<void (OnlineGameMode::*)(ModeManager*)>(&OnlineGameMode::Construct)) != 0,
              "OnlineGameMode::Construct must bind GameMode vtable slot 0");
static_assert(sizeof(static_cast<CgsSystem::EFrameRateManagerType (OnlineGameMode::*)() const>(&OnlineGameMode::GetFrameRateType)) != 0,
              "OnlineGameMode::GetFrameRateType must bind GameMode vtable slot 7");
static_assert(sizeof(static_cast<void (OnlineGameMode::*)(EGameModeEvent)>(&OnlineGameMode::SendEvent)) != 0,
              "OnlineGameMode::SendEvent must bind GameMode vtable slot 12");
}
