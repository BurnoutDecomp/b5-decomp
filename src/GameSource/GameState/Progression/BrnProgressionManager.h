#pragma once

#include "types.hpp"
#include "BrnCommonTypes.h" // CgsID (typedef u64)
#include "GameSource/GameState/BrnGameStateTypes.h" // BrnGameState::StuntElementType
#include "SharedClasses/Trigger/BrnGenericRegion.h"  // BrnTrigger::GenericRegion::Type (OnDriveThru param)
#include "BrnProfile.h"                              // BrnProgression::Profile (embedded sub-object, mProfile)
#include "GameShared/GameClasses/System/Resource/CgsResourcePtr.h" // CgsResource::ResourcePtr (mpProgressionData / mpAISectionData)
#include "GameShared/GameClasses/Containers/CgsArray.h"       // Array<T,N> (mQueueOfTrophyCarUnLocks)
#include "GameSource/GameState/BrnGameActions.h"                  // BrnGameState::GameStateModuleIO::TrophyUnlockAction (that array's element, by value)

#include <cstddef> // offsetof (uncalled _AssertLayout)
#include "GameSource/GameState/BrnGameStateSharedIO.h" // BrnGameState::GameStateModuleIO::GameActionQueue (real typedef)

namespace BrnAI { struct AISectionsData; }   // ResourcePtr<T> tag only (never dereferenced here)
namespace CgsModule { template <s32 BUFSIZE, s32 ALIGN> class VariableEventQueue; }   // SendGameCompletionResults param (pointer-only)
namespace CgsModule { template <s32 BUFSIZE, s32 ALIGN> class EventReceiverQueue; }   // Prepare2 / LoadProgressionData reply queue (pointer-only)
// The GameState module's output buffer -- LoadProgressionData reaches its RequestInterface<3072>
// through it. Pointer-only here; the .cpp includes the owning BrnGameStateModuleIO.h.
namespace BrnGameState { namespace GameStateModuleIO { struct OutputBuffer; } }

// Foreign types the additive DriveThruManager-facing accessors route by pointer (declare-only).
// Tags match the committed homes (CarData/ProgressionData = struct, AchievementManagerBase = class)
// to avoid a struct/class mismatch (C4099).
namespace BrnProgression  { struct CarData; struct ProgressionData; }
namespace BrnGameState    { class AchievementManagerBase; }
// [stuntrace waveB / agent 10] the event-finish progression writer's two foreign types.
// Both are POINTER-ONLY here (BrnTrainingManager.h itself includes THIS header, so pulling it
// in from here would be a cycle); BrnProgressionManager_EventFinish.cpp includes the real homes.
namespace BrnGameState    { class TrainingManager; }
// [drive-thru wave 2026-08-27] Construct's other two sibling managers, POINTER-ONLY here for the
// same reason as TrainingManager: BrnStuntManager.h forward-declares THIS class, and
// BrnGameStateStreetManager.h is a large closure. Tags match the committed homes -- both are
// `struct` (BrnGameStateStreetManager.h:234 / BrnStuntManager.h:67) -- to avoid C4099.
namespace BrnGameState    { struct StreetManager; struct StuntManager; class ModeManager; }
// [stuntrace waveB CLOSURE round] pointer-only parameters of GetStuntRunScoreTarget (declared
// below). Their real home is GameSource/GameState/ModeManager/GameModes/BrnGameModeParams.h,
// which this header must NOT pull in (it would drag the whole GameModeParams closure into every
// progression TU); the console body never dereferences the GameModeParams* at all.
namespace BrnGameState    { class GameModeParams; class StartGameModeParams; }
namespace BrnGameState    { namespace GameStateModuleIO { struct ShowModeResultsAction; } }
// [pause-stats wave 2026-08-29] GetGameStats' out-record. POINTER-ONLY here; its owning header
// (GameSource/GameState/SharedIO/BrnGameActionData.h) is included by
// BrnProgressionManager_GameStats.cpp, which is the only TU that fills one.
namespace BrnGameState    { namespace GameStateModuleIO { struct GameStats; } }
// mpVehicleList is a pointer member only (the bodies that walk it include the owning header).
namespace BrnResource     { struct VehicleList; struct VehicleListEntry; }
// BrnStreetData::ChallengeHighScoreEntry / ChallengePlayerScoreEntry come in via BrnProfile.h.

namespace BrnProgression
{
// [stuntrace waveB / agent 10] pointer-only in this header (SharedClasses/Progression/
// BrnRaceEventData.h is the owner; BrnProgressionManager_EventFinish.cpp includes it).
struct RaceEventData;
// [D1 profile-event-list wave, 2026-08-27] same treatment for the junction record
// AddEventTypeToEventTotals / UnlockToProgressionRank take by pointer. Same owner header.
struct EventJunction;

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
    // it (the body walks the LandmarkAISectionIndexPair table declared below). The
    // ONLY member OfflineGameMode::SelectRandomDestinations uses: for each accepted landmark
    // it stores the returned u16 into the lpaAISectionIndicesOut parallel output array.
    //
    // [stuntrace waveB MOUNT-CLOSURE round, 2026-08-26] BODIED (BrnProgressionManager.cpp). It
    // was one of the wave-B mount's 63 unresolved externals and had FIVE console callers
    // (OfflineGameMode::SelectRandomDestinations, ModeManager::SetOnlineLandmarks,
    // ModeManager::SetUpCheckPointsForGameMode, HACK_SetupRaceWithLandMarks,
    // GameStateModule::SendRouteRequestAction). The banner on the body states the one
    // bring-up caveat: its PRODUCER (ComputeLandmarkAISectionIndices) is not mounted yet.
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
    //
    // ⭐ [gateui] BODIED 2026-08-20 (owner `deps`). It was a measured UNDEF external in
    // StuntManager_gUI_00.obj -- the `miCurrentCount` of the action-58 HUD popup ("Billboards
    // 12/45") is literally this read. The console's `*(pm + 4104*type + 30568 + 4096)` IS
    // `mProfile.maStuntElements[type].GetLength()`: the embedded Profile sits at pm+368
    // (Profile::RecordPropHit is called as `(mpProgressionManager + 368, ...)` from
    // ProcessStuntElement) and Profile's own set array is at Profile+30200 -- 368 + 30200 ==
    // 30568, and Profile::GetStuntElementCount @0x82361950 reads `4104*type + this + 30200`
    // then `+4096`. Identical address, so this is a straight delegation, and the
    // set-sentinel assert the banner above promises lives in Set<>::GetLength (CgsSet.h:227,
    // "Set used before Construct/Clear was called") which that path already goes through.
    // FLAG (NAME, not shape): the DWARF spells this method `GetStuntElementCount`
    // (BrnProgressionManager.h:438, same `int32_t (StuntElementType) const` shape). The
    // committed repo name is kept because two live consumers spell it -- rename it and its two
    // call sites (AchievementManagerBase::OnCollectStunt, StuntManager::ProcessStuntElement)
    // in one pass when the full ProgressionManager TU lands.
    // (Body in BrnProgressionManager.cpp, next to its bodied accessor siblings.)
    s32 GetCollectedStuntElementCount(BrnGameState::StuntElementType leStuntType) const;

    // ADDITIVE GROW (declare-only) for the BrnTrainingManager TU.
    // X360 BrnProgression::ProgressionManager::GetProfile -- returns the embedded player Profile
    // (the X360 reaches it as the by-value sub-object at this+0x170). TrainingManager::
    // DEBUG_ClearTrainingFlags (0x82366050) / RequestTraining / SendTrainingTickerMessage call it,
    // assert the result is non-null, then poke training flags through it. Body + the real embedded
    // Profile member land with the ProgressionManager TU.
    //
    // ⓘ [gateui] 2026-08-20: this one is NOT a hole -- it is already bodied in this TU's .cpp
    // (BrnProgressionManager.cpp :: GetProfile). It shows up as an UNDEF external in
    // StuntManager_gUI_00.obj only because that is a different, and legitimately separate,
    // translation unit. Recorded here so the next reader does not re-chase it.
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
    //
    // ⛔ [gateui] PARKED 2026-08-20, NOT bodied. 0x82395C28 is short in shape --
    //     record = { s32 meGameMode = mpModeManager->meCurrentGameModeType (+3476);
    //                bool mbGameComplete; bool <profile flag @Profile+118032> }
    //     ... ComputeCompletionPercentage() >= 100.0 -> set both, and stamp the completion
    //         date via CgsSystem::DateAndTime::Update(&Profile+118008) once
    //     AddEvent(queue, &record, /*action*/208, /*size*/8)
    // -- but it is gated on the SAME missing 320-instruction
    // ProgressionManager::ComputeCompletionPercentage @0x8238A198, and it additionally needs
    // an mpModeManager back-pointer member (X360 +133436) that this header does not model and
    // nothing in the tree installs, plus three unmodelled Profile fields in the +118000
    // region. Posting the action with a fabricated percentage would put a wrong
    // "game complete" onto the game-action queue. See report_r2_deps.md.
    void SendGameCompletionResults( CgsModule::VariableEventQueue<13312, 16>* lpGameActionQueue );

    // ------------------------------------------------------------------------
    // ADDITIVE GROW (declare-only) for the BrnStuntManager TU.
    // The StuntManager spine (UpdateJumps / CheckForTrophyUnlocks) routes the stunt-element
    // done-check and the trophy/special-car unlocks through mpProgressionManager. Signatures +
    // semantics are X360-asm-attested; bodies land with the ProgressionManager TU. Declare-only.
    // ------------------------------------------------------------------------

    // ⛔ [gateui] TYPE DISCRIMINANT RESTORED 2026-08-20 (was `IsStuntElementDone(CgsID)`).
    // The completed-stunt-element sets are PER StuntElementType: the X360 Finds in
    // `mpProgressionManager + 4104*type + 30568` (StuntManager::ProcessStuntElement @0x8239CDB0,
    // `v29 = 4104 * HIDWORD(v8)`), and the 4104 stride is one Set<s64,512> per type. The old
    // type-less form had no way to choose a set (and no body anywhere in the tree): keying every
    // query off set 0 would make every billboard/smash read "not done" for ever, re-posting game
    // action 58 and re-popping the HUD on every re-smash.
    // Shape is the DWARF declaration verbatim:
    //   references/DecFIGS/dwarfdump/GameSource/GameState/Progression/BrnProgressionManager.h:447
    //   `bool IsStuntElementDone(BrnGameState::StuntElementType, CgsID) const;`
    // (its byte-exact sibling Profile::IsStuntElementDone(type, id) is already bodied at
    //  BrnProfile.cpp :: IsStuntElementDone). Body lands with owner `deps` this round.
    // ⓘ UpdateJumps' console call carries no type only because its type is JUMP == index 0, so
    // the `4104 * type` term folds away -- not because the query is type-less.
    // ⭐ [gateui] BODIED 2026-08-20 (owner `deps`), as the delegation the console inlines.
    // `Set<s64,512>::Find(mpProgressionManager + 4104*type + 30568, &key) != -1` and
    // `mProfile.maStuntElements[type].Find(id) != KU_INVALID` are the SAME address and the
    // SAME comparison: Profile::IsStuntElementDone @0x823619B0 is literally
    // `_int64_512_::Find(4104 * type + this + 30200, &id) != -1`, and the Profile sits at
    // ProgressionManager+368 (30200 + 368 == 30568). Neither side asserts, so nothing is
    // dropped by delegating. No standalone X360 symbol exists for the manager form -- it is
    // header-inlined on the console. (Body in BrnProgressionManager.cpp, next to its bodied
    // accessor siblings -- the same placement GetProfile / GetProgressionData already use.)
    bool IsStuntElementDone(BrnGameState::StuntElementType leStuntElementType,
                            CgsID                          lStuntElementKey) const;

    // X360 0x82389740. CheckForTrophyUnlocks fires this when an element-complete count tops out.
    // FLAG: the exact arg type/count is not recovered; modelled as a single trophy-id s32 (the
    // X360 li-immediate the call site passes). DWARF :345 types the parameter
    // `BrnProgression::TrophyUnlockData::UnlockType` (an enum with no owning header in this
    // tree yet), which is consistent with the s32 model.
    //
    // ⭐⭐ BODIED 2026-08-27 (drive-thru link-closure wave), in BrnProgressionManager_Unlocks.cpp.
    // The 2026-08-20 park below is DISCHARGED, and two of its three blockers had already gone
    // stale by the time it was written:
    //   * "an owning header for ProgressionData's trophy table -- MISSING" -- the table's ROOT
    //     was already modelled (BrnProgressionData.h muaTrophyUnlocks/muTrophyUnlockCount @0x40,
    //     with GetTrophyUnlock @0x823569F0 bodied). What was genuinely missing was the ELEMENT:
    //     TrophyUnlockData existed only as a members-less placeholder in BrnGameActions.h, so
    //     the bodied accessor was indexing a 16-byte serialised table with a stride of ONE. It
    //     now has its DWARF home, SharedClasses/Progression/BrnTrophyUnlockData.h.
    //   * "four unmodelled manager/Profile fields (+776, +780, +482 ...)" -- all three ARE
    //     modelled: manager+776/+780/+482 are Profile+408/+412/+114, i.e.
    //     miTotalTakedownCount / miTotalOnlineVerticleTakedownCount /
    //     mi8PowerParkingBetweenOtherPlayersBestRating. Only their accessors were missing.
    //   * UnlockCarFromTrophy @0x8237B0E8 was real, and is bodied in the same TU.
    // ⚠️ Parameter stays s32, not the DWARF's TrophyUnlockData::UnlockType: the console passes
    // it in r4 as a plain word and compares it with `cmpw` against the record's zero-extended
    // u16, Profile::AddDriveThru already returns the awarded type as s32, and the committed call
    // sites pass integer literals. The enum's values are the ones in BrnTrophyUnlockData.h.
    void OnTrophyUnlock(s32 liTrophyType);

    // X360 0x8237B0E8. Award the car a trophy unlocks. Returns false (and does nothing) when the
    // profile already owns it -- which is what stops OnTrophyUnlock's table walk from re-awarding
    // the same car on every re-evaluation, and is why OnTrophyUnlock breaks out of its loop on a
    // true. On success it adds the car as E_UNLOCK_TYPE_TROPHY, seeds its unlock-sequence deform
    // to 0.85f, and appends a TrophyUnlockAction to mQueueOfTrophyCarUnLocks below.
    // ⚠️ ARG SHAPE FROM ASM: r3=this, r4=the 64-bit CgsID, r5=the unlock type. Hex-Rays renders
    // this `(__int64 a1, int a2)` because it fused r3:r4 into one 64-bit `a1` -- the classic PPC
    // register-pair confusion. There is no doubleword first argument.
    bool UnlockCarFromTrophy(CgsID lCarId, s32 liTrophyType);

    // X360 0x8237AF38. Award every vehicle-list entry whose LIVERY TYPE (VehicleListEntry+0xE9,
    // GetLiveryType) equals lu8LiveryType and whose PARENT car the profile already owns.
    // CheckForSpecialCarUnlocks calls it with 4 (the rank-gated set) and 3 (the 100%-gated set).
    void UnlockSpecialCars(u8 lu8LiveryType);

    // ⭐⭐ X360 0x8238A198 (321 instructions) -- THE GAME-COMPLETION PERCENTAGE. The weighted sum
    // both special-car unlocks and the game-completion results record test against 100.0f.
    // The six .rdata weights close EXACTLY on 100, which is the cross-check that they were read
    // correctly: 9 (time road rules) + 9 (crash road rules) + 11 (rivals beaten) + 11 (the three
    // stunt-element fractions MULTIPLIED) + 2.5 (drive-thrus) + 2.5 (events found) == 45, and
    // GetPercentageOfEventsCompleted contributes the other 55.
    f32 ComputeCompletionPercentage();

    // X360 0x8237B390. The progression-rank half of the completion sum: the sum of the authored
    // per-rank contributions for every rank already earned, plus the next rank's contribution
    // scaled by progress toward its medal threshold, normalised to a 0..55 range.
    f32 GetPercentageOfEventsCompleted();

    // X360 0x8236FBC8 / 0x8236FB10. The rivals term's numerator and denominator. "True" rivals
    // are the ones NOT flagged Rival::mbIsUsedForRankUpGiftCar (+0x17) -- the gift-car entries
    // are not opponents you can beat.
    s32 GetNumberOfBeatenRivals();
    s32 GetTrueNumberOfRivals();

    // ⭐⭐ [pause-stats wave 2026-08-29] X360 0x8238A6A0 (566 instructions) -- THE PAUSE
    // SCREEN'S STAT PANEL. Fills a GameStats record from this manager, its embedded Profile,
    // the StreetManager, the StuntManager and the AchievementManager. Its ONE caller is
    // GameStateModule::ProcessGameEvents case 79 @0x823A2D18, which posts the filled record as
    // game action 180 (352 bytes); TranslateGameActionsToGuiEvents case 180 turns that into GUI
    // event 436, which BrnGui::CrashNavDriverDetails::HandleStatData and BrnGui::CrashNavStats
    // both read. Body: BrnProgressionManager_GameStats.cpp.
    //
    // ⚠️ THE THIRD PARAMETER IS X360-ONLY AND IT IS THE POINT. The PS3 DWARF declares
    // `void GetGameStats(GameStats*, BrnGameState::StuntManager*) const` -- two parameters. The
    // X360 call site loads THREE (`addi r3,r31,0x7E20 / bl CountCompletedChallenges / mr r6,r3`
    // @0x823A2D18..0x823A2D38), and the body stores that r6 straight into the record's
    // maIntValues[32] (`stw r10, 0x98(r28)` @0x8238AF68) -- the extra int enumerator the X360
    // GameStats has and the PS3 one does not. Same merge-window delta, seen from both ends.
    //
    // ⚠️ CONSTNESS: the DWARF says `const`, and the console body really does only read. It is
    // declared NON-const here because ComputeCompletionPercentage() -- which it calls, and whose
    // f32 return goes straight into maFloatValues[E_FLOAT_VALUE_PERCENTAGE_COMPLETE] -- is
    // non-const in this tree (the DWARF has that one const too). One deviation, named, rather
    // than a const_cast or a same-wave re-qualification of a function four other TUs call.
    void GetGameStats(BrnGameState::GameStateModuleIO::GameStats* lpGameStats,
                      const BrnGameState::StuntManager*           lpStuntManager,
                      s32                                         liNumChallengesCompleted);

    // X360 0x82370510 (50 instructions). The medal threshold of the player's CURRENT rank --
    // i.e. how many wins the next rank costs in total. Asserts the cached rank byte is
    // non-negative and in range, then returns the rank record's mu16MedalThresholdToNextRank
    // (`lhz r3, 0x4C(r31)` -- UNSIGNED, no extsh). DWARF BrnProgressionManager.h return type is
    // uint32_t. Body: BrnProgressionManager_GameStats.cpp (its only caller lives there).
    u32 GetTotalWinsForNextRank();

    // X360 0x82396058. Re-evaluates whether any special car should unlock after a stunt-element
    // milestone; CheckForTrophyUnlocks calls it unconditionally after the trophy path.
    //
    // ⛔ [gateui] PARKED 2026-08-20, NOT bodied. Same reason: 0x82396058 gates the silver-car
    // unlock on `mProfile.GetCurrentProgressionRank() >= ProgressionData[+20]` and the
    // gold-car unlock on `ComputeCompletionPercentage() >= 100.0`, and needs two missing
    // bodies --
    //     BrnProgression::ProgressionManager::ComputeCompletionPercentage @0x8238A198 (320 insns)
    //     BrnProgression::ProgressionManager::UnlockSpecialCars           @0x8237AF38 (106 insns)
    // -- plus the two Profile unlock flags at Profile+42516/+42517 and a ProgressionData
    // layout that is not modelled. (AchievementManagerBase::OnGameCompletion, the third
    // callee, DOES exist.) See report_r2_deps.md.
    void CheckForSpecialCarUnlocks();

    // ------------------------------------------------------------------------
    // ADDITIVE GROW (declare-only) for the BrnDriveThruManager TU.
    // The drive-thru discovery / body-shop-repair / paint-shop flow routes discovery, repair,
    // colour and achievement work through ProgressionManager. Signatures + semantics are
    // X360-asm-attested; bodies land with the ProgressionManager TU. Declare-only.
    // ------------------------------------------------------------------------

    // X360 0x82399DD0. Record discovery of drive-thru lId of kind leType and post the resulting
    // game actions onto lpQueue.
    void OnDriveThru(CgsID lId, BrnTrigger::GenericRegion::Type leType, BrnGameState::GameStateModuleIO::GameActionQueue* lpQueue);

    // The player's currently-selected car record (NULL when none). DriveThruManager body/paint
    // shops read its id and write its colour/palette.
    CarData* GetCurrentCarData();

    // The chosen-livery record for that car (NULL when none). ADDITIVE GROW (A9 scoring-feed
    // wave 2026-08-27): the console has no out-of-line accessor -- every reader inlines the
    // `manager + 133332` adjust -- and this is the second such reader to need it.
    //   AddDistanceDriven @0x823668F0 ACCUMULATES into `*(*(this + 133332) + 16)`;
    //   GameStateModule::CopyScoringDataToOutput @0x8236CDC0 PUBLISHES the same float
    //   (`lwzx r11, gsm+47920, 0x208D4 ; lfs f0, 0x10(r11) ; stfs f0, 0xAA4(out)`) as
    //   ScoringOutputInterface::mfDistanceDrivenInCurrentCar, or 0.0f when the pointer is null.
    // LiveryData::mfDistanceDriven is at +0x10 (BrnProgressionLiveryData.h, pinned by
    // Profile::SetChosenLiveryIdForBaseCar's `stfs 0.0, 0x10(r11)`), which is what makes the
    // +133332 seat -- and this accessor -- the right one rather than mpCurrentCarData@+133328.
    LiveryData* GetCurrentLiveryData();

    // The loaded ProgressionData resource (the event-junction table UnlockCarChallengeForCar walks).
    const ProgressionData* GetProgressionData() const;

    // X360 0x8237C0D8. Read car lCarId's current colour + palette indices into the two out-params.
    void GetCarColourAndPalette(CgsID lCarId, s32* lpiColour, s32* lpiPalette);

    // X360 0x82363630. Clear the deform/damage on the just-repaired car lCarId.
    void RepairUnlockedVehicle(CgsID lCarId);

    // The embedded achievement manager (X360 *(progmgr+133432)); DriveThruManager routes
    // OnFindAllCarParks / OnBodyShop through it.
    //
    // ⭐ [gateui] BODIED 2026-08-20 (owner `deps`). Measured UNDEF external in
    // StuntManager_gUI_00.obj. It is a plain read of the pointer Prepare2 installs: the
    // member below is documented as X360 +0x20938 == 133432, which is the very word
    // CheckForSpecialCarUnlocks @0x82396058 dereferences (`OnGameCompletion(*(a1 + 133432))`)
    // and that StreetManager / ChallengeManager / ImageManager all reach the same way. Not a
    // by-value sub-object despite the older "embedded" wording -- the X360 loads a pointer.
    // DWARF :588 types the return `StuntModeScoring::AchievementManager *`; the committed
    // `BrnGameState::AchievementManagerBase *` is what every in-tree consumer is written
    // against and what the X360 ledger attests, so it is kept.
    // ⚠️ FLAG (PC bring-up, NOT introduced here): nothing in the mounted set calls Prepare2,
    // so this returns NULL today. Every console caller asserts it non-null first and
    // StuntManager_gUI_00.cpp reproduces that assert -- so a null shows up as the console's
    // own diagnostic, not a silent deref.
    // (Body in BrnProgressionManager.cpp, next to its bodied accessor siblings.)
    BrnGameState::AchievementManagerBase* GetAchievementManager();

    // ------------------------------------------------------------------------
    // ADDITIVE GROW (declare-only) for the BrnGameState::CarSelectManager (junkyard car-select) TUs.
    // Signatures + semantics are X360-asm-attested; bodies land with the ProgressionManager TU.
    // ------------------------------------------------------------------------

    // X360 0x823701D8. The player's current progression rank, CLAMPED to the loaded
    // ProgressionData's rank table: a negative cached rank answers 0, and a rank at or past the
    // table's end answers count-1. Compared against a car's required rank in
    // IsThisCarInCurrentUnlockSequence, and sign-extended from a byte by
    // GameStateModule::OnPlayerCarChange.
    s32 GetProgressionRank() const;

    // ADDITIVE GROW [pause-stats wave 2026-08-29] -- miSponsorCarCount (+133468), read BY NAME so
    // the GUI bridge stays off the raw offset. TranslateGameActionsToGuiEvents case 180
    // @0x823EC8D4 reads it (`lwzx r7, r29, 0x69598C` == manager+133468) as the DENOMINATOR of the
    // panel's "Cars N of M" line, and again halved (`srawi r7,r7,1 / addze`) as the "drivers"
    // total. X360-inlined, no standalone symbol.
    // ⚠️ FLAG, AND IT CUTS AGAINST THE MEMBER'S OWN NAME: this member is documented below as
    // "AddCar increments it for every E_UNLOCK_TYPE_SPONSOR car and once more for CARBEAGT",
    // with the name FLAGGED as inferred from those two increments alone. This consumer uses it
    // as the total number of collectable cars. Both readings cannot be right; the offset is
    // certain, the NAME is not. Left as-is rather than renamed on one more witness.
    s32 GetSponsorCarCount() const { return miSponsorCarCount; }

    // ------------------------------------------------------------------------
    // [stuntrace waveB fix round, 2026-08-26] ADDITIVE GROW -- the two per-mode rank queries the
    // road-rage takedown target hangs on (verify batch 5 MF5). Shapes are DWARF-attested
    // (dwarfdump .../BrnProgressionManager.h:282 and :285) and the argument registers were re-read
    // from the asm, not taken from the implementer report.
    // [x] NO LONGER DECLARE-ONLY -- BOTH ARE BODIED as of the 2026-08-26 CLOSURE round, in
    // BrnProgressionManager.cpp, together with GetStuntRunScoreTarget below. The one type they
    // needed, BrnProgression::ProgressionRankData, grew the four DWARF-named rank-up threshold
    // bytes (BrnGameModeParams.h) to make it possible.
    // ------------------------------------------------------------------------

    // X360 0x8237B4E8. The player's progression rank scoped to ONE offline game mode. DWARF
    // returns int8_t, which is why every X360 caller `extsb`s r3 -- e.g.
    // ModeManager::GetRoadRageTakedownTarget @0x82327518 calls it three times, always as
    // `li r4,3 / lwz r3,0x6D5C(this) / bl ... / extsb`, and ModeManager::SetupGameMode
    // @0x8234B158 does the same at 0x8234B544..0x8234B558.
    s8  GetProgressionRankForGameMode(BrnGameState::GameStateModuleIO::EGameModeType leGameModeType) const;

    // ------------------------------------------------------------------------
    // ⭐⭐ [stuntrace wave D, D3] THE THREE RANK-AS-RATIO QUERIES StartGameModeParams::
    // SetProgressionRankAsRatio is fed from. GameStateModule::StartModeAtLights @0x82396CF8 forks
    // between the first two on the mode (@0x823970E0..0x82397134: modes 0, 3, 7 and 8 take the
    // per-mode pair, everything else takes the global one), and the value it publishes scales the
    // event's difficulty -- StuntAttackMode::Start and RaceMode::Start both read it back.
    // All three are BODIED in BrnProgressionManager.cpp; each is walked instruction for
    // instruction from its own export (the Hex-Rays pseudocode of all three is float/int-union
    // garbage -- the returns live in f1 and IDA drops them -- so the asm is the only source).
    // ------------------------------------------------------------------------

    // X360 0x82370340. `clamp(lfRank / (rankCount - 1), 0, 1)`, with the console's own
    // "Max Rank set to <n>" assert (BrnProgressionManager.cpp:3995) when the denominator is not
    // positive and its "Normalised rank is ..." debug line. The two clamps are the asm's fsel
    // pair @0x8237044C/0x82370458 against 0.0f (flt_82001CC0) and 1.0f (flt_82001C98).
    f32 GetProgressionRankNormalised(f32 lfRank) const;

    // X360 0x8237B610 (exported unnamed; called by StartModeAtLights @0x82397134 as the
    // GLOBAL arm of the rank fork). Feeds GetProgressionRankNormalised the player's rank --
    // the last authored rank when the cached rank byte has reached the rank COUNT, otherwise
    // GetProgressionRank(). NAME is descriptive: no symbol survives for it.
    f32 GetProgressionRankNormalisedForCurrentRank() const;

    // X360 0x8237BE10. The PER-MODE rank ratio: the mode's own rank, plus the player's fractional
    // progress between that rank's win threshold and the next one, expressed as a PERCENTAGE and
    // then scaled by 0.01 -- the console literally computes `100.0f / maxRank` (flt_820049E0 ==
    // 100.0f @0x8237BF10) and multiplies the sum by flt_82029F24 == 0.01f @0x8237BF6C.
    // Returns flt_82001C98 == 1.0f when the mode is already at (or past) the last rank.
    f32 GetProgressionRankForGameModeNormalised(
            BrnGameState::GameStateModuleIO::EGameModeType leGameModeType) const;

    // ------------------------------------------------------------------------
    // [stuntrace waveB CLOSURE round, 2026-08-26] ADDITIVE GROW (declare-only). X360 0x8237B6B0.
    // The stunt-race target score when the profile carries no per-event target: interpolate the
    // event's per-rank stunt scores across the player's position BETWEEN two progression ranks,
    // then round to 2 significant figures. Its ONE caller is StuntAttackMode::Start @0x82332150
    // (`lwz r3, 0x6D5C(modeMgr) / mr r4, r31 (lpGameModeParams) / mr r5, r29
    // (lpStartGameModeParams) / bl`), so the argument shape below is register-attested, not
    // inferred; the body reads the event record off the START params (`lwz r19, 0x32C(r5)` ==
    // StartGameModeParams::mpEventData) and never dereferences the GameModeParams* at all -- the
    // second parameter is carried for the console's signature, exactly as it is passed.
    // Return type is the s32 the caller `extsw`s at 0x82332154 before converting it to f32.
    //
    // [x] BODIED 2026-08-26 (CLOSURE round), BrnProgressionManager.cpp. The banner that stood here
    // listed FOUR blockers; three were already false when it was written and the fourth closed in
    // the same round, so the frontier is gone rather than deferred:
    //   (a) "a real ProgressionRankData LAYOUT" -- it needs exactly ONE byte, rank+0x61, which is
    //       now ProgressionRankData::GetNumWinsToRankUpStunt (DWARF BrnProgressionRankData.h:311)
    //       and is reached through GetRankThresholdForEvent, so no offset arithmetic enters the
    //       body. [2026-08-26 MOUNT-CLOSURE round: the real layout now exists in full --
    //       SharedClasses/Progression/BrnProgressionRankData.h, 112 bytes with a sizeof pin -- and
    //       the BrnGameModeParams.h stand-in this line used to name is retired.]
    //   (b) RaceEventData::GetRankScore @0x823543D0 -- was already bodied (BrnRaceEventData.cpp:42);
    //   (c) BrnMath::RoundWithNumSignificantFigures -- was already bodied (BrnMathUtils.cpp:123);
    //   (d) GetRankThresholdForEvent -- bodied this round, just below.
    // The de-inlined shape the old banner recorded proved correct against the asm, with two
    // corrections worth keeping: the top-rank test is against `(s8)(rankCount - 1)` (the console's
    // own liLastRankForGameMode, `extsb` of count-1), and the console's tail carries TWO
    // RoundWithNumSignificantFigures calls -- one inside the dropped debug-print block whose result
    // is discarded, and the real one at 0x8237BDFC. Rounding twice would be the naive transcription.
    // Assert: "lpStuntRunEventData != NULL" (line 3891).
    s32 GetStuntRunScoreTarget(const BrnGameState::GameModeParams* lpGameModeParams,
                               const BrnGameState::StartGameModeParams* lpStartGameModeParams) const;

    // X360 0x82370260. The event-count threshold at which a given rank is reached for a given
    // mode. DWARF :285 `int32_t GetRankThresholdForEvent(int32_t, EGameModeType)`; the argument
    // order is fixed by the two out-of-line calls in ModeManager::GetRoadRageTakedownTarget --
    // 0x82327774..0x82327780 (`li r5,3` == the mode, `mr r4,r28` == the rank) and
    // 0x82327788..0x823277A8 (`addi r4,r28,1`, same `li r5,3`).
    s32 GetRankThresholdForEvent(s32 liProgressionRank,
                                 BrnGameState::GameStateModuleIO::EGameModeType leGameModeType) const;

    // X360 0x8237A970. Add lCarId to the player's owned-car list with unlock-type leUnlockType and
    // return the new CarData record (asserts the result non-null internally). ⚠️ ARG SHAPE: the X360
    // call from ProgressionManager::OnPlayerCarChange @0x8237AC38 passes (this, carId, 0) -- the
    // Hex-Rays 11-argument prototype is register-pair noise. Modelled as the 2-arg form both real
    // call sites use.
    CarData* AddCar(CgsID lCarId, s32 leUnlockType);

    // X360 0x8237AC38. The progression layer's half of a player-car swap: persist the chosen car +
    // wheel onto the profile, make sure the car is owned, and cache the car's chosen-livery record.
    // When lbUpdateProfile is false it only clears the cached current-car record.
    // ARG SHAPE FROM ASM: r3=this, r4=carId, r5=wheelId, r6=the bool.
    void OnPlayerCarChange(CgsID lCarId, CgsID lWheelId, bool lbUpdateProfile);

    // X360 UpdateExitState de-inlined byte poke at ProgressionManager+133489 (`stbx 1`) -- a
    // rivals-update request flag. FLAG: de-inlined byte poke, not a named member in the exports.
    void RequestUpdateRivals();

    // X360 UpdateExitState de-inlined byte poke at ProgressionManager+133512 (`stbx 1`) -- a
    // drive-thrus/rivals dirty flag. FLAG: de-inlined byte poke, not a named member in the exports.
    void SetDriveThrusDirtyFlag();

    // X360 this+133448 (0x20948). The loaded vehicle list the progression layer resolves car
    // records through (ProgressionManager::OnPlayerCarChange / GetCarColourAndPalette / AddCar all
    // read it). ⚠️ FLAG (PC bring-up): nothing installs it yet -- Prepare2's caller does on the
    // console; every body that uses it null-checks first.
    void SetVehicleList(const BrnResource::VehicleList* lpVehicleList);

    // ========================================================================
    // [stuntrace waveB / agent 10 -- THE EVENT-FINISH PROGRESSION WRITERS]
    // Bodies in BrnProgressionManager_EventFinish.cpp (a per-function partfile of this TU,
    // the house Scoring/BrnScoringSystem_*.cpp precedent). Shapes are the DecFIGS DWARF's
    // (references/DecFIGS/dwarfdump/GameSource/GameState/Progression/BrnProgressionManager.h
    // :330 / :564 / :270) with the X360 asm as the tiebreaker on argument roles.
    // ========================================================================

    // X360 0x823A0040. THE offline progression payoff: called from ModeManager::ShowModeResults
    // @0x823436D0 for offline modes {0,3,5,7,8} once a mode has finished. Marks the profile's
    // ProfileEvent finished/won, tallies the game-mode completion + win, arms the "all win types"
    // deferred check, unlocks the event's car, and posts the autosave / traffic-scale actions.
    // DWARF :330 `void OnEventFinishUpdateProfile(InputBuffer::GameActionQueue*, uint32_t,
    // ShowModeResultsAction*, BrnGameState::GameStateModuleIO::EGameModeType);`
    void OnEventFinishUpdateProfile(BrnGameState::GameStateModuleIO::GameActionQueue* lpGameActionQueue,
                                    u32 luEventId,
                                    BrnGameState::GameStateModuleIO::ShowModeResultsAction* lpAction,
                                    BrnGameState::GameStateModuleIO::EGameModeType leGameModeType);

    // X360 0x82366B30. True when the profile's ProfileEvent for luEventId already carries
    // E_FLAG_RANK_WIN (the asm's `(flags >> 2) & 1`). ModeManager::ShowModeResults negates it to
    // fill the results action's "first win" byte. DWARF :564 `bool HasEventBeenWonPreviously(uint32_t);`
    bool HasEventBeenWonPreviously(u32 luEventId);

    // X360 0x82370180. True when the cached progression-rank byte has reached the loaded
    // ProgressionData's rank COUNT (i.e. the player is past the last authored rank). Note this
    // reads the RAW sign-extended byte, not the clamped GetProgressionRank(). DWARF :270
    // `bool PlayerHasFinishedLastRank() const;`
    bool PlayerHasFinishedLastRank() const;

    // Installer for the mpTrainingManager back-pointer below (X360 +133440). Same shape/precedent
    // as SetVehicleList. ⚠️ FLAG (PC bring-up): nothing calls it yet -- see the member's banner.
    void SetTrainingManager(BrnGameState::TrainingManager* lpTrainingManager);

    // ========================================================================
    // ⭐⭐ [D1 profile-event-list wave, 2026-08-27] THE PROFILE EVENT-LIST PRODUCER.
    // Bodies in BrnProgressionManager.cpp. These two are the ONLY console writers of the
    // profile's ProfileEvent table anywhere in the image: Profile::AddEvent @0x82359EB8 has
    // EXACTLY ONE xref in BURNOUT_X360_ARTIST.XEX and it is UnlockToProgressionRank.
    //
    // Before this landed, mProfile.GetEventCount() was 0 for the whole run and every consumer
    // that looks a record up got NULL -- which is the root of the "winning an offline event
    // asserts `lpEvent` (BrnProgressionManager.cpp:1669) then crashes" class, and of the five
    // [PC GUARD]s the 2026-08-27 flyby wave had to add in BrnPreRaceFlyBy_wJ_06/07.cpp.
    // ========================================================================

    // X360 0x8239DDE8. DWARF :674 `void UnlockToProgressionRank(int8_t, InputBuffer::
    // GameActionQueue*);`. Two disjoint bodies selected by the rank argument:
    //   li8Rank == 0 -> the NEW-PROFILE unlock: walk every authored EventJunction and give the
    //                   profile a ProfileEvent for each junction that has an OFFLINE event,
    //                   tallying its game-mode type; then the default-car / starting-drive-thru
    //                   arms and the rank tail.
    //   li8Rank != 0 -> the RANK-UP path: AchievementManagerBase::OnLicenseUpgrade + the
    //                   licence-upgrade telemetry event, then the same rank tail.
    // Console callers: UpdatePlayerMedals @0x8239FE50 (via PreWorldUpdate's medals-dirty gate)
    // and four ProgressionDebugComponent skip-to-rank entries. See the body banner for exactly
    // which arms are landed and which are parked on unmounted siblings.
    void UnlockToProgressionRank(s8 li8Rank,
                                 BrnGameState::GameStateModuleIO::GameActionQueue* lpGameActionQueue);

    // X360 0x82366628. DWARF :678 `void AddEventTypeToEventTotals(const EventJunction*);`.
    // Maps the junction's OFFLINE event's data mode (RaceEventData +0xEC) through GetEvent()
    // to a runtime EGameModeType and bumps mProfile.maGameModeTypeAmount for it -- the
    // denominator of every "Races 3/12"-style progression readout.
    void AddEventTypeToEventTotals(const EventJunction* lpEventJunction);

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
    // ⭐⭐ NAME + TYPE CORRECTION (drive-thru wave, 2026-08-27): a3 is the MODE MANAGER, not the
    // GameState module. It was committed as `void* lpGameStateModule` -> `mpGameStateModule`, but
    // (a) the sole caller already passes `&mModeManager` (BrnGameStateModule.cpp:887), and (b) the
    // console proves the type: SendGameCompletionResults @0x82395C28 does `lwzx r11, r31, 0x2093C`
    // then `lwz r11, 0xD94(r11)` -- +0xD94 == 3476 == ModeManager::meCurrentGameModeType, whose
    // getter this header can now call. A void* back-pointer that is silently the wrong class is how
    // a +3476 read lands in the wrong object.
    // FLAG: the SetupRoamingSections argument list is still modelled as the void* the X360 forwards.
    //
    // ⚠️ SIGNATURE CORRECTION (2026-08-11): the fourth argument is the GameState module's
    // EventReceiverQueue<3072,16> (X360 `a1 + 232384`, the same queue GameStateModule::Prepare
    // hands TriggerQueryManager::Prepare), NOT the VariableEventQueue<13312,16> game-action queue
    // the earlier declaration named. LoadProgressionData below drains replies out of it, so the
    // wrong type was not merely cosmetic.
    bool Prepare2(BrnGameState::GameStateModuleIO::OutputBuffer* lpOutput,
                  BrnGameState::ModeManager* lpModeManager,
                  CgsModule::EventReceiverQueue<3072, 16>* lpReceiverQueue,
                  void* lpTriggerData, BrnGameState::AchievementManagerBase* lpAchievementManager);

    // X360 0x82399ED0. The console's PROGRESSION.DAT loader -- the resumable five-stage machine
    // Prepare2 gates on. Returns true once the "ProgressionData" resource has been acquired and
    // bound into mpProgressionData; false while a reply is still outstanding.
    bool LoadProgressionData(BrnGameState::GameStateModuleIO::OutputBuffer* lpOutput,
                             CgsModule::EventReceiverQueue<3072, 16>* lpReceiverQueue);

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
    // [stuntrace waveB / agent 10] The "player is on the LAST authored rank" arm the console
    // emits TWICE inside OnEventFinishUpdateProfile (loc_823A02C4 and loc_823A034C, byte
    // identical): compare (s8)(rankCount - 1) against the cached rank byte and, on a match,
    // arm the deferred all-win-types check with the finished event's mode. Factored to one
    // private helper rather than duplicated; no console symbol of its own (it is inlined at
    // both sites). Body in BrnProgressionManager_EventFinish.cpp.
    void ArmAllWinTypesCheckIfAtLastRank(const RaceEventData* lpcRaceEventData);

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

    // [FLAG PC bring-up, 2026-08-24 deform-land wave] once-latch for the Profile::Construct
    // boot seam in Prepare2 (the console's outer Construct/Prepare pair own that call; neither
    // outer is reconstructed yet -- see the Prepare2 banner). Not a console member.
    bool mbProfileConstructed = false;

    // [FLAG PC bring-up, 2026-08-27 D1 profile-event-list wave] once-latch for the
    // UnlockToProgressionRank(0) boot seam in Prepare2. The console reaches that call through
    // PreWorldUpdate @0x823A4F68 -> (medals-dirty byte +133491) -> UpdatePlayerMedals
    // @0x8239FE50; NEITHER of those two is reconstructed, so the rank-0 unlock is driven from
    // the same Prepare2 seam that already owns Profile::Construct. Not a console member.
    // DELETE-WHEN UpdatePlayerMedals + PreWorldUpdate land and drive it the console's way.
    bool mbInitialRankUnlockDone = false;

    // ⭐ X360 +133128 (0x20808) -- THE TROPHY-CAR UNLOCK QUEUE, named by the console's own assert
    // strings: SendTrophyUnlockUpdate @0x823892B8 fires
    // "mQueueOfTrophyCarUnLocks[lTrophyUnlockToSend].meUnlockType != TrophyUnlockData::
    //  E_UNLOCKTYPE_NONE" and "...mCarToUnlock != kCGSID_NULL" against its elements.
    // UnlockCarFromTrophy appends; SendTrophyUnlockUpdate posts the tail element as game action
    // 204 (size 16) and Erases it. The 12-entry bound is the X360 template instantiation
    // Array<TrophyUnlockAction,12> (Append @0x8235E1F0 / Erase @0x8235E318), and the arithmetic
    // closes: 12 * 16 == 192 == 0xC0, and Construct @0x8237A5F8 zeroes exactly +133320 ==
    // 133128 + 192, which is the count word SendTrophyUnlockUpdate's "Array used before
    // Construct/Clear was called" assert reads.
    Array<BrnGameState::GameStateModuleIO::TrophyUnlockAction, 12>
        mQueueOfTrophyCarUnLocks;                              // X360 +133128 (count word +133320)

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
    BrnGameState::ModeManager*             mpModeManager;        // X360 +0x2093C (Prepare2's a3)
    BrnGameState::AchievementManagerBase*  mpAchievementManager; // X360 +0x20938 (a6)

    // ---- [drive-thru wave 2026-08-27] the two CONSTRUCT back-pointers ComputeCompletionPercentage
    // reads. NAMED BY THE CONSOLE'S OWN ASSERT STRINGS: ProgressionManager::Construct @0x8237A5F8
    // asserts "lpStreetManager != NULL" / "lpStuntManager != NULL" on its 3rd and 5th arguments and
    // then stores them at +133424 and +133444 respectively (`*(a1 + 133424) = a3;
    // *(a1 + 133444) = a5;`). These are Construct's, not Prepare2's.
    // ⚠️ FLAG (PC bring-up, NOT introduced here): nothing in the mounted set calls
    // ProgressionManager::Construct, so both read NULL today -- the same hole mpVehicleList /
    // mpAchievementManager / mpTrainingManager already sit in. ComputeCompletionPercentage guards
    // each one and logs once rather than dereferencing; see its body.
    BrnGameState::StreetManager*           mpStreetManager = 0;  // X360 +133424 (0x20930)
    BrnGameState::StuntManager*            mpStuntManager  = 0;  // X360 +133444 (0x20944)

    // ---- [stuntrace waveB MOUNT-CLOSURE round, 2026-08-26] the landmark -> AI-section cache ----
    // DWARF BrnProgressionManager.h:809/:810 give the record verbatim:
    //     struct LandmarkAISectionIndexPair { uint32_t mId; uint16_t muAISectionIndex; };
    // and BrnProgressionManager.h:826 gives the member:
    //     LandmarkAISectionIndexPair[512] maLandmarkAISectionIndices;
    //
    // THE X360 CONFIRMS BOTH INDEPENDENTLY, and the confirmation is exact rather than
    // circumstantial:
    //   * FindLandmarkAISectionIndex @0x82359AE0 walks the table from `this + 128904` (0x1F788)
    //     with `addi r11, r11, 8` -- an 8-byte stride, i.e. {u32, u16} padded to 8 -- reading
    //     `lwz r8, 0(r11)` for the id and returning `lhz r3, 4(r11)` for the section index.
    //   * ComputeLandmarkAISectionIndices @0x82370008, the table's PRODUCER, writes the same
    //     two slots at the same stride (`*v12 = landmark[+36]`, the section index into +4,
    //     `v12 += 2` over an int*), and its assert names the record's first field: the message
    //     is literally "lpEntry->mId != BrnWorld::KI_INVALID_SECTION_INDEX".
    //   * THE ARITHMETIC CLOSES. 512 * 8 == 4096 == 0x1000, so the table spans +0x1F788..+0x20788
    //     -- and +0x20788 is 133000, which is exactly the offset this header's own
    //     mDebugComponent comment (immediately below, and the very next DWARF member at :827)
    //     already records for the debug component. The array size, the stride and the two
    //     neighbours all agree; nothing here is inferred from a single witness.
    //
    // The LIVE length is NOT stored here -- both the reader and the producer take it from the
    // trigger data (`*(*(this + 0x20924) + 0x34)` == TriggerData::miLandmarkCount, reached
    // through mpTriggerData below). 512 is the authored capacity only.
    struct LandmarkAISectionIndexPair
    {
        u32 mId;                 // +0x00 (DWARF :809) the landmark's own id (Landmark +0x24)
        u16 muAISectionIndex;    // +0x04 (DWARF :810) the nearest AI section to that landmark
    };
    static const s32 KI_LANDMARK_AI_SECTION_INDEX_COUNT = 512;   // DWARF :826 array bound
    LandmarkAISectionIndexPair maLandmarkAISectionIndices[KI_LANDMARK_AI_SECTION_INDEX_COUNT];

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
    // X360 +133340 (0x2085C) -- the word LoadProgressionData @0x82399ED0 switches on (`v6 = a1 +
    // 133340`), i.e. the DWARF's ProgressionManager::LoadStage. It sits immediately before the
    // resource pointer below, exactly as the console lays it out. The five states are the
    // console's own switch cases 0..4.
    enum ELoadStage
    {
        E_LOADSTAGE_NOT_STARTED      = 0,   // nothing requested yet
        E_LOADSTAGE_BUNDLE_REQUESTED = 1,   // LoadBundle("Progression.dat") issued
        E_LOADSTAGE_BUNDLE_LOADED    = 2,   // reply in; about to acquire
        E_LOADSTAGE_ACQUIRE_REQUESTED= 3,   // AcquireResource("ProgressionData") issued
        E_LOADSTAGE_DONE             = 4    // mpProgressionData bound
    };
    ELoadStage meLoadStage = E_LOADSTAGE_NOT_STARTED;                             // X360 +133340

    CgsResource::ResourcePtr<BrnProgression::ProgressionData> mpProgressionData;  // X360 +133348 (0x20 stride)
    CgsResource::ResourcePtr<BrnAI::AISectionsData>           mpAISectionData;    // X360 +133380

    // ---- the player-car / unlock block (X360 +0x208D0 .. +0x20988) ----------------------------
    // X360 +133328 (0x208D0). The CarData record for the car the player is currently in.
    // OnPlayerCarChange writes it (and clears it on the lbUpdateProfile == false path);
    // GetCurrentCarData() hands it back.
    // ⚠️ The five members below carry in-class zero initialisers. The X360 ctor @0x827DEA50 does
    // NOT store to any of them (the console object is BSS-resident, so they start zeroed); on the
    // host this class is a by-value sub-object of GameStateModule inside BrnGameModule and would
    // otherwise start as garbage. Same precedent as GameStateModule::mpOutputBuffer. This is an
    // initialisation-site difference only -- no behavioural divergence.
    CarData*  mpCurrentCarData = 0;
    // X360 +133332 (0x208D4). The chosen-livery record for that car
    // (Profile::GetChosenLiveryDataForBaseCar's answer, cached by OnPlayerCarChange).
    // The element type (BrnProgression::LiveryData) is owned by BrnProfile.h, included above.
    LiveryData* mpCurrentLiveryData = 0;
    // X360 +133448 (0x20948). The loaded vehicle list (see SetVehicleList).
    const BrnResource::VehicleList* mpVehicleList = 0;
    // X360 +133468 (0x2095C). AddCar increments it for every E_UNLOCK_TYPE_SPONSOR car and once
    // more for "CARBEAGT" specifically. FLAG: name inferred from those two increments only.
    s32       miSponsorCarCount = 0;
    // X360 +133484 (0x2096C), read as an UNSIGNED byte by GetProgressionRank (`>= 0x80` == the
    // signed-negative "rank not set yet" case, which answers 0). Distinct from the Profile's own
    // mi8CurrentProgressionRank at Profile+112 -- this is the manager's live cache.
    s8        mi8ProgressionRank = 0;
    // X360 +133489 (0x20971) / +133512 (0x20988) -- the two one-byte request flags
    // CarSelectManager::UpdateExitState sets to 1 on junkyard exit.
    bool      mbUpdateRivalsRequested = false;
    bool      mbDriveThrusDirty = false;
    // ⭐ X360 +133487 (0x2096F). The FORCED-autosave request latch. CheckForSpecialCarUnlocks
    // raises it on the 100%-completion arm (`stbx r22, r31, 0x2096F` @0x82396288) and
    // PreWorldUpdate @0x823A4F68 drains it: `if (*(this + 133487)) { payload = 1;
    // AddEvent(queue, &payload, 55, 1); *(this + 133487) = 0; }`.
    // ⭐ The payload byte is what makes this the FORCED half of the autosave pair -- GuiModule's
    // id-356 arm ORs it into mbForceProfileAutosave, which bypasses the 60 s throttle. The
    // drive-thru arm posts the same action 55 with a payload of ZERO (unforced). Same action,
    // different urgency, and the byte is the whole difference.
    bool      mbAutosaveRequested = false;

    // ---- [stuntrace waveB / agent 10] the deferred "all win types for this mode" check -------
    // X360 +133440 (0x20940). The training manager the progression layer queues its
    // E_TRAINING_TYPE_WON_EVENT tip through. OnEventFinishUpdateProfile @0x823A0040 reads it as
    // `lwzx r31, r30, 0x20940` and then open-codes TrainingManager::RequestTraining's gauntlet.
    // ⚠️ FLAG (PC bring-up, NOT introduced by this wave): nothing in the mounted set calls
    // SetTrainingManager, so this reads NULL today and the tip leg no-ops with a one-shot log.
    // The console installer is the un-reconstructed outer ProgressionManager::Prepare/Construct
    // pair (the same hole mpVehicleList / mpAchievementManager already sit in).
    BrnGameState::TrainingManager* mpTrainingManager = 0;      // X360 +133440 (0x20940)

    // X360 +133493 (0x20975) / +133494 (0x20976). The gate pair ProgressionManager::PreWorldUpdate
    // @0x823A4F68 polls (`if (+133493) if (+133494)`) before running a 2 s timer at +133500 and
    // then calling CheckForAllModeTypeCompletion(queue, meModeToCheckForAllWinTypes); it clears
    // both afterwards. Construct @0x8237A5F8 seeds both 0. OnEventFinishUpdateProfile stores
    // 0 into the first and 1 into the second on the "player is at the last authored rank" arm.
    // ⚠️ FLAG (NAMES PROVISIONAL): the assert string only names the MODE member below, so which
    // of the two bytes is the "pending" one and which the "armed" one is an inference from the
    // writer/reader pair. Do not rename without a third witness.
    bool      mbCheckAllWinTypesPending = false;               // X360 +133493 (0x20975)
    bool      mbCheckAllWinTypesArmed   = false;               // X360 +133494 (0x20976)

    // X360 +133496 (0x20978). PINNED BY THE ASSERT STRING: PreWorldUpdate @0x823A4F68 fires
    // "meModeToCheckForAllWinTypes != RaceEventData::E_MODE_INVALID"
    // (BrnProgressionManager.cpp:382) against this very word before handing it to
    // CheckForAllModeTypeCompletion. Logical type BrnProgression::RaceEventData::EModeType;
    // stored as the s32 the X360 writes (`stwx` of the event record's +0xEC mode BYTE, so the
    // stored value is a zero-extended byte). Construct seeds -1 (E_MODE_INVALID).
    s32       meModeToCheckForAllWinTypes = -1;                // X360 +133496 (0x20978)

    // Pointer-INVARIANT layout facts only (host is the LLP64 gate target). The X360 byte offsets are
    // NOT asserted: they do not survive the 32->64-bit pointer widening of the embedded Profile.
    static void _AssertLayout()
    {
        static_assert(KI_HANDLE_SLOT_COUNT == 18, "X360 ctor resets exactly 18 head handle slots");
        static_assert(sizeof(HandleSlot) == 20,   "X360 head record stride is 0x14 (20) bytes");
        // The landmark cache IS pointer-free, so its console shape does survive to the host and
        // is worth pinning: FindLandmarkAISectionIndex @0x82359AE0 strides it by 8
        // (`addi r11, r11, 8`) and returns the halfword at +4 (`lhz r3, 4(r11)`), and the 512 *
        // 8 == 0x1000 span is what puts mDebugComponent at the +133000 this header records.
        static_assert(sizeof(LandmarkAISectionIndexPair) == 8,
                      "X360 landmark->AI-section record stride is 8 bytes");
        static_assert(KI_LANDMARK_AI_SECTION_INDEX_COUNT == 512,
                      "DWARF BrnProgressionManager.h:826 sizes the table at 512 entries");
    }
};
}
