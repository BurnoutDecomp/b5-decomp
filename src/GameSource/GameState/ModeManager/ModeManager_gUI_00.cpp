// b5-decomp/src/GameSource/GameState/ModeManager/ModeManager_gUI_00.cpp
//
// Partfile of the BrnGameState::ModeManager TU (owning header BrnModeManager.h).
//
// TWO ACCESSOR BODIES, landed for a LINK reason measured in the gateui wave's round-2 pass:
// StuntManager::ProcessStuntElement @0x8239CDB0 asserts `mpModeManager->GetScoringSystem()` and
// then calls ScoringSystem::DealWithStunt through it, so `ModeManager::GetScoringSystem` is an
// UNDEF external in StuntManager_gUI_00.obj. Its own TU (BrnModeManager.cpp) is a ~38 KB-object,
// many-hundred-function reconstruction that is NOT mounted and does not body these two, so the
// whole GameState chain would fail to link on a two-line accessor.
//
// ⓘ THE CONSOLE EMITS NO SYMBOL FOR EITHER -- there is no `ModeManager::GetScoringSystem` in
// progress/identity.json or the export set, because the X360 inlines it everywhere: each game-mode
// body reaches the ScoringSystem the ModeManager embeds BY VALUE at ModeManager+0xDB0 as a direct
// `this + 0xDB0` pointer adjust (e.g. OnlineRaceMode::PreWorldUpdate / GetOutroTimeout). The
// declaration in BrnModeManager.h is this repo's de-inlining of that adjust, added so no
// reconstructed body has to poke the byte offset; these are its bodies, and they are exactly the
// adjust and nothing more.
//
// ⚠️ DUPLICATE-SYMBOL WATCH: BrnModeManager.h's comment says "body + real member land with the
// ModeManager TU". If BrnModeManager.cpp is ever bodied WITH these two, delete this partfile (and
// its mount line) rather than letting both exist.
#include "GameSource/GameState/ModeManager/BrnModeManager.h"

#include "GameSource/GameState/ModeManager/Scoring/BrnScoringSystem.h"   // ScoringSystem (embedded by value)

namespace BrnGameState
{

ScoringSystem* ModeManager::GetScoringSystem()
{
    return &mScoringSystem;
}

const ScoringSystem* ModeManager::GetScoringSystem() const
{
    return &mScoringSystem;
}

}
