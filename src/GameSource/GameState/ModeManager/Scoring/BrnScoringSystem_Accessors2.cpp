// ============================================================================================
// b5-decomp/src/GameSource/GameState/ModeManager/Scoring/BrnScoringSystem_Accessors2.cpp
// ============================================================================================
// [takedown P1 wave 2026-09-03] The three ScoringSystem accessors AchievementManagerBase reads
// through its mpScoringSystem back-pointer that were still declare-only in BrnScoringSystem.h
// (the other two of the bat's "eight" -- GetNewlyWreckedCarCount and GetNumberOfTakedownsAgainst --
// already live in BrnScoringSystem_Queries.cpp). None of the three has a standalone X360 symbol:
// the console inlines each as one `lwz` off the ScoringSystem inside the achievement hook, so the
// evidence for every body is the hook's own asm, cited per function. Members BY NAME; the X360
// offsets are quoted only to show which member the console's load lands on.
// ============================================================================================
#include "GameSource/GameState/ModeManager/Scoring/BrnScoringSystem.h"

namespace BrnGameState
{
    // ----------------------------------------------------------------------------------------
    // GetPlayerScore(lbOnline) -- AchievementManagerBase::OnEventWin @0x82372978, case 7
    // (E_MODE_STUNT_ATTACK), 0x82372AEC..0x82372B20:
    //   bl   GameStateModule::IsOnlineGameMode
    //   lwz  r11, 0xC(r31)             ; mpScoringSystem
    //   addi r11, r11, 0x2620 (online) ; else addi r11, r11, 0x350 (offline)
    //   lwz  r11, 0x10(r11)            ; cmpw against 1000000
    // ss+0x350 is mStuntModeScoring and ss+0x2620 is mOnlineStuntModeScoring (both pinned in
    // BrnScoringSystem.h), and +0x10 of a StuntModeScoring is miCurrentScore
    // (BrnStuntModeScoring.h, "+0x10"). The mode selects the scorer; the score is that scorer's
    // running total.
    // ----------------------------------------------------------------------------------------
    s32 ScoringSystem::GetPlayerScore(bool lbOnline) const
    {
        const StuntModeScoring& lrStuntScorer = lbOnline ? mOnlineStuntModeScoring : mStuntModeScoring;
        return lrStuntScorer.GetCurrentScore();
    }

    // ----------------------------------------------------------------------------------------
    // GetPlayerModeTakedowns -- AchievementManagerBase::OnTakedown @0x8235AAE0, 0x8235ABC8..0x8235ABD4:
    //   lwz r11, 0xC(r31) ; lwz r10, 0x4B40(r11) ; cmpwi r10, 0xA
    // ss+0x4B40 is the FIRST word of mRoadRageModeScoring: the sub-object is immediately followed
    // by miMaximumPlayerCrashedNumber at ss+0x4B58 and its DWARF member run is exactly 24 bytes
    // (4+4+2+2+4+4+1+1+1+1), so 0x4B58 - 24 == 0x4B40 == RoadRageModeScoring::miNumTakedownsAchieved
    // -- the word IncrementPlayerNumTakedowns @0x823445D0 bumps as `lwz/addi/stw 0(r31)`
    // (0x8234461C/0x82344624/0x82344634).
    // ----------------------------------------------------------------------------------------
    s32 ScoringSystem::GetPlayerModeTakedowns() const
    {
        return mRoadRageModeScoring.GetNumTakedownsAchieved();
    }

    // ----------------------------------------------------------------------------------------
    // GetPlayerModeCrashes -- the same hook, 0x8235ABD8..0x8235ABDC:
    //   lwz r11, 0x4B5C(r11) ; cmpwi r11, 0
    // ss+0x4B5C == miCurrentPlayerCrashedNumber (BrnScoringSystem.h, DWARF :1214) -- the wrecks
    // still outstanding this mode; zero is the "perfect road rage" condition.
    // ----------------------------------------------------------------------------------------
    s32 ScoringSystem::GetPlayerModeCrashes() const
    {
        return miCurrentPlayerCrashedNumber;
    }
}
