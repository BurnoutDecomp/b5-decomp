// ============================================================================
// b5-decomp/src/GameSource/GameState/AchievementManager/BrnGameStateAchievementManagerBase_DriveThru.cpp
// ============================================================================
// A SPLIT of BrnGameStateAchievementManagerBase.cpp, not a second copy: the four bodies below
// were MOVED out of that file (see the pointer comments left in their place), so there is no ODR
// fork and mounting both files together is still legal.
//
// WHY THE SPLIT EXISTS. The drive-thru chain is mounted -- BrnDriveThruManager.cpp's
// HandleDriveThru / ProcessDriveThru call OnFindAllCarParks and OnBodyShop, and
// GameStateModule::CheckForAllEventsBeingFound needs OnFindAllEvents -- but the parent TU is
// DELIBERATELY NOT MOUNTED, and for a measured reason recorded in tools/build/build_game_exe.bat:
// mounting the whole base costs unresolved externals that have no definition anywhere in the tree
// (ScoringSystem::GetPlayerScore / GetPlayerModeCrashes / GetPlayerModeTakedowns /
// GetNumberOfTakedownsAgainst, ProgressionManager::GetCarChallengeWinCount /
// GetCollectedStuntElementCount / GetProfileTotalTakedowns), all pulled in by the OTHER gameplay
// hooks. Re-measured 2026-08-27: six of the original eight are still bodiless, so that note is
// current, not stale. ⛔ And "nothing calls them, /OPT:REF strips them" is NOT a defence -- the
// linker resolves before it discards.
//
// The four hooks here reach only the two protected virtuals and ONE ScoringSystem accessor
// (GetNewlyWreckedCarCount, bodied this wave in BrnScoringSystem_Queries.cpp), so this slice
// mounts for ZERO new unresolved externals. ⭐ DELETE-WHEN the seven symbols above land and the
// parent TU is mounted whole: fold these four back and drop this file.
//
// Ids stay X360-SKU numbering, spelled the same way and for the same reason as the parent TU's
// anonymous-namespace block (the X360 numbers its achievements differently from the PS3 DWARF
// EAchievement enum, so a raw id cannot be spelled with a PS3 enumerator name).
// ----------------------------------------------------------------------------

#include "GameSource/GameState/AchievementManager/BrnGameStateAchievementManagerBase.h"

#include "GameSource/GameState/ModeManager/Scoring/BrnScoringSystem.h" // GetNewlyWreckedCarCount

namespace BrnGameState
{

namespace
{
    // ---- X360-attested achievement ids (raw integers from the X360 .text). ----
    // FLAG: X360-SKU numbering; intentionally NOT the PS3 EAchievement names/values. Every value
    // is the live `li r4, <id>` immediate at the named call site.
    const EAchievement E_X360_ACHIEVEMENT_REPAIR_FIRST_WRECKED_CAR = static_cast<EAchievement>(0);  // OnBodyShop      @0x8235AA18
    const EAchievement E_X360_ACHIEVEMENT_SHUTDOWN_ROADRAGE_VAN    = static_cast<EAchievement>(8);  // OnBodyShop      @0x8235AA18
    const EAchievement E_X360_ACHIEVEMENT_FIND_ALL_EVENTS          = static_cast<EAchievement>(25); // (see OnFindAllEvents)
    const EAchievement E_X360_ACHIEVEMENT_FIND_ALL_CARPARKS        = static_cast<EAchievement>(26); // OnFindAllCarParks    @0x8235AED8
    const EAchievement E_X360_ACHIEVEMENT_FIND_ALL_DRIVE_THRUS     = static_cast<EAchievement>(32); // OnFindAllDriveThrus  @0x8235AE78
    const EAchievement E_X360_ACHIEVEMENT_COMPLETE_GAME            = static_cast<EAchievement>(34); // OnGameCompletion     @0x8235B1B0
}

// ----------------------------------------------------------------------------
// OnBodyShop  (X360 0x8235AA18)
//   Fires REPAIR_FIRST_WRECKED_CAR unconditionally (first time). Then, in
//   ROAD_RAGE (mode == 3) with exactly one car newly wrecked-but-not-repaired,
//   fires the road-rage repair shutdown achievement (id 8).
// ----------------------------------------------------------------------------
void AchievementManagerBase::OnBodyShop(GameStateModuleIO::EGameModeType leGameMode)
{
    if (!IsAchievementEarnt(E_X360_ACHIEVEMENT_REPAIR_FIRST_WRECKED_CAR))
    {
        AchievementEarnt(E_X360_ACHIEVEMENT_REPAIR_FIRST_WRECKED_CAR);
    }

    if (!IsAchievementEarnt(E_X360_ACHIEVEMENT_SHUTDOWN_ROADRAGE_VAN)
        && leGameMode == GameStateModuleIO::E_MODE_ROAD_RAGE
        && mpScoringSystem->GetNewlyWreckedCarCount() == 1)
    {
        AchievementEarnt(E_X360_ACHIEVEMENT_SHUTDOWN_ROADRAGE_VAN);
    }
}

// (OnGameCompletion @0x8235B1B0 lived here as a duplicate of the base TU's body; the base TU
//  BrnGameStateAchievementManagerBase.cpp is MOUNTED as of 2026-09-03, so the copy is gone -- LNK2005.)

// ----------------------------------------------------------------------------
// OnFindAllDriveThrus  (X360 0x8235AE78)
// ----------------------------------------------------------------------------
void AchievementManagerBase::OnFindAllDriveThrus()
{
    if (!IsAchievementEarnt(E_X360_ACHIEVEMENT_FIND_ALL_DRIVE_THRUS))
    {
        AchievementEarnt(E_X360_ACHIEVEMENT_FIND_ALL_DRIVE_THRUS);
    }
}

// ----------------------------------------------------------------------------
// OnFindAllCarParks  (X360 0x8235AED8)
// ----------------------------------------------------------------------------
void AchievementManagerBase::OnFindAllCarParks()
{
    if (!IsAchievementEarnt(E_X360_ACHIEVEMENT_FIND_ALL_CARPARKS))
    {
        AchievementEarnt(E_X360_ACHIEVEMENT_FIND_ALL_CARPARKS);
    }
}

// ----------------------------------------------------------------------------
// OnFindAllEvents  --  the id-25 member of the OnFindAll* family.
//
// ⚠️ THIS ONE HAS NO STANDALONE X360 SYMBOL: the console INLINES it into
// GameStateModule::CheckForAllEventsBeingFound @0x82382460, whose tail is, verbatim:
//     addis r31, r26, 3 / addi r31, r31, -0x3A50   ; gsm + 181680 == mAchievementManager
//     li    r4, 0x19                               ; 25
//     lwz   r11, 0(r31) / lwz r11, 4(r11) / bctrl  ; vtable slot 1 -> IsAchievementEarnt(25)
//     if the byte comes back 0:
//       lwz r11, 0(r31) / lwz r11, 0(r11) / li r4, 0x19 / bctrl   ; slot 0 -> AchievementEarnt(25)
// -- byte for byte the body of OnFindAllCarParks and OnFindAllDriveThrus above with a different
// id. That is the ATTESTED part.
//
// ⭐ AND THE METHOD ITSELF IS DWARF-ATTESTED, not an inference: the DecFIGS declaration list for
// this class carries `void OnFindAllEvents()` (BrnGameStateAchievementManagerBase.h:128) right
// beside OnShutdown / OnDrivenDistance, and the recon header has been declaring it -- bodiless --
// since it was written. Two independent things had to agree for it to be the right home and they
// do: the DWARF names the method, and both virtuals are `protected` in that same DWARF (:219 /
// :224), so GameStateModule could never have named them directly. The only judgement call left is
// the ID, and 25 is the live `li r4, 0x19` at 0x823824F0 / 0x8238251C.
// ----------------------------------------------------------------------------
void AchievementManagerBase::OnFindAllEvents()
{
    if (!IsAchievementEarnt(E_X360_ACHIEVEMENT_FIND_ALL_EVENTS))
    {
        AchievementEarnt(E_X360_ACHIEVEMENT_FIND_ALL_EVENTS);
    }
}

}   // namespace BrnGameState
