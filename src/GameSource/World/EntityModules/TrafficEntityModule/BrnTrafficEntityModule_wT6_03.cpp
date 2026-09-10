// ============================================================================
// BrnTrafficEntityModule_wT6_03.cpp -- the traffic module's game-action dispatch.
//
//   TrafficEntityModule::HandleExternalRequests @0x8274B660 (.cpp 5833)  491 insns  PARTIAL
//
// ⭐ WHY THIS EXISTS AS A PARTIAL. HandlePrepareForModeAction (_wT6_02.cpp) is the only
// non-debug writer of mbPlayingShowtimeMode, and this is its ONLY caller. Without it the
// handler is not merely unreached -- it is DISCARDED: /Gy + /OPT:REF drops a function with no
// caller, and it is measurably absent from Burnout_PC.map. So "the showtime gate is bodied"
// and "showtime is reachable through the real mode path" are two different claims, and only
// this file joins them.
//
// PARTIAL, and honestly so. The console's switch has sixteen arms over the post-physics game-
// action queue. Exactly ONE is reconstructed here -- action 23, E_ACTION_PREPARE_FOR_MODE.
// Every other arm needs a callee with no body in this tree (HandleStopModeAction,
// RestartTraffic, HideAllTraffic, UnhideAllTraffic, ClearupCrashedTraffic, FireKillZone,
// KillAllTrafficInCylinder, TrafficLightManager::SetCountdownValue, IsPaused), so each is a
// named gate rather than an invented body or a trap.
//
// ⭐ THIS CANNOT REGRESS ANYTHING, and that is worth stating plainly. Today the WHOLE function
// is gated at its call site in PostPhysicsUpdate (_wT1_01.cpp), so zero arms run. Running one
// real arm and logging the other fifteen is strictly closer to the console than running none.
//
// ---- the switch value is the ACTION ID, not the jump-table index ---------------------------
// IDA labels the arm below "jumptable 8274B7EC case 10", which is the TABLE index; the console
// biases the id by 13 before indexing (`cmplwi r11, 0xE7` @0x8274B7D0 bounds a 232-entry
// table). The real id is 23 -- the same E_ACTION_PREPARE_FOR_MODE that
// RaceCarEntityModule's own dispatch takes (BrnRaceCarEntityModule.cpp:2499). Reading the
// table index as the case label would have wired this handler to action 10.
// ============================================================================

#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModule.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModuleIO.h"

#include "GameSource/GameState/BrnGameActions.h"   // PrepareForModeAction, E_ACTION_* ids

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"


namespace BrnTraffic
{
namespace
{
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
// TrafficEntityModule::HandleExternalRequests  @ 0x8274B660   PARTIAL
//
// Caller: PostPhysicsUpdate @0x8274E6D0, before the state machine runs.
// ----------------------------------------------------------------------------
void TrafficEntityModule::HandleExternalRequests(
    const BrnTrafficIO::InputBuffer_PostPhysics* lpInput,
    BrnTrafficIO::OutputBuffer_PostPhysics*      lpOutput)
{
    // 0x8274B680..0x8274B6D0. Both tripwires, both non-gating on the console.
    CGS_ASSERT(lpInput != 0, "lpInput != NULL");      // baked .cpp 5833
    CGS_ASSERT(lpOutput != 0, "lpOutput != NULL");    // baked .cpp 5834

    // 0x8274B6D4..0x8274B700. `bl BrnTrafficIO::Inp` is InputBuffer_PostPhysics::
    // GetGameActionQueue() const @0x827117A8 (the read-locked &mGameActionQueue at this+62640),
    // and GetFirstEvent RETURNS THE EVENT TYPE while writing the record pointer and its size
    // through the two out-params.
    const BrnTrafficIO::InputBuffer_PostPhysics::GameActionQueueStorage* lpQueue =
        lpInput->GetGameActionQueue();

    const CgsModule::Event* lpEvent = 0;
    s32                     liSize  = 0;
    s32                     liType  = lpQueue->GetFirstEvent(&lpEvent, &liSize);

    while (lpEvent != 0)
    {
        switch (liType)
        {
        // --------------------------------------------------------------------------------
        // 0x8274BB90..0x8274BBE8 -- E_ACTION_PREPARE_FOR_MODE. THE SHOWTIME PATH.
        // --------------------------------------------------------------------------------
        case BrnGameState::GameStateModuleIO::E_ACTION_PREPARE_FOR_MODE:
        {
            const BrnGameState::GameStateModuleIO::PrepareForModeAction* lpPFMAction =
                reinterpret_cast<const BrnGameState::GameStateModuleIO::PrepareForModeAction*>(
                    lpEvent);

            CGS_ASSERT(lpPFMAction != 0, "lpPFMAction != NULL");   // baked .cpp 5852

            // 0x8274BBB0..0x8274BBD4. `stage == 0 || stage == 1`, which IS
            // PrepareForModeAction::IsFirstPrepareForMode() -- an all-in-one prepare and the
            // first of a split pair both arm the mode; the second of two must not re-arm it.
            if (lpPFMAction->IsFirstPrepareForMode())
            {
                HandlePrepareForModeAction(lpInput, lpPFMAction);
            }
            break;
        }

        // --------------------------------------------------------------------------------
        // 0x8274BB18..0x8274BB4C -- E_ACTION_SET_TRAFFIC_SCALE_BASED_ON_RANK (28), a bare f32.
        // `lfs f0, 0(record) ; stfs -> +0x71810 (mfBaseDensityScale)`, and when the simulation
        // is lockstep-free (`lbz +0x717E7`, mbAllowDivergentBehaviour) ALSO `stfs -> +0x71814`
        // (mfGameModeDensityScale), i.e. the rank scale takes effect immediately offline and only
        // at the next ResetEventData online. ProgressionManager::OnEventFinishUpdateProfile posts
        // it at every event finish, which is how the freeburn density is re-published after an
        // event. Landed 2026-09-10 with the STOP_MODE arm below.
        // --------------------------------------------------------------------------------
        case BrnGameState::GameStateModuleIO::E_ACTION_SET_TRAFFIC_SCALE_BASED_ON_RANK:
        {
            CGS_ASSERT(lpEvent != 0, "lpSetTrafficScaleAction");     // baked .cpp 5866
            const f32 lfTrafficScale = *reinterpret_cast<const f32*>(lpEvent);
            mfBaseDensityScale = lfTrafficScale;
            if (mbAllowDivergentBehaviour)
            {
                mfGameModeDensityScale = lfTrafficScale;
            }
            break;
        }

        // --------------------------------------------------------------------------------
        // 0x8274BE3C..0x8274BE44 -- E_ACTION_STOP_MODE (39): `HandleStopModeAction(this, lpInput,
        // record)`. THE arm that ends an event's traffic regime -- without it the event's density
        // scale, clear-traffic flags and start-line protection stayed latched for the rest of the
        // session (BurnoutDecomp/b5-decomp#22).
        // --------------------------------------------------------------------------------
        case BrnGameState::GameStateModuleIO::E_ACTION_STOP_MODE:
            HandleStopModeAction(
                lpInput,
                reinterpret_cast<const BrnGameState::GameStateModuleIO::StopModeAction*>(lpEvent));
            break;

        default:
            break;
        }

        // 0x8274C0B4..0x8274C0CC. Same three-value contract as GetFirstEvent, seeded with the
        // record just handled.
        liType = lpQueue->GetNextEvent(lpEvent, &lpEvent, &liSize);
    }

    {
        // The fifteen arms this partial does not run, each blocked on a callee with no body in
        // this tree. Listed by action id so the next wave can pick them off individually:
        //   13  empty-pool state advance (meEmptyTrafficPoolState IDLE->EMPTY, no callee --
        //       reconstructable today, left out only to keep this file to its one claim)
        //   28  SetTrafficScaleBasedOnRank -- LIVE above (2026-09-10)
        //   30  start-line sweep over every active race car  -> KillAllTrafficInCylinder
        //   34  StartPlayingMode                             -> TrafficLightManager::SetCountdownValue
        //   39  StopMode                                     -- LIVE above (2026-09-10)
        //   47  traffic-light countdown + pause bookkeeping   -> SetCountdownValue / IsPaused
        //   73  crash-camera proximity kill                   -> (inline, needs the +0x7143x block)
        //   75  HideAllTraffic                                -> HideAllTraffic
        //   77  UnhideAllTraffic / cylinder kill              -> UnhideAllTraffic, KillAllTrafficInCylinder
        //   97..100  crash clean-up                           -> ClearupCrashedTraffic, KillAllTrafficInCylinder
        //   110 kill-zone list                                -> FireKillZone
        //   143 predicted-hull reset
        //   192 streaming request / wait latch
        //   225,226,236  RestartTraffic (+ the hull re-activation copy)
        //   244 low-speed density halving
        // and the post-loop tail at 0x8274C0D0 (the +0x72910 bit-14 edge that fires a 90 m
        // KillAllTrafficInCylinder), also blocked on KillAllTrafficInCylinder.
        static bool sbLogged = false;
        LogMissingLeg(sbLogged,
            "HandleExternalRequests -- actions 23 (PREPARE_FOR_MODE), 28 (SET_TRAFFIC_SCALE) and "
            "39 (STOP_MODE) are reconstructed. The other thirteen arms and the post-loop proximity "
            "tail each need a callee with no body in this tree (KillAllTrafficInCylinder, RestartTraffic, "
            "Hide/UnhideAllTraffic, ClearupCrashedTraffic, FireKillZone, "
            "TrafficLightManager::SetCountdownValue, IsPaused). Nothing regresses: the whole "
            "function was gated at its call site until now, so zero arms ran");
    }
}


// ----------------------------------------------------------------------------
// TrafficEntityModule::HandleStopModeAction  @ 0x82716280  (export hole; image-read)
//
//   0x82716298  cmplwi r4, 0 ; bne  -> FireAssert("lpInput", BrnTrafficEntityModule.cpp, 6955)
//   0x827162C0  cmplwi r5, 0 ; bne  -> FireAssert("lpStopModeAction", ..., 6956)
//   0x827162EC  lbzx r11, this, 0x717E7   ; mbAllowDivergentBehaviour
//   0x827162F4  bne -> skip ; else stbx 1, this, 0x7180F   ; mbNeedToKillAllZombies = true
//   0x8271630C  bl ResetEventData
// Only the two asserts read the arguments; the record's contents are not consumed here.
// ----------------------------------------------------------------------------
void TrafficEntityModule::HandleStopModeAction(
    const BrnTrafficIO::InputBuffer_PostPhysics*           lpInput,
    const BrnGameState::GameStateModuleIO::StopModeAction* lpStopModeAction)
{
    CGS_ASSERT(lpInput != 0, "lpInput");                    // baked .cpp 6955
    CGS_ASSERT(lpStopModeAction != 0, "lpStopModeAction");  // baked .cpp 6956

    if (!mbAllowDivergentBehaviour)
    {
        mbNeedToKillAllZombies = true;
    }

    ResetEventData();
}

}
