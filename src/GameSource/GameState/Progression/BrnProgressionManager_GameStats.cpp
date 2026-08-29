// ===================================================================================
// b5-decomp/src/GameSource/GameState/Progression/BrnProgressionManager_GameStats.cpp
//
// ⭐⭐⭐ [pause-stats wave 2026-08-29] THE PAUSE SCREEN'S STAT PANEL, PRODUCER SIDE.
// Two BrnProgression::ProgressionManager members whose DWARF home is
// GameSource/GameState/Progression/BrnProgressionManager.cpp:
//
//   GetGameStats            @ 0x8238A6A0   (566 instructions)
//   GetTotalWinsForNextRank @ 0x82370510   (50 instructions -- GetGameStats' only caller)
//
// Split into a per-function TU rather than grown into BrnProgressionManager.cpp for the same
// reason BrnProgressionManager_Completion.cpp / _Unlocks.cpp / _EventFinish.cpp were: this pair
// pulls the StreetManager, StuntManager and VehicleList closures that the base TU deliberately
// keeps out.
//
// THE CHAIN THIS COMPLETES (the middle hop of three; the outer two were already live):
//   GUI 435 (GuiEventStatsRequest, posted by CrashNavDriverDetails::UpdateInitSetup)
//     -> game event 79   [BridgeGuiToGameState case 435, already live]
//     -> game action 180 [GameStateModule::ProcessGameEventsGameStatsRequestBringUp, this wave]
//     -> GUI 436 (GuiEventStatsResponse) [TranslateGameActionsToGuiEvents case 180, this wave]
// Without it CrashNavDriverDetails::HandleStatData never runs and the Driver Details stat panel
// draws its labels with no numbers.
//
// ⛔ HEX-RAYS IS NOT USABLE ON 0x8238A6A0. Its output opens with "local variable allocation has
// failed, the output may be wrong!" and then:
//   * drops EVERY `fctiwz` -- it renders `stats.SetValue(DISTANCE_DRIVEN_ONLINE,
//     (s32)profile.mfDistanceDrivenOnline)` as a raw word copy `*(a2+24) = *(v8+100)`, i.e. it
//     turns a float->int CONVERT into a bit REINTERPRET. Five fields would have shipped as
//     nonsense bit patterns;
//   * invents eight parameters for ComputeCompletionPercentage (it takes none, and returns f32
//     in f1, which Hex-Rays also loses);
//   * renders GetTotalCarsToShutDown's plain r3 return as `HIDWORD(TotalCarsToShutDown)`;
//   * loses the `lhz`/`extsh` width on every stunt-grid read (they are s16 on both sides).
// Everything below is read off the ASSEMBLY.
// ===================================================================================

#include "GameSource/GameState/Progression/BrnProgressionManager.h"

#include "GameSource/GameState/SharedIO/BrnGameActionData.h"           // GameStateModuleIO::GameStats
#include "GameSource/GameState/Progression/BrnProfile.h"               // Profile / CarData / RivalData / ProfileEvent
#include "GameSource/GameState/Progression/BrnProgressionRivalData.h"  // RivalData
#include "GameSource/GameState/Progression/BrnProgressionCarData.h"    // CarData::UnlockType
#include "GameSource/GameState/Offences/BrnStuntManager.h"             // StuntManager totals
#include "GameSource/GameState/StreetData/BrnGameStateStreetManager.h" // StreetManager roads-ruled + StreetData
#include "SharedClasses/StreetData/BrnStreetData.h"                    // StreetData::GetRoadCount
#include "SharedClasses/Progression/BrnProgressionData.h"              // ProgressionData rank table
#include "SharedClasses/Progression/BrnProgressionRankData.h"          // ProgressionRankData medal threshold
#include "SharedClasses/DataLists/VehicleList.h"                       // VehicleList::GetVehicleIndex / GetVehicleData
#include "SharedClasses/DataLists/VehicleListEntry.h"                  // IsTrophyCar / GetUnlockRank / GetLiveryType
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"             // CgsDev::Log::gpDebugPrint

namespace BrnProgression
{

namespace GsmIO = BrnGameState::GameStateModuleIO;

namespace
{
    // The console's own baked assert locations (BeginAssert/FireAssert/EndAssert is called
    // directly rather than through CGS_ASSERT so the file/line stay the binary's, not this
    // TU's -- the same treatment BrnGameActionData.cpp's Construct uses).
    const char* const KAC_PROGRESSION_MANAGER_CPP =
        "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../GameState/Progression/BrnProgressionManager.cpp";
    const char* const KAC_STUNT_MANAGER_H =
        "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\gamestate\\offences\\BrnStuntManager.h";

    // The three livery-type tags a non-sponsor car must carry to count as COLLECTED
    // (X360 @0x8238A848: `lbz r11, 0xE9(vd)` then `cmplwi 1 / cmplwi 3 / cmplwi 4`).
    // FLAG: the tag VALUES are the asm's; no enum for BrnResource::ELiveryType is homed yet.
    const u8 KU8_LIVERY_TYPE_COUNTS_AS_COLLECTED_A = 1;
    const u8 KU8_LIVERY_TYPE_COUNTS_AS_COLLECTED_B = 3;
    const u8 KU8_LIVERY_TYPE_COUNTS_AS_COLLECTED_C = 4;
}

// ===================================================================================
// ProgressionManager::GetTotalWinsForNextRank  @ 0x82370510
//
// The medal threshold of the player's CURRENT rank -- what the GUI calls "wins to next rank".
// Two asserts, then one unsigned halfword read:
//     lbz r11, 0x2096C(this) / cmplwi 0x80 / blt  -> "mi8CurrentProgressionRank >= 0" :4156
//     ResourcePtr<ProgressionData>::operator->    -> lwz r11, 0x14(pd) == the rank count
//     cmplw rank, count / blt                      -> "luIndex < muProgressionRankCount"
//                                                     (BrnProgressionData.h:330)
//     r31 = *(pd + 0x10) + 112 * rank              -> the rank record; assert non-null :4159
//     lhz r3, 0x4C(r31)                            -> mu16MedalThresholdToNextRank, ZERO-extended
// ⚠️ The rank index is the RAW sign-extended cached byte (`extsb`), not the clamped
// GetProgressionRank() -- exactly like PlayerHasFinishedLastRank.
// ===================================================================================
u32 ProgressionManager::GetTotalWinsForNextRank()
{
    // `cmplwi r11, 0x80 / blt` -- an UNSIGNED compare of the raw byte against 0x80, i.e. "the
    // signed byte is not negative".
    CGS_ASSERT(mi8ProgressionRank >= 0, "mi8CurrentProgressionRank >= 0");   // cpp:4156

    const ProgressionData* lpProgressionData = GetProgressionData();
    if (lpProgressionData == 0)
    {
        // [FLAG PC bring-up] The console has no guard here -- its ResourcePtr::operator-> fires
        // the CgsResourcePtr.h:563 assert and then reads through the null anyway. Nothing in the
        // mounted set can reach this function before PROGRESSION.DAT is bound, but a null here
        // would be an access violation rather than a wrong number, so it is answered instead.
        return 0;
    }

    const s32 liRank = static_cast<s32>(mi8ProgressionRank);
    CGS_ASSERT(static_cast<u32>(liRank) < lpProgressionData->GetProgressionRankCount(),
               "luIndex < muProgressionRankCount");                         // BrnProgressionData.h:330

    const ProgressionRankData* lpProgressionRank =
        lpProgressionData->GetProgressionRankData(static_cast<u32>(liRank));
    CGS_ASSERT(lpProgressionRank != 0, "lpProgressionRank");                // cpp:4159
    if (lpProgressionRank == 0)
    {
        return 0;
    }

    // `lhz` with no `extsh` -- zero-extended, and the DWARF return type is uint32_t.
    // (GetPercentageOfEventsCompleted reads the SAME field with `extsh`; this caller does not.)
    return static_cast<u32>(lpProgressionRank->GetMedalThresholdToNextRank());
}

// ===================================================================================
// ProgressionManager::GetGameStats  @ 0x8238A6A0
//
// Fill one GameStats record. Statement order below is the console's own, so the asm reads
// top-to-bottom against it.
//
// ⚠️ FIVE FIELDS ARE FLOAT->INT CONVERTS, NOT COPIES (`lfs` + `fctiwz` + `stfiwx`):
// DISTANCE_DRIVEN_ONLINE/OFFLINE, TIME_PLAYED, BEST_DRIFT and BEST_ONCOMING. Two more stay
// float end to end (`stfs`): BEST_AIRTIME and BEST_SPIN. Hex-Rays shows all seven as plain
// word copies; that would have put raw IEEE bit patterns on screen as integers.
//
// ⚠️ FOUR INT SLOTS ARE NEVER WRITTEN BY THE CONSOLE and therefore keep Construct()'s zero:
// NUM_EVENT_MEDALS(9), TOTAL_EVENT_MEDALS(10), NUM_ROAD_RULE_MEDALS(11) and
// TOTAL_ROAD_RULE_MEDALS(12). The store list @0x8238A6A0 has no `stw` at +0x3C/+0x40/+0x44/
// +0x48. That is the binary's behaviour, not an omission here: the GUI's "all medals"/"road
// rule medals" readouts really do read zero on this build's console counterpart.
//
// [FLAG PC bring-up] THREE back-pointers the console dereferences unguarded are NULL on this
// build because nothing calls ProgressionManager::Construct/Prepare2 yet -- mpVehicleList,
// mpStreetManager and mpAchievementManager (the identical hole ComputeCompletionPercentage
// already documents and guards the same way). Each is guarded and logged ONCE here rather than
// dereferenced; the affected fields keep Construct()'s zero and SAY SO in the log, so a zero on
// screen is attributable rather than mysterious. DELETE-WHEN Construct/Prepare2 run for real.
// ===================================================================================
void ProgressionManager::GetGameStats(GsmIO::GameStats*                 lpGameStats,
                                      const BrnGameState::StuntManager* lpStuntManager,
                                      s32                               liNumChallengesCompleted)
{
    CGS_ASSERT(lpGameStats != 0, "lpGameStats");
    if (lpGameStats == 0)
    {
        return;
    }

    lpGameStats->Construct();

    // ---- 1) the profile scalars the console converts on the way in ----------------------
    lpGameStats->SetValue(GsmIO::GameStats::E_INT_VALUE_TYPE_DISTANCE_DRIVEN_ONLINE,
                          static_cast<s32>(mProfile.GetDistanceDrivenOnline()));   // lfs 0x64 / fctiwz
    lpGameStats->SetValue(GsmIO::GameStats::E_INT_VALUE_TYPE_DISTANCE_DRIVEN_OFFLINE,
                          static_cast<s32>(mProfile.GetDistanceDrivenOffline()));  // lfs 0x68 / fctiwz
    // `lfsx f0, r21, 0x1CD28` -- the REAL-time counter at Profile+118056, not mfInCarTimePlayed.
    lpGameStats->SetValue(GsmIO::GameStats::E_INT_VALUE_TYPE_TIME_PLAYED,
                          static_cast<s32>(mProfile.GetRealTimePlayed()));
    lpGameStats->SetValue(GsmIO::GameStats::E_INT_VALUE_TYPE_BEST_POWER_PARKING,
                          static_cast<s32>(mProfile.GetPowerParkingBestRating()));            // lbz/extsb 0x71
    lpGameStats->SetValue(GsmIO::GameStats::E_INT_VALUE_TYPE_BEST_POWER_PARKING_BETWEEN_OTHER_PLAYERS,
                          static_cast<s32>(mProfile.GetPowerParkingBetweenOtherPlayersBestRating())); // 0x72

    // ---- 2) CARS COLLECTED -- one pass over the profile's car list ----------------------
    // A car counts when EITHER
    //   (a) its CarData unlock type is E_UNLOCK_TYPE_SPONSOR (5) and the player's progression
    //       rank has reached the vehicle entry's required rank (`lbz 0x99`), OR
    //   (b) the entry is flagged a "trophy" car (`lwz 0x94 & 1`) AND its livery-type tag is
    //       one of 1 / 3 / 4 (`lbz 0xE9`).
    // The two asserts are the console's: the index-range one is the inlined
    // Profile::GetCarData's (BrnProfile.h:1923) and the two null ones are this function's own
    // (BrnProgressionManager.cpp:3475 / :3478).
    s32 liCarsCollected = 0;
    if (mpVehicleList != 0)
    {
        const s32 liCarCount = mProfile.GetCarCount();
        for (s32 liCarIndex = 0; liCarIndex < liCarCount; ++liCarIndex)
        {
            if (liCarIndex < 0 || liCarIndex >= mProfile.GetCarCount())
            {
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert("liCarIndex >= 0 && liCarIndex < miCarCount",
                                           "..\\..\\..\\GameSource\\GameState/Progression/BrnProfile.h", 1923);
                CgsDev::Assert::EndAssert();
            }

            const CarData* lpCarData = mProfile.GetCarData(liCarIndex);
            if (lpCarData == 0)
            {
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert("lpCarData", KAC_PROGRESSION_MANAGER_CPP, 3475);
                CgsDev::Assert::EndAssert();
                continue;   // [marked deviation] the console reads on through the null
            }

            const s32 liVehicleIndex = mpVehicleList->GetVehicleIndex(lpCarData->GetId());
            const BrnResource::VehicleListEntry* lpVehicleData =
                (liVehicleIndex < 0) ? 0 : mpVehicleList->GetVehicleData(liVehicleIndex);
            if (lpVehicleData == 0)
            {
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert("lpVehicleData", KAC_PROGRESSION_MANAGER_CPP, 3478);
                CgsDev::Assert::EndAssert();
                continue;   // [marked deviation] the console reads on through the null
            }

            if (lpCarData->GetUnlockType() == CarData::E_UNLOCK_TYPE_SPONSOR)
            {
                // `extsb r10, r3` on GetProgressionRank's return -- it is an s8 on the console.
                if (GetProgressionRank() >= static_cast<s32>(lpVehicleData->GetUnlockRank()))
                {
                    ++liCarsCollected;
                }
            }
            else if (lpVehicleData->IsTrophyCar())
            {
                const u8 lu8LiveryType = lpVehicleData->GetLiveryType();
                if (lu8LiveryType == KU8_LIVERY_TYPE_COUNTS_AS_COLLECTED_A ||
                    lu8LiveryType == KU8_LIVERY_TYPE_COUNTS_AS_COLLECTED_B ||
                    lu8LiveryType == KU8_LIVERY_TYPE_COUNTS_AS_COLLECTED_C)
                {
                    ++liCarsCollected;
                }
            }
        }
    }
    else if (CgsDev::Log::gpDebugPrint != 0)
    {
        *CgsDev::Log::gpDebugPrint
            << "[stats] mpVehicleList is NULL -- CARS_COLLECTED reported as 0 (PC bring-up)\n";
    }
    lpGameStats->SetValue(GsmIO::GameStats::E_INT_VALUE_TYPE_CARS_COLLECTED, liCarsCollected);

    // ---- 3) the three medal tallies ------------------------------------------------------
    // ⚠️ NAME TENSION, NOT A BUG: Profile::GetTotalWinCount's three out-params are DWARF-named
    // (rankWins, nonRankWins, specialEventWins) and the console stores them, in that order,
    // into MEDALS_GOLD / MEDALS_SILVER / MEDALS_BRONZE (`stw` +0x30/+0x34/+0x38 @0x8238A8CC).
    // The GUI then shows them as the gold/silver/bronze medal counts. Transcribed as the asm
    // has it; the naming disagreement is the original's.
    u32 luGold = 0;
    u32 luSilver = 0;
    u32 luBronze = 0;
    mProfile.GetTotalWinCount(luGold, luSilver, luBronze);
    lpGameStats->SetValue(GsmIO::GameStats::E_INT_VALUE_TYPE_MEDALS_GOLD,   static_cast<s32>(luGold));
    lpGameStats->SetValue(GsmIO::GameStats::E_INT_VALUE_TYPE_MEDALS_SILVER, static_cast<s32>(luSilver));
    lpGameStats->SetValue(GsmIO::GameStats::E_INT_VALUE_TYPE_MEDALS_BRONZE, static_cast<s32>(luBronze));

    // ---- 4) collected stunt elements (profile sets) and their world totals ---------------
    // The console inlines Set<CgsID,512>::GetLength three times, each behind its own
    // CgsSet.h:227 "Set used before Construct/Clear was called" sentinel check; that IS
    // Profile::GetStuntElementCount, which carries the same assert.
    lpGameStats->SetValue(GsmIO::GameStats::E_INT_VALUE_TYPE_JUMPS,
                          mProfile.GetStuntElementCount(BrnGameState::E_STUNT_ELEMENT_TYPE_JUMP));
    lpGameStats->SetValue(GsmIO::GameStats::E_INT_VALUE_TYPE_SMASHES,
                          mProfile.GetStuntElementCount(BrnGameState::E_STUNT_ELEMENT_TYPE_SMASH));
    lpGameStats->SetValue(GsmIO::GameStats::E_INT_VALUE_TYPE_STUNTS,
                          mProfile.GetStuntElementCount(BrnGameState::E_STUNT_ELEMENT_TYPE_BILLBOARD));

    // `lhz 0x5C4 / 0x5C6 / 0x5C8` + `extsh` -- the world totals, SIGNED halfwords. Note the
    // GameStats enumerator named STUNTS_MAX is the BILLBOARD total (the DWARF's "stunts" is
    // this build's billboards; the district columns below agree, and so does the GUI).
    if (lpStuntManager != 0)
    {
        lpGameStats->SetValue(GsmIO::GameStats::E_INT_VALUE_TYPE_JUMPS_MAX,
                              lpStuntManager->GetTotalStuntElementCount(BrnGameState::E_STUNT_ELEMENT_TYPE_JUMP));
        lpGameStats->SetValue(GsmIO::GameStats::E_INT_VALUE_TYPE_SMASHES_MAX,
                              lpStuntManager->GetTotalStuntElementCount(BrnGameState::E_STUNT_ELEMENT_TYPE_SMASH));
        lpGameStats->SetValue(GsmIO::GameStats::E_INT_VALUE_TYPE_STUNTS_MAX,
                              lpStuntManager->GetTotalStuntElementCount(BrnGameState::E_STUNT_ELEMENT_TYPE_BILLBOARD));
    }

    lpGameStats->SetValue(GsmIO::GameStats::E_INT_VALUE_TYPE_TAKEDOWNS,
                          mProfile.GetTotalTakedownCount());                       // lwz 0x198

    // ---- 5) ACHIEVEMENTS -- the population count of the manager's 64-bit earned mask -----
    // ⛔ THE ONE FIELD THIS WAVE DOES NOT PRODUCE, AND IT IS SAID OUT LOUD RATHER THAN LEFT
    // TO LOOK FINISHED. X360 @0x8238A9D4: `lwzx r11, this, 0x20938` (mpAchievementManager),
    // `ld r11, 0x18(r11)`, then the open-coded 64-bit SWAR popcount
    //     x -= (x >> 1) & 0x5555555555555555
    //     x  = ((x >> 2) & 0x3333333333333333) + (x & 0x3333333333333333)
    //     x  = ((x >> 4) + x) & 0x0F0F0F0F0F0F0F0F
    //     result = (x * 0x0101010101010101) >> 56
    // -> maIntValues[ACHIEVEMENTS]. TWO reasons it cannot be written here yet, neither of them
    // "it was hard":
    //   (a) +0x18 is the first 8-byte-aligned slot PAST AchievementManagerBase's 0x14-byte head
    //       (vtable + the four DWARF-attested back-pointers, :228..:231 -- that DWARF member
    //       list ENDS there), i.e. it belongs to the CONCRETE manager. The console's concrete
    //       manager is BrnGameState::AchievementManagerX360 (its AchievementEarnt @0x82367000 /
    //       IsAchievementEarnt @0x82367240 both bound the index at 0x32 == 50 achievements, so
    //       the mask is one u64), and that class does not exist in this tree at all -- only a
    //       placeholder AchievementManagerPS3 with no members.
    //   (b) mpAchievementManager is NULL on this build regardless: nothing calls
    //       ProgressionManager::Prepare2 (the same hole this class's own header FLAGs).
    // Reaching +0x18 through a cast off the base pointer would be exactly the offset-hack this
    // project's faithfulness gate exists to stop. The field therefore keeps Construct()'s zero
    // and logs why -- a zero on screen that is attributable, not mysterious.
    if (CgsDev::Log::gpDebugPrint != 0)
    {
        *CgsDev::Log::gpDebugPrint
            << "[stats] ACHIEVEMENTS reported as 0 -- mpAchievementManager "
            << ((mpAchievementManager != 0) ? "present but AchievementManagerX360 is unreconstructed"
                                            : "is NULL (PC bring-up)")
            << "\n";
    }

    // ---- 6) the two 3x5 stunt-element district grids -------------------------------------
    // MAX comes from the StuntManager's authored world grid (+0x5CA, s16); CURRENT comes from
    // the profile's own per-county tally (+96440, s16). Both are read `lhz`+`extsh`.
    // The two asserts inside the county loop are the inlined
    // StuntManager::GetMaxStuntElementCountByCounty's own pair (BrnStuntManager.h:284/285).
    for (s32 liStuntElementType = 0;
         liStuntElementType < GsmIO::GameStats::KI_STUNT_ELEMENT_TYPE_COUNT;
         ++liStuntElementType)
    {
        for (s32 liCounty = 0; liCounty < GsmIO::GameStats::KI_COUNTY_VALID_COUNT; ++liCounty)
        {
            if (liStuntElementType == BrnGameState::E_STUNT_ELEMENT_TYPE_COUNT)
            {
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert("leType != E_STUNT_ELEMENT_TYPE_COUNT",
                                           KAC_STUNT_MANAGER_H, 284);
                CgsDev::Assert::EndAssert();
            }
            if (liCounty >= BrnWorld::E_COUNTY_VALID_COUNT)
            {
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert("leCounty < BrnWorld::E_COUNTY_VALID_COUNT",
                                           KAC_STUNT_MANAGER_H, 285);
                CgsDev::Assert::EndAssert();
            }

            if (lpStuntManager != 0)
            {
                lpGameStats->SetMaxStuntElementPerCounty(
                    liStuntElementType, liCounty,
                    lpStuntManager->GetTotalStuntElementCountByCounty(
                        static_cast<BrnGameState::StuntElementType>(liStuntElementType), liCounty));
            }

            lpGameStats->SetCurrentStuntElementPerCounty(
                liStuntElementType, liCounty,
                mProfile.GetStuntElementCountByCounty(
                    static_cast<BrnGameState::StuntElementType>(liStuntElementType),
                    static_cast<BrnWorld::ECounty>(liCounty)));
        }
    }

    // ---- 7) the 13 takedown-type tallies -------------------------------------------------
    // One clean 13-word copy from Profile+416 into GameStats+0xA8. The two asserts are the
    // inlined Profile::GetTakedownTypeCount (BrnProfile.h:2167) and
    // GameStats::SetTakedownTypeCount (BrnGameActionData.h:454) range checks.
    for (s32 liTakedownType = 0;
         liTakedownType < GsmIO::GameStats::KI_TAKEDOWN_TYPE_COUNT;
         ++liTakedownType)
    {
        lpGameStats->SetTakedownTypeCount(liTakedownType,
                                          mProfile.GetTakedownTypeCount(liTakedownType));
    }

    // ---- 8) roads ruled + the world's road count ------------------------------------------
    if (mpStreetManager != 0)
    {
        lpGameStats->SetRoadsRuledCount(
            0, mpStreetManager->GetNumberOfParTimeTrialRoadsRuledByLocalPlayer());   // +0xDC
        lpGameStats->SetRoadsRuledCount(
            1, mpStreetManager->GetNumberOfParShowTimeRoadsRuledByLocalPlayer());    // +0xE0
        lpGameStats->SetValue(GsmIO::GameStats::E_INT_VALUE_TYPE_TOTALROADSRULED,
                              mpStreetManager->GetNumberOfCompleteRoadsRuledByLocalPlayer());  // +0x90

        // `lwz r11, 0x1CC8(streetMgr)` (the ResourcePtr's main-memory word, behind the
        // CgsResourcePtr.h:563 assert) then `lwz r11, 0x20(r11)` == StreetData::miRoadCount.
        const BrnStreetData::StreetData* lpStreetData = mpStreetManager->GetStreetData();
        CGS_ASSERT(lpStreetData != 0,
                   "Can not instance resource pointer - it has no main memory resource");
        if (lpStreetData != 0)
        {
            lpGameStats->SetTotalRoads(lpStreetData->GetRoadCount());
        }
    }
    else
    {
        // ⛔⛔ [FLAG PC bring-up] THE ONE FIELD Construct() DOES NOT ZERO, SO THE GUARD MUST.
        // miTotalRoads is the single member the console's GameStats::Construct never stores to
        // (see its NOTE) -- because the console always overwrites it here, from the StreetData.
        // With mpStreetManager NULL the record is a STACK LOCAL whose miTotalRoads is whatever
        // was on the caller's frame: the first measured run printed "roads 0/32758" on the pause
        // screen. A garbage denominator that renders is worse than a blank one, so the guard
        // stores the zero the console's own path would have produced from an empty world.
        // This is the ONLY place this wave departs from "Construct's value survives".
        lpGameStats->SetTotalRoads(0);

        if (CgsDev::Log::gpDebugPrint != 0)
        {
            *CgsDev::Log::gpDebugPrint
                << "[stats] mpStreetManager is NULL -- roads-ruled reported as 0 and total-roads"
                   " FORCED to 0 (Construct does not zero it; PC bring-up)\n";
        }
    }

    // The console re-zeroes the favourite/forgotten car ids here (`std r20, 0(r28)` /
    // `std r20, 8(r28)` with r20 == 0) even though Construct already did -- neither has a
    // producer in this function. Reproduced so the record is not left looking under-written.
    lpGameStats->SetValue(GsmIO::GameStats::E_ID_VALUE_TYPE_FAVOURITE_CAR, static_cast<CgsID>(0));
    lpGameStats->SetValue(GsmIO::GameStats::E_ID_VALUE_TYPE_FORGOTTEN_CAR, static_cast<CgsID>(0));

    // ---- 9) the NEMESIS -- the rival who has taken the player down most --------------------
    // `r7 = profile + 0x6280` walked with `addi r7, r7, 0x38`; the compared field is
    // RivalData+0x18 == miTakedownFromCount and the kept value is the record's leading CgsID.
    // Strictly greater-than, so the FIRST rival at the maximum wins.
    CgsID lNemesisId       = 0;
    s32   liNemesisTakedowns = 0;
    const s32 liRivalCount = mProfile.GetRivalCount();
    for (s32 liRivalIndex = 0; liRivalIndex < liRivalCount; ++liRivalIndex)
    {
        const RivalData* lpRival = mProfile.GetRivalData(liRivalIndex);
        if (lpRival->miTakedownFromCount > liNemesisTakedowns)
        {
            lNemesisId         = lpRival->mRivalId;
            liNemesisTakedowns = lpRival->miTakedownFromCount;
        }
    }
    lpGameStats->SetValue(GsmIO::GameStats::E_ID_VALUE_TYPE_NEMESIS, lNemesisId);

    // ---- 10) the personal-best block ------------------------------------------------------
    lpGameStats->SetValue(GsmIO::GameStats::E_INT_VALUE_TYPE_BEST_NO_BARREL_ROLLS,
                          mProfile.GetCompletedBarrelRolls());                       // lwz 0x24C
    lpGameStats->SetValue(GsmIO::GameStats::E_FLOAT_VALUE_TYPE_BEST_SPIN,
                          mProfile.GetCompletedAirSpinAngle());                      // lfs 0x250 -> stfs
    lpGameStats->SetValue(GsmIO::GameStats::E_INT_VALUE_TYPE_BEST_DRIFT,
                          static_cast<s32>(mProfile.GetCompletedDriftDistance()));   // lfs 0x258 / fctiwz
    lpGameStats->SetValue(GsmIO::GameStats::E_INT_VALUE_TYPE_BEST_BOOST_CHAIN,
                          static_cast<s32>(mProfile.GetBestNewBurnoutChainScore())); // lwz 0x74
    lpGameStats->SetValue(GsmIO::GameStats::E_INT_VALUE_TYPE_BEST_ONCOMING,
                          static_cast<s32>(mProfile.GetOncomingDistance()));         // lfs 0x25C / fctiwz
    lpGameStats->SetValue(GsmIO::GameStats::E_FLOAT_VALUE_TYPE_BEST_AIRTIME,
                          mProfile.GetAirMaximum());                                 // lfs 0x260 -> stfs
    lpGameStats->SetValue(GsmIO::GameStats::E_INT_VALUE_TYPE_BEST_SHOWTIME,
                          mProfile.GetNewHighShowtimeScore());                       // lwz 0x264

    // ---- 11) wins to next rank -------------------------------------------------------------
    // `extsb(mi8ProgressionRank) == ProgressionData::GetProgressionRankCount()` is exactly
    // PlayerHasFinishedLastRank @0x82370180, inlined; -1 is the "no next rank" sentinel the
    // GUI's licence card already understands.
    if (PlayerHasFinishedLastRank())
    {
        lpGameStats->SetValue(GsmIO::GameStats::E_INT_VALUE_TYPE_TOTAL_WINS_FOR_NEXT_RANK, -1);
    }
    else
    {
        lpGameStats->SetValue(GsmIO::GameStats::E_INT_VALUE_TYPE_TOTAL_WINS_FOR_NEXT_RANK,
                              static_cast<s32>(GetTotalWinsForNextRank()));
    }

    lpGameStats->SetValue(GsmIO::GameStats::E_INT_VALUE_TYPE_TOTAL_CARS_TO_SHUTDOWN,
                          mProfile.GetTotalCarsToShutDown());
    // `stfs f1, 0xA4(r28)` -- the f32 return goes into the record as a FLOAT, and the GUI
    // bridge is what converts it to an integer percentage (`fctiwz`, case 180).
    lpGameStats->SetValue(GsmIO::GameStats::E_FLOAT_VALUE_PERCENTAGE_COMPLETE,
                          ComputeCompletionPercentage());

    // ---- 12) events found / total, and the stunt-run high score -----------------------------
    // The console walks maEvents' flag halfword directly (`r10 = profile + 0x7084`, `lhz`,
    // `& 1`, stride 8) -- that is ProfileEvent::IsFound().
    s32       liEventsFound = 0;
    const s32 liEventCount  = static_cast<s32>(mProfile.GetEventCount());
    for (s32 liEventIndex = 0; liEventIndex < liEventCount; ++liEventIndex)
    {
        const ProfileEvent* lpEvent = mProfile.GetEvent(static_cast<u32>(liEventIndex));
        if (lpEvent != 0 && lpEvent->IsFound())
        {
            ++liEventsFound;
        }
    }
    lpGameStats->SetValue(GsmIO::GameStats::E_INT_VALUE_TYPE_EVENTS_FOUND, liEventsFound);

    // The console emits each of these two stores TWICE (@0x8238AF50/+0x8238AF60 and
    // @0x8238AF58/0x8238AF6C) -- a repeated-expression artefact, not two different values.
    lpGameStats->SetValue(GsmIO::GameStats::E_INT_VALUE_TYPE_EVENTS_TOTAL, liEventCount);
    lpGameStats->SetValue(GsmIO::GameStats::E_INT_VALUE_TYPE_HIGHEST_STUNT_SCORE,
                          mProfile.GetBestStuntRunScore());

    // ---- 13) the X360-only third parameter --------------------------------------------------
    // `stw r10, 0x98(r28)` -- maIntValues[32], the count
    // ChallengeManager::CountCompletedChallenges handed the caller.
    lpGameStats->SetValue(GsmIO::GameStats::E_INT_VALUE_TYPE_FREEBURN_CHALLENGES_COMPLETE,
                          liNumChallengesCompleted);
}

} // namespace BrnProgression
