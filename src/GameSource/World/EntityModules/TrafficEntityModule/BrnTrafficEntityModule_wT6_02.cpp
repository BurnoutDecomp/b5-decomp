// ============================================================================
// BrnTrafficEntityModule_wT6_02.cpp -- the traffic module's per-event arming.
//
//   TrafficEntityModule::HandlePrepareForModeAction @0x827480D8 (DWARF .cpp 6808)  475 insns
//
// ⭐⭐⭐ THIS IS THE REAL SHOWTIME GATE. mbPlayingShowtimeMode (+0x717DD) has exactly TWO
// writers in the whole ARTIST image: UpdateCrashSlider @0x82715A28's mbDEBUGFakeShowtime
// mirror (bodied, _wT5_01.cpp) and this function. Everything downstream of showtime --
// SpawnShowtimeTraffic, the crash-magnet list, the killzone suppression, the divergent-
// behaviour split -- reads that one byte, so until this function existed a player who
// entered showtime through the actual mode path got ordinary traffic. That is why the
// previous wave had to drive showtime through the console's own "Fake Showtime" debug member
// to measure the spawner at all.
//
// It is also where the mode's traffic personality is set, which is why it feeds the
// responsive-traffic work as much as the showtime work:
//   meGameMode, mbIsOnlineGameMode, mbGameModeAllowsSwerving, mbHardcoreSwerveForMode,
//   mbGameModeAllowsKillzones, mbGameModeClearsTraffic, mbAllowDivergentBehaviour,
//   mfGameModeDensityScale, miBigVehicleAmount, mfSpeedMultiplier, and the four crash-slider
//   scalars.
//
// ---- THE PARAMETER LIST IS THREE, NOT FIVE -------------------------------------------------
// Hex-Rays prints `(int result, unsigned __int8 *a2, int a3, int a4, int a5)`. The prologue
// reads r3/r4/r5 and NOTHING else (`mr r16,r4` @0x827480EC, `mr r31,r3` @0x827480F4,
// `mr r30,r5` @0x827480F8); a4/a5 are decompiler noise from the variadic-looking StrStream
// call in the online-reset warning arm. The console's own assert strings name the two real
// arguments: "lpInput" (.cpp 6808) and "lpPFMAction" (.cpp 6809).
//
// ---- lpGameModeParams IS EMBEDDED, NOT A POINTER -------------------------------------------
// `addi r26, r30, 0x30` then the "lpGameModeParams != NULL" assert (.cpp 6812) on r26: the
// GameModeParams sits INSIDE the action at +0x30, and the console still null-checks the
// interior address (which can only be null if the action itself is at -0x30). The check is
// reproduced, because it is what the binary does; it is not a bug worth "fixing".
// BrnPhysicsModuleGameActions.cpp:109 records the identical `event + 0x30 ; lbz 0x94 ;
// lwz 0x148 ; ld 0x860` triple from the physics module's own copy of this handler.
//
// ---- constants -----------------------------------------------------------------------------
// ⛔ INIT-ORDER CHECKED, BOTH EDGES (2026-08-29). Every literal below is a plain static
// .rdata constant: scanning the ASSEMBLY of all 30,084 exported ARTIST functions finds ZERO
// store instructions targeting flt_820BA5C8 / flt_82001CC0 / flt_820BA5E4 / flt_82001C98 /
// flt_820BA2A8 / flt_820BA5E8 / flt_82004744 / flt_820BA5DC / flt_820BA294. No CRT thunk
// writes them, so no thunk can observe a half-built dependency and there is no init-order
// question to answer -- the image value IS the shipped value, and "recovering a truer value"
// would be the wrong move. Six of the nine are independently confirmed by _wT5_01.cpp's
// already-committed crash-slider table, derived by a different wave from a different function.
// ============================================================================

#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModule.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModuleIO.h"

#include "GameSource/GameState/BrnGameActions.h"                       // PrepareForModeAction
#include "GameSource/GameState/BrnGameStateSharedIO.h"                 // EGameModeType
#include "GameSource/GameState/ModeManager/GameModes/BrnGameModeParams.h"
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarEntityModuleOutputInterface.h"
#include "SharedClasses/Traffic/BrnTrafficDataResourceType.h"          // TrafficData::mpPvs
#include "SharedClasses/Traffic/BrnTrafficPvs.h"                       // Pvs::GetHullIndexForPoint

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"


namespace BrnTraffic
{
namespace
{
    // ---- the crash-slider block this function arms ------------------------------------------
    // SHOWTIME arms the slider ALREADY SPIKED. The three values it writes are literally
    // _wT5_01.cpp's KF_CRASH_SLIDER_SPIKE_* triple -- score 100, decay 0, factor 10 -- which
    // is the state UpdateCrashSlider otherwise only reaches at a spike. Same symbols, same
    // values; named here to match that TU so a reader can see they are one set.
    const f32 KF_CRASH_SLIDER_SPIKE_SCORE         = 100.0f;  // flt_820BA5C8
    const f32 KF_CRASH_SLIDER_SPIKE_DECAY         =   0.0f;  // flt_82001CC0
    const f32 KF_CRASH_SLIDER_SPIKE_FACTOR        =  10.0f;  // flt_820BA5E4
    const f32 KF_CRASH_SLIDER_SPIKE_FINAL_VALUE   =   1.0f;  // flt_82001C98
    const f32 KF_CRASH_SLIDER_SPIKE_GAP           =  30.0f;  // flt_820BA5E8
    const f32 KF_CRASH_SLIDER_SPIKE_GAP_VARIATION =  15.0f;  // flt_820BA2A8

    // The ordinary (non-showtime) slider: it has to earn its score.
    const f32 KF_CRASH_SLIDER_MODE_START_DECAY    =   0.2f;  // flt_82004744
    const f32 KF_CRASH_SLIDER_MODE_START_FACTOR   =   1.5f;  // flt_820BA5DC

    // The start-line traffic sweep. flt_820BA294 / flt_820BA5E4, `lfs f1` / `lfs f2`
    // @0x827486F0..0x827486F4 -- a 200 m radius, 10 m tall cylinder on the player.
    const f32 KF_START_LINE_CLEAR_RADIUS = 200.0f;
    const f32 KF_START_LINE_CLEAR_HEIGHT =  10.0f;

    // `lfs f13, flt_820BA5C8` @0x827481BC then fctiwz, clamped to [0, 100] by the
    // srawi/and/andi sign-mask pair at 0x827481CC..0x827481F8. GameModeParams carries the
    // large-vehicle share as a 0..1 probability; the module keeps it as a 0..100 integer.
    const f32 KF_LARGE_VEHICLE_PERCENT_SCALE = 100.0f;       // flt_820BA5C8
    const s32 KI_LARGE_VEHICLE_PERCENT_MAX   = 100;

    // NAMED LEG GATE, file-local by this cluster's convention.
    // [DIAG] NOT IN THE X360 BINARY. DELETE-WHEN-STABLE.
    inline void LogMissingLeg(bool& lrbAlreadyLogged, const char* lpcLegNameAndReason)
    {
        if (lrbAlreadyLogged)
        {
            return;
        }
        lrbAlreadyLogged = true;

        if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0 && CgsDev::Log::gpDebugPrint != 0)
        {
            *CgsDev::Log::gpDebugPrint
                << "[T6-traffic-leg] TrafficEntityModule leg NOT RECONSTRUCTED, skipped: "
                << lpcLegNameAndReason << " [FLAG PC partial gate]\n";
        }
    }
}

// ----------------------------------------------------------------------------
// TrafficEntityModule::HandlePrepareForModeAction  @ 0x827480D8
//
// Caller: HandleExternalRequests @0x8274B660, the game-action dispatch.
// ----------------------------------------------------------------------------
void TrafficEntityModule::HandlePrepareForModeAction(
    const BrnTrafficIO::InputBuffer_PostPhysics*   lpInput,
    const BrnGameState::GameStateModuleIO::PrepareForModeAction* lpPFMAction)
{
    // 0x827480FC..0x8274816C. Three tripwires, all non-gating on the console: it fires the
    // assert and then dereferences anyway.
    CGS_ASSERT(lpInput != 0, "lpInput");             // baked .cpp 6808
    CGS_ASSERT(lpPFMAction != 0, "lpPFMAction");     // baked .cpp 6809

    const BrnGameState::GameModeParams* lpGameModeParams = lpPFMAction->GetGameModeParams();
    CGS_ASSERT(lpGameModeParams != 0, "lpGameModeParams != NULL");   // baked .cpp 6812

    // ---- the mode identity block, 0x82748170..0x82748280 -----------------------------------
    const BrnGameState::GameStateModuleIO::EGameModeType leGameModeType =
        lpGameModeParams->GetGameModeType();

    meGameMode         = static_cast<s32>(leGameModeType);   // stwx -> +0x717D8
    mbIsOnlineGameMode = lpGameModeParams->mbIsOnline;        // lbz 0x94 -> +0x717DC

    mfGameModeDensityScale = lpGameModeParams->mfTrafficDensityScale;   // lfs 0x30 -> +0x71814

    // 0x827481B8..0x827481F8. probability(0..1) * 100 -> truncate -> clamp to [0, 100]. The
    // console builds the clamp branchlessly out of two sign masks; de-optimised here.
    {
        s32 liBigVehicleAmount = static_cast<s32>(lpGameModeParams->mfLargeVehicleProbability
                                                  * KF_LARGE_VEHICLE_PERCENT_SCALE);
        if (liBigVehicleAmount < 0)
        {
            liBigVehicleAmount = 0;
        }
        if (liBigVehicleAmount > KI_LARGE_VEHICLE_PERCENT_MAX)
        {
            liBigVehicleAmount = KI_LARGE_VEHICLE_PERCENT_MAX;
        }
        miBigVehicleAmount = liBigVehicleAmount;             // -> +0x71820
    }

    // 0x827481FC..0x82748278. Three flag bits out of GameModeParams::muFlags (+0x860). Note
    // the FIRST one is INVERTED: the shipped flag is DISABLE_TRAFFIC_SWERVING, so the module's
    // "allows swerving" member is its negation. The console spells that with a
    // cntlzw-normalise instead of a branch; same value.
    mbGameModeAllowsSwerving =
        !lpGameModeParams->GetFlag(BrnGameState::GameModeParams::KU_FLAG_DISABLE_TRAFFIC_SWERVING);
    mbHardcoreSwerveForMode =
        lpGameModeParams->GetFlag(BrnGameState::GameModeParams::KU_FLAG_HARDCORE_TRAFFIC_SWERVING);
    mbGameModeClearsTraffic =
        lpGameModeParams->GetFlag(BrnGameState::GameModeParams::KU_FLAG_CLEAR_NEARBY_TRAFFIC);

    mfSpeedMultiplier = lpGameModeParams->mfTrafficSpeedScale;          // lfs 0x38 -> +0x72880

    // ---- ⭐ THE SHOWTIME GATE, 0x82748284..0x827482D4 ---------------------------------------
    // `cmpwi r11, 2 / beq ; cmpwi r11, 0x10 / bne` -- 2 and 16 are E_MODE_OFFLINE_SHOWTIME and
    // E_MODE_ONLINE_SHOWTIME (BrnGameStateSharedIO.h). This one store is the entire reason
    // showtime traffic behaves differently from ordinary traffic.
    mbPlayingShowtimeMode =
        (leGameModeType == BrnGameState::GameStateModuleIO::E_MODE_OFFLINE_SHOWTIME)
     || (leGameModeType == BrnGameState::GameStateModuleIO::E_MODE_ONLINE_SHOWTIME);

    // 0x827482B8..0x827482D4. Killzones are an ordinary-traffic mechanic: showtime turns them
    // off. Same cntlzw-normalise, i.e. the exact negation of the byte above.
    mbGameModeAllowsKillzones = !mbPlayingShowtimeMode;                 // -> +0x717E0

    // 0x827482D8..0x827482EC. Online counts network players, offline counts rivals; +1 for the
    // local player either way.
    muNumberOfParticipantsInCurrentEvent = static_cast<u8>(
        (mbIsOnlineGameMode ? lpGameModeParams->miNumNetworkPlayers
                            : lpGameModeParams->miNumRivals) + 1);

    CGS_ASSERT(muNumberOfParticipantsInCurrentEvent > 0,
               "muNumberOfParticipantsInCurrentEvent > 0");                      // .cpp 6842
    CGS_ASSERT(!mbWaitingForStreaming,
               "Shouldn't be waiting for traffic streaming at mode start.");     // .cpp 6844

    // ---- the traffic-reset decision, 0x82748348..0x82748524 --------------------------------
    if (lpGameModeParams->GetFlag(BrnGameState::GameModeParams::KU_FLAG_DISABLE_TRAFFIC_RESET))
    {
        // 0x827484F8..0x82748520. No reset: just settle the divergence policy. Offline the
        // simulation may diverge freely; online it must stay lockstep with the other clients
        // -- UNLESS this is showtime, where every client is watching its own crash anyway.
        mbAllowDivergentBehaviour = (!mbIsOnlineGameMode) || mbPlayingShowtimeMode;
    }
    else if (meState == E_STATE_RUNNING)
    {
        // 0x82748388..0x8274849C. Tear the world's traffic down and remember which hulls to
        // bring back afterwards, so the event does not start inside a dead PVS region.
        mbActivateOnlineHullsAfterReset = true;

        if (lpGameModeParams->GetStartLocationCount() > 0)
        {
            // One hull per participant, taken from that participant's start-grid slot.
            for (u32 luSlot = 0; luSlot < muNumberOfParticipantsInCurrentEvent; ++luSlot)
            {
                const Vector3 lStartPosition =
                    lpGameModeParams->GetStartPosition(static_cast<s32>(luSlot));

                mau16HullsToActivateAfterReset[luSlot] = static_cast<u16>(
                    reinterpret_cast<const Pvs*>(mpData.operator->()->mpPvs)
                        ->GetHullIndexForPoint(lStartPosition));
            }
        }
        else if (meLocalPlayerIndex != E_ACTIVE_RACE_CAR_INDEX_INVALID)
        {
            // 0x82748434..0x8274847C. No grid (a free-roam-style start): one hull, the one the
            // local player is standing in, parked in that player's own slot.
            mau16HullsToActivateAfterReset[meLocalPlayerIndex] = static_cast<u16>(
                reinterpret_cast<const Pvs*>(mpData.operator->()->mpPvs)
                    ->GetHullIndexForPoint(mLocalPlayerPosition));
        }

        // 0x82748480..0x8274849C.
        mbDontCreateVehiclesNearAnyPlayers       = true;
        mbDontCreateStaticVehiclesNearAnyPlayers = true;
        EnterTearingDownState();
    }
    else if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0 && CgsDev::Log::gpDebugPrint != 0)
    {
        // 0x827484A4..0x827484E0. Already resetting: warn and carry on. Reproduced verbatim --
        // it is a real console diagnostic, not a gate.
        *CgsDev::Log::gpDebugPrint
            << "TRAF WARNING: We want to reset the traffic as we're starting an online event, "
               "but we're already resetting!! Our current state is "
            << static_cast<s32>(meState) << "\n";
    }

    if (!lpGameModeParams->GetFlag(BrnGameState::GameModeParams::KU_FLAG_DISABLE_TRAFFIC_RESET)
        && mbIsOnlineGameMode)
    {
        // 0x827484E4..0x827484F4. Both reset arms above fall into this; the DISABLE arm does
        // not. ⚠️ The stored word is 1, and 1 is E_RUNNINGSTATE_PAUSED, not "running" -- an
        // online event that resets its traffic comes back out of start-up PAUSED, and stays
        // that way until something un-pauses it. Reading the `stw r17` (r17 == 1) as "resume"
        // would invert the meaning of the whole arm.
        meRunningStateToUseAfterStartup = E_RUNNINGSTATE_PAUSED;        // stw 1 -> +0x30C
    }

    // ---- traffic-light trigger + start-line protection, 0x82748524..0x82748604 -------------
    mbAtStartLineSoProtectRaceCarsFromTraffic = mbGameModeClearsTraffic;   // -> +0x717E1
    mbNeedToSetUpLightsForEventStart          = mbGameModeClearsTraffic;   // -> +0x717E4
    mTrafficLightTriggerId = lpGameModeParams->mTrafficLightTriggerId;     // lwz 0x40 -> +0x717D4

    // 0x82748550..0x82748584. The id is invalid when its middle 16 bits are all-ones OR its
    // low byte is all-ones. ⚠️ Hex-Rays renders the second test as `v52 == 255` (a whole-word
    // compare); the asm masks first (`clrlwi r11, r11, 24 ; cmplwi r11, 0xFF` @0x82748568),
    // so it is the LOW BYTE. Asm wins.
    {
        const u32 luTriggerId  = mTrafficLightTriggerId;
        const bool lbIsValidId = ((luTriggerId & 0x00FFFF00u) != 0x00FFFF00u)
                              && ((luTriggerId & 0x000000FFu) != 0x000000FFu);

        CGS_ASSERT(lbIsValidId || !mbGameModeClearsTraffic,
                   "The Traffic Light Trigger Id is still invalid");                 // .cpp 6899
    }

    if (mbGameModeClearsTraffic)
    {
        // 0x82748618..0x82748680. Cache the grid so the spawner can keep clear of it.
        for (u32 luSlot = 0; luSlot < muNumberOfParticipantsInCurrentEvent; ++luSlot)
        {
            maEventGridStartPositions[luSlot] =
                lpGameModeParams->GetStartPosition(static_cast<s32>(luSlot));
        }

        mbEnsureTrafficLightDelay     = true;                          // -> +0x717E2
        mfTrafficLightChangeBackDelay = 0.0f;                          // -> +0x71408

        // 0x82748698..0x827486B0. Read unconditionally, used only by the arm below -- the
        // console hoists it out because the accessor asserts and it wants that assert to fire
        // whether or not the sweep runs.
        const Vector3 lPlayerPosition =
            lpInput->GetActiveRaceCarOutputInterface()->GetPlayerPosition();

        // 0x827486B4..0x827486F8. Only on a decision frame, and only while actually running:
        // sweep a 200 m x 10 m cylinder around the player so the grid is not pre-populated
        // with traffic the event is about to launch cars into.
        if (mbAtStartLineSoProtectRaceCarsFromTraffic
            && meState == E_STATE_RUNNING
            && IsDecisionFrame())
        {
            // GATE: KillAllTrafficInCylinder @0x82741C58 (121 insns) has no body in the tree
            // yet. Its signature IS recovered from the prologue -- r3 this, v1 the centre,
            // f1 the radius, f2 the height, r6 a bool (Hex-Rays' a4/a5 are the phantom slots
            // f1/f2 ate) -- so this is a missing BODY, not a missing shape. Gated rather than
            // trap-stubbed on purpose: this arm fires on every clear-nearby-traffic event
            // start, so a __debugbreak() here would turn "showtime is reachable" into "showtime
            // crashes", which is strictly worse than the console's traffic staying put.
            static bool sbLogged = false;
            LogMissingLeg(sbLogged,
                "HandlePrepareForModeAction leg KillAllTrafficInCylinder @0x82741C58 -- no "
                "body; the start-line sweep of a 200 m x 10 m cylinder on the player does not "
                "run, so an event that clears nearby traffic starts with the grid still "
                "populated. Everything else in this handler is live");
            (void)lPlayerPosition;
        }
    }

    // ---- the crash slider, 0x827486FC..0x82748840 ------------------------------------------
    if (mbPlayingShowtimeMode)
    {
        // Showtime opens with the slider ALREADY SPIKED -- score 100, no decay, factor 10 --
        // which is the state UpdateCrashSlider otherwise only reaches when a spike fires.
        mfCrashSliderCrashScore       = KF_CRASH_SLIDER_SPIKE_SCORE;
        mfCrashSliderCrashScoreDecay  = KF_CRASH_SLIDER_SPIKE_DECAY;
        mfCrashSliderCrashScoreFactor = KF_CRASH_SLIDER_SPIKE_FACTOR;
        mfCrashSliderFinalValue       = KF_CRASH_SLIDER_SPIKE_FINAL_VALUE;

        mfShowtimeTimer = 0.0f;

        // 0x82748788..0x827487DC. Same inlined mEffectRand draw UpdateCrashSlider uses, and
        // the same 30..45 s window -- but the console adds the literal 30 here rather than
        // mfShowtimeTimer + 30, which is identical only because the timer was zeroed one
        // instruction earlier. Transcribed as the console spells it.
        const f32 lfSpikeGapJitter = mEffectRand.RandomFloat();

        mfShowtimeTimeNextCrashSpike = KF_CRASH_SLIDER_SPIKE_GAP
                                     + lfSpikeGapJitter * KF_CRASH_SLIDER_SPIKE_GAP_VARIATION;

        mfShowtimeTimeLastCrashSpike = 0.0f;
        mfShowtimeMisBounceTimer     = 0.0f;
    }
    else
    {
        // 0x827487F8..0x82748830. An ordinary mode earns its slider from zero.
        mfCrashSliderCrashScore       = 0.0f;
        mfCrashSliderCrashScoreDecay  = KF_CRASH_SLIDER_MODE_START_DECAY;
        mfCrashSliderCrashScoreFactor = KF_CRASH_SLIDER_MODE_START_FACTOR;
        mfCrashSliderFinalValue       = 0.0f;
    }
}

}
