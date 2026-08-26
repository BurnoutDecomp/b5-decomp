#include "GameSource/GameState/ModeManager/GameModes/BrnOnlineStuntRunMode.h"

#include <cmath>   // std::acos (the de-optimised XMVectorACos)

#include "GameShared/GameClasses/Core/CgsAssert.h"                            // CGS_ASSERT
#include "GameSource/GameState/ModeManager/GameModes/BrnGameModeParams.h"     // StartGameModeParams / GameModeParams / EGameModeStartMechanism
#include "GameSource/GameState/ModeManager/BrnModeManager.h"                  // ModeManager (GetNetworkRoundManager / SetOnlineRaceCars / GetCurrentGameModeType / GetTrafficData)
#include "GameSource/GameState/NetworkRoundManager/BrnNetworkRoundManager.h"  // NetworkRoundManager (GetNetworkGameEvent / GetNetworkRoundEvent / GetCurrentRound / GetTotalRounds)
#include "GameSource/GameState/BrnGameEvents.h"                               // StartNetworkGameEvent / StartNetworkRoundEvent layouts
#include "GameSource/GameState/BrnGameStateSharedIO.h"                        // GameStateModuleIO::EGameModeType / EPlayerTeam
#include "GameSource/GameState/ModeManager/Scoring/BrnScoringSystem.h"        // ScoringSystem / CarData / OnlineStuntRunModeScoring
#include "GameSource/BurnoutConstants.h"                                      // EActiveRaceCarIndex
#include "SharedClasses/Traffic/BrnTrafficDataResourceType.h"                 // BrnTraffic::TrafficData (GetHull)
#include "SharedClasses/Traffic/BrnTrafficHull.h"                             // BrnTraffic::Hull
#include "SharedClasses/Traffic/BrnTrafficLightTrigger.h"                     // BrnTraffic::LightTriggerStartData
#include "rw/math/vpu/vector3_operation.h"                                    // rw::math::vpu Dot / Normalize / operator-

namespace BrnGameState
{
namespace
{
    // ---- runtime-initialised .data tuning floats the X360 build reads from the high 0x82CDB7xx
    // region. Those bytes are NOT carried in the available IDA exports (runtime-init, not .rodata),
    // so the literals below are named, honestly-FLAGGED stand-ins (provably not 0.0 -- the timer is
    // armed to one and counted down to compare against zero). Same convention the committed sibling
    // OnlineRaceMode.cpp uses for its unresolved .data floats. INTEGRATOR: resolve from the XEX.
    // RESOLVED 2026-08-26 from the image (offset VA-0x82000000, BE; the 0x82CDB7xx region is
    // verified-static, not dyn-init):
    //   flt_82CDB7B8 = 0x47D2F000 = 108000.0f  -- ⚠ SUSPECT NAME: at this magnitude it reads
    //     like a SCORE threshold, not seconds; the ShouldFinish comparison below that consumes
    //     it as elapsed-time needs re-deriving against the 0x8233A3F0 export before trust.
    //   flt_82CDB7B4 = 0x42B40000 = 90.0f      -- the online stunt run's mode time limit
    //     (written to GameModeParams+0x6C mfModeTimeLimit in Start below).
    const f32 KF_STUNT_RUN_TIME_SECONDS = 108000.0f; // flt_82CDB7B8 (see SUSPECT note above)
    const f32 KF_SCORING_GRACE_SECONDS  = 90.0f;     // flt_82CDB7B4 (the 90 s clock; name kept
                                                     // for the ShouldFinish use, same float)

    // flt_82020F98 == 5.0: the hard cap ShouldFinish clamps the remaining time down to once the only
    // still-playing team is the winning team (so the run wraps up promptly). Low-region .rodata,
    // recoverable from the asm-resolved literal.
    const f32 KF_END_RUSH_CLAMP_SECONDS = 5.0f;

    // flt_82021210 == 0.40000001f: the stunt-run traffic-density scale Start writes (the exact
    // single-precision store the X360 emits). Low-region .rodata, asm-resolved.
    const f32 KF_STUNT_RUN_TRAFFIC_DENSITY = 0.40000001f;

    // flt_82020AFC: the min-angle search seed (FLT_MAX). Low-region .rodata, asm-resolved.
    const f32 KF_FLT_MAX = 3.4028235e38f;

    // The online start flag the X360 build ORs into GameModeParams::muFlags (asm: `li r12,1;
    // extldi r12,r12,64,36` -> bit 36). Bit 36 is one past the committed KU_FLAG_* table (which
    // stops at bit 35, KU_FLAG_ALLOW_REVENGE_TAKEDOWNS). FLAGGED as a documented file-local mask
    // rather than growing the committed flag table on a guessed name (ODR). INTEGRATOR: name it.
    const u64 KU_FLAG_ONLINE_STUNT_RUN_START = 0x1000000000ull; // bit 36 (UNNAMED in committed table)

    // Max start lights gathered for one junction (X360 assert "liNumLightsFound <
    // KI_MAX_LIGHTS_IN_ONE_JUNCTION", bound 8).
    const s32 KI_MAX_LIGHTS_IN_ONE_JUNCTION = 8;

    // X360 asserts in GetBestStartGridID: "luHull < KU_MAX_HULLS" (0x190) and
    // "luLightTriggerIndex < 256".
    const u32 KU_MAX_HULLS               = 0x190;
    const u32 KU_MAX_LIGHT_TRIGGER_INDEX = 0x100;

    // NOTE: the team-id space ShouldFinish scans (the X360 loop runs teams 0..8, assert
    // "leEnumIndex <= E_PLAYER_TEAM_COUNT" fires past 9) is the committed BrnGameState::
    // KI_MAX_PLAYER_TEAMS (== 9, from BrnOnlineStuntRunModeScoring.h) -- reused, not redefined.
}

// X360: BrnGameState::OnlineStuntRunMode::GetName (0x827E25B0).
const char* OnlineStuntRunMode::GetName() const
{
    return "OnlineStuntRun";
}

// X360: BrnGameState::OnlineStuntRunMode::GetOutroTimeout (0x823162A8). Returns the fixed outro
// timeout (flt_82021230 == 15.0). DWARF base shape is f32; Hex-Rays widens the FP return.
f32 OnlineStuntRunMode::GetOutroTimeout() const
{
    return 15.0f;
}

// X360: BrnGameState::OnlineStuntRunMode::PreWorldUpdate (0x82331A30). Runs the GameMode base
// pre-world pass, then -- once the scoring system's mode timer has started -- ticks the stunt-run
// countdown down by the elapsed frame time.
//
// SIGNATURE WIDENED 2026-08-26 (wave-B fix round): this overrides GameMode vtable slot 2, which the
// console dispatches with SIX arguments (UpdateCurrentMode @0x82350EC8:
// `(*(**(a1+3480)+8))(mode, a2, a3, a8, a28, a30, a1+3504)`). The previous declaration was no-arg,
// matching the then-committed base; once the base was corrected to the console shape, a no-arg
// override would have silently minted a NEW vtable slot instead of binding to slot 2.
//
// [!] STILL PARKED -- the countdown decrement. The X360 body gates on a per-frame
// PreWorldInputBuffer flag (>= 0) and then does
//     mfTimeRemaining -= lpInput->GetTimerStatusInterface()
//                            ->maEntries[1].mfValue04 * ...->maEntries[1].mfValue08;
// The ARGUMENT is now present, but GameStateModuleIO::PreWorldInputBuffer is forward-declared only
// in this header's include set (BrnGameMode.h keeps BrnGameStateModuleIO.h out on purpose -- see
// the dual-enum ban there), so the accessor chain cannot be walked from this TU as it stands.
// DELETE-WHEN: this TU includes BrnGameStateModuleIO.h and the TimerStatusInterface entry-1 pair
// is reachable; then the decrement replaces this comment. While parked, mfTimeRemaining holds its
// armed value, so ShouldFinish below never trips on the countdown.
void OnlineStuntRunMode::PreWorldUpdate(GameStateModuleIO::OutputBuffer* lpOutput,
                                        const GameStateModuleIO::PreWorldInputBuffer* lpInput,
                                        const BrnWorld::RaceCarEntityModuleIO::RCEntityGlobalRaceCarOutputInterface* lpGlobalRaceCars,
                                        const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpActiveRaceCars,
                                        bool lbPaused,
                                        const ScoringSystem* lpScoringSystem)
{
    // The console forwards its own arguments straight through to the base pass.
    OnlineGameMode::PreWorldUpdate(lpOutput, lpInput, lpGlobalRaceCars, lpActiveRaceCars,
                                   lbPaused, lpScoringSystem);
}

// X360: BrnGameState::OnlineStuntRunMode::ShouldFinish (0x8233A3F0).
//
// Returns true only once the stunt-run countdown has run out. Otherwise, after the scoring grace
// window has elapsed, it scans every (non-winning, unless the mode is ending) team for a player who
// is still in (not eliminated and not disconnected): the moment one is found the run keeps going;
// if no non-winning team has any live player left, the remaining time is clamped down so the run
// wraps up promptly. The grace branch always returns false -- the only finishing path is the timer
// reaching zero.
bool OnlineStuntRunMode::ShouldFinish(ScoringSystem* lpScoringSystem)
{
    if (mfTimeRemaining <= 0.0f)   // flt_82001CC0 == 0.0
    {
        return true;
    }

    if ((KF_STUNT_RUN_TIME_SECONDS - mfTimeRemaining) >= KF_SCORING_GRACE_SECONDS)
    {
        // The leading team's id. Players on it are not what keeps the run alive (unless the mode is
        // in its end phase). Reached through the embedded online stunt-run scorer (ScoringSystem
        // +0x4D44 == GetOnlineStuntRunScorer()).
        const s32 liWinnerTeam = lpScoringSystem->GetOnlineStuntRunScorer()->GetWinnerTeam(lpScoringSystem);

        bool lbLivePlayerRemains = false;

        for (s32 liTeam = 0; liTeam < KI_MAX_PLAYER_TEAMS; ++liTeam)
        {
            // The X360 build asserts the team index stays within the per-player team bound at the
            // loop tail.
            CGS_ASSERT(liTeam <= KI_MAX_PLAYER_TEAMS, "leEnumIndex <= E_PLAYER_TEAM_COUNT");

            // Skip the winning team unless the mode is ending (E_MODE_ONLINE_MODE_END == 17). The
            // X360 reads the current mode type off the ModeManager (+0xD94).
            if (liTeam == liWinnerTeam
                && GetModeManager()->GetCurrentGameModeType() != GameStateModuleIO::E_MODE_ONLINE_MODE_END)
            {
                continue;
            }

            s32 liMember = static_cast<s32>(lpScoringSystem->GetNextTeamMember(
                E_ACTIVE_RACE_CAR_INDEX_INVALID,
                static_cast<GameStateModuleIO::EPlayerTeam>(liTeam)));

            while (liMember != -1)
            {
                const CarData* lpCarData =
                    lpScoringSystem->GetCarData(static_cast<EActiveRaceCarIndex>(liMember));
                CGS_ASSERT(lpCarData, "lpCarData");

                // X360 reads the per-car eliminated flag (CarScoreData +0xD9) and the disconnected
                // flag (CarScoreData +0x69), both through the embedded score record.
                if (!lpCarData->GetScoreData()->GetEliminated()
                    && !lpCarData->GetScoreData()->GetDisconnected())
                {
                    lbLivePlayerRemains = true;
                    break;
                }

                liMember = static_cast<s32>(lpScoringSystem->GetNextTeamMember(
                    static_cast<EActiveRaceCarIndex>(liMember),
                    static_cast<GameStateModuleIO::EPlayerTeam>(liTeam)));
            }

            if (lbLivePlayerRemains)
            {
                break;
            }
        }

        // No non-winning team still has a live player -> hurry the run to a close.
        if (!lbLivePlayerRemains && mfTimeRemaining > KF_END_RUSH_CLAMP_SECONDS)   // flt_82020F98 == 5.0
        {
            mfTimeRemaining = KF_END_RUSH_CLAMP_SECONDS;
        }
    }

    return false;
}

// X360: BrnGameState::OnlineStuntRunMode::Start (0x82339E70).
//
// Builds the mutable online stunt-run GameModeParams from the immutable StartGameModeParams plus the
// per-player data cached on the NetworkRoundManager's StartNetworkGameEvent, seeds the per-event
// random start-grid shuffle (mRandom, only on the first round), picks the best-aligned start light,
// places the online race cars and arms the countdown timer.
//
// The Hex-Rays output is flagged "local variable allocation has failed" -- the giant inlined block
// in the middle is CgsNumeric::Random::Construct (the LCG default-seed + 8-slot float-buffer prime,
// the same 0xC87CD8C91AD0891B / 0x4C957F2D5851F42D constants) immediately re-seeded from the game's
// shared random seed; de-inlined here to mRandom.Construct() + mRandom.SetSeed(). The raw-offset
// pokes on the X360 GameModeParams layout are restored to the committed named members/setters by
// member meaning (the committed byte offsets differ from the console, so the mapping is semantic,
// exactly as the sibling modes do).
void OnlineStuntRunMode::Start(const StartGameModeParams* lpStartGameModeParams,
                               GameModeParams*            lpGameModeParams,
                               ScoringSystem*             /*lpScoringSystem*/)
{
    // v6 in the pseudocode: the cached NetworkRoundManager (reached through the ModeManager, X360
    // *(this+0xA0)->+0x6D64). The StartNetworkGameEvent is embedded at its offset 0, so the X360
    // reads of words 0..255 hit the event while words 296/300 hit the manager's round counters.
    const NetworkRoundManager* lpNetworkRoundManager = GetModeManager()->GetNetworkRoundManager();
    const GameStateModuleIO::StartNetworkGameEvent* lpStartNetworkGameEvent =
        lpNetworkRoundManager->GetNetworkGameEvent();

    // Re-roll the random start grid only on the very first round of the game.
    // [stuntrace waveB fix round, 2026-08-26] CORRECTED (verify batch 5 MF4). The console at
    // 0x82339E9C..0x82339EB8 is
    //     lwz r11,0x12C(nrm) / lwz r10,0x128(nrm) / subf r11,r10,r11 / addi r11,r11,-1
    //     / cntlzw / extrwi          ->  (miTotalRounds - miRoundsRemaining - 1) == 0
    // and that whole expression IS NetworkRoundManager::GetCurrentRound() under the round-accessor
    // ruling now landed in BrnNetworkRoundManager.cpp. The previous spelling here,
    // `GetTotalRounds() - GetCurrentRound() == 1`, evaluates to `miRoundsRemaining + 1 == 1`, i.e.
    // it fired on the LAST round instead of the first.
    const bool lbFirstRound = (lpNetworkRoundManager->GetCurrentRound() == 0);

    // `stb r27, 0xAC(r29)` @0x82339EA4 -> *(this+172) = 1.
    // [!] NAME CORRECTED 2026-08-26 (wave-B fix round): +0xAC is mbIsOnline, not `mbConstructed`.
    // OfflineGameMode::Construct @0x8232FE78 stores 0 into the same byte and OnlineGameMode::
    // Construct @0x8232FEB4 stores 1; no OFFLINE mode's Start touches +0xAC, while all three
    // ONLINE Start bodies re-assert it. See the +160..+179 table in BrnGameMode.h.
    mbIsOnline = true;

    lpGameModeParams->Construct(lpStartGameModeParams->GetGameModeType());

    lpGameModeParams->mbIsOnline = true;                                    // *(a3+148) = 1

    // muFlags |= KU_FLAG_CLEAR_NEARBY_TRAFFIC (0x800).
    lpGameModeParams->SetFlag(GameModeParams::KU_FLAG_CLEAR_NEARBY_TRAFFIC);

    lpGameModeParams->SetTrafficDensityScale(KF_STUNT_RUN_TRAFFIC_DENSITY);  // *(a3+48) = 0.40000001
    // *(a3+0x840)=0 / *(a3+0x844)=0 are deep GameModeParams fields already cleared by Construct;
    // not re-emitted as raw pokes.
    lpGameModeParams->mbInfiniteBoost = false;                              // *(a3+316) = 0
    lpGameModeParams->SetProgressionRankAsRatio(0.0f);                      // *(a3+4) = 0.0 (flt_82001CC0)
    lpGameModeParams->SetStartMechanism(E_GAMEMODESTARTMECHANISM_SPIN_WHEELS_AT_LIGHTS); // *(a3+60) = 2
    lpGameModeParams->meOnlineBoostStrategy =
        static_cast<EBoostType_Stub>(lpStartNetworkGameEvent->meBoostType); // *(a3+320) = *(v6+236)
    // *(a3+108) = flt_82CDB7B4 (90.0f): +0x6C is mfModeTimeLimit under the pinned medal run
    // (+0x60 bronze / +0x64 silver / +0x68 gold / +0x6C time limit -- StuntAttackMode::Start's
    // own 120.0f default lands at 0x6C, `stfs f0,0x6C(r31)` @0x823322A4). This is the online
    // stunt run's 90-second clock, not a bronze threshold. Fixed 2026-08-26 closure verify.
    lpGameModeParams->mfModeTimeLimit = KF_SCORING_GRACE_SECONDS;

    // The online start flag is added when this mode reports a loading screen and the START PARAMS
    // event type is the online-fugitive mode (12), or unconditionally for the mode-end id (17). The
    // gate reads StartGameModeParams::GetGameModeType() (X360 `lwz r11, 0x2D0(r25)`), NOT the event.
    const GameStateModuleIO::EGameModeType leGameModeType = lpStartGameModeParams->GetGameModeType();
    if ((HasLoadingScreen() && leGameModeType == GameStateModuleIO::E_MODE_ONLINE_FUGITIVE)
        || leGameModeType == GameStateModuleIO::E_MODE_ONLINE_MODE_END)
    {
        lpGameModeParams->SetFlag(KU_FLAG_ONLINE_STUNT_RUN_START);
    }

    // Pick the best-aligned start light for the round's light-trigger junction (the X360 reads the
    // cached StartNetworkRoundEvent::mLightTriggerID, round mgr +0x120). The reference direction is
    // the zero vector, matching the call's `vspltisw v1, 0`.
    const GameStateModuleIO::StartNetworkRoundEvent* lpStartNetworkRoundEvent =
        lpNetworkRoundManager->GetNetworkRoundEvent();
    lpGameModeParams->SetTrafficLightTriggerId(
        GetBestStartGridID(lpStartNetworkRoundEvent->mLightTriggerID, lpStartGameModeParams,
                           Vector3{ 0.0f, 0.0f, 0.0f, 0.0f }));

    // Traffic off -> density 0; traffic-checking off -> disable traffic checking.
    if (!lpStartNetworkGameEvent->mbIsTrafficOn)
    {
        lpGameModeParams->SetTrafficDensityScale(0.0f);                     // *(a3+48) = 0.0
    }
    if (!lpStartNetworkGameEvent->mbIsTrafficCheckingOn)
    {
        lpGameModeParams->SetFlag(GameModeParams::KU_FLAG_TRAFFIC_CHECKING_NOT_ALLOWED); // |= 0x80000
    }

    // Prime the per-event random start-grid generator (only on the first round). The big inlined LCG
    // block in the X360 body is Random::Construct (default-seed prime, asm constant
    // 0xC87CD8C91AD0891B) immediately re-seeded from the game's shared random seed
    // (event +0x0C, muRandomSeedForGame) so the grid shuffle is deterministic across all peers.
    if (lbFirstRound)
    {
        mRandom.Construct();
        mRandom.SetSeed(lpStartNetworkGameEvent->muRandomSeedForGame);
    }

    // Copy the eight per-player team assignments from the start event into the mode params (X360
    // copies event +0x78..+0x94 -> GameModeParams +0x118..+0x134; event +0x78 == maePlayerTeam[8]).
    for (s32 liPlayer = 0; liPlayer < GameStateModuleIO::KI_MAX_RACE_CARS; ++liPlayer)
    {
        lpGameModeParams->maePlayerTeam[liPlayer] =
            static_cast<EPlayerTeam_Stub>(lpStartNetworkGameEvent->maePlayerTeam[liPlayer]);
    }

    // The event/junction ids come straight off the immutable start params (a2 words 202/204).
    lpGameModeParams->muEventJunctionID = lpStartGameModeParams->GetEventJunctionId();
    lpGameModeParams->muJunctionID      = lpStartGameModeParams->GetJunctionID();

    // Lay the grid out with the shuffle and place the online cars.
    GetModeManager()->SetupOnlineStartingGrid(lpGameModeParams, lpStartNetworkGameEvent->miNumRaceCars,
                                              &mRandom, false);
    GetModeManager()->SetOnlineRaceCars(lpGameModeParams, lpStartNetworkGameEvent);

    // Arm the countdown timer.
    mbHasCheckedFinish = false;                       // *(this+244) = 0
    mfTimeRemaining    = KF_STUNT_RUN_TIME_SECONDS;    // *(this+240) = flt_82CDB7B8
}

// X360: BrnGameState::OnlineStuntRunMode::GetBestStartGridID (0x82331708).
//
// Walks the current track's TrafficData hull light-trigger start blocks for the junction encoded in
// luTriggerId and returns the packed light-trigger id whose start direction best aligns (minimum
// angle) with lv3Reference.
LightTriggerId OnlineStuntRunMode::GetBestStartGridID(LightTriggerId luTriggerId,
                                                      const StartGameModeParams* /*lpStartGameModeParams*/,
                                                      Vector3 lv3Reference)
{
    // The packed id splits into a hull index (bits 8..23) and a junction byte (bits 0..7).
    const u32 luHull     = (luTriggerId >> 8) & 0xFFFFu;
    const u8  luJunction = static_cast<u8>(luTriggerId & 0xFFu);

    // Resolve the live traffic data + hull. The X360 chain is
    //   (*(ModeManager+0x6D60)+0x640)->GetMemoryStructure()  ->  mpapHulls[luHull]
    // de-inlined to ModeManager::GetTrafficData() (the intermediate streaming holder is un-homed --
    // see the FLAG on that accessor) + the committed TrafficData::GetHull / BrnTraffic::Hull.
    const BrnTraffic::TrafficData* lpTrafficData = GetModeManager()->GetTrafficData();
    const BrnTraffic::Hull* lpHull = lpTrafficData->GetHull(luHull);

    CGS_ASSERT(lpHull->muNumLightTriggers == lpHull->muNumLightTriggersStartData,
               "lpHull->muNumLightTriggers == lpHull->muNumLightTriggersStartData");

    // The junction byte from the trigger id is itself an index into the per-light junction-lookup
    // table; the entry there is the JUNCTION-GROUP id whose lights we gather. The X360 reads
    // luGroup = mpaLightTriggerJunctionLookup[luJunction] (`lbzx r31, luJunction, +0x3C`) and then,
    // in the gather loop, keeps every light whose own lookup entry equals luGroup.
    const u8 luJunctionGroup = lpHull->mpaLightTriggerJunctionLookup[luJunction];

    // Gather every light trigger in this hull that belongs to the same junction group, recording its
    // packed id, start position and start direction.
    LightTriggerId aPackedIds[KI_MAX_LIGHTS_IN_ONE_JUNCTION];
    Vector3        aStartPositions[KI_MAX_LIGHTS_IN_ONE_JUNCTION];
    Vector3        aStartDirections[KI_MAX_LIGHTS_IN_ONE_JUNCTION];
    s32            liNumLightsFound = 0;

    for (u32 luLight = 0; luLight < lpHull->muNumLightTriggers; ++luLight)
    {
        if (luJunctionGroup != lpHull->mpaLightTriggerJunctionLookup[luLight])
        {
            continue;
        }

        CGS_ASSERT(liNumLightsFound < KI_MAX_LIGHTS_IN_ONE_JUNCTION,
                   "liNumLightsFound < KI_MAX_LIGHTS_IN_ONE_JUNCTION");
        CGS_ASSERT(luHull < KU_MAX_HULLS, "luHull < KU_MAX_HULLS");
        CGS_ASSERT(luLight < KU_MAX_LIGHT_TRIGGER_INDEX, "luLightTriggerIndex < 256");

        const BrnTraffic::LightTriggerStartData& lStartData = lpHull->mpaLightTriggerStartData[luLight];

        aPackedIds[liNumLightsFound]       = (luHull << 8) | 0x39000000u | luLight;
        aStartDirections[liNumLightsFound] = lStartData.GetStartDirection(0);
        aStartPositions[liNumLightsFound]  = lStartData.GetStartPosition(0);
        ++liNumLightsFound;
    }

    CGS_ASSERT(liNumLightsFound > 0, "liNumLightsFound > 0");

    // Pick the gathered light whose start direction best aligns with the reference: the X360 SIMD
    // block normalises the start direction and the (reference - start position) delta and takes the
    // arc-cosine of their dot, keeping the minimum angle. De-optimised to clean Vector3 math (the
    // rsqrt-refinement steps collapse to exact Normalize/Dot; XMVectorACos -> std::acos).
    s32 liBestLightIndex = -1;
    f32 lfBestAngle      = KF_FLT_MAX;   // flt_82020AFC == FLT_MAX

    for (s32 liLight = 0; liLight < liNumLightsFound; ++liLight)
    {
        const Vector3 lv3Direction   = rw::math::vpu::Normalize(aStartDirections[liLight]);
        const Vector3 lv3ToReference = rw::math::vpu::Normalize(lv3Reference - aStartPositions[liLight]);

        f32 lfCosAngle = rw::math::vpu::Dot(lv3Direction, lv3ToReference);
        if (lfCosAngle < -1.0f) { lfCosAngle = -1.0f; }   // vmaxfp128 v0, v0, -1
        if (lfCosAngle >  1.0f) { lfCosAngle =  1.0f; }   // vminfp128 v1, v0,  1

        const f32 lfAngle = std::acos(lfCosAngle);
        if (lfAngle < lfBestAngle)
        {
            lfBestAngle      = lfAngle;
            liBestLightIndex = liLight;
        }
    }

    CGS_ASSERT(liBestLightIndex >= 0, "liBestLightIndex >= 0");

    return aPackedIds[liBestLightIndex];
}

// X360 vtable slot 24 (vtbl+96): 0x82C296C8 == `li r3,1; blr` at slot 24 of vtable 0x820D08E0,
// against the GameMode base's 0x827E2F38 == `li r3,0; blr`. SetupGameMode @0x8234B158 reads it
// twice and HandleLoadingScreenLoaded @0x8234B8A8 once. Start() above also reads it directly, in
// the online-start-flag gate -- while this override was missing that gate could never fire.
// NOTE: this is the ONLINE stunt run; the OFFLINE StuntAttackMode inherits the base FALSE.
bool OnlineStuntRunMode::HasLoadingScreen() const
{
    return true;
}
}
