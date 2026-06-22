#pragma once

#include "types.hpp"
#include "BrnCommonTypes.h" // CgsID (typedef u64)
#include "GameSource/GameState/BrnGameStateTypes.h" // BrnGameState::StuntElementType

namespace BrnProgression
{
class Profile;  // GetProfile() return type; full slice in BrnProfile.h
// MINIMAL OWNING HEADER for BrnProgression::ProgressionManager.
//
// SCOPE: this is a deliberately *thin* slice. The full ProgressionManager is a large,
// 45-function TU (GameSource/GameState/Progression/BrnProgressionManager.cpp in the
// DecFIGS DWARF: nested enums LoadStage/AILoadStage, the LandmarkAISectionIndexPair
// struct, ~40 methods, and a wide member layout dominated by manager back-pointers and
// the trophy/unlock-queue state). Reconstructing all of that is separate future work and
// belongs to the ProgressionManager TU itself; do NOT grow this header into that shape --
// extend it method-by-method only as concrete callers need each piece.
//
// This header exists solely so that BrnGameState::OfflineGameMode::SelectRandomDestinations
// (X360 0x82321E38) can complete the call it makes through
// ModeManager::GetProgressionManager() -> ProgressionManager::FindLandmarkAISectionIndex().
// Before this header existed, BrnModeManager.h only forward-declared
//   class ProgressionManager;
// which is enough to hand the pointer back, but the OfflineGameMode .cpp must #include this
// header to actually dereference that pointer and call the method below.
//
// The forward declaration in BrnModeManager.h uses `class ProgressionManager;`. The DecFIGS
// DWARF spells the type `struct ProgressionManager` (BrnProgressionManager.h:118). Under
// C++ the two are interchangeable for a non-template type and only differ in default member
// access; to stay consistent with the existing forward declaration (and avoid MSVC C4099
// "type seen using both class and struct") this owning definition is written with `class`
// and an explicit `public:` section. When the full TU is reconstructed the keyword/layout
// can be reconciled then -- it does not affect callers that only use the pointer + this method.
class ProgressionManager
{
public:
    // X360 0x82359AE0 (identity.json-attested; has_pseudocode). DecFIGS DWARF
    // BrnProgressionManager.h:385 / mangled _ZNK14BrnProgression18ProgressionManager26
    // FindLandmarkAISectionIndexEy -> const member, single param `y` (u64 == CgsID),
    // returns uint16_t.
    //
    // Maps a landmark's CgsID to the AI-section index the progression layer has cached for
    // it (the body walks the LandmarkAISectionIndexPair table that the full TU owns). The
    // ONLY member OfflineGameMode::SelectRandomDestinations uses: for each accepted landmark
    // it stores the returned u16 into the lpaAISectionIndicesOut parallel output array.
    //
    // DECLARATION ONLY (no body): the definition lives with the ProgressionManager TU. A
    // declaration is sufficient for the cl /c compile gate on the OfflineGameMode TU, which
    // only needs the method's signature to type-check the call site.
    u16 FindLandmarkAISectionIndex(CgsID lLandmarkId) const;

    // ------------------------------------------------------------------------
    // ADDITIVE GROW (declare-only) for the AchievementManagerBase TU.
    //
    // FLAG: these accessors name the deep reads AchievementManagerBase::OnTakedown,
    // OnEventWin and OnCollectStunt make THROUGH the mpProgressionManager back-pointer.
    // The X360 reads them as raw offsets off the ProgressionManager (and its embedded
    // Profile / collected-stunt CgsSet array) whose full layout is owned by the
    // ProgressionManager TU and is NOT modelled here. Signatures + semantics are
    // X360-asm-attested; member offsets / the Profile sub-object are out of scope.
    // Bodies land with the ProgressionManager TU; declare-only suffices for `cl /c`.
    // ------------------------------------------------------------------------

    // OnTakedown (X360 0x8235AAE0): the embedded Profile (this+0x170) lifetime takedown
    // tally read at Profile+0x198 and compared >= 500 (E_ACHIEVEMENT_GET_500_TAKEDOWNS).
    s32 GetProfileTotalTakedowns() const;

    // OnEventWin (X360 0x82372978) case E_MODE_MARKED_MAN: a win-count tally read at
    // this+0x358 and compared >= 25 (win-10-XS / 0x15) then >= 35 (win-25-XS / 0x18).
    s32 GetCarChallengeWinCount() const;

    // OnCollectStunt (X360 0x82366EB8): per-stunt-type collected-element count. The X360
    // indexes an embedded CgsSet array at this+0x7768 (stride 0x1008 / 4104 bytes per
    // element) and returns the set's element-count field (set base +0x1000). The leading
    // set-sentinel check (set base +0x1000 == -1 -> "Set used before Construct/Clear")
    // is reproduced inside this accessor in the full TU.
    s32 GetCollectedStuntElementCount(BrnGameState::StuntElementType leStuntType) const;

    // ADDITIVE GROW (declare-only) for the BrnTrainingManager TU.
    // X360 BrnProgression::ProgressionManager::GetProfile -- returns the embedded player Profile
    // (the X360 reaches it as the by-value sub-object at this+0x170). TrainingManager::
    // DEBUG_ClearTrainingFlags (0x82366050) / RequestTraining / SendTrainingTickerMessage call it,
    // assert the result is non-null, then poke training flags through it. Body + the real embedded
    // Profile member land with the ProgressionManager TU. FLAG: declare-only additive grow.
    Profile* GetProfile();
};
}
