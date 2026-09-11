// ============================================================================
// b5-decomp/src/GameSource/GameState/GameStateModule_GetActiveRaceCarIndexFromNetworkPlayer.cpp
// ============================================================================
// ONE body: BrnGameState::GameStateModule::GetActiveRaceCarIndex(BrnNetwork::NetworkPlayerID)
// -- X360 0x82363978. Its declaration has been in BrnGameStateModule.h since the wave-C
// ChallengeManager pass (an ADDITIVE GROW made declare-only for the per-TU `cl /c` gate); this
// partfile is the body, landed for the ChallengeManager mount, which is the only thing in the
// image that calls it (NetworkPlayerRemoved @0x8234E420 and SetRemotePlayersChallengeCompleted
// @0x82323DF8, both through mpGameStateModule).
//
// It lives in its own partfile rather than in BrnGameStateModule.cpp because that file is not
// this pass's to edit; folding it back in is a free follow-up.
// ============================================================================

#include "GameSource/GameState/ModeManager/Scoring/BrnScoringSystem.h"  // ScoringSystem::GetCarData / CarData::GetActiveRaceCarIndex
#include "GameSource/GameState/ModeManager/BrnModeManager.h"            // ModeManager::GetScoringSystem
#include "GameSource/GameState/BrnGameStateModule.h"                    // the owning class

namespace BrnGameState
{

// ----------------------------------------------------------------------------
// GetActiveRaceCarIndex -- X360 0x82363978. Whole body, register-attested:
//     0x82363984  addi  r3, r3, 0x1DD0        ; r3 = this + 7632
//     0x82363988  bl    sub_8231DD88          ; r4 (lPlayerID) UNTOUCHED -> forwarded
//     0x8236398C  cmplwi cr6, r3, 0
//     0x82363990  beq   -> return -1
//     0x82363994  lwz   r3, 0x144(r3)         ; carData + 324
//
// Two things the Hex-Rays pseudocode gets wrong and the asm settles:
//   * it renders `this` as the only parameter (`GetActiveRaceCarIndex(int a1)`) and drops the
//     network player id -- but r4 is never written between entry and the `bl`, so the id IS
//     forwarded to the callee, which reads it (`mr r26, r4` at 0x8231DD94);
//   * it therefore shows the callee taking one argument. sub_8231DD88 is
//     ScoringSystem::GetCarData(BrnNetwork::NetworkPlayerID) -- identified by its own baked
//     assert "lNetworkPlayerID != CgsNetwork::K_INVALID_PLAYER_ID"
//     (BrnScoringSystem.h:2832) and by its search over `base + 0x5048` at stride 0x158 (344 ==
//     sizeof(CarData)) returning `base + 0x4F00 + 344*i`.
//
// this + 0x1DD0 (7632) is `mModeManager.mScoringSystem`: mModeManager sits at GameStateModule
// +4128 (pinned independently -- the console reads mpCurrentGameMode at gsm+7608 == ModeManager
// +3480) and mScoringSystem at ModeManager +3504, and 4128 + 3504 == 7632 exactly. The console
// inlines GetScoringSystem() into that one `addi`; it is de-inlined to the named accessor here.
//
// carData + 324 is CarData::meRaceCarIndex -- the member immediately ahead of mNetworkPlayerID
// (+328, the field the callee's own search compares), so the pair is pinned from both ends.
// ----------------------------------------------------------------------------
::EActiveRaceCarIndex GameStateModule::GetActiveRaceCarIndex(BrnNetwork::NetworkPlayerID lPlayerID)
{
    const CarData* lpCarData = mModeManager.GetScoringSystem()->GetCarData(lPlayerID);

    if (lpCarData == NULL)
    {
        return E_ACTIVE_RACE_CAR_INDEX_INVALID;
    }

    return lpCarData->GetActiveRaceCarIndex();
}

} // namespace BrnGameState
