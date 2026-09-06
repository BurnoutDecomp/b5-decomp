// ===================================================================================
// BrnProgression::ProgressionManager -- THE GAME-COMPLETION PERCENTAGE CHAIN.
//   GameSource/Unity/../GameState/Progression/BrnProgressionManager.cpp  (per-function
//   partfile of that TU, the house BrnProgressionManager_EventFinish.cpp precedent.)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   ProgressionManager::GetTrueNumberOfRivals          @0x8236FB10  (44 instructions)
//   ProgressionManager::GetNumberOfBeatenRivals        @0x8236FBC8  (113 instructions)
//   ProgressionManager::GetPercentageOfEventsCompleted @0x8237B390  (85 instructions)
//   ProgressionManager::ComputeCompletionPercentage    @0x8238A198  (321 instructions)
//   ProgressionManager::CheckForSpecialCarUnlocks      @0x82396058  (144 instructions)
//   ProgressionManager::SendGameCompletionResults      @0x82395C28  (44 instructions)
//
// ⚠️⚠️ HEX-RAYS IS UNUSABLE FOR THE BIG ONE. Its output for 0x8238A198 opens with
// "local variable allocation has failed, the output may be wrong!", loses the entire
// clamp-and-easter-egg tail (it renders the final CgsIDCompress("CARBEAGT") lookup as a
// dead expression whose result is discarded), and mangles the two `fsel`s. Everything
// below is read off the ASSEMBLY; the pseudocode was used only to cross-check the shape
// of the six weighted terms.
//
// ⭐ THE WEIGHTS CLOSE ON 100, WHICH IS HOW WE KNOW THEY WERE READ CORRECTLY. Six .rdata
// floats feed the sum here -- 9.0 + 9.0 + 11.0 + 11.0 + 2.5 + 2.5 == 45 -- and
// GetPercentageOfEventsCompleted's own tail multiplies its 0..100 rank sum by 0.01 and
// then by 55.0. 45 + 55 == 100 exactly. (Read with the verified .rdata reader over the
// unpacked ARTIST image; the same reader reproduces two constants this subsystem had
// already derived independently, 0.85f @flt_82029BB8 and 60.0f @flt_82004C6C.)
// ===================================================================================

#include "GameSource/GameState/Progression/BrnProgressionManager.h"

#include "GameSource/GameState/Progression/BrnProfile.h"             // Profile / ProfileEvent / RivalData
#include "GameSource/GameState/Progression/BrnProgressionRivalData.h"// RivalData::EState
#include "SharedClasses/Progression/BrnProgressionData.h"            // ProgressionData
#include "SharedClasses/Progression/BrnRival.h"                      // BrnProgression::Rival
#include "SharedClasses/Progression/BrnProgressionRankData.h"        // ProgressionRankData (medal threshold)
#include "SharedClasses/StreetData/BrnStreetData.h"                  // BrnStreetData::StreetData (GetRoadCount)
#include "GameSource/GameState/StreetData/BrnGameStateStreetManager.h" // BrnGameState::StreetManager::GetStreetData
#include "GameSource/GameState/Offences/BrnStuntManager.h"           // BrnGameState::StuntManager::GetTotalStuntElementCount
#include "GameSource/GameState/AchievementManager/BrnGameStateAchievementManagerBase.h" // OnGameCompletion
#include "GameSource/GameState/ModeManager/BrnModeManager.h"         // ModeManager (SendGameCompletionResults' mode read)
#include "GameSource/GameState/BrnGameActions.h"                     // GameStateModuleIO action ids
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"     // VariableEventQueue<13312,16>::AddEvent
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Core/CgsID.h"                       // CgsIDCompress ("CARBEAGT")
#include "GameSource/GameState/Progression/BrnDerivedCars.h"          // DerivedCarArray (UnlockDerivedCarCollection)
#include "GameSource/GameState/Progression/BrnProgressionCarData.h"  // CarData::UnlockType (AddCar's E_UNLOCK_TYPE_GIFT)
#include "SharedClasses/Progression/BrnRaceEventData.h"              // RaceEventData::EModeType / GetMode, EventJunction
#include "SharedClasses/Progression/BrnTrophyUnlockData.h"           // TrophyUnlockData::UnlockType
#include "SharedClasses/Progression/BrnTrainingTypes.h"              // BrnProgression::E_TRAINING_TYPE_POWER_PARK
#include "GameSource/GameState/TrainingManager/BrnTrainingManager.h" // TrainingManager::RequestTraining
#include "GameShared/GameClasses/Development/Log/CgsLog.h"           // CgsDev::Log::gpDebugPrint ([completion] witnesses)

#include <stdlib.h>                                                  // getenv (the [completion] witness gate)

namespace BrnProgression
{

namespace GsmIO = BrnGameState::GameStateModuleIO;

// ---- the .rdata constants, all read from the unpacked ARTIST image -------------------
static const f32 KF_WEIGHT_TIME_ROAD_RULES  =   9.0f;   // flt_82CDB948
static const f32 KF_WEIGHT_CRASH_ROAD_RULES =   9.0f;   // flt_82CDB94C
static const f32 KF_WEIGHT_RIVALS_BEATEN    =  11.0f;   // flt_82CDB950
static const f32 KF_WEIGHT_STUNT_ELEMENTS   =  11.0f;   // flt_82CDB954
static const f32 KF_WEIGHT_EVENTS_FOUND     =   2.5f;   // flt_82CDB958
static const f32 KF_WEIGHT_DRIVE_THRUS      =   2.5f;   // flt_82CDB95C
static const f32 KF_EVENTS_COMPLETED_SCALE  =  55.0f;   // flt_82CDB960
static const f32 KF_PERCENT_SCALE           =   0.01f;  // flt_82029F24
static const f32 KF_ONE_DRIVE_THRU          =   0.02857142873108387f;  // flt_820317A0 == 1/35
static const f32 KF_COMPLETE                = 100.0f;   // flt_820049E0
static const f32 KF_COMPLETE_EPSILON        =   1.52587890625e-05f;    // flt_82029B74 == 2^-16
static const f32 KF_ONE_HUNDRED_AND_ONE     = 101.0f;   // flt_8203179C -- yes, really; see the tail
static const f32 KF_ZERO                    =   0.0f;   // flt_82001CC0

// The authored per-rank contribution to the completion percentage, flt_82CDB964[]. Six entries,
// one per progression rank, and they sum to exactly 100 -- which is the internal cross-check on
// the table's LENGTH: BrnProgressionRankData.h already records the six authored medal thresholds
// (2, 7, 15, 26, 40, 120) for the same six ranks, and the next .rdata word after this table is
// 540.0f, plainly a different constant.
static const f32 KAF_RANK_COMPLETION_CONTRIBUTION[] = { 2.5f, 7.5f, 10.0f, 17.5f, 27.5f, 35.0f };
static const s32 KI_RANK_COMPLETION_COUNT =
    static_cast<s32>(sizeof(KAF_RANK_COMPLETION_CONTRIBUTION) / sizeof(f32));

// ===================================================================================
// ProgressionManager::GetTrueNumberOfRivals  @ 0x8236FB10
//
// The rivals-beaten denominator: how many authored rivals are real opponents.
// `lbz r11, 0x17(r11)` on a 56-byte-strided Rival record is mbIsUsedForRankUpGiftCar --
// an entry that only exists to carry a rank-up gift car is not something you can beat,
// so it is excluded.
// ===================================================================================
s32 ProgressionManager::GetTrueNumberOfRivals()
{
    const ProgressionData* lpProgressionData = GetProgressionData();
    if (lpProgressionData == 0)
    {
        return 0;   // PC bring-up guard; the console reaches the table through operator->
    }

    s32 liTrueRivals = 0;
    for (s32 liIndex = 0; liIndex < lpProgressionData->GetRivalCount(); ++liIndex)
    {
        // GetRival owns the "liIndex < miRivalCount" assert the console bakes as
        // BrnProgressionData.h:460 and re-fires every iteration.
        const Rival* lpRival = lpProgressionData->GetRival(liIndex);
        if (!lpRival->GetIsUsedForRankUpGiftCar())
        {
            ++liTrueRivals;
        }
    }
    return liTrueRivals;
}

// ===================================================================================
// ProgressionManager::GetNumberOfBeatenRivals  @ 0x8236FBC8
//
// The numerator: of those same real rivals, how many the profile has BEATEN
// (RivalData::meState @+0x10 == E_STATE_BEATEN == 3).
//
// ⚠️ The console looks the profile record up TWICE with the same key (0x8236FCA0 and
// 0x8236FD20) -- once to test "is it there at all", once to read the state. Both scans
// are Profile::FindRival open-coded (Profile+0x6280, stride 0x38, `ld`/`cmpld` at +0).
// ⛔ AND THE SECOND LOOKUP'S MISS PATH DEREFERENCES NULL: at 0x8236FD40 the console does
// `li r11, 0` then `lwz r11, 0x10(r11)`, i.e. it reads absolute address 0x10. It cannot
// be reached -- the first lookup already proved the record exists and nothing between
// them mutates the list -- and on the console's low-memory map it would read a mapped
// word rather than fault. Reproducing an unreachable wild read would be the only way to
// turn a console non-event into a PC access violation, so the miss path is guarded here
// and the divergence is named rather than hidden.
// ===================================================================================
s32 ProgressionManager::GetNumberOfBeatenRivals()
{
    const ProgressionData* lpProgressionData = GetProgressionData();
    if (lpProgressionData == 0)
    {
        return 0;
    }

    s32 liBeatenRivals = 0;
    for (s32 liIndex = 0; liIndex < lpProgressionData->GetRivalCount(); ++liIndex)
    {
        const Rival* lpRival = lpProgressionData->GetRival(liIndex);
        if (lpRival->GetIsUsedForRankUpGiftCar())
        {
            continue;
        }

        // `ldx r8, r10, r29` -- the authored rival's own 64-bit id is the profile key.
        const RivalData* lpRivalData = mProfile.FindRival(lpRival->GetId());
        if (lpRivalData != 0 && lpRivalData->meState == RivalData::E_STATE_BEATEN)
        {
            ++liBeatenRivals;
        }
    }
    return liBeatenRivals;
}

// ===================================================================================
// ProgressionManager::GetPercentageOfEventsCompleted  @ 0x8237B390
//
//   sum   = SUM over the ranks already earned of KAF_RANK_COMPLETION_CONTRIBUTION[i]
//   frac  = totalWins / thisRank.mu16MedalThresholdToNextRank      (0 when no wins yet)
//   return (KAF_RANK_COMPLETION_CONTRIBUTION[rank] * frac + sum) * 0.01f * 55.0f
//
// ⚠️ THE WIN TOTAL IS THE FIRST AND THIRD OUT-PARAM ONLY. `lwz r10, var_60` +
// `lwz r11, var_58` + `add r28, r11, r10` -- rank wins plus special-event wins. The
// middle (non-rank) count is deliberately excluded, exactly as in OnTrophyUnlock.
// ⚠️ THE FRACTION'S GUARD IS ON THE NUMERATOR. `cmplwi cr6, r28, 0 / beq` -- zero wins
// short-circuits to 0.0f; the DENOMINATOR is never checked, so an authored threshold of
// 0 divides by zero on both console and host. Reproduced (the shipped table's six
// thresholds are 2/7/15/26/40/120, none of them zero).
// ===================================================================================
f32 ProgressionManager::GetPercentageOfEventsCompleted()
{
    f32 lfRankSum = KF_ZERO;

    // `extsb r31, r3` -- the rank is a signed byte.
    const s32 liRank = GetProgressionRank();
    s32       liNextRankIndex = 0;
    if (liRank > 0)
    {
        for (s32 liIndex = 0; liIndex < liRank; ++liIndex)
        {
            // FLAG (host guard, not the console's): the console indexes flt_82CDB964[]
            // unbounded. The table has six entries and the rank is clamped to the authored
            // rank count by GetProgressionRank, so this cannot trip -- but a table read
            // driven by save data is not something to leave unbounded on the host.
            if (liIndex < KI_RANK_COMPLETION_COUNT)
            {
                lfRankSum += KAF_RANK_COMPLETION_CONTRIBUTION[liIndex];
            }
        }
        liNextRankIndex = liRank;
    }

    u32 luRankWins         = 0;
    u32 luNonRankWins      = 0;
    u32 luSpecialEventWins = 0;
    mProfile.GetTotalWinCount(luRankWins, luNonRankWins, luSpecialEventWins);
    const u32 luTotalWins = luRankWins + luSpecialEventWins;

    const ProgressionData* lpProgressionData = GetProgressionData();
    if (lpProgressionData == 0)
    {
        return KF_ZERO;
    }

    f32 lfFractionOfNextRank = KF_ZERO;
    if (luTotalWins != 0)
    {
        // GetProgressionRankData owns the "luIndex < muProgressionRankCount" assert the
        // console bakes as BrnProgressionData.h:330; the 112-byte stride is its `mulli 0x70`.
        const ProgressionRankData* lpRankData =
            lpProgressionData->GetProgressionRankData(static_cast<u32>(liRank));
        // `lhz r11, 0x4C(r11)` + `extsh` -- mu16MedalThresholdToNextRank.
        const s32 liMedalThreshold =
            static_cast<s32>(lpRankData->GetMedalThresholdToNextRank());
        lfFractionOfNextRank = static_cast<f32>(luTotalWins) / static_cast<f32>(liMedalThreshold);
    }

    f32 lfNextRankContribution = KF_ZERO;
    if (liNextRankIndex < KI_RANK_COMPLETION_COUNT)
    {
        lfNextRankContribution = KAF_RANK_COMPLETION_CONTRIBUTION[liNextRankIndex];
    }

    // `fmadds f13, f13, f0, f31` then the two `fmuls`.
    return (lfNextRankContribution * lfFractionOfNextRank + lfRankSum)
           * KF_PERCENT_SCALE * KF_EVENTS_COMPLETED_SCALE;
}

// ===================================================================================
// ProgressionManager::ComputeCompletionPercentage  @ 0x8238A198
//
// Seven weighted terms, clamped to [0, 100] -- and then ONE deliberate exception.
//
// ⭐⭐⭐ 101% IS REAL. The tail (0x8238A5CC..0x8238A688) compresses the literal "CARBEAGT",
// scans the profile's owned-car list for it, and -- when the clamped total is within
// 2^-16 of 100.0f AND that car is owned -- returns flt_8203179C, which reads 101.0f.
// Hex-Rays drops this entirely: it renders the CgsIDCompress call as a dead expression
// and returns the wrong register. The two `fsel`s that do the clamp are also lost in its
// output (they appear as inline `__asm` blocks with the wrong operands). This function
// is therefore reconstructed from the assembly alone.
// ⚠️ Do not "fix" the 101: CheckForSpecialCarUnlocks and SendGameCompletionResults both
// test `>= 100.0f`, so 101 passes them exactly like 100 does. It is a display value with
// no gate consequence, and clamping it to 100 would silently delete an authored reward.
//
// ⚠️ THE THREE STUNT-ELEMENT FRACTIONS ARE MULTIPLIED, NOT AVERAGED
// (`fmuls f13, f30, f29` / `fmuls f13, f13, f27` / `fmadds f29, f13, 11.0f, f28`), so the
// stunt term is 11.0 only when all three types are 100% complete and collapses toward 0
// if any one of them is low. That is unusual enough to be worth stating; it is what the
// binary does.
// ===================================================================================
f32 ProgressionManager::ComputeCompletionPercentage()
{
    // ---- terms 1 + 2: the two road-rule tallies over the world's road count ----------
    // Numerators are the manager's own +133460/+133456; the shared denominator is
    // `StreetData_::oper(mpStreetManager + 7368)` then `lwz r11, 0x20(r3)` -- the CONSOLE's
    // +0x20 is StreetData::miRoadCount (the host record widens its four pointers, so the
    // same member sits at +0x30 there; read by name, so the widening is irrelevant).
    // ⚠️ Each term is guarded on its own NUMERATOR being non-zero, and only then is the
    // StreetData fetched -- so a zero tally never touches the resource. Faithful.
    f32 lfTimeRoadRulesFraction  = KF_ZERO;
    f32 lfCrashRoadRulesFraction = KF_ZERO;

    const BrnStreetData::StreetData* lpStreetData =
        (mpStreetManager != 0) ? mpStreetManager->GetStreetData() : 0;
    if (lpStreetData != 0)
    {
        if (miNumberOfParTimeRoadRulesRuledByPlayer != 0)
        {
            lfTimeRoadRulesFraction =
                static_cast<f32>(miNumberOfParTimeRoadRulesRuledByPlayer) /
                static_cast<f32>(lpStreetData->GetRoadCount());
        }
        if (miNumberOfParCrashRoadRulesRuledByPlayer != 0)
        {
            lfCrashRoadRulesFraction =
                static_cast<f32>(miNumberOfParCrashRoadRulesRuledByPlayer) /
                static_cast<f32>(lpStreetData->GetRoadCount());
        }
    }

    f32 lfTotal = KF_WEIGHT_TIME_ROAD_RULES  * lfTimeRoadRulesFraction
                + KF_WEIGHT_CRASH_ROAD_RULES * lfCrashRoadRulesFraction;

    // ---- term 3: rivals beaten -------------------------------------------------------
    // ⚠️ The guard is `fcmpu f30, f31` on the BEATEN count (the numerator), not on the
    // rival total -- zero beaten short-circuits, and a zero denominator is not checked.
    f32       lfRivalsFraction = KF_ZERO;
    const f32 lfBeatenRivals   = static_cast<f32>(GetNumberOfBeatenRivals());
    const f32 lfTrueRivals     = static_cast<f32>(GetTrueNumberOfRivals());
    if (lfBeatenRivals != KF_ZERO)
    {
        lfRivalsFraction = lfBeatenRivals / lfTrueRivals;
    }
    lfTotal += KF_WEIGHT_RIVALS_BEATEN * lfRivalsFraction;

    // ---- term 4: the three stunt-element sets, MULTIPLIED -----------------------------
    // Numerators are the profile's per-type completed-element Set lengths (the console
    // reads the raw count word behind the CgsSet.h:227 "Set used before Construct/Clear"
    // assert, which is exactly Profile::GetStuntElementCount); denominators are the world
    // totals on the StuntManager (+0x5C4/+0x5C6/+0x5C8, s16). The console evaluates them
    // in the order JUMP, BILLBOARD, SMASH -- the same index feeds both sides of each
    // fraction, so the ordering is presentation only.
    f32 lfJumpFraction      = KF_ZERO;
    f32 lfSmashFraction     = KF_ZERO;
    f32 lfBillboardFraction = KF_ZERO;
    if (mpStuntManager != 0)
    {
        const s32 liJumpsDone = mProfile.GetStuntElementCount(BrnGameState::E_STUNT_ELEMENT_TYPE_JUMP);
        if (liJumpsDone != 0)
        {
            lfJumpFraction = static_cast<f32>(liJumpsDone) / static_cast<f32>(
                mpStuntManager->GetTotalStuntElementCount(BrnGameState::E_STUNT_ELEMENT_TYPE_JUMP));
        }
        const s32 liBillboardsDone = mProfile.GetStuntElementCount(BrnGameState::E_STUNT_ELEMENT_TYPE_BILLBOARD);
        if (liBillboardsDone != 0)
        {
            lfBillboardFraction = static_cast<f32>(liBillboardsDone) / static_cast<f32>(
                mpStuntManager->GetTotalStuntElementCount(BrnGameState::E_STUNT_ELEMENT_TYPE_BILLBOARD));
        }
        const s32 liSmashesDone = mProfile.GetStuntElementCount(BrnGameState::E_STUNT_ELEMENT_TYPE_SMASH);
        if (liSmashesDone != 0)
        {
            lfSmashFraction = static_cast<f32>(liSmashesDone) / static_cast<f32>(
                mpStuntManager->GetTotalStuntElementCount(BrnGameState::E_STUNT_ELEMENT_TYPE_SMASH));
        }
    }
    lfTotal += KF_WEIGHT_STUNT_ELEMENTS
               * (lfSmashFraction * lfBillboardFraction * lfJumpFraction);

    // ---- term 5: drive-thrus found ---------------------------------------------------
    // `GetDriveThrusFound()` is called TWICE -- once as the guard, once for the value.
    // The scale is 1/35, i.e. the authored total number of drive-thrus.
    f32 lfDriveThruTerm = KF_ZERO;
    if (mProfile.GetDriveThrusFound() != 0)
    {
        lfDriveThruTerm = static_cast<f32>(mProfile.GetDriveThrusFound()) * KF_ONE_DRIVE_THRU;
    }
    lfTotal += KF_WEIGHT_DRIVE_THRUS * lfDriveThruTerm;

    // ---- term 6: events discovered ---------------------------------------------------
    // The console counts ProfileEvent flag bit 0 (E_FLAG_DISCOVERED) over miEventCount,
    // then -- if the count is non-zero -- COUNTS THE WHOLE LIST A SECOND TIME to produce
    // the numerator it actually divides. Reproduced as one count plus the same guard: the
    // second walk is byte-identical to the first and cannot produce a different answer.
    f32       lfEventsFoundFraction = KF_ZERO;
    const u32 luEventCount          = mProfile.GetEventCount();
    u32       luEventsFound         = 0;
    for (u32 luEvent = 0; luEvent < luEventCount; ++luEvent)
    {
        if (mProfile.GetEvent(luEvent)->IsFlagSet(ProfileEvent::E_FLAG_DISCOVERED))
        {
            ++luEventsFound;
        }
    }
    if (luEventsFound != 0)
    {
        lfEventsFoundFraction = static_cast<f32>(luEventsFound) / static_cast<f32>(luEventCount);
    }
    lfTotal += KF_WEIGHT_EVENTS_FOUND * lfEventsFoundFraction;

    // ---- term 7 + the clamp ----------------------------------------------------------
    lfTotal += GetPercentageOfEventsCompleted();

    // `fneg f13, f0 / fsel f0, f13, f31, f0` == "(-total >= 0) ? 0 : total", the low clamp;
    // `fsubs f13, f31, f0 / fsel f30, f13, f0, f31` == "(100 - total >= 0) ? total : 100".
    if (lfTotal <= KF_ZERO)
    {
        lfTotal = KF_ZERO;
    }
    if (lfTotal > KF_COMPLETE)
    {
        lfTotal = KF_COMPLETE;
    }

    // ⭐ The 101% award. `CgsIDCompress("CARBEAGT")` then the profile's owned-car scan; the
    // "is it 100" test is |total - 100| < 2^-16, not an equality.
    const bool lbIsComplete =
        (lfTotal - KF_COMPLETE < KF_COMPLETE_EPSILON) &&
        (KF_COMPLETE - lfTotal < KF_COMPLETE_EPSILON);
    if (lbIsComplete && mProfile.FindCar(CgsIDCompress("CARBEAGT")) != 0)
    {
        return KF_ONE_HUNDRED_AND_ONE;
    }
    return lfTotal;
}

// ===================================================================================
// ProgressionManager::CheckForSpecialCarUnlocks  @ 0x82396058
//
// The two derived-car tiers, each a one-shot latched in the profile:
//   SILVER (Profile+42516) unlocks at GetCurrentProgressionRank() >= ProgressionData+0x14
//   GOLD   (Profile+42517) unlocks at ComputeCompletionPercentage() >= 100.0f, and also
//          reports the game complete to the achievement manager.
// ⚠️ The two flags were committed under each other's names until this wave; the console's
// own debug strings print +42516 as AreSilverCarsUnlocked() and +42517 as
// AreGoldCarsUnlocked(), and the gates above corroborate. See BrnProfile.h.
//
// The three `gxMessageFilterFlags & 1` debug-print blocks the console carries are dropped
// per the project convention on the gpcMessageBuffer/StrStream machinery; they have no
// side effects (they read the same two flags the arms below test).
// ===================================================================================
void ProgressionManager::CheckForSpecialCarUnlocks()
{
    if (!mProfile.GetSilverCarsUnlocked())
    {
        // `lbz r19, 0x1E0(r31)` + `extsb` == Profile+112, mi8CurrentProgressionRank, compared
        // SIGNED against ProgressionData+0x14 (the authored rank count).
        // ⚠️ BOTH SIDES ARE SIGN-EXTENDED FROM A BYTE, and the right-hand one is the odd
        // part: the console loads the rank count with `lwz r10, 0x14(r3)` -- a full WORD -- and
        // then narrows it with `extsb r10, r10` before `cmpw` (0x8239615C..0x82396168). So the
        // comparand is the low byte of muProgressionRankCount sign-extended, not the word. With
        // the shipped table's six ranks the two are identical; reproduced as written because a
        // "tidy" widening would change the comparison for any authored count above 127.
        const ProgressionData* lpProgressionData = GetProgressionData();
        if (lpProgressionData != 0 &&
            static_cast<s32>(mProfile.GetCurrentProgressionRank())
                >= static_cast<s32>(static_cast<s8>(
                       static_cast<u8>(lpProgressionData->GetProgressionRankCount()))))
        {
            UnlockSpecialCars(4);
            mProfile.SetSilverCarsUnlocked(true);
        }
    }

    if (!mProfile.GetGoldCarsUnlocked())
    {
        if (ComputeCompletionPercentage() >= KF_COMPLETE)
        {
            UnlockSpecialCars(3);
            mProfile.SetGoldCarsUnlocked(true);

            // `OnGameCompletion(*(a1 + 133432))` -- the achievement manager, then the
            // manager's own one-byte "game complete" latch at +133487.
            BrnGameState::AchievementManagerBase* lpAchievementManager = GetAchievementManager();
            if (lpAchievementManager != 0)
            {
                lpAchievementManager->OnGameCompletion();
            }
            mbAutosaveRequested = true;   // `stbx r22, r31, 0x2096F` -- PreWorldUpdate drains it
        }
    }
}

// ===================================================================================
// ProgressionManager::SendGameCompletionResults  @ 0x82395C28
//
// Posts the 8-byte game-completion record (game action 208) onto the game-action queue,
// and -- the first time the game reads as complete -- stamps the completion date into the
// profile.
//
// THE RECORD, from the console's own frame (var_30 is the base handed to AddEvent with
// `li r6, 8`):
//     +0x00  s32  meGameMode          = mpModeManager->GetCurrentGameModeType()  (+3476)
//     +0x04  bool mbGameComplete
//     +0x05  bool mbCompletionAlreadyRecorded (the profile flag read at Profile+118032)
// ⚠️ Hex-Rays shows `v14`/`v15` as var_2C/var_2B, i.e. record+0x04 and +0x05, and it has
// them the right way round; the asm confirms (`stb` of 1 into var_2C on the complete arm).
// ===================================================================================
void ProgressionManager::SendGameCompletionResults(CgsModule::VariableEventQueue<13312, 16>* lpGameActionQueue)
{
    GsmIO::GameCompletionResultsAction lRecord = {};

    // `lwzx r11, r31, 0x2093C` then `lwz r11, 0xD94(r11)` -- the MODE manager's current mode.
    // (That +0xD94 read is the evidence that the +0x2093C back-pointer is a ModeManager; it was
    // committed as an untyped `void* mpGameStateModule` until this wave. See the header.)
    if (mpModeManager != 0)
    {
        lRecord.meGameMode = mpModeManager->GetCurrentGameModeType();
    }

    if (ComputeCompletionPercentage() >= KF_COMPLETE)
    {
        lRecord.mbGameComplete = true;
        // `lbzx r10, r31, 0x1CE80` == manager+118400 == Profile+118032.
        lRecord.mbCompletionAlreadyRecorded = mProfile.GetSeen100PercentCompletionSequence();

        // `lbzx r11, r31, 0x1CE85` == manager+118405 == Profile+118037. First completion only:
        // stamp the date and latch the flag (the console does both, in that order, and nothing
        // else writes either -- which is why Profile exposes them as one method).
        if (!mProfile.GetHaveSet100PercentCompletedDate())
        {
            mProfile.Set100PercentCompletedDateAsNow();
        }
    }

    lpGameActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lRecord),
                                GsmIO::E_ACTION_GAME_COMPLETION_RESULTS,
                                static_cast<s32>(sizeof(lRecord)));
}


// ############################################################################################
// ############################################################################################
// [progression wave 2026-09-06, lane "completion"] THE THREE COMPLETION-REWARD LEGS whose
// console CALLER was already live on PC while the body was missing -- the same species as the
// odometer (issue #10): a seat exists, nothing sits in it.
//
//   ProgressionManager::CheckForAllModeTypeCompletion        @0x82389698   (41 instructions)
//   ProgressionManager::GetEventCountForType                 @0x8236F9D8   (78 instructions)
//   ProgressionManager::GetEventTypeUniqueWinCount           @0x82370758   (83 instructions)
//   ProgressionManager::UnlockTrophyForEventTypeAllCompleted @0x82395FE8   (a 9-case jump table)
//   ProgressionManager::SendTrophyUnlockUpdate               @0x823892B8   (60 instructions)
//   ProgressionManager::UnlockDerivedCarCollection           @0x8237AD70   (113 instructions)
//   ProgressionManager::OnPowerParkResult                    @0x8238AF78   (76 instructions)
//
// (DerivedCarArray::DEBUG_PrintArray @0x8236ACE8 landed in BrnDerivedCars.h, its console home --
//  the CGS_ASSERT __FILE__ evidence in that header's banner.)
//
// ⭐ WHAT WAS ACTUALLY BROKEN, in one line each:
//   * SendTrophyUnlockUpdate is the ONLY drain of mQueueOfTrophyCarUnLocks. UnlockCarFromTrophy
//     (_Unlocks.cpp) Appends into it today, so the queue could only ever FILL: every trophy car
//     the player earned sat in a 12-slot Array that nothing emptied, the GUI never heard action
//     204, and the twelfth award would have hit "Array container out of space".
//   * UnlockDerivedCarCollection is what turns a car you own into its paint/livery family. Both
//     its console callers were live and both were parked (CarSelectManager::
//     StartCarModificationState @0x82387410 -- every junkyard/paint-shop visit -- and
//     OnEventFinishUpdateProfile's first-special-win arm).
//   * CheckForAllModeTypeCompletion is PreWorldUpdate's 2 s hold. The gate pair and the timer
//     were faithful; the callee was a park, so the "you have won every race/road rage/..." HUD
//     message could never be posted and the profile bit that suppresses it was never set.
//
// ⚠️ THE ASM IS THE SPINE. Two Hex-Rays renderings are wrong and each is called out at its site:
//   (a) UnlockDerivedCarCollection's `if (&v3[6 * v6 + 92] == -640)` is the compiler's always-true
//       null check on `&array[i]`, not a real comparison -- the branch it guards is the
//       ALREADY-OWNED skip.
//   (b) GetEventCountForType / GetEventTypeUniqueWinCount render the junction scan's "not found"
//       tail as `v14 = 0` followed by a use; on the console that use is a load off address 0.
//
// Every member is reached BY NAME through BrnProgressionManager.h / BrnProfile.h.
// ############################################################################################

// The console's baked assert path for BrnProgressionManager.cpp (this partfile's console home),
// so the file/line in the log stay the binary's rather than this TU's.
static const char* const KAC_COMPLETION_FILE =
    "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../GameState/Progression/BrnProgressionManager.cpp";

namespace
{
    // [FLAG PC witness] `[completion] ...` -- NOT IN THE X360 BINARY. Opt-in
    // (BRN_PROGRESSION_COMPLETION), bounded to the first KI_WITNESS_LINE_BUDGET lines a run.
    // Read by tools/tests/cases/progression_completion.ps1.
    const s32 KI_WITNESS_LINE_BUDGET = 16;
    s32       giWitnessLinesPrinted  = 0;

    bool CompletionWitnessEnabled()
    {
        static const bool sbDiag = (getenv("BRN_PROGRESSION_COMPLETION") != 0);
        return sbDiag && CgsDev::Log::gpDebugPrint != 0 &&
               giWitnessLinesPrinted < KI_WITNESS_LINE_BUDGET;
    }

    // ------------------------------------------------------------------------------------
    // The junction-table scan BOTH count functions inline (X360 0x8236FA84..0x8236FAE4 and
    // 0x82370808..0x82370868, byte-identical): find the authored EventJunction whose id equals
    // the profile event's id and return its OFFLINE event.
    //
    // DWARF names it `ProgressionData::FindOfflineEvent` (the callee hint list of
    // BrnProgressionManager.cpp:4283), i.e. it belongs on ProgressionData -- but that class's
    // owning header/TU is not this lane's to grow, and the X360 emits no standalone symbol for
    // it, so it lives here as a file-local de-inlining with its real home named.
    // DELETE-WHEN ProgressionData::FindOfflineEvent lands in BrnProgressionData.h/.cpp; this
    // helper is then one call site away from being replaced by it.
    //
    // ⚠️ A MATCH WITH A ZERO OFFLINE SLOT IS A MISS. `lwz r31, 4(r11) / cmplwi r31, 0 / bne`:
    // the console falls into the same "lpEventData" assert path a no-match would.
    // ------------------------------------------------------------------------------------
    const RaceEventData* FindOfflineEvent(const ProgressionData* lpProgressionData, u32 luEventID)
    {
        const u32 luJunctionCount = lpProgressionData->GetEventJunctionCount();   // +0x1C
        for (u32 luIndex = 0; luIndex < luJunctionCount; ++luIndex)
        {
            const EventJunction* lpJunction = lpProgressionData->GetEventJunction(luIndex);
            if (lpJunction->GetID() == luEventID)                                 // `lwz r8, 0(r11)`
            {
                return lpJunction->GetOfflineEvent();                             // `lwz r31, 4(r11)`
            }
        }
        return 0;
    }
}

// ===================================================================================
// ProgressionManager::GetEventCountForType  @ 0x8236F9D8
//
// How many of the profile's event records are authored with mode leModeType.
//
// The loop bound is `lwz r21, 0x3E8(this)` == this+1000 == Profile+0x278 == miEventCount, read
// ONCE before the loop -- and then re-read every iteration for the BrnProfile.h:3004 range
// assert, which is Profile::GetEvent inlined (its body carries that assert, so this walk calls
// the accessor rather than duplicating it). The record base is `addi r22, r3, 0x170` +
// `li r28, 0x7080` == Profile+28800 == maEvents, stride 8.
//
// ⛔ THE :1627 ASSERT CANNOT FIRE. `add r31, r28, r22 / cmplwi r31, 0 / bne` is a null test on
// `&maEvents[i]`, i.e. on an interior address of a live object; the compiler emitted it because
// the source wrote `CGS_ASSERT(lpProfileEvent, ...)` after taking the address. Reproduced as the
// same test on the accessor's return so the assert exists where the console has it.
// ===================================================================================
u32 ProgressionManager::GetEventCountForType(s32 leModeType) const
{
    const u32 luEventCount = mProfile.GetEventCount();
    u32       luEventTypeCount = 0;

    for (u32 luIndex = 0; luIndex < luEventCount; ++luIndex)
    {
        const ProfileEvent* lpProfileEvent = mProfile.GetEvent(luIndex);
        if (lpProfileEvent == 0)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("lpProfileEvent", KAC_COMPLETION_FILE, 1627);
            CgsDev::Assert::EndAssert();
        }

        const ProgressionData* lpProgressionData = GetProgressionData();
        const RaceEventData*   lpEventData =
            (lpProgressionData != 0) ? FindOfflineEvent(lpProgressionData, lpProfileEvent->GetID()) : 0;
        if (lpEventData == 0)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("lpEventData", KAC_COMPLETION_FILE, 1630);
            CgsDev::Assert::EndAssert();
            // [PC GUARD] the console then does `lbz r11, 0xEC(r31)` with r31 == 0 -- a read of
            // absolute address 0xEC, which its low-memory map tolerates and the host does not.
            // Skipping the entry is the only behaviour that is not an invented answer: on the
            // console the byte read there is not this event's mode either.
            continue;
        }

        // `lbz r11, 0xEC(r31) / cmpw r11, r19` -- RaceEventData::mu8Mode against the argument.
        if (static_cast<s32>(lpEventData->GetMode()) == leModeType)
        {
            ++luEventTypeCount;
        }
    }

    return luEventTypeCount;
}

// ===================================================================================
// ProgressionManager::GetEventTypeUniqueWinCount  @ 0x82370758
//
// The same walk as GetEventCountForType, additionally requiring the record's FLAGS to carry the
// caller's win bit: `lhz r11, 4(r30) / and r11, r11, r17 / cmpwi 0 / beq`. The mask arrives as a
// u16 (r5, used through `and` on a halfword load), which is why the parameter is ProfileEvent::
// Flags in the DWARF and a u16 here.
//
// The console's own assert line numbers differ from the twin above (:4401 / :4404) because the
// two functions sit ~2800 lines apart in BrnProgressionManager.cpp; they are the same two texts.
// ===================================================================================
u32 ProgressionManager::GetEventTypeUniqueWinCount(s32 leModeType, u16 lu16WinTypeMask) const
{
    const u32 luEventCount = mProfile.GetEventCount();
    u32       luWinCount   = 0;

    for (u32 luIndex = 0; luIndex < luEventCount; ++luIndex)
    {
        const ProfileEvent* lpProfileEvent = mProfile.GetEvent(luIndex);
        if (lpProfileEvent == 0)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("lpProfileEvent", KAC_COMPLETION_FILE, 4401);
            CgsDev::Assert::EndAssert();
        }

        const ProgressionData* lpProgressionData = GetProgressionData();
        const RaceEventData*   lpEventData =
            (lpProgressionData != 0) ? FindOfflineEvent(lpProgressionData, lpProfileEvent->GetID()) : 0;
        if (lpEventData == 0)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("lpEventData", KAC_COMPLETION_FILE, 4404);
            CgsDev::Assert::EndAssert();
            continue;   // [PC GUARD] -- see GetEventCountForType's note on the same tail
        }

        if (static_cast<s32>(lpEventData->GetMode()) == leModeType &&
            (lpProfileEvent->GetFlags() & lu16WinTypeMask) != 0)
        {
            ++luWinCount;
        }
    }

    return luWinCount;
}

// ===================================================================================
// ProgressionManager::CheckForAllModeTypeCompletion  @ 0x82389698
//
// PreWorldUpdate's 2 s hold fires this once per armed mode: if the profile has not already been
// shown the "all <mode> events won" message, and the number of DISTINCT events of that mode the
// player has won has caught up with the number authored, post AllEventTypeWonAction (game action
// 206, size 4) and latch the profile bit so it is shown once ever.
//
// ⭐ THE WIN FLAG DEPENDS ON THE MODE, AND ONLY ON ONE MODE. `cmpwi cr6, r31, 4 / beq` at
// 0x823896DC: mode 4 == RaceEventData::E_MODE_BURNING_ROUTE asks for
// ProfileEvent::E_FLAG_WON_SPECIAL_EVENT_BEFORE (16); every other mode asks for
// E_FLAG_RANK_WIN (4). That is the binary distinguishing a burning route -- which is won by
// beating its target and is flagged as a "special event" win -- from a race you rank-win.
// (The `li r4, 4` on that same arm is the compiler re-materialising leModeType, which is
// already 4 there; the MASK is the only thing that changes.)
//
// ⚠️ THE COMPARISON IS `>=`, UNSIGNED (`cmplw r3, r28 / blt -> skip`). A player who has won more
// unique events of a mode than the authored count still passes; it is not an equality.
//
// ⓘ THE PAYLOAD IS AN EVENT TYPE, NOT A GAME MODE. `mr r3, r31 / bl GetEvent` -- the console
// calls ProgressionManager::GetEvent with the MODE in r3 and stores its returned word. GetEvent
// @0x82359850 ignores its `this` entirely (its switch is on r3), which is why the disassembler
// prints it as a one-argument function; the tree declares it as the non-static const member the
// DWARF gives, and the value it returns is identical either way. See BrnGameActions.h's note on
// AllEventTypeWonAction::leGameEventType.
// ===================================================================================
void ProgressionManager::CheckForAllModeTypeCompletion(
        GsmIO::GameActionQueue* lpGameActionQueue, s32 leModeType)
{
    // `clrlwi r11, r3, 24 / cmplwi 0 / bne` -- already shown for this mode, nothing to do.
    if (mProfile.GetSeenAllEventTypeWonMessage(leModeType))
    {
        return;
    }

    const u32 luEventCountForType = GetEventCountForType(leModeType);

    const u16 lu16WinTypeMask =
        (leModeType == RaceEventData::E_MODE_BURNING_ROUTE)
            ? static_cast<u16>(ProfileEvent::E_FLAG_WON_SPECIAL_EVENT_BEFORE)   // li r5, 0x10
            : static_cast<u16>(ProfileEvent::E_FLAG_RANK_WIN);                  // li r5, 4

    const u32 luEventTypeUniqueWinCount = GetEventTypeUniqueWinCount(leModeType, lu16WinTypeMask);

    if (luEventTypeUniqueWinCount < luEventCountForType)
    {
        return;
    }

    GsmIO::AllEventTypeWonAction lAllEventTypeWonAction;
    lAllEventTypeWonAction.leGameEventType =
        static_cast<GsmIO::EGameModeType>(GetEvent(leModeType));
    lpGameActionQueue->AddEvent(
        reinterpret_cast<const CgsModule::Event*>(&lAllEventTypeWonAction),
        GsmIO::E_ACTION_ALL_EVENT_TYPE_WON,                                     // li r5, 0xCE
        static_cast<s32>(sizeof(lAllEventTypeWonAction)));                      // li r6, 4

    mProfile.SetSeenAllEventTypeWonMessage(leModeType);

    if (CompletionWitnessEnabled())
    {
        ++giWitnessLinesPrinted;
        *CgsDev::Log::gpDebugPrint
            << "[completion] all-mode-type won: mode=" << leModeType
            << " authored="   << luEventCountForType
            << " uniqueWins=" << luEventTypeUniqueWinCount
            << " action206="  << static_cast<s32>(lAllEventTypeWonAction.leGameEventType) << "\n";
    }
}

// ===================================================================================
// ProgressionManager::UnlockTrophyForEventTypeAllCompleted  @ 0x82395FE8
//
// OnEventFinishUpdateProfile's "every event of this game mode is now completed" seat: map the
// EGameModeType onto the TrophyUnlockData::UnlockType that rewards completing all of them, and
// tail-call OnTrophyUnlock. The console is a 9-entry jump table (cases 0..8; anything above 8
// returns immediately through `bgtlr`), and four of the nine cases deliberately do nothing.
//
// ⓘ CASE 8 IS NOT A TYPO. E_MODE_MARKED_MAN maps to E_UNLOCKTYPE_COMPLETE_ALL_SURVIVORS (15),
// not to an "all marked man" type -- Marked Man was authored as the Survivor mode and the trophy
// table kept the older name. Case 7 (STUNT_ATTACK) -> 16 and case 5 (BURNING_ROUTE) -> 13 are
// the neighbouring proofs that the mapping is by MODE and not by enumerator arithmetic.
// ⓘ The four silent cases are FACE_OFF (1), OFFLINE_SHOWTIME (2), PURSUIT (4) and ELIMINATOR (6)
// -- E_UNLOCKTYPE_COMPLETE_ALL_ELIMINATORS (14) exists in the table but nothing reaches it here.
// ===================================================================================
void ProgressionManager::UnlockTrophyForEventTypeAllCompleted(GsmIO::EGameModeType leGameModeType)
{
    switch (leGameModeType)
    {
        case GsmIO::E_MODE_OFFLINE_RACE:                                        // jumptable case 0
            OnTrophyUnlock(TrophyUnlockData::E_UNLOCKTYPE_COMPLETE_ALL_RACES);          // li r4, 0xB
            break;
        case GsmIO::E_MODE_ROAD_RAGE:                                           // case 3
            OnTrophyUnlock(TrophyUnlockData::E_UNLOCKTYPE_COMPLETE_ALL_ROADRAGES);      // li r4, 0xC
            break;
        case GsmIO::E_MODE_BURNING_ROUTE:                                       // case 5
            OnTrophyUnlock(TrophyUnlockData::E_UNLOCKTYPE_COMPLETE_ALL_BURNINGROUTES);  // li r4, 0xD
            break;
        case GsmIO::E_MODE_STUNT_ATTACK:                                        // case 7
            OnTrophyUnlock(TrophyUnlockData::E_UNLOCKTYPE_COMPLETE_ALL_STUNTATTACK);    // li r4, 0x10
            break;
        case GsmIO::E_MODE_MARKED_MAN:                                          // case 8
            OnTrophyUnlock(TrophyUnlockData::E_UNLOCKTYPE_COMPLETE_ALL_SURVIVORS);      // li r4, 0xF
            break;
        default:
            // cases 1, 2, 4, 6 (`blr`) and everything above 8 (`bgtlr`): no trophy.
            break;
    }
}

// ===================================================================================
// ProgressionManager::SendTrophyUnlockUpdate  @ 0x823892B8
//
// THE ONLY DRAIN OF mQueueOfTrophyCarUnLocks. PreWorldUpdate calls it every frame the queue is
// non-empty; it posts the TAIL element as game action 204 (E_ACTION_TROPHY_UNLOCK, 16 B) and
// Erases it, so one queued trophy car leaves per frame.
//
// ⭐ THE TAIL, NOT THE HEAD. `lwz r11, 0xC0(r31) / addi r30, r11, -1` -- lTrophyUnlockToSend is
// GetLength() - 1, and Erase() then shifts nothing (it is the last slot). Draining from the
// front would have re-ordered nothing visible but would have moved 15 bytes per element per
// frame; this is the binary's choice and it is also why the queue behaves as a stack.
//
// ⚠️ NO EMPTY-QUEUE GUARD, ON PURPOSE. With a length of 0 the index is (u32)-1 and the array's
// own checked operator[] fires "Array index out of bounds" -- exactly what the console does
// (`addi r30, r11, -1` with r11 == 0). The gate lives at the CALL SITE, in PreWorldUpdate's
// `if (GetLength() > 0)`, which is where the console puts it too.
//
// The two asserts are the console's, by name and by line; both read the element through a fresh
// operator[] call, as the three separate `bl sub_82360210` in the asm show.
// ===================================================================================
void ProgressionManager::SendTrophyUnlockUpdate(GsmIO::GameActionQueue* lpGameActionQueue)
{
    // GetLength() owns the CgsArray.h:336 "Array used before Construct/Clear was called" assert
    // the console fires here inline (`lwz r11, 0xC0(r31) / cmpwi -1`).
    const u32 luTrophyUnlockToSend = mQueueOfTrophyCarUnLocks.GetLength() - 1;

    if (mQueueOfTrophyCarUnLocks[luTrophyUnlockToSend].meUnlockType ==
            TrophyUnlockData::E_UNLOCKTYPE_NONE)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert(
            "mQueueOfTrophyCarUnLocks[lTrophyUnlockToSend].meUnlockType != "
            "TrophyUnlockData::E_UNLOCKTYPE_NONE",
            KAC_COMPLETION_FILE, 639);
        CgsDev::Assert::EndAssert();
    }

    if (mQueueOfTrophyCarUnLocks[luTrophyUnlockToSend].mCarToUnlock == 0)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert(
            "mQueueOfTrophyCarUnLocks[lTrophyUnlockToSend].mCarToUnlock != kCGSID_NULL",
            KAC_COMPLETION_FILE, 640);
        CgsDev::Assert::EndAssert();
    }

    const GsmIO::TrophyUnlockAction& lrUnlock = mQueueOfTrophyCarUnLocks[luTrophyUnlockToSend];
    lpGameActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lrUnlock),
                                GsmIO::E_ACTION_TROPHY_UNLOCK,                  // li r5, 0xCC
                                static_cast<s32>(sizeof(GsmIO::TrophyUnlockAction)));   // li r6, 0x10

    const CgsID lPostedCarId = lrUnlock.mCarToUnlock;
    const s32   liPostedType = static_cast<s32>(lrUnlock.meUnlockType);

    mQueueOfTrophyCarUnLocks.Erase(luTrophyUnlockToSend);

    if (CompletionWitnessEnabled())
    {
        ++giWitnessLinesPrinted;
        *CgsDev::Log::gpDebugPrint
            << "[completion] trophy posted id=" << static_cast<u32>(lPostedCarId)
            << " type="      << liPostedType
            << " queueLeft=" << mQueueOfTrophyCarUnLocks.GetLength() << "\n";
    }
}

// ===================================================================================
// ProgressionManager::UnlockDerivedCarCollection  @ 0x8237AD70
//
// Award the derived cars of one car's livery family. Element 0 of the array is the PARENT (both
// DerivedCarArray builders Append it first), so the walk starts at index 1 -- you do not "unlock"
// the car you already picked.
//
// PER-ENTRY, in the console's order:
//   1. `bl __int64_8___GetItem` -- the entry's CgsID.
//   2. Profile::FindCar open-coded (`lwz r9, 0x26C(profile)` as the bound, `addi r10, profile,
//      0x280` as the base, 24-byte stride, 64-bit `cmpld` at offset 0). ALREADY OWNED -> skip the
//      whole entry. (Hex-Rays renders that skip as `if (&v3[6*v6 + 92] == -640)`, which is the
//      compiler's always-true null check on `&maCars[i]`, not a comparison anyone wrote.)
//   3. GetLiveryType(i) -- inlined, which is why the "luIndex < KU_MAX_AMOUNT_OF_DERIVED_CARS"
//      assert's baked location is BrnDerivedCars.h:232. The accessor owns it; this walk does not
//      duplicate it.
//   4. ⭐ THE PER-KIND GATE, and it is the whole point of the function:
//        kind == 3 (E_LIVERY_TYPE_COLOUR_ALT) -> gated on Profile+42517, GetGoldCarsUnlocked()
//        kind == 4 (E_LIVERY_TYPE_SILVER)     -> gated on Profile+42516, GetSilverCarsUnlocked()
//        anything else                        -> ungated (`li r30, 1` stands)
//      The two offsets are the console's `lbzx r30, r26, 0xA785` / `lbzx r30, r26, 0xA784`, i.e.
//      manager+42885 / manager+42884 == Profile+42517 / Profile+42516 -- the same pair
//      CheckForSpecialCarUnlocks above latches, and the gold/silver assignment agrees with it
//      (UnlockSpecialCars(3) is the 100%-gated set, UnlockSpecialCars(4) the rank-gated one).
//   5. AddCar(id, 1) == CarData::E_UNLOCK_TYPE_GIFT.
//   6. kind == 1 (E_LIVERY_TYPE_COLOUR) ALSO marks the unlock sequence already shown, so a plain
//      colour variant does not queue its own "new car" cinematic. Kinds 3 and 4 do queue one.
//
// ⚠️ THE LENGTH IS RE-READ EVERY ITERATION (`lwz r11, 0x40(r27)` at 0x8237ADE8 and 0x8237AE0C,
// inside the loop, with the sentinel assert re-fired each pass) rather than cached -- reproduced,
// because AddCar can in principle reach code that touches the array.
// ⚠️ THE INDEX IS A BYTE (`clrlwi r31, r11, 24`; DWARF `uint8_t lCurrentCar`), widened for the
// compare. Kept as the u8 it is; the capacity is 8, so nothing hangs on the wrap.
// ===================================================================================
void ProgressionManager::UnlockDerivedCarCollection(const DerivedCarArray& lrDerivedCarArray)
{
    // The leading `lwz r11, 0x40(r27) / cmpwi -1` sentinel assert, owned by GetLength().
    const u32 luInitialLength = lrDerivedCarArray.GetLength();

    u32 luUnlocked     = 0;
    u32 luAlreadyOwned = 0;

    if (luInitialLength > 1)                                       // `cmplwi r11, 1 / ble`
    {
        for (u8 lu8Index = 1;
             static_cast<u32>(lu8Index) < lrDerivedCarArray.GetLength();
             ++lu8Index)
        {
            const CgsID lDerivedCarId = lrDerivedCarArray.GetItem(lu8Index);

            if (mProfile.FindCar(lDerivedCarId) != 0)
            {
                ++luAlreadyOwned;
                continue;
            }

            const BrnResource::VehicleListEntry::ELiveryType leLiveryType =
                lrDerivedCarArray.GetLiveryType(lu8Index);

            bool lbUnlock = true;                                  // `li r30, 1`
            if (leLiveryType == BrnResource::VehicleListEntry::E_LIVERY_TYPE_COLOUR_ALT)
            {
                lbUnlock = mProfile.GetGoldCarsUnlocked();         // Profile+42517
            }
            else if (leLiveryType == BrnResource::VehicleListEntry::E_LIVERY_TYPE_SILVER)
            {
                lbUnlock = mProfile.GetSilverCarsUnlocked();       // Profile+42516
            }

            if (!lbUnlock)
            {
                continue;
            }

            AddCar(lDerivedCarId, CarData::E_UNLOCK_TYPE_GIFT);    // li r5, 1
            ++luUnlocked;

            if (leLiveryType == BrnResource::VehicleListEntry::E_LIVERY_TYPE_COLOUR)   // cmpwi r31, 1
            {
                mProfile.SetCarUnlockAlreadyShown(lDerivedCarId);
            }
        }
    }

    if (CompletionWitnessEnabled())
    {
        ++giWitnessLinesPrinted;
        *CgsDev::Log::gpDebugPrint
            << "[completion] derived: base="
            << ((luInitialLength > 0) ? static_cast<u32>(lrDerivedCarArray.GetItem(0)) : 0u)
            << " entries="  << luInitialLength
            << " unlocked=" << luUnlocked
            << " owned="    << luAlreadyOwned << "\n";
    }
}

// ===================================================================================
// ProgressionManager::OnPowerParkResult  @ 0x8238AF78
//
// The player just parallel-parked. Record the rating (best-of, per "was anyone else parked
// beside me"), award the online parallel-park trophy, and offer the POWER_PARK training tip.
//
//   1. Profile::RecordPowerParkingRating(liResult, lbBetweenOtherPlayers) -- inlined by the
//      console (0x8238AF94..0x8238AFCC, the two `lbz/extsb/cmpw/stb` arms on Profile+114 and
//      Profile+113); de-inlined into the DWARF-named accessor, which now lives in BrnProfile.h.
//   2. ⭐ ONLY THE "BETWEEN OTHER PLAYERS" RESULT AWARDS A TROPHY. `cmplwi r10, 0 / beq` at
//      0x8238AFCC skips OnTrophyUnlock entirely for a solo park, and the type it passes is 32 ==
//      E_UNLOCKTYPE_NUM_PERCENTAGE_PARALLELPARK_ONLINE -- which is the ONLINE trophy, so the
//      flag's meaning is "there were other players' cars parked either side", not "online".
//   3. The training-tip request. The console open-codes TrainingManager::RequestTraining for
//      type 37 exactly as OnEventFinishUpdateProfile open-codes it for type 17: the
//      state==INACTIVE / !mbInPictureParadise / IsTipAllowedInGameMode / lpProfile assert
//      (BrnTrainingManager.cpp:382) / !HasPlayerSeenTrainingType / 5-second spacing
//      (`lfs 0x6C(profile) - lfs 0x18(tm) >= flt_8200426C == 5.0f`) gauntlet, then the tail store
//      pair `*(tm+4) = 37; *(tm+0) = 1`. Type 37 is not in the intro/licence family that skips
//      the spacing gate, which is why that arm is visible here and not at the type-17 site --
//      i.e. the inlining is RequestTraining specialised for 37, and it is reversed into the real
//      call, whose body (BrnTrainingManager.cpp:507) carries every one of those steps.
//
// ⛔ NO SEAT ON PC. GameStateModule::ProcessGameEvents @0x823A0A18 is this function's only
// console caller: the power-park arm reads the game event, calls OnPowerParkResult, then
// AchievementManagerBase::OnPowerParking, then posts PowerParkResultAction (game action 147,
// 8 B, `li r5, 0x93` @0x823A3EBC). That ARM DOES NOT EXIST in the tree -- the extracted
// ProcessGameEvents*BringUp family in GameStateModule_g*_00.cpp covers cases 27/33/35/36/79/80/
// 93/111/115 and nothing else -- so the body below is landed and unreached today.
// [FLAG PC bring-up] the missing seat is the E_EVENT_POWER_PARK_RESULT (BrnGameEvents.h == 54)
// arm of GameStateModule::ProcessGameEvents @0x823A0A18. DELETE-WHEN that arm is extracted; it
// is then a three-call arm (this, OnPowerParking, the action-147 post) with every callee bodied.
// ===================================================================================
void ProgressionManager::OnPowerParkResult(s32 liResult, bool lbBetweenOtherPlayers)
{
    mProfile.RecordPowerParkingRating(liResult, lbBetweenOtherPlayers);

    if (lbBetweenOtherPlayers)
    {
        OnTrophyUnlock(TrophyUnlockData::E_UNLOCKTYPE_NUM_PERCENTAGE_PARALLELPARK_ONLINE);  // li r4, 0x20
    }

    if (mpTrainingManager != 0)
    {
        mpTrainingManager->RequestTraining(E_TRAINING_TYPE_POWER_PARK);   // li r4, 0x25 == 37
    }
    else if (CgsDev::Log::gpDebugPrint != 0)
    {
        // Same hole OnEventFinishUpdateProfile's tip leg already reports: nothing in the mounted
        // set calls ProgressionManager::SetTrainingManager, so this back-pointer reads NULL.
        // DELETE-WHEN the outer ProgressionManager Construct/Prepare pair installs it.
        *CgsDev::Log::gpDebugPrint
            << "[FLAG PC bring-up] ProgressionManager::OnPowerParkResult: mpTrainingManager "
               "(X360 +133440) is NULL, so the POWER_PARK training tip was not requested.\n";
    }
}

// ===================================================================================
// [FLAG PC harness stimulus -- NOT IN THE X360 BINARY]
// ProgressionManager::DEBUG_HarnessSeedTrophyQueue
//
// Append ONE TrophyUnlockAction to mQueueOfTrophyCarUnLocks, once per run, behind
// BRN_PROGRESSION_COMPLETION_SEEDTROPHY=1 (default off, and flow_run.ps1 clears every BRN_*
// before a run, so it is off unless a case's DiagEnv asks for it).
//
// WHY IT EXISTS: SendTrophyUnlockUpdate above is the drain, and the only console producer is
// OnTrophyUnlock -> UnlockCarFromTrophy, which needs a COMPLETED TROPHY CATEGORY (all smashes,
// all drive-thrus, N medals ...). Nothing a 60-second harness scenario can do earns one, so
// without a stimulus tools/tests/cases/progression_completion.ps1 could not tell a working drain
// from a missing one -- and "the queue is empty" is exactly the state the bug also produces.
//
// WHAT IT IS NOT: it does not call AddCar, does not touch the profile, and awards no car. The
// only state it moves is the transient in-memory queue, which SendTrophyUnlockUpdate empties on
// the next frame. The record it builds is the one the two console asserts demand: a non-null
// CgsID and a non-NONE unlock type.
// DELETE-WHEN a harness scenario can complete a trophy category on its own.
// ===================================================================================
void ProgressionManager::DEBUG_HarnessSeedTrophyQueue()
{
    static const bool sbSeedEnabled = (getenv("BRN_PROGRESSION_COMPLETION_SEEDTROPHY") != 0);
    static bool       sbSeeded      = false;

    if (!sbSeedEnabled || sbSeeded)
    {
        return;
    }
    sbSeeded = true;

    GsmIO::TrophyUnlockAction lUnlock;
    lUnlock.mCarToUnlock = CgsIDCompress("CARBEAGT");
    lUnlock.meUnlockType = TrophyUnlockData::E_UNLOCKTYPE_NUM_MEDELS;
    mQueueOfTrophyCarUnLocks.Append(lUnlock);

    if (CgsDev::Log::gpDebugPrint != 0)
    {
        *CgsDev::Log::gpDebugPrint
            << "[completion] HARNESS STIMULUS (BRN_PROGRESSION_COMPLETION_SEEDTROPHY=1): one "
               "TrophyUnlockAction seeded onto mQueueOfTrophyCarUnLocks; no car was awarded.\n";
    }
}

}
