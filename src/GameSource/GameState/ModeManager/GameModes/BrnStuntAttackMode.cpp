#include "GameSource/GameState/ModeManager/GameModes/BrnStuntAttackMode.h"

#include <cmath>                                                        // std::asin / std::fabs (the de-SIMD'd XMVectorASin + fabs)

#include "GameShared/GameClasses/Core/CgsAssert.h"                      // CGS_ASSERT
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"        // GameActionQueue::AddEvent, CgsModule::Event
#include "GameSource/GameState/BrnGameStateModuleIO.h"                  // OutputBuffer::GetGameActionQueue / PreWorldInputBuffer::GetTimerStatusInterface
#include "GameSource/GameState/ModeManager/BrnModeManager.h"            // ModeManager::SetStartingGrid / GetProgressionManager
#include "GameSource/GameState/ModeManager/GameModes/BrnGameModeParams.h" // GameModeParams / StartGameModeParams / ProgressionRankData
#include "GameSource/GameState/ModeManager/Scoring/BrnScoringSystem.h"  // ScoringSystem + the embedded StuntModeScoring
#include "GameSource/GameState/Progression/BrnProgressionManager.h"     // ProgressionManager::GetProfile / GetProgressionRankForGameMode / GetStuntRunScoreTarget
#include "GameSource/GameState/Progression/BrnProfile.h"                // Profile::GetTargetEvent
#include "GameSource/GameState/SharedIO/BrnTargetEventScore.h"          // GameStateModuleIO::TargetEventScore::miScore (+0x20)
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarEntityModuleOutputInterface.h" // the active race-car accessors
#include "SharedClasses/Progression/BrnRaceEventData.h"                 // RaceEventData::GetRankTime
#include "rw/math/vpu/vector3_operation.h"                              // Vector3 Cross / Dot / Normalize / Magnitude

namespace BrnGameState
{
// ============================================================================================
// TU-LOCAL CONSTANTS AND RECORDS. Every recovered value is image-cited
// (scratch/postfx_step9_final/envfix/work/image.bin, offset = VA - 0x82000000, big-endian);
// nothing here is invented.
// ============================================================================================
namespace
{
    // ---- Start @0x82331E98 ------------------------------------------------------------------
    // 0x82331F68 `lfs f0, flt_82021138` -> params+0x30. image.bin @0x21138 == 3E 80 00 00 == 0.25f.
    const f32 KF_STUNT_TRAFFIC_DENSITY_SCALE = 0.25f;

    // 0x82332040/0x82332044 `oris r11,r11,0x8D1 / ori r11,r11,0x803` on the 64-bit muFlags word
    // read+written at params+0x860. The value is therefore exactly 0x08D10803. Spelled out against
    // BrnGameModeParams.h's KU_FLAG_* table so the bits are readable rather than a magic literal:
    //   0x00000001 SET_CARS_TO_START_GRID          0x00000002 REMOVE_RIVALS_FROM_WORLD
    //   0x00000800 CLEAR_NEARBY_TRAFFIC            0x00010000 SET_ALL_CARS_TO_STARTING_AI_CONTROL
    //   0x00100000 DISABLE_UPCOMING_ROAD_SIGNS     0x00400000 DISABLE_ALL_TDS
    //   0x00800000 DISABLE_CRASH_EXTENSIONS        0x08000000 DONUT_START
    // (their sum is 1+2+0x800+0x10000+0x100000+0x400000+0x800000+0x8000000 == 0x08D10803.)
    const u64 KU_STUNT_MODE_FLAGS =
        GameModeParams::KU_FLAG_SET_CARS_TO_START_GRID              |
        GameModeParams::KU_FLAG_REMOVE_RIVALS_FROM_WORLD            |
        GameModeParams::KU_FLAG_CLEAR_NEARBY_TRAFFIC                |
        GameModeParams::KU_FLAG_SET_ALL_CARS_TO_STARTING_AI_CONTROL |
        GameModeParams::KU_FLAG_DISABLE_UPCOMING_ROAD_SIGNS         |
        GameModeParams::KU_FLAG_DISABLE_ALL_TDS                     |
        GameModeParams::KU_FLAG_DISABLE_CRASH_EXTENSIONS            |
        GameModeParams::KU_FLAG_DONUT_START;

    // 0x82332170..0x823321F8 `lis r9,6 / ori r9,r9,0x7508 / lwz r10,0x48(r31) / cmplw`.
    // 0x00067508 == 423176 -- ONE hard-coded junction id (GameModeParams::muJunctionID) whose
    // stunt-race start direction is authored in the code rather than in the trigger data.
    const u32 KU_HARDCODED_START_DIRECTION_JUNCTION_ID = 423176u;

    // 0x823321FC..0x82332218: x <- flt_82026CDC, y <- 0, z <- flt_82026CD8, w <- 0.
    // image.bin @0x26CDC == 3F 53 49 38 ==  0.8253359794616699f
    // image.bin @0x26CD8 == BF 10 8C 61 == -0.5646420121192932f
    const f32 KF_HARDCODED_START_DIRECTION_X =  0.8253359794616699f;
    const f32 KF_HARDCODED_START_DIRECTION_Z = -0.5646420121192932f;

    // 0x8233229C `lfs f0, flt_82004A28` -> params+0x6C when the event has no authored rank time.
    // image.bin @0x4A28 == 42 F0 00 00 == 120.0f.
    const f32 KF_DEFAULT_STUNT_TIME_LIMIT_SECONDS = 120.0f;

    // The zero test the console spells inline at 0x8233224C..0x82332268 as
    // `!(t > flt_82020B30) && (t >= flt_82002514)`. image.bin @0x20B30 == 34 00 00 00 ==
    // 1.1920928955078125e-07f and @0x2514 == B4 00 00 00 == its negation -- i.e. the project's
    // float epsilon, used as "GetRankTime() answered zero". Kept as the same two-sided compare
    // rather than `== 0.0f`, because that is what the binary does.
    const f32 KF_ZERO_EPSILON = 1.1920928955078125e-07f;

    // ---- PreWorldUpdate @0x82344EE0 ---------------------------------------------------------
    // 0x82345168 `lfs f0, flt_82CDB884`. image.bin @0xCDB884 == 3D B2 B8 C2 == 0.0872664600610733f
    // == 5 degrees in radians (5 * pi / 180 == 0.0872664626). AUTHORED as a float constant in the
    // data segment, not computed.
    const f32 KF_START_DIRECTION_TOLERANCE_RADIANS = 0.0872664600610733f;

    // 0x823451A0 `lfs f0, flt_82022E34`. image.bin @0x22E34 == 41 A0 00 00 == 20.0f. Compared
    // against |RaceCarState::mfSpeedMPH|, so the unit is mph.
    const f32 KF_MOVING_FAST_ENOUGH_MPH = 20.0f;

    // 0x823451B4 `lfs f0, flt_820211C8`. image.bin @0x211C8 == 40 C0 00 00 == 6.0f. The countdown
    // hold gives up after this many seconds no matter which way the player is pointing. (Same
    // VALUE as KF_INTRO_TIME_SECONDS but a DIFFERENT constant -- flt_82021240 vs flt_820211C8.)
    const f32 KF_MAX_COUNTDOWN_HOLD_SECONDS = 6.0f;

    // ---- action 170 --------------------------------------------------------------------------
    // [!] HEADER REQUEST (interim TU-local mirror, per the cross-seam audit's ruling that an
    // asm-cited TU-local KI_ACTION_* is the correct interim carrier until BrnGameActions.h grows
    // the enumerator). X360 0x82344FD4/0x82344FD0 `li r5,0xAA / li r6,0x14` == id 170, size 20.
    //   * The DWARF enumerator is `E_ACTION_SET_BOOST = 162` (dwarfdump .../BrnGameActions.h),
    //     and this band of the enum carries the SAME +8 X360 shift BrnGameActions.h:227-233
    //     already records for E_ACTION_FREEBURN_CHALLENGE (DWARF 145 -> X360 153). 162 + 8 == 170.
    //   * The DWARF record (dwarfdump BrnGameActions.h:1490) is
    //     `SetBoostAction : GameAction<E_ACTION_SET_BOOST>` with members
    //     {EActiveRaceCarIndex meRaceCarIndex, s32 mxFlags, bool mbInfiniteBoost,
    //      f32 mfBoostAmount, s32 miBoostSegments} and the flag constants
    //     KX_SET_INFINITE_BOOST_FLAG=1, KX_SET_BOOST_AMOUNT=2, KX_SET_BOOST_SEGMENTS=4.
    //   * [!] MEMBER ORDER: the X360 writes `stw idx -> +0x00`, `stw 2 -> +0x04`,
    //     `stfs 1.0 -> +0x08` (flt_82001C98 == 1.0f, image.bin @0x1C98). Since +0x04 is
    //     mxFlags == KX_SET_BOOST_AMOUNT, the float at +0x08 can only be mfBoostAmount -- so on
    //     X360 mfBoostAmount sits at +0x08 and the PS3 DWARF's mbInfiniteBoost/miBoostSegments
    //     follow it in the remaining 8 bytes of the 20-byte record. Modelled that way below.
    //   * The console leaves the last 8 bytes UNINITIALISED on the wire (only three stores
    //     precede its AddEvent); reproduced -- do not "helpfully" zero them.
    const s32 KI_ACTION_SET_BOOST = 170;   // == DWARF E_ACTION_SET_BOOST(162) + the band's X360 +8

    const s32 KI_SET_BOOST_FLAG_AMOUNT = 2;   // DWARF SetBoostAction::KX_SET_BOOST_AMOUNT

    const f32 KF_FULL_BOOST_AMOUNT = 1.0f;    // image.bin @0x1C98 == 3F 80 00 00 == 1.0f

    struct SetBoostActionRecord
    {
        // [!] EXPLICITLY GLOBAL-QUALIFIED. This tree carries TWO distinct
        // `enum EActiveRaceCarIndex : s32` -- the real one (BurnoutConstants.h:8) and a
        // BrnGameState-scoped stand-in (BrnTakedownManagerTypes.h:18) that unqualified lookup
        // finds first from inside `namespace BrnGameState`. The value stored here comes from
        // RCEntityActiveRaceCarOutputInterface::GetPlayerActiveRaceCarIndex, which returns the
        // GLOBAL one, so the member must name it the same way (same reason BrnGameMode.h:53 and
        // BrnGameStateModuleIO.h:630 spell theirs `::EActiveRaceCarIndex`).
        ::EActiveRaceCarIndex meRaceCarIndex; // +0x00
        s32                 mxFlags;          // +0x04
        f32                 mfBoostAmount;    // +0x08
        s32                 miBoostSegments;  // +0x0C  (not written by this producer)
        s32                 mbInfiniteBoost;  // +0x10  (not written by this producer)
    };
}

const f32 StuntAttackMode::KF_INTRO_TIME_SECONDS = 6.0f;

// X360: 0x827E2528.
const char* StuntAttackMode::GetName() const
{
    return "Stunt Race";
}

// X360: 0x827E2538. Returns the inlined KF_INTRO_TIME_SECONDS (6.0). DWARF shape is f32.
f32 StuntAttackMode::GetIntroDurationSeconds() const
{
    return KF_INTRO_TIME_SECONDS;
}

// X360: 0x827E2548. Stunt Attack has no outro hold, so the timeout is zero.
f32 StuntAttackMode::GetOutroTimeout() const
{
    return 0.0f;
}

// X360: 0x827E2558. The countdown-end hook CountdownState polls: hold the player on the line
// until they face the required start direction. Hex-Rays renders this as a raw read of the bool
// member at +212 (mbPlayerPointingInStartDirection), restored here to the named member.
// The SOLE writer of that member is PreWorldUpdate below.
bool StuntAttackMode::ShouldCountdownEnd() const
{
    return mbPlayerPointingInStartDirection;
}

// X360 vtable slot 13 (vtbl+52), folded leaf 0x827E2F38 == `li r3,0; blr` at slot 13 of the
// StuntAttackMode vtable 0x820D0720 (the offline base carries GameMode::ShouldExit 0x82315B80
// there instead). A stunt race ends through ShouldFinish (slot 14), never through the shared
// "player has been stationary / has not touched the controls" idle-exit test -- which matters,
// because standing still while lining up a stunt is normal play.
bool StuntAttackMode::ShouldExit(const ScoringSystem* lpScoringSystem) const
{
    (void)lpScoringSystem;
    return false;
}

// X360 vtable slot 23 (vtbl+92), folded leaf 0x827E2F38 == `li r3,0; blr`; the GameMode base is
// 0x82C296C8 == `li r3,1`. SetupGameMode @0x8234B158 reads this through vtbl+92 to decide whether
// to wait on GameStateModule::WaitForStreaming -- so a stunt race does NOT take that path.
bool StuntAttackMode::RequiresStreaming() const
{
    return false;
}

// ============================================================================================
// StuntAttackMode::Start -- X360 0x82331E98 (vtable slot 5, vtbl+20)
// ============================================================================================
// Builds the mutable GameModeParams for a stunt race out of the immutable StartGameModeParams
// plus the per-rank tuning record, then seats the grid and installs the score/time targets.
// Reconstructed from the ASSEMBLY (the Hex-Rays rendering of this body is heavily distorted --
// it shows the VMX start-direction block as inline `__asm`, mis-renders the `progMgr + 0x170`
// profile sub-object test as `== -368`, and calls RaceEventData::GetRankTime twice because the
// compiler rematerialised it).
//
// REGISTER MAP (asm 0x82331EA8..0x82331EB0): r25 = this, r29 = lpStartGameModeParams,
// r31 = lpGameModeParams. r6 -- the ScoringSystem* the base slot passes -- is NEVER READ, which
// is why the parameter is unused below (same as the committed RaceMode/Pursuit/BurningRoute
// Start bodies).
//
// CONSOLE OFFSET -> NAME, every one re-derived this pass, none taken on report:
//   StartGameModeParams  +0x2D0 meGameModeType        +0x2F0 mStartDirection
//                        +0x310 meStartMechanism      +0x328 muEventJunctionId
//                        +0x32C mpEventData           +0x330 muJunctionID
//                        +0x334 mpProgressionRankData +0x338 mfProgressionRankAsRatio
//   GameModeParams       +0x00 miNumRivals   +0x01 miNumNetworkPlayers   +0x04 mfProgressionRankAsRatio
//                        +0x30 mfTrafficDensityScale  +0x34 mfLargeVehicleProbability
//                        +0x3C meStartMechanism       +0x40 mTrafficLightTriggerId
//                        +0x44 muEventJunctionID      +0x48 muJunctionID
//                        +0x68 mfNeedForGold          +0x6C mfModeTimeLimit
//                        +0x74 mfOvertakingDifficulty[8]     +0x860 muFlags
// The +0x30/+0x34/+0x3C/+0x40/+0x44/+0x48 run is INDEPENDENTLY corroborated by the committed
// ModeManager::SetStartingGrid banner (BrnModeManager_IntroPlay.cpp:195-199), which derived the
// identical map from a different call site. The +0x60/+0x64/+0x68/+0x6C run is pinned by
// GameModeParams::Construct @0x8231C3A8..0x8231C3BC (four consecutive `stfs 0.0`) together with
// BurningRouteMode::Start @0x82331D7C..0x82331D88, which writes ONE time value into all four --
// the classic {bronze, silver, gold, mode time limit} quartet. So +0x68 is the gold threshold
// (for a stunt race: the target SCORE) and +0x6C is the mode time limit.
// (i) ScoringSystem::OnModeStart @0x823382A8 confirms the consumer: for mode 7 it does
//     `lfs f0, 0x68(params) ; fctiwz ; StuntModeScoring::Activate(&mStuntModeScoring, (s32)f0)`.
//
// [x] THE TWO LINK-TIME HOLES ARE CLOSED (2026-08-26, wave-B CLOSURE round). This banner used to
// read "declaration-complete, body-incomplete" for:
//   * BrnProgression::ProgressionManager::GetProgressionRankForGameMode (X360 0x8237B4E8)
//   * BrnProgression::ProgressionManager::GetStuntRunScoreTarget        (X360 0x8237B6B0)
// Both are now BODIED in BrnProgressionManager.cpp, along with the third function they stand on,
// ProgressionManager::GetRankThresholdForEvent (X360 0x82370260). The one thing that had blocked
// all three -- a name for the per-rank stunt threshold byte at ProgressionRankData+0x61 -- is now
// GetNumWinsToRankUpStunt (DWARF BrnProgressionRankData.h:311, declared in BrnGameModeParams.h).
// Consequence for this body: `lpGameModeParams->mfNeedForGold` below now receives a REAL
// interpolated target on the no-TargetEventScore path, not a link error.
//
// DROPPED (deliberately, matching the committed PursuitMode::Start precedent): the
// `CgsDev::Message::gxMessageFilterFlags & 1` debug-print of
// "lpProgressionRankData->GetLargeVehicleProbability() :    " at 0x82331F8C..0x82331FD4. It has
// no effect on state; this tree has no CgsDev::Log stream vocabulary wired into GameModes/.
// ============================================================================================
void StuntAttackMode::Start(const StartGameModeParams* lpStartGameModeParams,
                            GameModeParams*            lpGameModeParams,
                            ScoringSystem*             /*lpScoringSystem*/)
{
    // 0x82331EB4..0x82331EE0 -- assert strings verbatim, file/line come from CGS_ASSERT.
    CGS_ASSERT(lpStartGameModeParams->GetEventData() != 0, "lpEventData");                       // :66
    CGS_ASSERT(lpStartGameModeParams->GetProgressionRankData() != 0, "mpProgressionRankData != NULL");

    const BrnProgression::ProgressionRankData* lpProgressionRankData =
        lpStartGameModeParams->GetProgressionRankData();
    CGS_ASSERT(lpProgressionRankData != 0, "lpProgressionRankData");                             // :69

    // 0x82331F40..0x82331F48.
    lpGameModeParams->Construct(lpStartGameModeParams->GetGameModeType());

    // 0x82331F4C..0x82331F78. The console's six stores here are INTERLEAVED by the scheduler
    // (`stw 0x44` @F5C, `stb 1` @F64, `stb 0` @F6C, `stfs 0x30` @F70, `stw 0x48` @F74), not
    // grouped -- they are six independent writes to six distinct fields with no read between
    // them, so declaration order is the same program. Written in field order for legibility.
    lpGameModeParams->muEventJunctionID = lpStartGameModeParams->GetEventJunctionId();
    lpGameModeParams->muJunctionID      = lpStartGameModeParams->GetJunctionID();
    lpGameModeParams->SetNumRivals(0);                         // `stb r23, 0(r31)`
    lpGameModeParams->miNumNetworkPlayers = 0;                 // `stb r23, 1(r31)`
    lpGameModeParams->SetTrafficDensityScale(KF_STUNT_TRAFFIC_DENSITY_SCALE);

    // 0x82331FD8..0x82331FDC (the store the dropped debug print straddles).
    lpGameModeParams->SetLargeVehicleProbability(lpProgressionRankData->GetLargeVehicleProbability());

    // 0x82331FE0..0x82332000 -- the SECOND mpProgressionRankData guard (BrnGameModeParams.h:960
    // on the console; it belongs to the inlined StartGameModeParams accessor, restated here
    // because the console restates it).
    CGS_ASSERT(lpStartGameModeParams->GetProgressionRankData() != 0, "mpProgressionRankData != NULL");
    lpGameModeParams->SetProgressionRankAsRatio(lpStartGameModeParams->GetProgressionRankAsRatio());

    // 0x82332008..0x82332030 -- the 8-float copy the console emits as an unrolled loop from
    // rankData+0x2C into params+0x74. That IS ProgressionRankData::GetOvertakingDifficulty
    // (DWARF :192, "copies maOvertakingDifficulty[8], byte +44"); same de-inlining the committed
    // PursuitMode::Start uses for the identical loop.
    lpProgressionRankData->GetOvertakingDifficulty(lpGameModeParams->mfOvertakingDifficulty);

    // 0x82332034..0x82332048 -- read-modify-write of the 64-bit flag word.
    lpGameModeParams->SetFlag(KU_STUNT_MODE_FLAGS);

    // 0x8233204C..0x82332050.
    lpGameModeParams->SetStartMechanism(lpStartGameModeParams->GetStartMechanism());

    // 0x82332054..0x82332078. The console reads the CURRENT miNumRivals back out of the params
    // (`lbz r11, 0(r31); extsb; addi r5, r11, 1`) to size the grid -- rivals plus the player -- so
    // for a stunt race that is exactly one car. lbPushForwards is `li r6,1`.
    lpGameModeParams->SetTrafficLightTriggerId(lpStartGameModeParams->GetTrafficLightTriggerId());
    GetModeManager()->SetStartingGrid(lpGameModeParams, lpGameModeParams->GetNumRivals() + 1, true);

    // 0x8233207C..0x82332088. `li r4,7` == E_MODE_STUNT_ATTACK; the caller sign-extends the s8
    // result (`extsb r30, r30` @0x82332240) before using it as a rank index.
    BrnProgression::ProgressionManager* lpProgressionManager = GetModeManager()->GetProgressionManager();
    const s32 liProgressionRank = lpProgressionManager->GetProgressionRankForGameMode(
                                      GameStateModuleIO::E_MODE_STUNT_ATTACK);

    // 0x8233208C..0x8233210C -- three guards, all fired AFTER the call above (the console
    // evaluates the rank first and only then checks the pointers; kept in that order).
    CGS_ASSERT(GetModeManager() != 0, "mpModeManager");                                          // :105
    CGS_ASSERT(GetModeManager()->GetProgressionManager() != 0, "mpModeManager->GetProgressionManager()"); // :106
    CGS_ASSERT(lpProgressionManager->GetProfile() != 0, "mpModeManager->GetProgressionManager()->GetProfile()"); // :107

    // 0x82332110..0x8233215C. If the profile already carries a target-event record for this
    // event, its stored score (TargetEventScore::miScore @+0x20) is the target; otherwise ask the
    // progression manager to interpolate one across the player's rank.
    // [!] The console keys GetTargetEvent on the word it JUST stored at params+0x44
    // (`lwz r4, 0x44(r31)`), i.e. muEventJunctionID -- the event's junction id doubles as the
    // event CgsID here. Read from the params, not from the start params, exactly as shipped.
    const GameStateModuleIO::TargetEventScore* lpTargetEvent =
        lpProgressionManager->GetProfile()->GetTargetEvent(lpGameModeParams->muEventJunctionID);

    const s32 liStuntScoreTarget =
        (lpTargetEvent != 0)
            ? lpTargetEvent->miScore
            : lpProgressionManager->GetStuntRunScoreTarget(lpGameModeParams, lpStartGameModeParams);

    // 0x82332160..0x82332184 `fcfid / frsp / stfs 0x68(r31)` -- an s64->f64->f32 conversion of the
    // sign-extended integer target.
    lpGameModeParams->mfNeedForGold = static_cast<f32>(liStuntScoreTarget);

    // 0x82332188..0x823321EC -- the start direction. The console loads the 16-byte
    // StartGameModeParams::mStartDirection, stores it to mStartDir, ZEROES the Y lane, stores it
    // again, then normalises in place (vrsqrtefp + two Newton refinements) and stores a third
    // time. Semantically one assignment; the PC Normalize is an exact std::sqrt (see the header
    // banner in rw/math/vpu/vector3_operation.h).
    mStartDir   = lpStartGameModeParams->GetStartDirection();
    mStartDir.y = 0.0f;
    mStartDir   = rw::math::vpu::Normalize(mStartDir);

    // 0x823321F0..0x82332228 -- ONE authored junction overrides the trigger-data direction.
    if (lpGameModeParams->muJunctionID == KU_HARDCODED_START_DIRECTION_JUNCTION_ID)
    {
        mStartDir = Vector3{ KF_HARDCODED_START_DIRECTION_X, 0.0f, KF_HARDCODED_START_DIRECTION_Z, 0.0f };
    }

    // 0x8233222C..0x82332230 -- arm the one-shot "fill the player's boost" post that
    // PreWorldUpdate drains on its next tick.
    mbNeedToFillBoost = true;

    // 0x82332234..0x823322B4 -- the mode time limit. GetRankTime answering (within epsilon) zero
    // is treated exactly like "no event data at all": both take the 120 s default. The console
    // re-calls GetRankTime on the non-zero arm (rematerialisation); one call here.
    const BrnProgression::RaceEventData* lpEventData = lpStartGameModeParams->GetEventData();
    f32 lfModeTimeLimit = KF_DEFAULT_STUNT_TIME_LIMIT_SECONDS;
    if (lpEventData != 0)
    {
        const f32 lfRankTime = lpEventData->GetRankTime(static_cast<u32>(liProgressionRank));
        if (lfRankTime > KF_ZERO_EPSILON || lfRankTime < -KF_ZERO_EPSILON)
        {
            lfModeTimeLimit = lfRankTime;
        }
    }
    lpGameModeParams->mfModeTimeLimit = lfModeTimeLimit;

    // `stfs f31(0.0), 0xD0(r25)` on BOTH tails (0x8233228C and 0x823322A8).
    mfCountdownTimer = 0.0f;
}

// ============================================================================================
// StuntAttackMode::PreWorldUpdate -- X360 0x82344EE0 (vtable slot 2, vtbl+8)
// ============================================================================================
// THE SOLE WRITER of mbPlayerPointingInStartDirection, which ShouldCountdownEnd returns -- so
// without this body a started stunt race hangs in countdown forever (hazard H8's prediction).
//
// REGISTER MAP (asm 0x82344F08..0x82344F24): r31 = this, r29 = lpOutput, r27 = lpInput,
// r26 = lpGlobalRaceCars, r30 = lpActiveRaceCars, r25 = lbPaused, r24 = lpScoringSystem. The
// base call at 0x82344FE8..0x82345004 forwards ALL SIX unchanged.
//
// Structure:
//   1. two null tripwires on the two buffers;
//   2. the one-shot boost fill (drains mbNeedToFillBoost, which Start arms);
//   3. GameMode::PreWorldUpdate (the base drives the state machine);
//   4. ONLY while meCurrentState == E_GMS_COUNTDOWN (0): advance mfCountdownTimer and recompute
//      mbPlayerPointingInStartDirection.
// ============================================================================================
void StuntAttackMode::PreWorldUpdate(GameStateModuleIO::OutputBuffer* lpOutput,
                                     const GameStateModuleIO::PreWorldInputBuffer* lpInput,
                                     const BrnWorld::RaceCarEntityModuleIO::RCEntityGlobalRaceCarOutputInterface* lpGlobalRaceCars,
                                     const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpActiveRaceCars,
                                     bool lbPaused,
                                     const ScoringSystem* lpScoringSystem)
{
    CGS_ASSERT(lpOutput != 0, "lpOutput != NULL");   // :161
    CGS_ASSERT(lpInput  != 0, "lpInput != NULL");    // :162

    // ---- 0x82344F74..0x82344FE4 : the one-shot boost fill ----------------------------------
    if (mbNeedToFillBoost)
    {
        // The console fires the interface's own "Player car index hasn't been set" tripwire here
        // (BrnRaceCarEntityModuleOutputInterface.h:980) because it inlined the accessor; calling
        // the accessor reproduces it (its committed body carries the same diagnostic), so it is
        // NOT restated at this call site.
        SetBoostActionRecord lSetBoostAction;
        lSetBoostAction.meRaceCarIndex = lpActiveRaceCars->GetPlayerActiveRaceCarIndex();
        lSetBoostAction.mxFlags        = KI_SET_BOOST_FLAG_AMOUNT;
        lSetBoostAction.mfBoostAmount  = KF_FULL_BOOST_AMOUNT;
        // miBoostSegments / mbInfiniteBoost are deliberately NOT written -- the console posts the
        // 20-byte record with those 8 bytes still holding stack residue (only three stores precede
        // its AddEvent), and mxFlags says only the amount is meaningful.

        lpOutput->GetGameActionQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lSetBoostAction),
            KI_ACTION_SET_BOOST,
            static_cast<s32>(sizeof(lSetBoostAction)));   // X360 li r5,0xAA; li r6,0x14

        mbNeedToFillBoost = false;
    }

    // ---- 0x82344FE8..0x82345004 : the base, all six arguments forwarded ---------------------
    GameMode::PreWorldUpdate(lpOutput, lpInput, lpGlobalRaceCars, lpActiveRaceCars,
                             lbPaused, lpScoringSystem);

    // ---- 0x82345008..0x823451CC : countdown-only ---------------------------------------------
    // `lwz r11, 0x28(r31) ; cmpwi 0 ; bne -> skip` -- GameMode::meCurrentState (+40) == 0 ==
    // E_GMS_COUNTDOWN. Everything below therefore only runs while the player is on the line.
    if (GetCurrentState() != 0)
    {
        return;
    }

    // 0x82345014..0x82345038. The console calls PreWorldInputBuffer::GetTimerStatusInterface
    // (0x8231CE28) and reads the SECOND 0x18-byte entry: `lfs f13, 0x20(r11)` and
    // `lfs f12, 0x1C(r11)` are maEntries[1].mfValue08 and maEntries[1].mfValue04
    // (0x1C == 0x18 + 0x04, 0x20 == 0x18 + 0x08), then `fmadds f0, f13, f12, f0`.
    // FLAG: this tree's TimerStatusInterface::Entry members are still positional names
    // (miWord00/mfValue04/mfValue08/...) -- the PAIR is the console's per-frame timer delta times
    // its scale; when that record is properly named, rename here too.
    const GameStateModuleIO::TimerStatusInterface* lpTimerStatus = lpInput->GetTimerStatusInterface();
    mfCountdownTimer += lpTimerStatus->maEntries[1].mfValue08 * lpTimerStatus->maEntries[1].mfValue04;

    // 0x8234503C..0x823450C4 (first GetPlayerDirection + normalise + dot).
    // The console flattens the car's forward vector onto the XZ plane, normalises it, and dots it
    // with mStartDir (which Start already normalised).
    Vector3 lFlattenedPlayerDirection = lpActiveRaceCars->GetPlayerDirection();
    lFlattenedPlayerDirection.y = 0.0f;
    const f32 lfDirectionDot =
        rw::math::vpu::Dot(rw::math::vpu::Normalize(lFlattenedPlayerDirection), mStartDir);

    // 0x823450C8..0x82345164 (second GetPlayerDirection -- the RAW one, pitch included -- then
    // the cross product, the signed magnitude and XMVectorASin).
    // The console computes `Cross(playerDirection, mStartDir)` with the two-permute/vnmsubfp
    // form, takes its magnitude, applies the SIGN OF THE CROSS PRODUCT'S Y LANE via two vsel
    // (`+1` when y > 0, `0` when y == 0, `-1` when y < 0), and feeds that signed magnitude to
    // asin. For near-unit inputs |cross| == |sin(angle)|, so this is the signed angle between the
    // two directions about the world up axis.
    const Vector3 lCrossProduct = rw::math::vpu::Cross(lpActiveRaceCars->GetPlayerDirection(), mStartDir);

    f32 lfSign = 0.0f;
    if (lCrossProduct.y > 0.0f)
    {
        lfSign = 1.0f;
    }
    else if (lCrossProduct.y < 0.0f)
    {
        lfSign = -1.0f;
    }

    const f32 lfSignedAngleRadians =
        static_cast<f32>(std::asin(static_cast<double>(lfSign * rw::math::vpu::Magnitude(lCrossProduct))));

    // 0x82345168..0x823451CC -- the three-way OR the console builds with two fall-through
    // branches into `li r11,1`.
    // [!] ASYMMETRY IS THE CONSOLE'S, NOT A TRANSCRIPTION SLIP: the angle test is
    // `angle < 5 degrees && angle > 0` -- STRICTLY POSITIVE. A player rotated the other way (the
    // cross product's Y lane negative, so a negative angle) does NOT satisfy this arm and has to
    // fall through to the speed test or the 6-second timeout. The asm is
    // `bge -> fallback ; ble(0.0) -> fallback ; dot <= 0 -> fallback` at 0x82345174..0x8234518C;
    // there is no absolute value anywhere in the sequence. Do not "fix" it.
    const bool lbFacingStartDirection = (lfSignedAngleRadians < KF_START_DIRECTION_TOLERANCE_RADIANS) &&
                                        (lfSignedAngleRadians > 0.0f) &&
                                        (lfDirectionDot > 0.0f);

    // 0x82345190..0x823451AC. sub_82310240 == GetPlayerRaceCarState (see the identification
    // banner in BrnRCEntityActiveRaceCarOutputInterface.cpp); +0x3CC off the returned element is
    // RaceCarState::mfSpeedMPH. Already moving fast enough -> stop holding the player.
    const f32 lfPlayerSpeedMPH = lpActiveRaceCars->GetPlayerRaceCarState()->mfSpeedMPH;

    mbPlayerPointingInStartDirection =
        lbFacingStartDirection ||
        (std::fabs(lfPlayerSpeedMPH) > KF_MOVING_FAST_ENOUGH_MPH) ||
        (mfCountdownTimer > KF_MAX_COUNTDOWN_HOLD_SECONDS);
}

// ============================================================================================
// StuntAttackMode::ShouldFinish -- X360 0x823162B8 (vtable slot 14, vtbl+56)
// ============================================================================================
// Polled every frame by ModeManager::UpdateCurrentMode and ModeManager::PreWorldUpdate through
// `(*(*mode+56))(mode, &mScoringSystem)`. The whole body is eight instructions:
//
//   lbz  r11, 0x37A(r4)         ; scoring +890
//   bne  -> loc_823162FC        ; set  -> zero the two timers and answer false
//   lfs  f13, 0x5CF4(r4)        ; scoring +23796 == mfPlayerTimeWithoutInput
//   lfs  f0,  flt_820211D4      ; image.bin @0x211D4 == 40 80 00 00 == 4.0f
//   ble  -> false
//   lfs  f13, 0x5CF0(r4)        ; scoring +23792 == mfPlayerTimeStationary
//   lfs  f0,  flt_82020F90      ; image.bin @0x20F90 == 40 40 00 00 == 3.0f
//   bgt  -> true                ; else false
//   loc_823162FC: lfs f0, flt_82001CC0 (== 0.0f) ; stfs -> 0x5CF4 ; stfs -> 0x5CF0 ; return 0
//
// OFFSET -> NAME, both halves re-derived rather than left as raw offsets:
//   * +23796 / +23792 are mfPlayerTimeWithoutInput / mfPlayerTimeStationary -- the SAME pair
//     GameMode::ShouldExit @0x82315B80 reads (see its banner in BrnGameMode.cpp), reached here
//     through the four committed named accessors Get/ResetPlayerNoInputTime and
//     Get/ResetPlayerStationaryTime (BrnScoringSystem.h:609-612).
//   * +890 (0x37A) is INSIDE the embedded offline stunt scorer: BrnScoringSystem.h:710 pins
//     mStuntModeScoring at ss+0x350, and 0x350 + 0x2A == 0x37A. StuntModeScoring +0x2A is
//     mbComboInProgress -- pinned independently by the one-line getter
//     StuntModeScoring::IsComboInProgress @0x82313510, whose entire body is `lbz r3, 0x2A(r3); blr`.
//     So the console's guard is "a stunt COMBO is still running": while it is, the idle timers are
//     held at zero and the mode cannot finish.
//   * The OFFLINE scorer (ss+0x350) is the right one -- StuntAttackMode is an offline mode; the
//     online twin lives at ss+0x2620 and is reached by GetOnlineStuntScorer().
//
// The two float thresholds are the same authored constants ShouldExit uses (4 s without input,
// 3 s stationary), so a stunt race ends on the SAME idle rule the base would have exited on --
// which is exactly why StuntAttackMode also overrides ShouldExit to false (slot 13): the event
// must FINISH (results, scoring, progression) rather than silently exit.
// ============================================================================================
bool StuntAttackMode::ShouldFinish(ScoringSystem* lpScoringSystem)
{
    const f32 KF_MAX_NO_INPUT_TIME_FOR_MODE_FINISH   = 4.0f;   // image.bin @0x211D4
    const f32 KF_MAX_STATIONARY_TIME_FOR_MODE_FINISH = 3.0f;   // image.bin @0x20F90

    if (lpScoringSystem->GetStuntScorer()->IsComboInProgress())
    {
        // `stfs f0(0.0), 0x5CF4(r4)` then `stfs f0, 0x5CF0(r4)` -- order preserved.
        lpScoringSystem->ResetPlayerNoInputTime();
        lpScoringSystem->ResetPlayerStationaryTime();
        return false;
    }

    return (lpScoringSystem->GetPlayerNoInputTime()    > KF_MAX_NO_INPUT_TIME_FOR_MODE_FINISH) &&
           (lpScoringSystem->GetPlayerStationaryTime() > KF_MAX_STATIONARY_TIME_FOR_MODE_FINISH);
}
}
