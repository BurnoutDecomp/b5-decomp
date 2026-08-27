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

}
