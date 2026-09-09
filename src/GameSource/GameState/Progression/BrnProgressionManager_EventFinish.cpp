// ===================================================================================
// BrnProgression::ProgressionManager -- THE EVENT-FINISH PROGRESSION WRITERS.
//   GameSource/Unity/../GameState/Progression/BrnProgressionManager.cpp  (per-function
//   partfile of that TU, the house Scoring/BrnScoringSystem_*.cpp precedent; the console
//   homes all three functions in BrnProgressionManager.cpp, which is why every assert
//   below is fired with that file's baked path + the console's own line number rather
//   than through CGS_ASSERT's __FILE__/__LINE__.)
//
// [stuntrace waveB / agent 10] Conductor decision #5: "an event that finishes must update
// progression". These are the three bodies ModeManager::ShowModeResults @0x823436D0 calls:
//
//   OnEventFinishUpdateProfile  @0x823A0040   (the writer -- 380 instructions)
//   HasEventBeenWonPreviously   @0x82366B30   (the "was this a first win?" query)
//   PlayerHasFinishedLastRank   @0x82370180   (the rank-exhausted query)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. THE ASM IS THE SPINE -- the Hex-Rays
// pseudocode for 0x823A0040 mis-renders three things and each one is called out at its
// site: (a) `HIDWORD(v9) = 133484` is register-pair noise for the +0x2096C offset load,
// (b) `*(a1 + 133493) = 1` is wrong (the asm stores 0 there and 1 at +133494), and
// (c) `FindCar(*(rank+104), *(rank+108))` is one 8-byte `ld r4, 0x68(r11)`, not two words.
//
// Every member is reached BY NAME through BrnProgressionManager.h / BrnProfile.h; no raw
// offset arithmetic on `this`.
//
// -----------------------------------------------------------------------------------
// ⛔⛔ HONEST PARTIAL -- READ THIS BEFORE "FINISHING" OnEventFinishUpdateProfile ⛔⛔
//
// The console body calls TEN functions that have no body anywhere in b5-decomp/src. Each
// of them is PARKED at its call site with a banner naming the exact missing symbol and a
// ONE-SHOT log line, because this TU is MOUNTED (tools/build/build_game_exe.bat:2590) and
// landing the calls would add ten unresolved externals to the link -- the F2 failure mode
// this campaign keeps re-learning. The parks are, in body order:
//
//   P1  DerivedCarArray::ConstructPatternLiveryList @0x823751C0  (declare-only, BrnDerivedCars.h
//       :96 -- its own banner says "will stay unresolved at link until someone writes their
//       bodies HERE") + ProgressionManager::UnlockDerivedCarCollection @0x8237AD70 (absent)
//       -> the WON-EVENT CAR UNLOCK. ProgressionManager::AddCar and CarData::
//       SetUnlockDeformationAmount both exist; only the derived-car list build is missing.
//   P2  ProgressionManager::OnTrophyUnlock @0x82389740      (declared, parked in the header)
//   P3  ProgressionManager::CheckForSpecialCarUnlocks @0x82396058 (declared, parked in the header)
//   P4  ProgressionManager::UnlockTrophyForEventTypeAllCompleted @0x82395FE8 (absent).
//       ⭐ NARROWED 2026-09-06 (progression wave, lane profile): Profile::
//       GetGameModeTypeCompletedAmountSinceTheStart @0x82354B98 now has a body, so the
//       console's `GetGameModeTypeAmount(mode) == GetGameModeTypeCompletedAmountSinceTheStart
//       (mode)` condition RUNS; only the trophy call inside it is still parked.
//   P5  ✅ PAID [progression wave: medals, 2026-09-06] -- ProgressionManager::FixGameModeRanks
//       @0x82395CD8 is bodied in BrnProgressionManager_Medals.cpp and CALLED at its seat below.
//   P6  ✅ PAID [progression wave: medals, 2026-09-06] -- ProgressionManager::UpdatePlayerMedals
//       @0x8239FE50 (producer of action 200) is bodied in BrnProgressionManager_Medals.cpp and
//       CALLED at its seat below.
//   P7  ProgressionManager::UnlockRivals                     (absent; fills lpAction->mu64FieldC8)
//   P8  Paid: rank-up free car uses the recovered ProgressionRankData record.
//   P9  AchievementManagerBase::OnEventWin -- BODIED, but BrnGameStateAchievementManagerBase.cpp
//       is deliberately NOT MOUNTED (build_game_exe.bat:2610 "mounting it costs EIGHT ...").
//   P10 ProgressionManager::ComputeCompletionPercentage @0x8238A198 (absent; 320 instructions)
//
// WHAT IS WHOLE (the payoff): the ProfileEvent finished/won flag writes, the previous-win
// classification, the event-junction -> RaceEventData resolution, the deferred "all win
// types" arm, the medal tally, AddGameModeTypeCompleted / AddWinForGameMode, the results
// action's own field writes, the training-tip request, and the two action posts.
// -----------------------------------------------------------------------------------
// ===================================================================================

#include "BrnProgressionManager.h"
#include "BrnProfile.h"
#include "SharedClasses/Progression/BrnProgressionRankData.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"              // CgsDev::Log::gpDebugPrint (the park lines)
#include "SharedClasses/Progression/BrnProgressionData.h"               // ProgressionData (rank count / junction table)
#include "SharedClasses/Progression/BrnRaceEventData.h"                 // EventJunction / RaceEventData
#include "GameSource/GameState/BrnGameActions.h"                        // GameStateModuleIO::ShowModeResultsAction
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"        // VariableEventQueue<13312,16>::AddEvent
#include "GameSource/GameState/TrainingManager/BrnTrainingManager.h"    // TrainingManager::RequestTraining
#include "BrnDerivedCars.h"                                             // DerivedCarArray (the maBlock58 image)

namespace
{
// The verbatim X360-baked source path this TU's asserts reference (identical spelling to
// BrnProgressionManager.cpp's own KAC_PROGMGR_FILE -- the console homes these functions there).
const char* const KAC_PROGMGR_FILE =
    "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../GameState/Progression/BrnProgressionManager.cpp";

// ---------------------------------------------------------------------------------------
// Action ids posted by OnEventFinishUpdateProfile (X360 `li r5` immediates + the byte size
// in `li r6`). Named as file-local constants rather than grown into
// GameStateModuleIO::EGameActionType, which this agent does not own -- the two enumerator
// additions are filed as a header_request. BOTH values are attested at BOTH ends:
//
//   28 (size 4, one f32) == E_ACTION_SET_TRAFFIC_SCALE_BASED_ON_RANK
//        producer 0x823A0608 `li r5,0x1C` / `li r6,4`, payload flt_82001C98
//        consumer TrafficEntityModule::HandleExternalRequests @0x8274B660 `case 28`, whose
//                 assert string is literally "lpSetTrafficScaleBasedOnRankAction != NULL"
//                 (BrnTrafficEntityModule.cpp:5866) and which stores the f32 at +464912.
//        DWARF BrnGameActions.h:24 E_ACTION_SET_TRAFFIC_SCALE_BASED_ON_RANK == 24 (+4 on X360 --
//        the SAME +4 this enum already records for E_ACTION_PREPARE_FOR_MODE 19 -> 23).
//
//   55 (size 1, one bool) == E_ACTION_REQUEST_AUTOSAVE
//        producer 0x823A05DC `li r5,0x37` / `li r6,1`, payload the literal 1
//        consumer BrnGameModule::TranslateGameActionsToGuiEvents @0x823E9CE0 `case 55` ->
//                 AddGuiEvent<BrnGui::GuiAutosaveRequestEvent>
//        DWARF BrnGameActions.h:50 E_ACTION_REQUEST_AUTOSAVE == 50 (+5 on X360 -- the same +5
//        shift this enum already records for the mode-lifecycle block).
// ---------------------------------------------------------------------------------------
// [x] RETIRED 2026-08-26 (stuntrace waveB CLOSURE round): both ids now live in
// GameStateModuleIO::EGameActionType as E_ACTION_REQUEST_AUTOSAVE (55) and
// E_ACTION_SET_TRAFFIC_SCALE_BASED_ON_RANK (28), and are used BY NAME below. The 55 mapping
// gained three more producers in the closure sweep (DriveThruManager::ProcessDriveThru x2,
// DriveThruManager::UnlockCarChallengeForCar, StreetManager::ProcessNewRoadScore -- all id 55
// size 1), and 28 gained GameStateModule::OnProfileLoaded posting the same id/size.

// IMAGE-CITED CONSTANTS (big-endian dump of image.bin, offset == VA - 0x82000000):
//   flt_82029BB8 = 0x3F59999A == 0.85f  -- the unlock deformation amount stamped onto every
//                                          car this function adds (`stfs f0, 0xC(carData)`).
//   flt_82001C98 = 0x3F800000 == 1.0f   -- the action-28 traffic-scale payload.
const f32 KF_UNLOCK_DEFORM_AMOUNT   = 0.85f;   // flt_82029BB8
const f32 KF_TRAFFIC_SCALE_AT_RANK  = 1.0f;    // flt_82001C98

// One-shot park reporters. Each parked leg announces itself ONCE per run so a boot trace
// says exactly which console leg did not execute (campaign house rule: a park must be
// visible, not silent).
void ParkOnce(bool& lrbAlreadySaid, const char* lpcMessage)
{
    if (lrbAlreadySaid)
    {
        return;
    }
    lrbAlreadySaid = true;
    if (CgsDev::Log::gpDebugPrint != 0)
    {
        *CgsDev::Log::gpDebugPrint << lpcMessage;
    }
}

bool gbSaidCarUnlock       = false;   // P1
bool gbSaidTrophyUnlock    = false;   // P2 + P3
bool gbSaidAllCompleted    = false;   // P4
// ⛔ RETIRED 2026-09-06 (progression wave, lane medals): `gbSaidFixRanks` (P5) and
// `gbSaidUpdateMedals` (P6). Both parks are paid -- FixGameModeRanks @0x82395CD8 and
// UpdatePlayerMedals @0x8239FE50 are bodied in BrnProgressionManager_Medals.cpp and called at
// their seats. Do not re-mint them.
bool gbSaidUnlockRivals    = false;   // P7
bool gbSaidAchievement     = false;   // P9
bool gbSaidCompletionPct   = false;   // P10
bool gbSaidNoTrainingMgr   = false;   // the uninstalled mpTrainingManager back-pointer
}

namespace BrnProgression
{

namespace GsmIO = BrnGameState::GameStateModuleIO;

// ------------------------------------------------------------------------------------
// ProgressionManager::HasEventBeenWonPreviously  @ 0x82366B30
//
// Linear scan of the embedded Profile's discovered-event records for luEventId, then a
// single bit test. The X360 is entirely open-coded (the Profile sits at this+0x170, so its
// miEventCount reads as `lwz r9, 0x278(r8)` and its maEvents base as `addi r10, r8, 0x7080`
// with an 8-byte stride); the bit test is `lhz r11, 4(r11) / srwi 2 / clrlwi 31`, i.e.
// bit 2 of the u16 flag word == ProfileEvent::E_FLAG_RANK_WIN (4). Not "any win" -- a
// NON_RANK or SPECIAL_EVENT win answers false, which is what makes ModeManager::
// ShowModeResults' negated copy of this mean "first RANK win for this event".
//
// Sole console callers: ModeManager::ShowModeResults @0x823436D0.
// ------------------------------------------------------------------------------------
bool ProgressionManager::HasEventBeenWonPreviously(u32 luEventId)
{
    const s32 liEventCount = static_cast<s32>(mProfile.GetEventCount());
    if (liEventCount <= 0)
    {
        return false;                                  // X360 `ble cr6, loc_82366B80 / li r3,0`
    }

    for (s32 liIndex = 0; liIndex < liEventCount; ++liIndex)
    {
        const ProfileEvent* lpcEvent = mProfile.GetEvent(static_cast<u32>(liIndex));
        if (lpcEvent->GetID() == luEventId)
        {
            return lpcEvent->IsFlagSet(ProfileEvent::E_FLAG_RANK_WIN);
        }
    }

    return false;                                      // fell off the end -- `li r3,0 / blr`
}

// ------------------------------------------------------------------------------------
// ProgressionManager::PlayerHasFinishedLastRank  @ 0x82370180
//
//   `return ProgressionData->muProgressionRankCount == (s8)mi8ProgressionRank;`
//
// The whole body is four instructions plus the resource-pointer hop: `lwz r10, 0x14(r3)`
// (ProgressionData +0x14 == muProgressionRankCount), `lbzx r11, r31, 0x2096C` + `extsb`
// (the manager's own cached rank byte at +133484, SIGN-extended), `subf` + `cntlzw` +
// `extrwi r3, r11, 1, 26` == "the difference is zero".
//
// ⚠️ NOTE, deliberately faithful: this reads the RAW byte, NOT the clamped
// GetProgressionRank() (which answers 0 for the negative "rank not set" seed and clamps to
// count-1 at the top). So "finished the last rank" means the cached rank has run one PAST
// the authored table -- count, not count-1. The count-1 comparison OnEventFinishUpdateProfile
// makes is a different test on the same pair, and both are reproduced as written.
//
// Console callers: ModeManager::ShowModeResults, GameStateModule::StartModeAtLights
// @0x82396CF8, GameStateModule::ProcessGameEvents @0x823A0A18, and OnEventFinishUpdateProfile.
// ------------------------------------------------------------------------------------
bool ProgressionManager::PlayerHasFinishedLastRank() const
{
    const s32 liRankCount = static_cast<s32>(mpProgressionData->GetProgressionRankCount());
    return liRankCount == static_cast<s32>(mi8ProgressionRank);
}

// ------------------------------------------------------------------------------------
// ProgressionManager::SetTrainingManager
// Installer for the X360 +133440 back-pointer (see the member's banner). No console symbol:
// the console's outer Construct/Prepare pair stores it directly.
// ------------------------------------------------------------------------------------
void ProgressionManager::SetTrainingManager(BrnGameState::TrainingManager* lpTrainingManager)
{
    mpTrainingManager = lpTrainingManager;
}

// ------------------------------------------------------------------------------------
// ProgressionManager::OnEventFinishUpdateProfile  @ 0x823A0040
//
// ARG SHAPE FROM ASM: r3=this, r4=lpGameActionQueue, r5=luEventId, r6=lpAction,
// r7=leGameModeType -- exactly the DWARF's four-parameter declaration (:330).
//
// CONSOLE FLOW (labels are the asm's):
//   1. `lwz r11, 4(r19); cmpwi 1; blt` -- nothing to do unless the player actually finished
//      (lpAction->miFinishPosition >= 1). NOTE +0x04 is the FINISH POSITION, not the mode;
//      see the offset-correction banner on ShowModeResultsAction in BrnGameActions.h.
//   2. find the profile's ProfileEvent for luEventId; assert it, assert it is DISCOVERED.
//   3. set E_FLAG_FINISHED, and remember which win flag (if any) the event ALREADY carried.
//   4. clear the results action's block-58 gate and the derived-car list length inside it.
//   5. if the finish position is exactly 1 (a win): resolve the event's RaceEventData through
//      the ProgressionData event-JUNCTION table, then either
//         (a) the event has no unlock car -> rewrite the win flags to RANK_WIN|WON_EVENT_BEFORE, or
//         (b) it has one and was already special-won -> just re-set WON_SPECIAL_EVENT_BEFORE, or
//         (c) it has one and is a first special win -> unlock the car family, publish it into
//             the results action, then set WON_SPECIAL_EVENT_BEFORE.
//      (a) and (c) additionally arm the deferred "all win types for this mode" check when the
//      player sits on the LAST authored rank.
//      Then: trophy check, per-mode completion tally, medal tally + win tally, medal GUI
//      refresh, the WON_EVENT training tip, rival unlocks, the next-rank car, the achievement
//      hook and the autosave request.
//   6. tail (reached on BOTH paths): post the traffic-scale action once the player has a rank,
//      and stamp the completion percentage into the results action.
// ------------------------------------------------------------------------------------
void ProgressionManager::OnEventFinishUpdateProfile(GsmIO::GameActionQueue* lpGameActionQueue,
                                                    u32 luEventId,
                                                    GsmIO::ShowModeResultsAction* lpAction,
                                                    GsmIO::EGameModeType leGameModeType)
{
    // ---- 1. the finish-position gate ---------------------------------------------------
    if (lpAction->miFinishPosition < 1)
    {
        return;
    }

    // ---- 2. the profile's record for this event ----------------------------------------
    // X360: the scan is open-coded over Profile+0x7080 with an 8-byte stride and Profile+0x278
    // as the bound. Routed through the named Profile accessors here; the const_cast is the
    // precedent Profile::GetPlayerBaseDeformAmount already sets in this class (the console's
    // inlined scan walks the mutable array, and only the const getter is bodied).
    ProfileEvent* lpEvent = 0;
    {
        const s32 liEventCount = static_cast<s32>(mProfile.GetEventCount());
        for (s32 liIndex = 0; liIndex < liEventCount; ++liIndex)
        {
            const ProfileEvent* lpcCandidate = mProfile.GetEvent(static_cast<u32>(liIndex));
            if (lpcCandidate->GetID() == luEventId)
            {
                lpEvent = const_cast<ProfileEvent*>(lpcCandidate);
                break;
            }
        }
    }

    if (lpEvent == 0)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("lpEvent", KAC_PROGMGR_FILE, 1669);
        CgsDev::Assert::EndAssert();
        return;   // the X360 falls through into a null deref; bail instead of faulting
                  // (the OnPlayerCarChange / GetCarColourAndPalette precedent in this TU)
    }

    if (!lpEvent->IsFlagSet(ProfileEvent::E_FLAG_DISCOVERED))
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("lpEvent->IsFlagSet( ProfileEvent::E_FLAG_DISCOVERED )",
                                   KAC_PROGMGR_FILE, 1672);
        CgsDev::Assert::EndAssert();
    }

    // ---- 3. mark it finished, and classify the win it ALREADY had ----------------------
    // X360 0x8235010C: `lhz / ori 2 / sth`, then three `rlwinm` bit tests in this order.
    lpEvent->SetFlags(static_cast<u16>(lpEvent->GetFlags() | ProfileEvent::E_FLAG_FINISHED));

    s32 lePreviousWinFlag = 0;
    {
        const u16 lu16Flags = lpEvent->GetFlags();
        if ((lu16Flags & ProfileEvent::E_FLAG_RANK_WIN) != 0)
        {
            lePreviousWinFlag = ProfileEvent::E_FLAG_RANK_WIN;
        }
        else if ((lu16Flags & ProfileEvent::E_FLAG_NON_RANK_WIN) != 0)
        {
            lePreviousWinFlag = ProfileEvent::E_FLAG_NON_RANK_WIN;
        }
        else if ((lu16Flags & ProfileEvent::E_FLAG_WON_SPECIAL_EVENT_BEFORE) != 0)
        {
            lePreviousWinFlag = ProfileEvent::E_FLAG_WON_SPECIAL_EVENT_BEFORE;
        }
    }

    // ---- 4. reset the results action's derived-car block --------------------------------
    // X360 `stb r23, 0xDF(r19)` then `stw r23, 0x98(r19)`.
    // +0x98 is NOT a free field: it is +0x58 + 0x40, i.e. the COUNT WORD of the
    // Array<CgsID,8> base sub-object of the DerivedCarArray image that lives in maBlock58
    // (BrnDerivedCars.h pins the layout: CgsID elements @+0x00, count @+0x40, livery types
    // @+0x48, their count @+0x68 -- 0x70 bytes, exactly this block's size). So the console is
    // emptying the list it is about to (maybe) fill.
    lpAction->mbHasBlock58 = 0;
    reinterpret_cast<DerivedCarArray*>(lpAction->maBlock58)->Clear();

    // ---- 5. the WIN path ----------------------------------------------------------------
    if (lpAction->miFinishPosition == 1)
    {
        // Resolve the event's RaceEventData through the event-JUNCTION table (X360 reads
        // ProgressionData +0x1C count / +0x18 base with a 16-byte stride and takes the
        // junction's OFFLINE event slot at +0x04).
        const RaceEventData* lpcRaceEventData = 0;
        {
            const u32 luProfileEventId = lpEvent->GetID();
            const ProgressionData* lpcProgressionData = mpProgressionData.operator->();
            const u32 luJunctionCount = lpcProgressionData->GetEventJunctionCount();
            for (u32 luIndex = 0; luIndex < luJunctionCount; ++luIndex)
            {
                const EventJunction* lpcJunction = lpcProgressionData->GetEventJunction(luIndex);
                if (lpcJunction->GetID() == luProfileEventId)
                {
                    lpcRaceEventData = lpcJunction->GetOfflineEvent();
                    break;
                }
            }
        }

        if (lpcRaceEventData == 0)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("lpRaceEventData != NULL", KAC_PROGMGR_FILE, 1705);
            CgsDev::Assert::EndAssert();
            return;   // the X360 falls through into a null deref; bail instead of faulting
        }

        // The event's unlock car id: the 8-byte doubleword at RaceEventData +0x10 that the
        // X360 loads with a single `ld r11, 0x10(r29)` and tests against 0.
        // ⚠️ NAME NOTE: the semantically-right accessor is RaceEventData::GetUnlockCarId()
        // (BrnRaceEventData.h:258, "the car id this event unlocks (X360 word +0x14)") but it
        // is DECLARE-ONLY -- no body anywhere. GetEventInstanceId() is bodied
        // (BrnRaceEventData.cpp:89) and returns the SAME eight bytes at +0x10, so it is used
        // here rather than adding an unresolved external. header_request filed to body
        // GetUnlockCarId as a straight delegation (the Profile::IsStuntElementDone precedent).
        const CgsID lUnlockCarId = static_cast<CgsID>(lpcRaceEventData->GetEventInstanceId());

        bool lbAlreadyWonSpecialEventBefore = false;

        if (lUnlockCarId == 0)
        {
            // ---- 5(a) no unlock car: rewrite the win flags -------------------------------
            // X360 `andi. r11, r11, 0xFFD3 / ori r11, r11, 0x24 / sth`:
            // clear {RANK_WIN, NON_RANK_WIN, WON_EVENT_BEFORE} then set
            // {RANK_WIN, WON_EVENT_BEFORE} -- i.e. promote any non-rank win to a rank win.
            const u16 lu16Mask = static_cast<u16>(~static_cast<u16>(ProfileEvent::E_FLAG_RANK_WIN |
                                                                    ProfileEvent::E_FLAG_NON_RANK_WIN |
                                                                    ProfileEvent::E_FLAG_WON_EVENT_BEFORE));
            lpEvent->SetFlags(static_cast<u16>((lpEvent->GetFlags() & lu16Mask) |
                                               static_cast<u16>(ProfileEvent::E_FLAG_RANK_WIN |
                                                                ProfileEvent::E_FLAG_WON_EVENT_BEFORE)));

            ArmAllWinTypesCheckIfAtLastRank(lpcRaceEventData);
        }
        else if (lpEvent->IsFlagSet(ProfileEvent::E_FLAG_WON_SPECIAL_EVENT_BEFORE))
        {
            // ---- 5(b) already special-won: nothing to unlock ----------------------------
            lbAlreadyWonSpecialEventBefore = true;
            lpEvent->SetFlags(static_cast<u16>(lpEvent->GetFlags() |
                                               ProfileEvent::E_FLAG_WON_SPECIAL_EVENT_BEFORE));
        }
        else
        {
            // ---- 5(c) FIRST special win: unlock the event's car family ------------------
            // X360 `ori r11, r11, 0x24 / sth` == set {RANK_WIN, WON_EVENT_BEFORE}.
            lpEvent->SetFlags(static_cast<u16>(lpEvent->GetFlags() |
                                               static_cast<u16>(ProfileEvent::E_FLAG_RANK_WIN |
                                                                ProfileEvent::E_FLAG_WON_EVENT_BEFORE)));

            // ⭐ UN-PARKED P1 [progression wave 2026-09-06, lane completion] -- THE WON-EVENT
            // CAR UNLOCK. Both halves of the old park have bodies now: DerivedCarArray::
            // ConstructPatternLiveryList @0x823751C0 landed with the map arm (2026-08-27) and
            // ProgressionManager::UnlockDerivedCarCollection @0x8237AD70 is bodied in
            // BrnProgressionManager_Completion.cpp. The console, asm 0x823A0234..0x823A02C0:
            //     DerivedCarArray lCarVariants;                       // stw -1 into BOTH counts
            //     lCarVariants.ConstructPatternLiveryList(mpVehicleList, lUnlockCarId);
            //     memcpy(&lpAction->maBlock58, &lCarVariants, 0x70);
            //     lpAction->mbHasBlock58 = true;                      // stb r27(1), 0xDF(r19)
            //     CarData* lpCarData = AddCar(lCarVariants.GetItem(1), 1);   // E_UNLOCK_TYPE_GIFT
            //     CGS_ASSERT(lpCarData != NULL, ...);                 // :1723
            //     lpCarData->SetUnlockDeformationAmount(0.85f);       // flt_82029BB8
            //     UnlockDerivedCarCollection(lCarVariants);
            //
            // ⚠️ GetItem(1) IS UNGUARDED ON THE CONSOLE and stays so here: index 1 is the first
            // SIBLING of the parent, and a family with no siblings makes the checked operator[]
            // fire "Array index out of bounds" -- an assert, not a guard, so the console reads
            // the slot anyway. Adding a length test would be inventing a decision the binary
            // does not make; the authored data is what guarantees a special event's car has a
            // pattern-livery sibling.
            // ⓘ The 0x70-byte memcpy is written as the whole-object assignment it is: the type
            // is pointer-free (CgsID elements + a count word + the livery kinds + their count),
            // so the copy is byte-for-byte the console's and needs no <cstring>.
            {
                DerivedCarArray lCarVariants;

                if (mpVehicleList != 0)
                {
                    lCarVariants.ConstructPatternLiveryList(mpVehicleList, lUnlockCarId);
                }
                else
                {
                    // [FLAG PC bring-up] the console dereferences mpVehicleList (+133448) here
                    // with no test; on PC it is installed by GameStateModule (SetVehicleList) and
                    // is non-null on every mounted path, but a null one must not fault.
                    // Clear() leaves an empty-but-constructed list, which is what the block the
                    // action already carries holds. DELETE-WHEN the outer Construct/Prepare pair
                    // owns the install unconditionally.
                    lCarVariants.Clear();
                    ParkOnce(gbSaidCarUnlock,
                             "[FLAG PC bring-up] ProgressionManager::OnEventFinishUpdateProfile: "
                             "mpVehicleList (X360 +133448) is NULL, so the won-event car's livery "
                             "family could not be built and no car was awarded.\n");
                }

                *reinterpret_cast<DerivedCarArray*>(lpAction->maBlock58) = lCarVariants;
                lpAction->mbHasBlock58 = 1;

                if (mpVehicleList != 0)
                {
                    CarData* lpCarData = AddCar(lCarVariants.GetItem(1),
                                                CarData::E_UNLOCK_TYPE_GIFT);
                    if (lpCarData == 0)
                    {
                        CgsDev::Assert::BeginAssert();
                        CgsDev::Assert::FireAssert("lpCarData != NULL", KAC_PROGMGR_FILE, 1723);
                        CgsDev::Assert::EndAssert();
                    }
                    else
                    {
                        // [PC GUARD] the console stores THROUGH r31 at 0x823A02BC whether or not
                        // the assert above fired (`stfs f0, 0xC(r31)` sits after loc_823A02B0),
                        // i.e. a null lpCarData writes 0.85f to address 0x0C. AddCar cannot
                        // return null in this tree -- Profile::AddCar asserts and returns a real
                        // record -- so the `else` costs no behaviour and removes the only way
                        // this leg could turn a console non-event into a host access violation.
                        lpCarData->SetUnlockDeformationAmount(KF_UNLOCK_DEFORM_AMOUNT);
                    }

                    UnlockDerivedCarCollection(lCarVariants);
                }
            }

            ArmAllWinTypesCheckIfAtLastRank(lpcRaceEventData);

            // X360 loc_823A0310 / loc_823A032C: BOTH sub-paths of 5(c) end by setting this.
            lpEvent->SetFlags(static_cast<u16>(lpEvent->GetFlags() |
                                               ProfileEvent::E_FLAG_WON_SPECIAL_EVENT_BEFORE));
        }

        // ---- the trophy gate ------------------------------------------------------------
        // X360: `r31 = *(this + 0x3E8)` == Profile+0x278 == miEventCount -- the console calls
        // that value "luMedalCount" in its own assert text.
        const u32 luMedalCount = mProfile.GetEventCount();
        u32 luRankWins = 0;
        u32 luNonRankWins = 0;
        u32 luSpecialEventWins = 0;
        const u32 luTotalWinCount =
            mProfile.GetTotalWinCount(luRankWins, luNonRankWins, luSpecialEventWins);

        if (luRankWins > luMedalCount)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("luRankWin <= luMedalCount", KAC_PROGMGR_FILE, 1771);
            CgsDev::Assert::EndAssert();
        }

        if (luTotalWinCount >= luMedalCount)
        {
            // ⛔ PARK P2 -- console: `OnTrophyUnlock(9);` (X360 `li r4,9`, i.e. the
            // TrophyUnlockData::UnlockType the "every event rank-won" trophy carries).
            // ProgressionManager::OnTrophyUnlock @0x82389740 is declared in
            // BrnProgressionManager.h and PARKED there (a 12-case trophy machine over the
            // PROGRESSION.DAT trophy table; it needs UnlockCarFromTrophy @0x8237B0E8 and an
            // owning header for the trophy table). DELETE-WHEN it has a body.
            ParkOnce(gbSaidTrophyUnlock,
                     "[FLAG PC bring-up] ProgressionManager::OnEventFinishUpdateProfile: "
                     "OnTrophyUnlock(9) and CheckForSpecialCarUnlocks() are NOT reconstructed "
                     "(both parked in BrnProgressionManager.h). The all-events-won trophy and "
                     "the gold/silver car re-check did not run.\n");
        }
        // ⛔ PARK P3 -- console: `CheckForSpecialCarUnlocks();` UNCONDITIONALLY here.
        // @0x82396058, declared + parked in BrnProgressionManager.h (needs
        // ComputeCompletionPercentage @0x8238A198 and UnlockSpecialCars @0x8237AF38).
        // (Shares the one-shot line above so a boot trace is not spammed twice.)
        ParkOnce(gbSaidTrophyUnlock,
                 "[FLAG PC bring-up] ProgressionManager::OnEventFinishUpdateProfile: "
                 "OnTrophyUnlock(9) and CheckForSpecialCarUnlocks() are NOT reconstructed "
                 "(both parked in BrnProgressionManager.h). The all-events-won trophy and "
                 "the gold/silver car re-check did not run.\n");

        // ---- the per-mode completion tally ----------------------------------------------
        mProfile.AddGameModeTypeCompleted(leGameModeType);

        // ---- the "every event of this mode is completed" check ---------------------------
        // Console (asm 0x823A0414..0x823A0438):
        //     if (GetGameModeTypeAmount(mode) == GetGameModeTypeCompletedAmountSinceTheStart(mode))
        //         UnlockTrophyForEventTypeAllCompleted(mode);
        // ⭐ [progression wave 2026-09-06, lane profile] the CONDITION is no longer parked:
        // Profile::GetGameModeTypeCompletedAmountSinceTheStart @0x82354B98 now has a body
        // (BrnProfile.cpp), so both sides of the comparison are the console's own reads and the
        // park below only fires on the frame the console would have unlocked the trophy.
        if (mProfile.GetGameModeTypeAmount(leGameModeType)
            == mProfile.GetGameModeTypeCompletedAmountSinceTheStart(leGameModeType))
        {
            // ⭐ UN-PARKED P4 [progression wave 2026-09-06, lane completion]: console
            // `UnlockTrophyForEventTypeAllCompleted(leGameModeType);` @0x82395FE8 (DWARF
            // BrnProgressionManager.cpp:351), bodied in BrnProgressionManager_Completion.cpp --
            // a 9-case map from the game mode onto the TrophyUnlockData::UnlockType that rewards
            // completing every event of it (RACE->11, ROAD_RAGE->12, BURNING_ROUTE->13,
            // STUNT_ATTACK->16, MARKED_MAN->15; the other four modes award nothing).
            UnlockTrophyForEventTypeAllCompleted(leGameModeType);
        }

        // ---- the medal + win tallies (THE payoff writes) --------------------------------
        // X360 `cmplwi r21, 4 / beq` -- skipped entirely when the event had ALREADY been
        // rank-won, so a re-win never double-counts.
        if (lePreviousWinFlag != ProfileEvent::E_FLAG_RANK_WIN)
        {
            if (!lbAlreadyWonSpecialEventBefore)
            {
                // X360 `addis r11, r20, 1 / addi r11, r11, -0x59F0` == Profile + 0xA610
                // == Profile+42512 == muMedalCountFromTheStart; `lwz / addi 1 / stw`.
                mProfile.SetMedalCountFromTheStart(mProfile.GetMedalCountFromTheStart() + 1);
            }

            mProfile.AddWinForGameMode(leGameModeType);

            // ✅ [progression wave: medals, 2026-09-06] PARK P5 IS PAID. FixGameModeRanks
            // @0x82395CD8 (DWARF :579) is bodied in BrnProgressionManager_Medals.cpp, so the win
            // this arm just tallied is followed by the console's own per-mode rank fix-up.
            FixGameModeRanks();
        }

        // ✅ [progression wave: medals, 2026-09-06] PARK P6 IS PAID. UpdatePlayerMedals
        // @0x8239FE50 -- the producer of game action 200 (E_ACTION_UPDATE_PLAYER_MEDALS ->
        // GuiEventMedalUpdate 307, the record BrnGameActions.h models as
        // UpdatePlayerMedalsAction) and the only console caller of UnlockToProgressionRank -- is
        // bodied in BrnProgressionManager_Medals.cpp. Winning an event now re-counts the medals,
        // ranks the licence up when the total crosses the next threshold, and refreshes the
        // medal HUD, exactly here.
        UpdatePlayerMedals(lpGameActionQueue);

        // ---- the WON_EVENT training tip -------------------------------------------------
        // X360 open-codes TrainingManager::RequestTraining(E_TRAINING_TYPE_WON_EVENT) here --
        // the state==INACTIVE / !mbInPictureParadise / IsTipAllowedInGameMode /
        // !HasPlayerSeenTrainingType gauntlet with the "lpProfile" assert
        // (BrnTrainingManager.cpp:382) in the middle -- and the tail store pair
        // `*(tm+0)=1; *(tm+4)=0x11`. That IS RequestTraining specialised for type 17 (the
        // type-8 free-burn-clock arm and the type-50 boost gauntlet both fold away), so the
        // inlining is reversed into the real call, which is bodied at
        // BrnTrainingManager.cpp:507.
        if (mpTrainingManager != 0)
        {
            mpTrainingManager->RequestTraining(E_TRAINING_TYPE_WON_EVENT);
        }
        else
        {
            ParkOnce(gbSaidNoTrainingMgr,
                     "[FLAG PC bring-up] ProgressionManager::OnEventFinishUpdateProfile: "
                     "mpTrainingManager (X360 +133440) is NULL -- nothing calls "
                     "ProgressionManager::SetTrainingManager yet, so the WON_EVENT training "
                     "tip was not requested.\n");
        }

        // ⭐ UN-PARKED P7 [progression wave 2026-09-06, lane rivals]: console
        // `lpAction->mu64FieldC8 = UnlockRivals(lpGameActionQueue);` (DWARF :661,
        // `CgsID UnlockRivals(GameActionQueue*)`), bodied in BrnProgressionManager_Rivals.cpp
        // @0x8236F658. It answers 0 -- the "no rival unlocked" value the memset already left --
        // until the profile's medal count reaches the next rival's GetNumMedalsToUnlock(), and
        // the CgsID it answers is the unlocked rival's CAR id (`ld r3, 8(rival)`), even though
        // the DWARF names this field mNewlyUnlockedRivalID. See the body banner.
        lpAction->mu64FieldC8 = static_cast<u64>(UnlockRivals(lpGameActionQueue));

        // X360 `std r23, 0x40(r19)` / `stb r23, 0xDE(r19)` -- clear the next-rank car slot
        // and its gate before the next-rank leg below can set them.
        lpAction->mu64Field40 = 0;
        lpAction->mbHasField40 = 0;

        // ---- the next-rank car ----------------------------------------------------------
        // X360 reads the cached rank byte UNSIGNED here (`lbz / cmplwi 0`), so the -2
        // "not started" seed counts as non-zero.
        if (static_cast<u8>(mi8ProgressionRank) != 0 && !PlayerHasFinishedLastRank())
        {
            // ARTIST 0x823A0544..05A4: one 64-bit CgsID, not the decompiler's
            // spurious pair of arguments. The authored rank record owns this award.
            const ProgressionRankData* lpRank =
                GetProgressionData()->GetProgressionRankData(static_cast<s8>(mi8ProgressionRank));
            const CgsID lCarId = lpRank->GetFreeCarForRankUpID();
            if (!mProfile.FindCar(lCarId))
            {
                lpAction->mu64Field40 = lCarId;
                lpAction->mbHasField40 = true;
                AddCar(lCarId, 1)->SetUnlockDeformationAmount(KF_UNLOCK_DEFORM_AMOUNT);
            }
        }

        // ---- the achievement hook -------------------------------------------------------
        if (mpAchievementManager == 0)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("mpAchievementManager", KAC_PROGMGR_FILE, 1831);
            CgsDev::Assert::EndAssert();
        }
        // ⛔ PARK P9 -- console: `AchievementManagerBase::OnEventWin(mpAchievementManager,
        // leGameModeType);` The body EXISTS (BrnGameStateAchievementManagerBase.cpp:211) but its
        // TU is deliberately NOT MOUNTED (tools/build/build_game_exe.bat:2610 -- "mounting it
        // costs EIGHT ..."), so calling it from this mounted TU would break the link.
        // DELETE-WHEN that TU is mounted; this is a MOUNT decision, not a missing body.
        ParkOnce(gbSaidAchievement,
                 "[FLAG PC bring-up] ProgressionManager::OnEventFinishUpdateProfile: "
                 "AchievementManagerBase::OnEventWin() not called -- its TU "
                 "(BrnGameStateAchievementManagerBase.cpp) is not mounted.\n");

        // ---- the autosave request -------------------------------------------------------
        // X360 0x823A05D8: `li r6,1 / li r5,0x37`, payload one byte == 1.
        {
            u8 lu8Autosave = 1;
            lpGameActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lu8Autosave),
                                        GsmIO::E_ACTION_REQUEST_AUTOSAVE, 1);
        }
    }

    // ---- 6. the tail, reached from BOTH paths (X360 loc_823A05F0) ----------------------
    // `lbzx / extsb / cmpwi 1 / blt` -- SIGN-extended here (unlike the unsigned read above),
    // so the -2 "not started" seed does NOT post.
    if (static_cast<s32>(mi8ProgressionRank) >= 1)
    {
        f32 lfTrafficScale = KF_TRAFFIC_SCALE_AT_RANK;   // flt_82001C98 == 1.0f
        lpGameActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lfTrafficScale),
                                    GsmIO::E_ACTION_SET_TRAFFIC_SCALE_BASED_ON_RANK, 4);
    }

    // ⛔ PARK P10 -- console: `lpAction->+0xD0 = ComputeCompletionPercentage();`
    // (`stfs f1, 0xD0(r19)` -- so +0xD0 is an f32, currently inside ShowModeResultsAction's
    // maPadD0). ProgressionManager::ComputeCompletionPercentage @0x8238A198 (320 instructions)
    // is absent from the tree and is ALREADY the stated blocker for two other parked members of
    // this class (SendGameCompletionResults, CheckForSpecialCarUnlocks). The field keeps
    // ShowModeResults' memset zero, i.e. "0% complete" in the post-event GUI.
    // DELETE-WHEN 0x8238A198 lands (and carve `f32 mfCompletionPercentage; // +0xD0` out of
    // maPadD0 in the same pass -- header_request filed).
    ParkOnce(gbSaidCompletionPct,
             "[FLAG PC bring-up] ProgressionManager::OnEventFinishUpdateProfile: "
             "ComputeCompletionPercentage() is NOT reconstructed (@0x8238A198). The results "
             "action's completion percentage stays 0.\n");
}

// ------------------------------------------------------------------------------------
// The "player is sitting on the LAST authored rank" arm, which the console emits TWICE
// (loc_823A02C4 for the special-win path and loc_823A034C for the no-unlock-car path) with
// byte-identical code. Both copies do:
//     if ((s8)(mpProgressionData->muProgressionRankCount - 1) == (s8)mi8ProgressionRank)
//     {
//         mbCheckAllWinTypesPending    = false;   // stbx r23(0), this, 0x20975
//         mbCheckAllWinTypesArmed      = true;    // stbx r27(1), this, 0x20976
//         meModeToCheckForAllWinTypes  = lpcRaceEventData->GetMode();   // lbz 0xEC / stwx
//     }
// ⚠️ FAITHFUL DETAIL: the X360 `extsb`s BOTH sides, so the comparison is between the LOW
// BYTES of (rankCount - 1) and the rank -- reproduced with the explicit s8 casts rather
// than widened to s32, because a rankCount of 0 makes the two differ (-1 vs 0xFF-as-s8).
// The reader is ProgressionManager::PreWorldUpdate @0x823A4F68, whose assert string
// ("meModeToCheckForAllWinTypes != RaceEventData::E_MODE_INVALID",
// BrnProgressionManager.cpp:382) is what PINS the third member's name.
// ------------------------------------------------------------------------------------
void ProgressionManager::ArmAllWinTypesCheckIfAtLastRank(const RaceEventData* lpcRaceEventData)
{
    const s32 liRankCount = static_cast<s32>(mpProgressionData->GetProgressionRankCount());
    if (static_cast<s8>(liRankCount - 1) != static_cast<s8>(mi8ProgressionRank))
    {
        return;
    }

    mbCheckAllWinTypesPending   = false;
    mbCheckAllWinTypesArmed     = true;
    meModeToCheckForAllWinTypes = static_cast<s32>(lpcRaceEventData->GetMode());
}

} // namespace BrnProgression
