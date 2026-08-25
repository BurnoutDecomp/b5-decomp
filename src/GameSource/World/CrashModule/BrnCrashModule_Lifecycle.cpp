// =================================================================================================
// GameSource/World/CrashModule/BrnCrashModule_Lifecycle.cpp   (crash exit wave, 2026-08-25)
//
// The four lifecycle members of BrnWorld::CrashModule, from the X360 ARTIST spine:
//   CrashModule::Construct @0x827CAA28   (24 insns)
//   CrashModule::Prepare   @0x827B16D8   (41 insns)
//   CrashModule::Release   @0x827B1780   (41 insns)
//   CrashModule::Reset     @0x827BF318   (54 insns)
//
// DWARF home is World/CrashModule/BrnCrashModule.cpp (the assert strings in all four name it).
// They live in their own file for the same reason WorldBridgeCrashPostScene.cpp does: the home TU
// still carries ~20 unreconstructed traffic/network helpers, and these four can land without
// waiting on them. DELETE-WHEN the home TU becomes mountable whole.
//
// =================================================================================================
// ⭐⭐ WHY THIS FILE IS THE FIRST THING THE CRASH EXIT NEEDED, AND WHY NOBODY NOTICED
// =================================================================================================
// BrnCrashModule.h declared THREE methods and no lifecycle. `mCrashModule.Construct()` at
// BrnWorldModule.cpp:505 therefore bound to the BASE CgsModule::ModuleSingleBuffered::Construct,
// which knows nothing about crash tunables. Every one of them stayed at whatever the pool held:
//
//     mbClearUpEnabled      = 0     <- and TickCrashes' FIRST assert is `mbClearUpEnabled`,
//                                      while CrashModule::PreSceneUpdate GATES both TickCrashes
//                                      and ClearupCrashes on it. ⇒ THE COUNTDOWN COULD NEVER RUN.
//     mfPlayerCrashTime     = 0.0f  <- every player crash would have been created already expired
//     miNumCrashExtensions  = 0     <- no wreck could ever earn its slide-out extension
//     meLocalActiveRaceCarIndex = 0 <- not the -1 "no player yet" sentinel
//     both crash Arrays        unconstructed (the ctor's MarkUnconstructed, never Reset)
//
// So the campaign could have landed all ~1900 instructions of ProcessCrashedRaceCarEvents /
// TickCrashes / RaceCarCrash::Tick / ClearupCrashes and a crash STILL would not have ended --
// silently, with no assert firing, because the module simply never entered the arm that ticks.
// This is the [[hollow-shell-classes]] shape one level up: not "a class that declares none of its
// virtuals" but a MODULE THAT DECLARES NONE OF ITS LIFECYCLE, so every lifecycle call lands on a
// base default that does nothing for it. The tell was available all along -- the boot gate at
// BrnWorldModule.cpp:1163 says in as many words "the call resolves to the BASE
// ModuleSingleBuffered::Prepare" -- and it was read as a reason to SKIP rather than as the defect.
//
// ⭐ The Prepare gate's stated reason for skipping is now measured and it was HALF right. The base
// Prepare does assert on the data-structure path -- but every arm of it is guarded by
// `if (!mbIsNewModule)`, and the LAST thing CrashModule::Construct does (0x827CAA80 `stw r11(1),
// 4(r3)`) is set mbIsNewModule. The assert was only reachable BECAUSE the derived Construct was
// missing. Landing Construct is what makes Prepare safe; skipping Prepare could never have.
// =================================================================================================

#include "GameSource/World/CrashModule/BrnCrashModule.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include "GameSource/BurnoutConstants.h"             // E_ACTIVE_RACE_CAR_INDEX_*

#include <cstring>                                   // memset

namespace BrnWorld
{

// =================================================================================================
// Construct @ 0x827CAA28   (24 insns)
//
//   0x827CAA34  bl  ModuleSingleBuffered::Construct
//   0x827CAA3C  stw r11(2),  0x22C(this)   -- meReleaseStage = E_RELEASESTAGE_DONE
//   0x827CAA44  stfs f0,     0x1524(this)  -- mfPlayerCrashTime = 4.0f
//   0x827CAA48  stw r11(0),  0x228(this)   -- mePrepareStage = E_PREPARESTAGE_START
//   0x827CAA4C  stb r11(0),  0x1529(this)  -- mbIsOnlineGameMode   = false
//   0x827CAA50  stb r11(0),  0x152A(this)  -- mbIsShowtimeGameMode = false
//   0x827CAA54  stb r11(0),  0x152F(this)  -- mbIsInAGameMode      = false
//   0x827CAA58  stb r11(1),  0x152B(this)  -- mbClearUpEnabled     = TRUE
//   0x827CAA5C  stb r11(10), 0x152C(this)  -- miNumCrashExtensions = 10
//   0x827CAA60  stb r11(0),  0x1528(this)  -- mbFastCrashesForAI   = false
//   0x827CAA64  stb r11(0),  0x152E(this)  -- mbNeedToSendEndingMessage = false
//   0x827CAA68  stw r11(-1), 0x1520(this)  -- meLocalActiveRaceCarIndex = INVALID
//   0x827CAA70  bl  EventQueue<TrafficRemovedEvent,25>::Construct(this + 0x1530)
//   0x827CAA78  bl  CrashModule::Reset
//   0x827CAA80  stw r11(1),  4(this)       -- Module::mbIsNewModule = true
//
// The 4.0f is the console's own default player crash duration; CrashModule::HandleGameActions'
// case 39 (GAME MODE STOP) restores exactly this same set, which is what makes free burn -- where
// no game mode ever starts -- run on these values for the whole session.
// =================================================================================================
void CrashModule::Construct()
{
    CgsModule::ModuleSingleBuffered::Construct();

    meReleaseStage            = E_RELEASESTAGE_DONE;
    mfPlayerCrashTime         = 4.0f;
    mePrepareStage            = E_PREPARESTAGE_START;
    mbIsOnlineGameMode        = false;
    mbIsShowtimeGameMode      = false;
    mbIsInAGameMode           = false;
    mbClearUpEnabled          = true;
    miNumCrashExtensions      = 10;
    mbFastCrashesForAI        = false;
    mbNeedToSendEndingMessage = false;
    meLocalActiveRaceCarIndex = E_ACTIVE_RACE_CAR_INDEX_INVALID;

    mRecycledTrafficQueue.Construct();

    Reset();

    // 0x827CAA80. The crash module is a "new module": it owns no DataStructure pair, so every
    // buffer/data-structure arm of the base Prepare/Release must be skipped. See the file banner.
    mbIsNewModule = true;
}

// =================================================================================================
// Prepare @ 0x827B16D8   (41 insns)
//
//   0x827B16F4  lwz    r11, 0x228(this)     -- mePrepareStage
//   0x827B16F8  cmplwi r11, 1 ; blt/beq     -- stage 0 or 1 -> run the manager step
//   0x827B1704  cmplwi r11, 3 ; blt         -- stage 2      -> already done, fall to the tail
//   0x827B170C  else FireAssert("Invalid Stage\n", BrnCrashModule.cpp:457) and return false
//   0x827B1738  stw    r30(1), 0x228(this)  -- mePrepareStage = E_PREPARESTAGE_MANAGER
//   0x827B173C  bl     ModuleSingleBuffered::Prepare ; return false if it is not finished
//   0x827B1758  stw    r30(1), 0x228(this)  -- mePrepareStage = E_PREPARESTAGE_MANAGER (again)
//   0x827B1760  stw    r11(0), 0x22C(this)  -- meReleaseStage = E_RELEASESTAGE_START
//   0x827B175C  li     r3, 1                -- return true
//
// ⚠️ TRANSCRIBED AS WRITTEN, INCLUDING THE ODD BIT: the tail stores MANAGER(1) into
// mePrepareStage, NOT DONE(2), even though DONE is what the stage test above accepts as
// "already prepared". `stw r30` with `li r30, 1` at 0x827B16F0 is unambiguous -- it is a 1. The
// consequence on the console is that a second Prepare() re-enters the manager step, which is
// harmless because the base Prepare is itself staged and returns true immediately once its own
// stage is DONE. Do NOT "fix" this to E_PREPARESTAGE_DONE: it is the console's behaviour, and the
// resume path depends on the base's stage, not this one. (Release @0x827B1780 is the exact mirror,
// stage word 0x22C, assert line 508.)
// =================================================================================================
bool CrashModule::Prepare()
{
    const EPrepareStage lePrepareStage = mePrepareStage;

    if (lePrepareStage <= E_PREPARESTAGE_MANAGER)
    {
        mePrepareStage = E_PREPARESTAGE_MANAGER;
        if (!CgsModule::ModuleSingleBuffered::Prepare())
        {
            return false;
        }
    }
    else if (lePrepareStage > E_PREPARESTAGE_DONE)
    {
        CGS_ASSERT(false, "Invalid Stage\n");   // BrnCrashModule.cpp:457
        return false;
    }

    mePrepareStage = E_PREPARESTAGE_MANAGER;   // see the banner -- 1, not 2, on purpose
    meReleaseStage = E_RELEASESTAGE_START;
    return true;
}

// =================================================================================================
// Release @ 0x827B1780   (41 insns). The exact mirror of Prepare over meReleaseStage (0x22C),
// with the base ModuleSingleBuffered::Release and assert line BrnCrashModule.cpp:508.
// =================================================================================================
bool CrashModule::Release()
{
    const EReleaseStage leReleaseStage = meReleaseStage;

    if (leReleaseStage <= E_RELEASESTAGE_MANAGER)
    {
        meReleaseStage = E_RELEASESTAGE_MANAGER;
        if (!CgsModule::ModuleSingleBuffered::Release())
        {
            return false;
        }
    }
    else if (leReleaseStage > E_RELEASESTAGE_DONE)
    {
        CGS_ASSERT(false, "Invalid Stage\n");   // BrnCrashModule.cpp:508
        return false;
    }

    meReleaseStage = E_RELEASESTAGE_MANAGER;
    mePrepareStage = E_PREPARESTAGE_START;
    return true;
}

// =================================================================================================
// Reset @ 0x827BF318   (54 insns). Drop every tracked crash and every per-vehicle crash bit.
//
//   0x827BF32C  stw 0, 0x2F0(this)          -- mRaceCarCrashes.Clear()   (the count word)
//   0x827BF330  stw 0, 0x7F8(this)          -- mTrafficCrashes.Clear()
//   0x827BF334+ 21 x `std 0` over +0x800..+0x8A0 -- the three FastBitArrays, zeroed
//   0x827BF38C  bl  memset(this + 0x8A8, -1, 600)  -- maiSlammedTrafficOwners = "no owner"
//   0x827BF394  stw 0, 0x1538(this)         -- mRecycledTrafficQueue.Clear()  (miLength)
//   0x827BF3A0- the 8-iteration loop at stride 324 from +0xC40 -- maCrashingTrafficForPlayers[i]
//               .Clear(), carrying BurnoutConstants.h:39's own
//               "leEnumIndex <= E_ACTIVE_RACE_CAR_INDEX_COUNT" tripwire from the ++ operator
//
// ⚠️ Written BY NAME, not by offset. The console offsets above are documentation: this build's
// FastBitArray/Set/EventQueue sub-objects widen, and only the member ORDER is load-bearing
// ([[reconstruction-gotchas]] -- parity by named members).
// ⚠️ The memset length really is 600 (KU_MAX_TOTAL_TRAFFIC) while the array is [601]; the last
// byte is left alone. Faithful, and it is the console's own off-by-one, not a transcription slip.
// =================================================================================================
void CrashModule::Reset()
{
    mRaceCarCrashes.Clear();
    mTrafficCrashes.Clear();

    // FastBitArray spells "zero every field" UnSetAll(); the console's 21 `std 0` stores over
    // +0x800..+0x8A0 are exactly the three arrays' bit-field words.
    mCrashingRaceCars.UnSetAll();
    mCrashingTraffic.UnSetAll();
    mCrashingNetworkTraffic.UnSetAll();

    memset(maiSlammedTrafficOwners, -1, 600);

    mRecycledTrafficQueue.Clear();

    for (s32 liPlayer = 0; liPlayer < E_ACTIVE_RACE_CAR_INDEX_COUNT; ++liPlayer)
    {
        maCrashingTrafficForPlayers[liPlayer].Clear();
    }
}

}   // namespace BrnWorld
