#pragma once

#include "types.hpp"
#include "BrnCommonTypes.h" // CgsID (typedef u64)
#include "GameSource/GameState/BrnGameStateTypes.h" // BrnGameState::StuntElementType
#include "SharedClasses/Trigger/BrnGenericRegion.h"  // BrnTrigger::GenericRegion::Type (OnDriveThru param)

// Foreign types the additive DriveThruManager-facing accessors route by pointer (declare-only).
// Tags match the committed homes (CarData/ProgressionData = struct, AchievementManagerBase = class)
// to avoid a struct/class mismatch (C4099).
namespace BrnProgression  { struct CarData; struct ProgressionData; }
namespace BrnGameState    { class AchievementManagerBase; }
namespace InputBuffer     { class GameActionQueue; }

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

    // ------------------------------------------------------------------------
    // ADDITIVE GROW (declare-only) for the BrnStuntManager TU.
    // The StuntManager spine (UpdateJumps / CheckForTrophyUnlocks) routes the stunt-element
    // done-check and the trophy/special-car unlocks through mpProgressionManager. Signatures +
    // semantics are X360-asm-attested; bodies land with the ProgressionManager TU. Declare-only.
    // ------------------------------------------------------------------------

    // X360 UpdateJumps reads the completed-stunt-element Set<__int64,512> at this+30568 (0x7768)
    // via Find -> a present key == that stunt element was already completed before.
    bool IsStuntElementDone(CgsID lStuntElementKey) const;

    // X360 0x82389740. CheckForTrophyUnlocks fires this when an element-complete count tops out.
    // FLAG: the exact arg type/count is not recovered; modelled as a single trophy-id s32 (the
    // X360 li-immediate the call site passes).
    void OnTrophyUnlock(s32 liTrophyType);

    // X360 0x82396058. Re-evaluates whether any special car should unlock after a stunt-element
    // milestone; CheckForTrophyUnlocks calls it unconditionally after the trophy path.
    void CheckForSpecialCarUnlocks();

    // ------------------------------------------------------------------------
    // ADDITIVE GROW (declare-only) for the BrnDriveThruManager TU.
    // The drive-thru discovery / body-shop-repair / paint-shop flow routes discovery, repair,
    // colour and achievement work through ProgressionManager. Signatures + semantics are
    // X360-asm-attested; bodies land with the ProgressionManager TU. Declare-only.
    // ------------------------------------------------------------------------

    // X360 0x82399DD0. Record discovery of drive-thru lId of kind leType and post the resulting
    // game actions onto lpQueue.
    void OnDriveThru(CgsID lId, BrnTrigger::GenericRegion::Type leType, InputBuffer::GameActionQueue* lpQueue);

    // The player's currently-selected car record (NULL when none). DriveThruManager body/paint
    // shops read its id and write its colour/palette.
    CarData* GetCurrentCarData();

    // The loaded ProgressionData resource (the event-junction table UnlockCarChallengeForCar walks).
    const ProgressionData* GetProgressionData() const;

    // X360 0x8237C0D8. Read car lCarId's current colour + palette indices into the two out-params.
    void GetCarColourAndPalette(CgsID lCarId, s32* lpiColour, s32* lpiPalette);

    // X360 0x82363630. Clear the deform/damage on the just-repaired car lCarId.
    void RepairUnlockedVehicle(CgsID lCarId);

    // The embedded achievement manager (X360 *(progmgr+133432)); DriveThruManager routes
    // OnFindAllCarParks / OnBodyShop through it.
    BrnGameState::AchievementManagerBase* GetAchievementManager();

    // ------------------------------------------------------------------------
    // ADDITIVE GROW (declare-only) for the BrnGameState::CarSelectManager (junkyard car-select) TUs.
    // Signatures + semantics are X360-asm-attested; bodies land with the ProgressionManager TU.
    // ------------------------------------------------------------------------

    // X360 0x823701D8. The player's current progression rank, compared against a car's required rank
    // in IsThisCarInCurrentUnlockSequence.
    s32 GetProgressionRank() const;

    // X360 0x8237A970. Add lCarId to the player's owned-car list with unlock-type leUnlockType and
    // return the new CarData record (asserts the result non-null internally). The full X360 symbol has
    // a long inlined-temporary parameter list; the CarSelect call site passes only (carId, unlockType),
    // so it is modelled as that 2-arg form. FLAG: reconcile the full signature with the ProgressionManager TU.
    CarData* AddCar(CgsID lCarId, s32 leUnlockType);

    // X360 UpdateExitState de-inlined byte poke at ProgressionManager+133489 -- a rivals-update request flag.
    // FLAG: de-inlined byte poke, not a named member in the exports.
    void RequestUpdateRivals();

    // X360 UpdateExitState de-inlined byte poke at ProgressionManager+133512 -- a drive-thrus/rivals
    // dirty flag. FLAG: de-inlined byte poke, not a named member in the exports.
    void SetDriveThrusDirtyFlag();
};
}
