// ============================================================================
// b5-decomp/src/GameSource/GameState/GameStateModule_gDC_00.cpp
//
// Partfile of the BrnGameState::GameStateModule TU (owning header BrnGameStateModule.h; the
// module's other committed bodies live in BrnGameStateModule.cpp, GameStateModule_gUI_00.cpp,
// GameStateModule_gSR_00.cpp, GameStateModule_gRR_00.cpp and GameStateModule_gTD_00.cpp).
//
// [link-closure lane P3 2026-09-03] ONE body: the module-level "is this active-race-car slot still
// a live car" predicate that BrnGameStateModule.h:1110 has declared -- and left unbodied -- since
// the PaybackManager wave. It is the last game-side undefined symbol standing between
// BrnPaybackManager.cpp and a link. BrnGameStateModule.cpp is conductor-owned, so the body lands
// here rather than there; the declaration is untouched.
// ============================================================================

#include "GameSource/GameState/BrnGameStateModule.h"

#include "GameSource/GameState/ModeManager/BrnModeManager.h"            // ModeManager::GetScoringSystem
#include "GameSource/GameState/ModeManager/Scoring/BrnScoringSystem.h"  // ScoringSystem::GetCarData / CarData::GetNetworkPlayerID
#include "GameSource/Network/SharedIO/BrnNetworkSharedIO.h"             // BrnNetwork::NetworkPlayerID (== s32)

namespace BrnGameState
{

namespace
{
    // CgsNetwork::K_INVALID_PLAYER_ID stand-in -- the same -1 sentinel, spelled the same way, as
    // BrnScoringSystem_Lookup.cpp's local (no committed home for the constant).
    const BrnNetwork::NetworkPlayerID K_INVALID_PLAYER_ID = -1;
}

// ----------------------------------------------------------------------------------------------
// GameStateModule::IsActiveRaceCarStillPresent   (declared BrnGameStateModule.h:1110)
//
// NO out-of-line X360 symbol: this is a de-inlined name for the guard the console folds into its
// callers. The fold that pins it is BrnGameState::PaybackManager::HandleHavingPayback @0x82397B30,
// whose first four instructions are the whole predicate:
//
//     0x82397B4C  lwz  r11, 0x268(r31)     ; PaybackManager::mpGameStateModule
//     0x82397B50  lwz  r4,  0x244(r31)     ; mePaybackVictimRaceCarIndex
//     0x82397B54  addi r3,  r11, 0x1DD0    ; == the module's ScoringSystem
//     0x82397B58  bl   sub_8231DCD0        ; ScoringSystem::GetCarData(EActiveRaceCarIndex) (const twin)
//     0x82397B5C  cmplwi r3, 0   ; beq  -> "gone"
//     0x82397B64  lwz  r11, 0x148(r3)      ; CarData::mNetworkPlayerID
//     0x82397B68  cmpwi r11, -1  ; bne  -> "still present" (the function returns immediately)
//                                ; eq   -> falls into the "the car has left the game" teardown
//
// so: present == (GetCarData(idx) != NULL) && (that record's network player id != -1).
//
// gsm+0x1DD0 (7632) is NOT a module member of its own -- it is mModeManager.GetScoringSystem():
// mModeManager sits at gsm+4128 and ScoringSystem at ModeManager+0xDB0 (3504), and 4128 + 3504 ==
// 7632 exactly (the same identity BrnGameStateModule.cpp:274 records for AchievementManagerBase::
// Construct's `a1 + 7632` argument). Reached BY NAME here, never by that offset.
//
// CarData+0x148 is mNetworkPlayerID: sub_8231DCD0's own search matches the index against
// element+0x144 (its loop base is this+0x5044 against an element base of this+0x4F00), which is
// meRaceCarIndex, and mNetworkPlayerID is the member the DWARF declares immediately after it.
//
// THE OVERLOAD IS PINNED THROUGH A MEMBER POINTER, for the reason GameStateModule_gRR_00.cpp:191
// spells out: ScoringSystem::GetCarData is overloaded on EActiveRaceCarIndex and on
// BrnNetwork::NetworkPlayerID (== s32), two distinct `enum EActiveRaceCarIndex` exist in this tree,
// and an enum that is not the parameter's own would decay to s32 and SILENTLY bind the
// NetworkPlayerID overload. Naming the by-active-index overload's exact type makes that a compile
// error instead. The CONST twin (0x8231DCD0) is the one the console calls and the one this const
// method can call.
// ----------------------------------------------------------------------------------------------
bool GameStateModule::IsActiveRaceCarStillPresent(::EActiveRaceCarIndex leActiveRaceCarIndex) const
{
    typedef const CarData* (ScoringSystem::*ConstCarDataByActiveIndexFn)(::EActiveRaceCarIndex) const;
    const ConstCarDataByActiveIndexFn lpfnGetCarData = &ScoringSystem::GetCarData;

    const ScoringSystem* lpScoringSystem = mModeManager.GetScoringSystem();
    const CarData*       lpCarData       = (lpScoringSystem->*lpfnGetCarData)(leActiveRaceCarIndex);

    return (lpCarData != NULL) && (lpCarData->GetNetworkPlayerID() != K_INVALID_PLAYER_ID);
}

}
