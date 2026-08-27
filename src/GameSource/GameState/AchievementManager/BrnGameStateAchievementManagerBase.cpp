// ============================================================================
// b5-decomp/src/GameSource/GameState/AchievementManager/BrnGameStateAchievementManagerBase.cpp
// ============================================================================
// Reconstruction of the platform-neutral achievement-tracking base
// (BrnGameState::AchievementManagerBase). The 15 X360-attested bodies owned by
// this TU watch gameplay events and, for each, fire the achievement through the
// two protected virtuals when its threshold is first met:
//
//   IsAchievementEarnt(id)  == X360 vtable slot 1 ( (*(*this + 4))(this, id) )
//   AchievementEarnt(id)    == X360 vtable slot 0 ( (**this)(this, id)       )
//
// The achievement ids are referenced through the X360-asm-attested
// E_X360_ACHIEVEMENT_* constants below: the X360 SKU numbers its achievements
// DIFFERENTLY from the PS3 DWARF EAchievement enum (e.g. OnFlatSpin fires id 9,
// where the PS3 enum names id 9 "5X_MULTIPLIER_IN_SHOWTIME"), so the raw ids the
// X360 passes cannot be spelled with the PS3 enumerator names. These constants
// keep every id a NAMED value (no bare integer literals in the bodies) while
// preserving the X360-attested numbering. They are file-local to this TU.
// ----------------------------------------------------------------------------

#include "GameSource/GameState/AchievementManager/BrnGameStateAchievementManagerBase.h"

#include "GameSource/GameState/Progression/BrnProgressionManager.h" // mpProgressionManager accessors
#include "GameSource/GameState/ModeManager/Scoring/BrnScoringSystem.h" // mpScoringSystem accessors / takedown query
#include "GameSource/GameState/BrnGameStateModule.h"                 // mpGameStateModule accessors
#include "GameSource/BurnoutConstants.h"                             // EActiveRaceCarIndex

namespace BrnGameState
{

namespace
{
    // ---- X360-attested achievement ids (raw integers from the X360 .text). ----
    // FLAG: X360-SKU numbering; intentionally NOT the PS3 EAchievement names/values.
    const EAchievement E_X360_ACHIEVEMENT_REPAIR_FIRST_WRECKED_CAR = static_cast<EAchievement>(0);  // OnBodyShop
    const EAchievement E_X360_ACHIEVEMENT_ROAD_RULE_TIME_WATT_ST   = static_cast<EAchievement>(1);  // OnSetRoadRule (TIME)
    const EAchievement E_X360_ACHIEVEMENT_ROAD_RULE_CRASH_CRAWFORD = static_cast<EAchievement>(2);  // OnSetRoadRule (CRASH)
    const EAchievement E_X360_ACHIEVEMENT_WIN_FIRST_RACE           = static_cast<EAchievement>(3);  // OnEventWin RACE
    const EAchievement E_X360_ACHIEVEMENT_COLLECT_5_STUNTS         = static_cast<EAchievement>(4);  // OnCollectStunt BILLBOARD
    const EAchievement E_X360_ACHIEVEMENT_COLLECT_25_SMASHES       = static_cast<EAchievement>(5);  // OnCollectStunt SMASH
    const EAchievement E_X360_ACHIEVEMENT_PERFECT_ROAD_RAGE        = static_cast<EAchievement>(7);  // OnTakedown
    const EAchievement E_X360_ACHIEVEMENT_SHUTDOWN_ROADRAGE_VAN    = static_cast<EAchievement>(8);  // OnBodyShop
    const EAchievement E_X360_ACHIEVEMENT_FLATSPIN                 = static_cast<EAchievement>(9);  // OnFlatSpin
    const EAchievement E_X360_ACHIEVEMENT_COLLECT_5_JUMPS          = static_cast<EAchievement>(11); // OnCollectStunt JUMP
    const EAchievement E_X360_ACHIEVEMENT_WIN_MARKED_MAN_NO_TAKEDOWN= static_cast<EAchievement>(12); // OnEventWin MARKED_MAN
    const EAchievement E_X360_ACHIEVEMENT_POWER_PARK               = static_cast<EAchievement>(16); // OnPowerParking
    const EAchievement E_X360_ACHIEVEMENT_BARREL_ROLL              = static_cast<EAchievement>(17); // OnBarrelRoll
    const EAchievement E_X360_ACHIEVEMENT_MILLIONAIRES_CLUB        = static_cast<EAchievement>(20); // OnEventWin STUNT_ATTACK
    const EAchievement E_X360_ACHIEVEMENT_WIN_10_XS_CARS           = static_cast<EAchievement>(21); // OnEventWin MARKED_MAN (>=25)
    const EAchievement E_X360_ACHIEVEMENT_GET_500_TAKEDOWNS        = static_cast<EAchievement>(22); // OnTakedown
    const EAchievement E_X360_ACHIEVEMENT_WIN_25_XS_CARS           = static_cast<EAchievement>(24); // OnEventWin MARKED_MAN (>=35)
    const EAchievement E_X360_ACHIEVEMENT_FIND_ALL_CARPARKS        = static_cast<EAchievement>(26); // OnFindAllCarParks
    const EAchievement E_X360_ACHIEVEMENT_COLLECT_ALL_BILLBOARDS   = static_cast<EAchievement>(27); // OnCollectAllStunts BILLBOARD
    const EAchievement E_X360_ACHIEVEMENT_COLLECT_ALL_SMASHES      = static_cast<EAchievement>(28); // OnCollectAllStunts SMASH
    const EAchievement E_X360_ACHIEVEMENT_COLLECT_ALL_JUMPS        = static_cast<EAchievement>(29); // OnCollectAllStunts JUMP
    const EAchievement E_X360_ACHIEVEMENT_SET_ALL_ROAD_RULE_TIME   = static_cast<EAchievement>(30); // OnSetAllRoadRules (TIME)
    const EAchievement E_X360_ACHIEVEMENT_SET_ALL_ROAD_RULE_CRASH  = static_cast<EAchievement>(31); // OnSetAllRoadRules (CRASH)
    const EAchievement E_X360_ACHIEVEMENT_FIND_ALL_DRIVE_THRUS     = static_cast<EAchievement>(32); // OnFindAllDriveThrus
    const EAchievement E_X360_ACHIEVEMENT_COMPLETE_GAME            = static_cast<EAchievement>(34); // OnGameCompletion

    // OnSetRoadRule street ids (X360 64-bit CgsID literals).
    const CgsID KU_ROAD_ID_TIME_WATT_ST   = 0x5E2C9u;   // 385737  (score type TIME / 0)
    const CgsID KU_ROAD_ID_CRASH_CRAWFORD = 0x61121u;   // 397601  (score type CRASH / 1)

    // Thresholds (X360-attested compare immediates).
    const s32 KI_BARREL_ROLLS_FOR_ACHIEVEMENT  = 2;
    const f32 KF_FLATSPIN_DEGREES              = 360.0f;
    const s32 KI_POWER_PARK_CARS               = 100;
    const s32 KI_COLLECT_25                    = 25;
    const s32 KI_COLLECT_5                     = 5;
    const s32 KI_MARKED_MAN_WINS_FOR_10_CARS   = 25;
    const s32 KI_MARKED_MAN_WINS_FOR_25_CARS   = 35;
    const s32 KI_MILLIONAIRE_SCORE             = 1000000;
    const s32 KI_PERFECT_RAGE_TAKEDOWNS        = 10;
    const s32 KI_GET_500_TAKEDOWNS             = 500;
}

// ----------------------------------------------------------------------------
// Construct  (X360 0x8235A948)
//   Stores the four manager back-pointers, each guarded non-null.
// ----------------------------------------------------------------------------
void AchievementManagerBase::Construct(BrnProgression::ProgressionManager* lpProgressionManager,
                                       StreetManager* lpStreetManager,
                                       ScoringSystem* lpScoringSystem,
                                       GameStateModule* lpGameStateModule)
{
    CGS_ASSERT(lpProgressionManager, "lpProgressionManager");
    mpProgressionManager = lpProgressionManager;

    CGS_ASSERT(lpStreetManager, "lpStreetManager");
    mpStreetManager = lpStreetManager;

    CGS_ASSERT(lpScoringSystem, "lpScoringSystem");
    mpScoringSystem = lpScoringSystem;

    CGS_ASSERT(lpGameStateModule, "lpGameStateModule");
    mpGameStateModule = lpGameStateModule;
}

// ----------------------------------------------------------------------------
// OnBodyShop  (X360 0x8235AA18)  -- MOVED, not deleted.
//   Its body now lives in the sibling BrnGameStateAchievementManagerBase_DriveThru.cpp,
//   together with OnFindAllDriveThrus / OnFindAllCarParks / OnFindAllEvents, because the
//   mounted drive-thru chain link-requires those four while this TU as a whole still cannot
//   be mounted (six of its other hooks' ScoringSystem / ProgressionManager callees are
//   bodiless -- see that file's banner and the note in tools/build/build_game_exe.bat).
//   ⭐ DELETE-WHEN those land: fold the split file back in here.
// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
// OnSetRoadRule  (X360 0x8235AC08)
//   On setting a TIME road rule on Watt St, or a CRASH road rule on East
//   Crawford Drive, fire the matching achievement.
// ----------------------------------------------------------------------------
void AchievementManagerBase::OnSetRoadRule(BrnStreetData::ScoreType leScoreType, CgsID lRoadId)
{
    if (leScoreType == BrnStreetData::E_SCORE_TYPE_TIME)
    {
        if (!IsAchievementEarnt(E_X360_ACHIEVEMENT_ROAD_RULE_TIME_WATT_ST)
            && lRoadId == KU_ROAD_ID_TIME_WATT_ST)
        {
            AchievementEarnt(E_X360_ACHIEVEMENT_ROAD_RULE_TIME_WATT_ST);
        }
    }
    else if (leScoreType == BrnStreetData::E_SCORE_TYPE_CRASH)
    {
        if (!IsAchievementEarnt(E_X360_ACHIEVEMENT_ROAD_RULE_CRASH_CRAWFORD)
            && lRoadId == KU_ROAD_ID_CRASH_CRAWFORD)
        {
            AchievementEarnt(E_X360_ACHIEVEMENT_ROAD_RULE_CRASH_CRAWFORD);
        }
    }
    else
    {
        CGS_ASSERT(false, "dodgy road rule type!");
    }
}

// ----------------------------------------------------------------------------
// OnSetAllRoadRules  (X360 0x8235ACF8)
//   On completing every TIME (or every CRASH) road rule, fire the matching
//   set-all achievement.
// ----------------------------------------------------------------------------
void AchievementManagerBase::OnSetAllRoadRules(BrnStreetData::ScoreType leScoreType)
{
    if (leScoreType == BrnStreetData::E_SCORE_TYPE_TIME)
    {
        if (!IsAchievementEarnt(E_X360_ACHIEVEMENT_SET_ALL_ROAD_RULE_TIME))
        {
            AchievementEarnt(E_X360_ACHIEVEMENT_SET_ALL_ROAD_RULE_TIME);
        }
    }
    else if (leScoreType == BrnStreetData::E_SCORE_TYPE_CRASH)
    {
        if (!IsAchievementEarnt(E_X360_ACHIEVEMENT_SET_ALL_ROAD_RULE_CRASH))
        {
            AchievementEarnt(E_X360_ACHIEVEMENT_SET_ALL_ROAD_RULE_CRASH);
        }
    }
    else
    {
        CGS_ASSERT(false, "dodgy road rule type!");
    }
}

// ----------------------------------------------------------------------------
// OnTakedown  (X360 0x8235AAE0)
//   - GET_500_TAKEDOWNS: lifetime profile takedown tally >= 500.
//   - PERFECT_ROAD_RAGE: in ROAD_RAGE, the player has >= 10 mode takedowns and
//     zero mode crashes.
// ----------------------------------------------------------------------------
void AchievementManagerBase::OnTakedown()
{
    CGS_ASSERT(mpProgressionManager, "mpProgressionManager");
    // The X360 takes the address of the embedded Profile (mpProgressionManager + 0x170)
    // and asserts it non-null ("lpProfile"). With a non-null mpProgressionManager that
    // interior address is always non-null; the assert is preserved for parity below.
    CGS_ASSERT(mpProgressionManager != nullptr, "lpProfile");

    if (!IsAchievementEarnt(E_X360_ACHIEVEMENT_GET_500_TAKEDOWNS)
        && mpProgressionManager->GetProfileTotalTakedowns() >= KI_GET_500_TAKEDOWNS)
    {
        AchievementEarnt(E_X360_ACHIEVEMENT_GET_500_TAKEDOWNS);
    }

    if (!IsAchievementEarnt(E_X360_ACHIEVEMENT_PERFECT_ROAD_RAGE)
        && mpGameStateModule->GetCurrentGameModeType() == GameStateModuleIO::E_MODE_ROAD_RAGE
        && mpScoringSystem->GetPlayerModeTakedowns() >= KI_PERFECT_RAGE_TAKEDOWNS
        && mpScoringSystem->GetPlayerModeCrashes() == 0)
    {
        AchievementEarnt(E_X360_ACHIEVEMENT_PERFECT_ROAD_RAGE);
    }
}

// ----------------------------------------------------------------------------
// OnEventWin  (X360 0x82372978)
//   Dispatch on the won mode type. RACE win, MARKED_MAN car-challenge wins,
//   STUNT_ATTACK millionaire, and FACE_OFF win-without-takedown-against checks.
// ----------------------------------------------------------------------------
void AchievementManagerBase::OnEventWin(GameStateModuleIO::EGameModeType leGameMode)
{
    switch (leGameMode)
    {
    case GameStateModuleIO::E_MODE_OFFLINE_RACE:
        if (!IsAchievementEarnt(E_X360_ACHIEVEMENT_WIN_FIRST_RACE))
        {
            AchievementEarnt(E_X360_ACHIEVEMENT_WIN_FIRST_RACE);
        }
        break;

    case GameStateModuleIO::E_MODE_ROAD_RAGE:
        // (X360 case 3 falls straight through to the function exit -- no achievement.)
        break;

    case GameStateModuleIO::E_MODE_BURNING_ROUTE:
        // X360 case 5: car-challenge wins drive the win-10 / win-25 XS-car achievements.
        if (!IsAchievementEarnt(E_X360_ACHIEVEMENT_WIN_10_XS_CARS)
            && mpProgressionManager->GetCarChallengeWinCount() >= KI_MARKED_MAN_WINS_FOR_10_CARS)
        {
            AchievementEarnt(E_X360_ACHIEVEMENT_WIN_10_XS_CARS);
        }
        if (!IsAchievementEarnt(E_X360_ACHIEVEMENT_WIN_25_XS_CARS)
            && mpProgressionManager->GetCarChallengeWinCount() >= KI_MARKED_MAN_WINS_FOR_25_CARS)
        {
            AchievementEarnt(E_X360_ACHIEVEMENT_WIN_25_XS_CARS);
        }
        break;

    case GameStateModuleIO::E_MODE_STUNT_ATTACK:
        // X360 case 7: millionaires-club -- player score >= 1,000,000 (online or offline block).
        if (!IsAchievementEarnt(E_X360_ACHIEVEMENT_MILLIONAIRES_CLUB))
        {
            CGS_ASSERT(mpScoringSystem, "mpScoringSystem");
            CGS_ASSERT(mpGameStateModule, "mpGameStateModule");

            const bool lbOnline = mpGameStateModule->IsOnlineGameMode();
            if (mpScoringSystem->GetPlayerScore(lbOnline) >= KI_MILLIONAIRE_SCORE)
            {
                AchievementEarnt(E_X360_ACHIEVEMENT_MILLIONAIRES_CLUB);
            }
        }
        break;

    case GameStateModuleIO::E_MODE_MARKED_MAN:
        // X360 case 8: win marked-man with zero takedowns against the player.
        if (!IsAchievementEarnt(E_X360_ACHIEVEMENT_WIN_MARKED_MAN_NO_TAKEDOWN))
        {
            const EActiveRaceCarIndex lePlayerCar = mpGameStateModule->GetPlayerActiveRaceCarIndex();
            if (mpScoringSystem->GetNumberOfTakedownsAgainst(lePlayerCar) == 0)
            {
                AchievementEarnt(E_X360_ACHIEVEMENT_WIN_MARKED_MAN_NO_TAKEDOWN);
            }
        }
        break;

    default:
        CGS_ASSERT(false, " invalid game mode type ");
        break;
    }
}

// ----------------------------------------------------------------------------
// OnLicenseUpgrade  (X360 0x8235ADC8)
//   Map the new license rank (1..6) to its achievement via KAE_LICENSE_ACHIEVEMENTS
//   and fire it the first time.
// ----------------------------------------------------------------------------
void AchievementManagerBase::OnLicenseUpgrade(u8 luNewLicenseRank)
{
    CGS_ASSERT(luNewLicenseRank > 0 && luNewLicenseRank <= 6,
               "liNewLicenseRank > 0 && liNewLicenseRank <= 6");

    const EAchievement leAchievement = KAE_LICENSE_ACHIEVEMENTS[luNewLicenseRank - 1];
    if (!IsAchievementEarnt(leAchievement))
    {
        AchievementEarnt(leAchievement);
    }
}

// ----------------------------------------------------------------------------
// OnCollectStunt  (X360 0x82366EB8)
//   Per-stunt-type collected-element counts cross their thresholds:
//     JUMP     -> COLLECT_5_JUMPS    when count >= 5
//     SMASH    -> COLLECT_25_SMASHES when count >= 25
//     BILLBOARD-> COLLECT_5_STUNTS   when count >= 5
// ----------------------------------------------------------------------------
void AchievementManagerBase::OnCollectStunt(StuntElementType leStuntType)
{
    const s32 liCount = mpProgressionManager->GetCollectedStuntElementCount(leStuntType);

    switch (leStuntType)
    {
    case E_STUNT_ELEMENT_TYPE_JUMP:
        if (!IsAchievementEarnt(E_X360_ACHIEVEMENT_COLLECT_5_JUMPS) && liCount >= KI_COLLECT_5)
        {
            AchievementEarnt(E_X360_ACHIEVEMENT_COLLECT_5_JUMPS);
        }
        break;

    case E_STUNT_ELEMENT_TYPE_SMASH:
        if (!IsAchievementEarnt(E_X360_ACHIEVEMENT_COLLECT_25_SMASHES) && liCount >= KI_COLLECT_25)
        {
            AchievementEarnt(E_X360_ACHIEVEMENT_COLLECT_25_SMASHES);
        }
        break;

    case E_STUNT_ELEMENT_TYPE_BILLBOARD:
        if (!IsAchievementEarnt(E_X360_ACHIEVEMENT_COLLECT_5_STUNTS) && liCount >= KI_COLLECT_5)
        {
            AchievementEarnt(E_X360_ACHIEVEMENT_COLLECT_5_STUNTS);
        }
        break;

    default:
        CGS_ASSERT(false, "dodgy stunt type!");
        break;
    }
}

// ----------------------------------------------------------------------------
// OnFindAllDriveThrus (X360 0x8235AE78) and OnFindAllCarParks (X360 0x8235AED8)
//   MOVED, not deleted -- see the OnBodyShop note above. Both bodies now live in
//   BrnGameStateAchievementManagerBase_DriveThru.cpp beside the new OnFindAllEvents.
// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
// OnCollectAllStunts  (X360 0x8235AF38)
//   On collecting every element of a stunt type, fire the matching all-collected
//   achievement (JUMP -> 29, SMASH -> 28, BILLBOARD -> 27).
// ----------------------------------------------------------------------------
void AchievementManagerBase::OnCollectAllStunts(StuntElementType leStuntType)
{
    if (leStuntType == E_STUNT_ELEMENT_TYPE_JUMP)
    {
        if (!IsAchievementEarnt(E_X360_ACHIEVEMENT_COLLECT_ALL_JUMPS))
        {
            AchievementEarnt(E_X360_ACHIEVEMENT_COLLECT_ALL_JUMPS);
        }
    }
    else if (leStuntType == E_STUNT_ELEMENT_TYPE_SMASH)
    {
        if (!IsAchievementEarnt(E_X360_ACHIEVEMENT_COLLECT_ALL_SMASHES))
        {
            AchievementEarnt(E_X360_ACHIEVEMENT_COLLECT_ALL_SMASHES);
        }
    }
    else if (leStuntType >= E_STUNT_ELEMENT_TYPE_COUNT)
    {
        CGS_ASSERT(false, "dodgy stunt type!");
    }
    else // E_STUNT_ELEMENT_TYPE_BILLBOARD
    {
        if (!IsAchievementEarnt(E_X360_ACHIEVEMENT_COLLECT_ALL_BILLBOARDS))
        {
            AchievementEarnt(E_X360_ACHIEVEMENT_COLLECT_ALL_BILLBOARDS);
        }
    }
}

// ----------------------------------------------------------------------------
// OnBarrelRoll  (X360 0x8235B040)
//   Fire the barrel-roll achievement when a single jump performs >= 2 rolls.
// ----------------------------------------------------------------------------
void AchievementManagerBase::OnBarrelRoll(s32 liNumRolls)
{
    if (!IsAchievementEarnt(E_X360_ACHIEVEMENT_BARREL_ROLL)
        && liNumRolls >= KI_BARREL_ROLLS_FOR_ACHIEVEMENT)
    {
        AchievementEarnt(E_X360_ACHIEVEMENT_BARREL_ROLL);
    }
}

// ----------------------------------------------------------------------------
// OnFlatSpin  (X360 0x8235B0B8)
//   Fire the flat-spin achievement when a single spin reaches >= 360 degrees.
// ----------------------------------------------------------------------------
void AchievementManagerBase::OnFlatSpin(f32 lfDegrees)
{
    if (!IsAchievementEarnt(E_X360_ACHIEVEMENT_FLATSPIN) && lfDegrees >= KF_FLATSPIN_DEGREES)
    {
        AchievementEarnt(E_X360_ACHIEVEMENT_FLATSPIN);
    }
}

// ----------------------------------------------------------------------------
// OnPowerParking  (X360 0x8235B138)
//   Fire the power-park achievement when a single park reaches >= 100 cars.
// ----------------------------------------------------------------------------
void AchievementManagerBase::OnPowerParking(s32 liNumCars)
{
    if (!IsAchievementEarnt(E_X360_ACHIEVEMENT_POWER_PARK) && liNumCars >= KI_POWER_PARK_CARS)
    {
        AchievementEarnt(E_X360_ACHIEVEMENT_POWER_PARK);
    }
}

// ----------------------------------------------------------------------------
// OnGameCompletion  (X360 0x8235B1B0)
// ----------------------------------------------------------------------------
void AchievementManagerBase::OnGameCompletion()
{
    if (!IsAchievementEarnt(E_X360_ACHIEVEMENT_COMPLETE_GAME))
    {
        AchievementEarnt(E_X360_ACHIEVEMENT_COMPLETE_GAME);
    }
}

}

// ----------------------------------------------------------------------------
// KAE_LICENSE_ACHIEVEMENTS  (X360 @ 0x8202AF68)
//
// FLAG (best-effort, NOT asm-verified): the six rodata bytes at 0x8202AF68 are
// not in the code exports. OnLicenseUpgrade indexes this table by (rank - 1), so
// rank 1..6 map to the six license-grade achievements in ascending grade order.
// The X360-numbered ids for the license grades are not independently attested by
// this TU; the PS3-named EAchievement enumerators are used as the closest known
// mapping. If the table bytes are later recovered, replace these values.
// ----------------------------------------------------------------------------
const EAchievement KAE_LICENSE_ACHIEVEMENTS[6] =
{
    E_ACHIEVEMENT_LICENSE_GRADE_D,        // rank 1
    E_ACHIEVEMENT_LICENSE_GRADE_C,        // rank 2
    E_ACHIEVEMENT_LICENSE_GRADE_B,        // rank 3
    E_ACHIEVEMENT_LICENSE_GRADE_A,        // rank 4
    E_ACHIEVEMENT_LICENSE_GRADE_BURNOUT,  // rank 5
    E_ACHIEVEMENT_LICENSE_GRADE_ELITE,    // rank 6
};
