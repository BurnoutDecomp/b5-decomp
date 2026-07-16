#pragma once

#include "types.hpp"
#include "BrnCommonTypes.h" // CgsID (typedef u64)
#include "GameSource/GameState/BrnGameStateTypes.h" // BrnGameState::StuntElementType
#include "SharedClasses/Trigger/BrnGenericRegion.h"  // BrnTrigger::GenericRegion::Type (OnDriveThru param)
#include "BrnProfile.h"                              // BrnProgression::Profile (embedded sub-object, mProfile)
#include "GameShared/GameClasses/System/Resource/CgsResourcePtr.h" // CgsResource::ResourcePtr (mpProgressionData / mpAISectionData)

#include <cstddef> // offsetof (uncalled _AssertLayout)

namespace BrnAI { struct AISectionsData; }   // ResourcePtr<T> tag only (never dereferenced here)
namespace CgsModule { template <s32 BUFSIZE, s32 ALIGN> class VariableEventQueue; }   // SendGameCompletionResults param (pointer-only)

// Foreign types the additive DriveThruManager-facing accessors route by pointer (declare-only).
// Tags match the committed homes (CarData/ProgressionData = struct, AchievementManagerBase = class)
// to avoid a struct/class mismatch (C4099).
namespace BrnProgression  { struct CarData; struct ProgressionData; }
namespace BrnGameState    { class AchievementManagerBase; }
namespace InputBuffer     { class GameActionQueue; }
// BrnStreetData::ChallengeHighScoreEntry / ChallengePlayerScoreEntry come in via BrnProfile.h.

namespace BrnProgression
{
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

    // (The StreetManager keystone's GetProgressionData() grow collided with the identical
    //  pre-existing DriveThruManager-batch declaration further down -- X360 attestation for it:
    //  DWARF BrnProgressionManager.h:426; the X360 inlines ResourcePtr<ProgressionData>::
    //  operator-> at each call site, e.g. StreetManager::FindRivalsByDistrict @ 0x82336360,
    //  null-checking mpResourceMemory first. ONE declaration kept below.)

    // ADDITIVE GROW (declare-only) for the StreetManager keystone (wave B). X360
    // StreetManager::UpdateUserScoresFromServerRecords @ 0x82348FC0 tail-calls
    // SendGameCompletionResults(pm, lpOutput->GetGameActionQueue()) after the trophy
    // fan-out -- posts the game-completion results onto the output game-action queue
    // (VariableEventQueue<13312,16>). Body lands with the ProgressionManager TU.
    void SendGameCompletionResults( CgsModule::VariableEventQueue<13312, 16>* lpGameActionQueue );

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

    // ========================================================================
    // BODIED in this TU (BrnProgressionManager.cpp). All nine X360-asm-attested. Each reaches
    // its members BY NAME through the layout modelled below; no raw-offset pointer arithmetic.
    // ========================================================================

    // X360 0x827DEA50. Constructor: resets the 18 manager-handle head slots to the -1 sentinel,
    // marks the embedded Profile's index->element containers unconstructed, installs the debug
    // component's vtable, and empties the two intrusive event lists. EXECUTED in the boot trace.
    ProgressionManager();

    // X360 0x8239DC98. Two-phase load entry: validates the output/queue/trigger/achievement
    // pointers, loads the progression resource, wires the trigger-data + achievement back-pointers,
    // computes landmark AI-section indices, processes the loaded preset races, registers the debug
    // component and sets up the roaming sections. Returns true on a successful load.
    // FLAG: a3 (mpGameStateModule back-pointer, stored at +0x2093C) and the SetupRoamingSections
    // argument list are modelled as the void* the X360 forwards; reconcile when those TUs land.
    bool Prepare2(void* lpOutput, void* lpGameStateModule, InputBuffer::GameActionQueue* lpReceiverQueue,
                  void* lpTriggerData, BrnGameState::AchievementManagerBase* lpAchievementManager);

    // X360 0x82311520. True when road rules are available: the player has reached medal-progress >= 4
    // OR either road-rules-availability flag is set. (Read off the embedded Profile + two tail flags.)
    bool AreRoadRulesAvailable() const;

    // X360 0x82359850. Map an offline game-mode index (0..5) to its E_RACE_EVENT_TYPE event id. Asserts
    // (and returns -1) for an unknown mode. Pure index->constant map, no member access.
    s32 GetEvent(s32 liGameType) const;

    // X360 0x82359960. Map an online game-mode index (0..2) to its event id. Asserts (returns -1) for an
    // unknown mode. Pure index->constant map, no member access.
    s32 GetOnlin(u32 luGameType) const;

    // X360 0x823635C0. True when the player already owns lCarId (linear scan of the embedded Profile's
    // owned-car list). Routes through the named Profile car accessors.
    bool IsCarUnlocked(CgsID lCarId) const;

    // RepairUnlockedVehicle(CgsID) (X360 0x82363630) is declared once above (the DriveThruManager
    // additive grow); its body is reconstructed in this TU's .cpp.

    // X360 0x823114A8. Replace the whole 64-entry road-rules challenge-score table from lpaChallengeScores
    // (2560-byte copy). Asserts the source non-null then delegates to the embedded Profile.
    void SetRoadRuleChallengeData(const BrnStreetData::ChallengePlayerScoreEntry* lpaChallengeScores);

    // X360 0x82311430. Replace the whole 64-entry road-rules network high-score table from
    // lpaChallengeHighScores (3584-byte copy). Asserts the source non-null then delegates to the Profile.
    void SetRoadRuleNetworkHighScores(const BrnStreetData::ChallengeHighScoreEntry* lpaChallengeHighScores);

    // ------------------------------------------------------------------------
    // ADDITIVE GROW (StreetManager wave-C keystone) -- the roads-ruled counter accessors the
    // DWARF declares at BrnProgressionManager.h:516-531. Header-inline in the original: the
    // X360 folds them into StreetManager::GetNumberOfParShowTimeRoadsRuledByLocalPlayer
    // @0x8233F230 (+133456), GetNumberOfParTimeTrialRoadsRuledByLocalPlayer @0x8233F2C0
    // (+133460), GetNumberOfCompleteRoadsRuledByLocalPlayer @0x8233F350 (+133464) and
    // FillInRoadRulesQuery @0x823365A8 (the >= 64 owns-all-roads read). The "only if
    // greater" max-updates are the StreetManager callsites' own attested branches, not
    // these accessors'. DWARF constness kept verbatim (the Complete getter is non-const).
    // ------------------------------------------------------------------------
    u32  GetNumberOfParCrashRoadRulesRuledByPlayer() const
    {
        return static_cast<u32>(miNumberOfParCrashRoadRulesRuledByPlayer);
    }
    void SetNumberOfParCrashRoadRulesRuledByPlayer(u32 luNumber)
    {
        miNumberOfParCrashRoadRulesRuledByPlayer = static_cast<s32>(luNumber);
    }
    u32  GetNumberOfParTimeRoadRulesRuledByPlayer() const
    {
        return static_cast<u32>(miNumberOfParTimeRoadRulesRuledByPlayer);
    }
    void SetNumberOfParTimeRoadRulesRuledByPlayer(u32 luNumber)
    {
        miNumberOfParTimeRoadRulesRuledByPlayer = static_cast<s32>(luNumber);
    }
    u32  GetNumberOfCompleteRoadRulesRuledByPlayer()
    {
        return static_cast<u32>(miNumberOfNumberOfCompleteRoadRulesRuledByPlayer);
    }
    void SetNumberOfCompleteRoadRulesRuledByPlayer(u32 luNumber)
    {
        miNumberOfNumberOfCompleteRoadRulesRuledByPlayer = static_cast<s32>(luNumber);
    }

private:
    // ========================================================================
    // MINIMAL MEMBER LAYOUT (field ORDER X360-attested; exact byte offsets are X360-only -- the
    // PC build is 64-bit so the embedded Profile / pointer members are naturally wider, and every
    // function reaches its members BY NAME, identical behaviour regardless of byte offset).
    //
    // SCOPE: only the members the nine bodied functions touch are named. The X360 ProgressionManager
    // is far larger (the head region holds 18 manager-handle records whose internal field shape is
    // NOT recovered, and the tail holds further state); those are reserved honestly rather than
    // fabricated. This is NOT the full ProgressionManager layout.
    // ========================================================================

    // Head: 18 manager-handle records the ctor resets to the -1 sentinel (X360 ctor loop: 18 stores of
    // -1 at +0x10, stride 0x14). FLAG: the per-record internal field shape is unrecovered; only the
    // leading id word the ctor writes is named, the rest reserved to preserve the 20-byte X360 stride.
    struct HandleSlot
    {
        s32 mi32Id;        // +0x00 -- ctor stores the -1 "unset handle" sentinel here
        u8  mPad[16];      // +0x04 -- remaining record bytes (shape not recovered)
    };
    static const s32 KI_HANDLE_SLOT_COUNT = 18;
    HandleSlot maHandleSlots[KI_HANDLE_SLOT_COUNT];   // X360 +0x10 .. +0x170

    // The player's persisted profile (the X360 reaches it as the by-value sub-object at this+0x170).
    // Every Profile-facing bodied function (IsCarUnlocked / RepairUnlockedVehicle / SetRoadRule* /
    // AreRoadRulesAvailable's medal read) goes through this named member.
    Profile mProfile;                                  // X360 +0x170

    // The player's road-rules-ruled tallies (X360 +133456 / +133460 / +133464).
    // *** FLAG -- COMMITTED-NAME CORRECTION (StreetManager keystone, wave B) ***
    // Previously committed as mi32RoadRulesAvailableFlagA/B ("availability flags");
    // the DecFIGS DWARF (BrnProgressionManager.h:152/:155/:158) names them, and the
    // StreetManager tally functions prove the semantics:
    //   GetNumberOfParShowTimeRoadsRuledByLocalPlayer @ 0x8233F230 maxes +133456
    //     with the CRASH-score tally,
    //   GetNumberOfParTimeTrialRoadsRuledByLocalPlayer @ 0x8233F2C0 maxes +133460
    //     with the TIME-score tally,
    //   GetNumberOfCompleteRoadsRuledByLocalPlayer @ 0x8233F350 stores the
    //     both-scores tally at +133464.
    // AreRoadRulesAvailable's nonzero-OR reads stay correct under the rename.
    s32 miNumberOfParCrashRoadRulesRuledByPlayer;              // X360 +133456 (DWARF :152)
    s32 miNumberOfParTimeRoadRulesRuledByPlayer;               // X360 +133460 (DWARF :155)
    s32 miNumberOfNumberOfCompleteRoadRulesRuledByPlayer;      // X360 +133464 (DWARF :158; sic -- DWARF spelling)

    // Prepare2 back-pointers (X360 +0x20924 / +0x2093C / +0x20938). Typed as the X360 forwards them.
    void*                                  mpTriggerData;        // X360 +0x20924 (a5)
    void*                                  mpGameStateModule;    // X360 +0x2093C (a3)
    BrnGameState::AchievementManagerBase*  mpAchievementManager; // X360 +0x20938 (a6)

    // The progression debug component the ctor installs (vtable off_820CDE4C) and Prepare2 constructs +
    // registers (X360 +133000 region, this+0x788 in the +0x20000 page). FLAG: full DebugComponent
    // sub-layout owned by ProgressionDebugComponent's TU; reserved here, only the installed vtable named.
    struct DebugComponentSlot
    {
        const void* mpVTable;   // ctor: = &off_820CDE4C
        u8          mPad[60];   // remaining ProgressionDebugComponent bytes (not modelled here)
    };
    DebugComponentSlot mDebugComponent;

    // *** FLAG -- COMMITTED-TYPE CORRECTION (StreetManager keystone, wave B) ***
    // Previously committed as two "IntrusiveListHead mEventListA/mEventListB" whose
    // Reset() modelled the ctor stores (count=0; three &self links; zeros). That
    // 0,0,0,self,self,self,0 pattern IS BaseResourcePtr's default-construct state
    // (mpResourceMemory/mHandle zeroed @+0x00..+0x08, mpNext/mpPrev/mpThis self-linked
    // @+0x0C/+0x10/+0x14, muThreadId 0 @+0x18), and the DecFIGS DWARF
    // (BrnProgressionManager.h:837/:838) names the two members as the resource
    // pointers below. StreetManager::FindRivalsByDistrict @ 0x82336360 proves the
    // first: it reads +133348's mpResourceMemory and calls
    // ResourcePtr<ProgressionData>::operator-> on it. No committed code used the
    // old names except the ctor's Reset() calls (now the members' own default ctors).
    CgsResource::ResourcePtr<BrnProgression::ProgressionData> mpProgressionData;  // X360 +133348 (0x20 stride)
    CgsResource::ResourcePtr<BrnAI::AISectionsData>           mpAISectionData;    // X360 +133380

    // Pointer-INVARIANT layout facts only (host is the LLP64 gate target). The X360 byte offsets are
    // NOT asserted: they do not survive the 32->64-bit pointer widening of the embedded Profile.
    static void _AssertLayout()
    {
        static_assert(KI_HANDLE_SLOT_COUNT == 18, "X360 ctor resets exactly 18 head handle slots");
        static_assert(sizeof(HandleSlot) == 20,   "X360 head record stride is 0x14 (20) bytes");
    }
};
}
