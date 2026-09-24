// ===================================================================================
// b5-decomp/src/GameSource/GameState/PaybackManager/BrnPaybackManager.cpp
//
// The online payback / dirty-trick manager. Reconstructed from BURNOUT_X360_ARTIST.XEX
// (semantic parity, not byte-matching). The manager's functions that live here are listed at
// each body; the remaining declared members (Prepare/Release/ShowDTAvailableHudNotification/
// UpdateFSMTimers/SetDirtyTrickButtonState/SetTimerInterface) have no body in the tree yet and no
// caller here.
//
// [FX-GS crash-parity 2026-09-23] ResetState + Destruct (G12-D12), the three victim-side arms
// HandleActivePayback / HandleCrashDueToPayback / HandleSurvivingPayback (G12-D8/D9/D10) with
// the X360-inlined helpers they are written through (the victim ChangeState, IsCountdownComplete,
// RemoveCountdown, DirtyTrickEnding), and HandleTriggeringPayback's GUI record through
// DirtyTrickTriggered (G12-D2).
// [FX-GS2 2026-09-23] HandleWaitForPaybackAggressorToCrash reads CrashingRaceCarInterface::
// IsCrashing (b5 27ad7089) instead of a pinned false (G12-D1); HandleWaitingToAwardPayback, the
// aggressor arm [2] that was parked on the same read (G12-D5); HandleAwardingPayback, arm [3],
// with DirtyTrickAwarded, the helper it inlines on the X360 (G12-D6); the victim arm [1]
// HandleReceivingPayback with StartCountdown, which it inlines (G12-D7); OnRoundStart, and
// OnRoundEnd's reseed from the frame count through the repaired Random::SetSeed (G12-D11).
//
// Source-of-truth: X360 ASM (behaviour + calling convention) > DecFIGS DWARF (shape) > none.
// ===================================================================================

#include "GameSource/GameState/PaybackManager/BrnPaybackManager.h"

#include "GameSource/GameState/BrnGameStateModule.h"                     // GameStateModule::Get{Player...,OutputGuiEventQueue}
#include "GameSource/GameState/BrnGameStateModuleIO.h"                   // PreWorldInputBuffer / OutputBuffer accessors
#include "GameSource/GameState/SharedIO/BrnGameStateToGuiIOInterfaces.h" // GameStateToGuiInterface (complete)
#include "GameSource/Network/SharedIO/BrnNetworkModuleGameStateIOInterfaces.h" // GameStateToNetworkInterface (complete; ->GetDirtyTrickQueue)
#include "GameSource/GameState/TakedownManager/BrnTakedownManagerTypes.h" // BrnGameState::TakedownEvent
#include "GameSource/GameState/BrnGameActions.h"                         // GameStateModuleIO::PaybackActivatedAction (215)
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleOutputInterface.h" // BrnPhysics::Vehicle::{VehicleOutputInterface,CrashingRaceCarInterface}
#include "GameShared/GameClasses/Core/CgsAssert.h"                       // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"               // gpDebugPrint ([payback] witness)

namespace BrnGameState
{
    // The GUI/HUD event-type tags the manager posts onto the output queues (X360 immediates). The
    // payload structs are owned by the GUI module; here we post the raw little records the X360
    // copies by byte image (AddEvent memcpys `liSize` bytes), so the tags are modelled as named
    // local constants and the payloads as small PODs.
    namespace
    {
        // module output-GUI-event queue (VariableEventQueue<18432,16>) tags
        const s32 KI_GUI_EVENT_PAYBACK_STATE_CHANGE = 176; // 0xB0, 4-byte s32 flag payload
        const s32 KI_GUI_EVENT_PAYBACK_COUNTDOWN    = 235; // 0xEB, 4-byte f32 payload

        // output-buffer GUI/action queue (VariableEventQueue<13312,16>) tags
        const s32 KI_GUI_EVENT_HAVE_PAYBACK         = 212; // 0xD4, 1-byte bool payload
        const s32 KI_GUI_EVENT_DT_ENDED_ON_YOU      = 217; // 0xD9, 8-byte {aggressor,victim} payload
        // PaybackOverAction (DWARF name, AddGameAction<PaybackOverAction> in both victim end arms):
        // `li r5,0xD8 ; li r6,1` @0x82397E80 (HandleCrashDueToPayback) and @0x82397FA0
        // (HandleSurvivingPayback) -- the same id and size in both.
        const s32 KI_ACTION_PAYBACK_OVER            = 216; // 0xD8, 1-byte payload
        // PaybackLostAction (DWARF `typedef GameAction<E_ACTION_PAYBACK_LOST> PaybackLostAction`,
        // DWARF value 203 -- the PS3 twin posts 203 -- and +8 on the X360): `li r5,0xD3 ; li r6,1`
        // @0x82397AD8/0x82397AD4 (HandleAwardingPayback). A local constant, not the shared enum:
        // BrnGameActions.h still carries the unshifted E_ACTION_ONLINE_PLAYER_ADDED at 211.
        const s32 KI_ACTION_PAYBACK_LOST            = 211; // 0xD3, 1-byte payload

        // The dirty-trick statuses the manager broadcasts. BrnNetwork::EDirtyTrickStatus's shared
        // home names only E_DIRTY_TRICK_NONE, so the X360 immediates are named here after the PS3
        // DecFIGS enumerators (BrnNetworkSharedIO.h:173 E_DIRTY_TRICK_STATUS_AVAILABLE, and
        // E_DIRTY_TRICK_STATUS_SURVIVED / _CRASHED).
        const BrnNetwork::EDirtyTrickStatus KE_DIRTY_TRICK_STATUS_AVAILABLE =
            static_cast<BrnNetwork::EDirtyTrickStatus>(1);   // `li r7,1` @0x82397A88
        const BrnNetwork::EDirtyTrickStatus KE_DIRTY_TRICK_STATUS_SURVIVED =
            static_cast<BrnNetwork::EDirtyTrickStatus>(3);   // `li r7,3` @0x82397F34
        const BrnNetwork::EDirtyTrickStatus KE_DIRTY_TRICK_STATUS_CRASHED  =
            static_cast<BrnNetwork::EDirtyTrickStatus>(4);   // `li r7,4` @0x82397E14

        // A "payback HUD element shown/hidden" flag record (the type-176 s32 payload). The X360
        // posts an s32 0/1 toggle; modelled as a named POD so the queue write is not a bare int.
        struct PaybackStateChangeEvent : public CgsModule::Event
        {
            s32 miShow;
        };

        // The type-217 record: the aggressor + victim active-race-car indices of a dirty trick that
        // just ended on the local player (X360 stores {aggressor,victim} as two s32s; see below).
        struct DirtyTrickEndedOnYouEvent : public CgsModule::Event
        {
            ::EActiveRaceCarIndex meAggressorRaceCarIndex;
            ::EActiveRaceCarIndex meVictimRaceCarIndex;
        };

        // The type-212 record: 1-byte "you have a payback available / lost it" flag.
        struct HavePaybackEvent : public CgsModule::Event
        {
            u8 mu8Flag;
        };

        // The type-235 record: the remaining countdown time (f32).
        struct PaybackCountdownEvent : public CgsModule::Event
        {
            f32 mfRemaining;
        };

        // The type-216 record (PaybackOverAction). The console posts a one-byte stack local it never
        // writes (`addi r4, r1, var_50` with no store to var_50 in either victim end arm): the type id
        // carries the whole meaning, so the byte stays uninitialised here too.
        struct PaybackOverAction : public CgsModule::Event
        {
            u8 muUnused;
        };

        // The type-211 record (PaybackLostAction). Same shape as the one above: HandleAwardingPayback
        // posts `addi r4, r1, var_60` (0x82397ADC) and never stores to var_60, so the byte stays
        // uninitialised here too.
        struct PaybackLostAction : public CgsModule::Event
        {
            u8 muUnused;
        };

    }

    // -----------------------------------------------------------------------------------
    // Construct  @ 0x82377238 (boot-trace EXECUTED)
    // Initialise both FSMs to idle, clear the indices/timers/dirty-trick state, construct the
    // outbound dirty-trick queue, prime the RNG, clear the timer status, and wire + register the
    // embedded payback debug component.
    // -----------------------------------------------------------------------------------
    void
    PaybackManager::Construct(GameStateModule* lpGameStateModule)
    {
        mfCountdownTimer  = -1.0f;
        mfPaybackAggTimer = -1.0f;
        mpGameStateModule = lpGameStateModule;

        mePaybackAggressorState = E_PAYBACK_AGGRESSOR_STATE_IDLE;
        mePaybackVictimState    = E_PAYBACK_VICTIM_STATE_IDLE;

        mePaybackAggressorRaceCarIndex = ::E_ACTIVE_RACE_CAR_INDEX_INVALID;
        mePaybackVictimRaceCarIndex    = ::E_ACTIVE_RACE_CAR_INDEX_INVALID;

        meActiveDirtyTrickType = KE_NO_DIRTY_TRICK;   // raw 3
        meAwardedDirtyTrick    = KE_NO_DIRTY_TRICK;   // raw 3

        mbDirtyTrickButtonDown    = false;
        mbDirtyTrickButtonWasDown = false;
        mbPaybackAwarded          = false;

        // The "cleared" inbound event sentinel the manager keeps as mEvent (X360 stores -1,-1,3,5).
        mEvent.meAggressorActiveRaceCarIndex = ::E_ACTIVE_RACE_CAR_INDEX_INVALID;
        mEvent.meVictimActiveRaceCarIndex    = ::E_ACTIVE_RACE_CAR_INDEX_INVALID;
        mEvent.meDirtyTrickType              = KE_NO_DIRTY_TRICK;                       // 3
        mEvent.meDirtyTrickStatus            = static_cast<BrnNetwork::EDirtyTrickStatus>(5);

        mDirtyTrickOutputQueue.Construct();
        mDirtyTrickOutputQueue.Clear();

        mRdmNumGenerator.Construct();

        mTimerStatusInterface.Clear();

        // Wire + register the embedded debug component (X360: ctor, *(this+632)=this, Register).
        mDebugComponent.mpPaybackManager = this;
        mDebugComponent.Register();
    }

    // -----------------------------------------------------------------------------------
    // OnRoundStart  @ 0x8236D290 (sole caller GameStateModule::ProcessGameEvents @0x823A0A18,
    // `bl` @0x823A2274 in its E_EVENT_START_NETWORK_ROUND (18) arm)
    // Return both FSMs to idle and reseed the award RNG from the current game frame.
    //     0x8236D2B0..0x8236D304  ResetState() inlined (the twelve stores; +0x250 untouched)
    //     0x8236D308  lwz r8, 0(r3)   mTimerStatusInterface's game frame count (TimerStatus +0)
    //     0x8236D314  extsw           sign-extended into the 64-bit seed
    //     0x8236D310..0x8236D4C4      Random::SetSeed inlined (8 ring fills, cursor back to 0)
    // The PS3 twin (DecFIGS 0x244678) calls ResetState() by name and inlines the same SetSeed.
    // [FX-GS2 2026-09-23, G12-D11] Bodied. No PC caller yet: the console's only one is the
    // network-round-start arm of ProcessGameEvents, which this build does not have (event 18 is
    // posted only by the online StateManager).
    // -----------------------------------------------------------------------------------
    void
    PaybackManager::OnRoundStart()
    {
        ResetState();
        mRdmNumGenerator.SetSeed(static_cast<u64>(static_cast<s64>(
            mTimerStatusInterface.GetGameTimerStatus()->GetFrameCount())));
    }

    // -----------------------------------------------------------------------------------
    // OnRoundEnd  @ 0x8236D4D0 (sole caller GameStateModule::OnModeEnd @0x823767E0)
    // When asked to reset, the same as OnRoundStart; otherwise nothing.
    //     0x8236D4D4  clrlwi r11, r4, 24 ; beq -> return
    //     0x8236D4FC..0x8236D550  ResetState() inlined (the same twelve stores as OnRoundStart)
    //     0x8236D554  lwz r8, 0(r3) ; 0x8236D560 extsw ; 0x8236D55C..0x8236D710 Random::SetSeed
    // The PS3 twin (DecFIGS 0x24470C): `if (lbResetState) { ResetState(); <inline SetSeed> }`.
    // [FX-GS2 2026-09-23, G12-D11] The RNG used to be re-primed with Construct() (the default
    // seed); the console reseeds it from the frame count.
    // -----------------------------------------------------------------------------------
    void
    PaybackManager::OnRoundEnd(bool lbResetState)
    {
        if (!lbResetState)
            return;

        ResetState();
        mRdmNumGenerator.SetSeed(static_cast<u64>(static_cast<s64>(
            mTimerStatusInterface.GetGameTimerStatus()->GetFrameCount())));
    }

    // -----------------------------------------------------------------------------------
    // SetTimerInterface  (DWARF BrnPaybackManager.h:109, body BrnPaybackManager.cpp:1023)
    // [FX-FLOW 2026-09-24, NEW-PAYBACK-WIRING] No out-of-line X360 body: its only caller,
    // GameStateModule::CopyInputDataToPaybackManager @0x8239AA78, inlines it as the 48-byte
    // member-wise copy 0x8239AAA0..0x8239AB0C (two 24-byte TimerStatus runs into this+0x00 /
    // this+0x18: lwz/stw +0, lfs/stfs +4/+8, lbz/stb +0xC, lwz/stw +0x10, lfs/stfs +0x14) --
    // the DWARF hint is exactly `CgsSystem::TimerStatusInterface::operator=`.
    // Without a caller the copy stayed at Construct's Clear(), so every read of it -- the
    // aggressor timer's per-frame step in Update and the frame-count reseed in OnRoundStart /
    // OnRoundEnd -- read zero on this build.
    // -----------------------------------------------------------------------------------
    void
    PaybackManager::SetTimerInterface(const CgsSystem::TimerStatusInterface* lpTimerStatusInterface)
    {
        mTimerStatusInterface = *lpTimerStatusInterface;
    }

    // -----------------------------------------------------------------------------------
    // SetDirtyTrickButtonState  (DWARF BrnPaybackManager.h:106, body BrnPaybackManager.cpp:880)
    // [FX-FLOW 2026-09-24, NEW-PAYBACK-WIRING] Inlined by the same caller, 0x8239AB14..0x8239AB58:
    //     lbz r11, 0xC(ControllerInput)  ; the new press (mbDirtyTrickPressed)
    //     lbz r10, 0x264(this)           ; the previous mbDirtyTrickButtonDown
    //     stb r11, 0x264 ; stb r10, 0x265
    //     press edge (new && !old) && meAwardedDirtyTrick (+0x258) != 3 &&
    //     mePaybackAggressorState (+0x25C) == 4  ->  ChangeState(5) @0x823919B0
    // i.e. the player's dirty-trick button fires the trick they are holding.
    // -----------------------------------------------------------------------------------
    void
    PaybackManager::SetDirtyTrickButtonState(bool lbButtonPressed)
    {
        mbDirtyTrickButtonWasDown = mbDirtyTrickButtonDown;   // +0x265
        mbDirtyTrickButtonDown    = lbButtonPressed;          // +0x264

        if (mbDirtyTrickButtonDown && !mbDirtyTrickButtonWasDown &&
            meAwardedDirtyTrick != KE_NO_DIRTY_TRICK &&
            mePaybackAggressorState == E_PAYBACK_AGGRESSOR_STATE_YOU_TRIGGERED_DT)
        {
            ChangeState(E_PAYBACK_AGGRESSOR_STATE_TRIGGER_DT);
        }
    }

    // -----------------------------------------------------------------------------------
    // ResetState  (DWARF BrnPaybackManager.h:170; PS3 DecFIGS 0x23CB04)
    // No out-of-line X360 body: the console inlines it into Destruct @0x8236D110
    // (0x8236D150..0x8236D190) and OnRoundStart @0x8236D290 (0x8236D2B0..0x8236D304), and both
    // inline copies store the same twelve fields:
    //     +0x24C/+0x248 = flt_820037C8 (-1.0)   +0x266/+0x264/+0x265 = 0   +0x25C/+0x260 = 0
    //     +0x240/+0x244 = -1                    +0x258/+0x254 = 3          mEvent = {-1,-1,3,5}
    // The sentinel is the X360's 3 (the PS3 build stores 4; see the header note on
    // KE_NO_DIRTY_TRICK). mfPaybackVictimTimer (+0x250) is NOT touched by either copy.
    // -----------------------------------------------------------------------------------
    void
    PaybackManager::ResetState()
    {
        mfPaybackAggTimer       = -1.0f;                             // +0x24C
        mfCountdownTimer        = -1.0f;                             // +0x248
        mbPaybackAwarded        = false;                             // +0x266
        mePaybackAggressorState = E_PAYBACK_AGGRESSOR_STATE_IDLE;    // +0x25C

        mePaybackAggressorRaceCarIndex = ::E_ACTIVE_RACE_CAR_INDEX_INVALID;   // +0x240
        mePaybackVictimRaceCarIndex    = ::E_ACTIVE_RACE_CAR_INDEX_INVALID;   // +0x244

        meAwardedDirtyTrick    = KE_NO_DIRTY_TRICK;   // +0x258 = 3
        meActiveDirtyTrickType = KE_NO_DIRTY_TRICK;   // +0x254 = 3

        mEvent.meAggressorActiveRaceCarIndex = ::E_ACTIVE_RACE_CAR_INDEX_INVALID;   // +0x1FC
        mEvent.meVictimActiveRaceCarIndex    = ::E_ACTIVE_RACE_CAR_INDEX_INVALID;   // +0x200
        mEvent.meDirtyTrickType              = KE_NO_DIRTY_TRICK;                   // +0x204 = 3
        mEvent.meDirtyTrickStatus            = static_cast<BrnNetwork::EDirtyTrickStatus>(5);   // +0x208

        mePaybackVictimState      = E_PAYBACK_VICTIM_STATE_IDLE;   // +0x260
        mbDirtyTrickButtonDown    = false;                         // +0x264
        mbDirtyTrickButtonWasDown = false;                         // +0x265
    }

    // -----------------------------------------------------------------------------------
    // Destruct  @ 0x8236D110 (sole caller GameStateModule::Destruct @0x82375420, `bl` @0x823755A0)
    // Unhook the debug component, clear the timer copy and the outbound queue, reset both FSMs,
    // drop the owner pointer. The X360 body, in order:
    //     0x8236D12C  addi r3, r31, 0x26C ; stw 0, 0xC(r3)   mDebugComponent.mpPaybackManager = 0
    //     0x8236D134  bl   0x8284CB38                         mDebugComponent.Destruct() -- an
    //                                                          ICF-folded bare `blr` on the console
    //     0x8236D13C  bl   TimerStatusInterface::Clear        (this + 0)
    //     0x8236D148  stw  0, 0x38(r31)                       mDirtyTrickOutputQueue.Clear()
    //     0x8236D150..0x8236D190                               ResetState() inlined
    //     0x8236D194  stw  0, 0x268(r31)                      mpGameStateModule = 0
    // The PS3 twin (DecFIGS 0x267A98) is the same minus the debug-component pair, which the
    // X360 build added with the component.
    // -----------------------------------------------------------------------------------
    void
    PaybackManager::Destruct()
    {
        mDebugComponent.mpPaybackManager = 0;
        mDebugComponent.Destruct();

        mTimerStatusInterface.Clear();
        mDirtyTrickOutputQueue.Clear();

        ResetState();

        mpGameStateModule = 0;
    }

    // -----------------------------------------------------------------------------------
    // ChangeState (aggressor overload)  @ 0x823919B0
    // Cancel any aggressor timer, set the new aggressor state, clear the payback-awarded flag and
    // post the "payback HUD shown" state-change event onto the module's output GUI queue.
    // -----------------------------------------------------------------------------------
    void
    PaybackManager::ChangeState(EPaybackAggressorState leNewPaybackAggState)
    {
        GameStateModule* lpModule = mpGameStateModule;

        mfPaybackAggTimer        = -1.0f;
        mePaybackAggressorState  = leNewPaybackAggState;
        mbPaybackAwarded         = false;

        PaybackStateChangeEvent lStateChange;
        lStateChange.miShow = 1;
        lpModule->GetOutputGuiEventQueue()->AddEvent(
            &lStateChange, KI_GUI_EVENT_PAYBACK_STATE_CHANGE, sizeof(s32));
    }

    // -----------------------------------------------------------------------------------
    // UpdateCountdown  @ 0x82391938
    // Decrement the active-payback countdown by this frame's time step. While time remains, publish
    // it to the HUD; once it runs out, mark the countdown finished (-1.0).
    // -----------------------------------------------------------------------------------
    void
    PaybackManager::UpdateCountdown()
    {
        const f32 lfTimeStep =
            mTimerStatusInterface.GetGameTimerStatus()->GetCurrentTimeStep();

        mfCountdownTimer -= lfTimeStep;

        if (mfCountdownTimer > 0.0f)
        {
            PaybackCountdownEvent lCountdown;
            lCountdown.mfRemaining = mfCountdownTimer;
            mpGameStateModule->GetOutputGuiEventQueue()->AddEvent(
                &lCountdown, KI_GUI_EVENT_PAYBACK_COUNTDOWN, sizeof(f32));
        }
        else
        {
            mfCountdownTimer = -1.0f;
        }
    }

    // -----------------------------------------------------------------------------------
    // The victim-side helpers. None has an out-of-line X360 body -- the console inlines each
    // one at its call sites (cited per helper) -- but the DWARF declares all of them
    // (BrnPaybackManager.h:206/215/229/232/242) and the PS3 DecFIGS build keeps them out of
    // line, so the handlers below are written through them (inlining reversal).
    // -----------------------------------------------------------------------------------

    // ChangeState (victim overload), PS3 0x23CB9C: the state word and nothing else. X360 inline
    // stores to +0x260 at 0x82397D20 / 0x82397D54 (HandleActivePayback), 0x82397E90
    // (HandleCrashDueToPayback) and 0x82397FB8 (HandleSurvivingPayback).
    void
    PaybackManager::ChangeState(EPaybackVictimState leNewPaybackVictimState)
    {
        mePaybackVictimState = leNewPaybackVictimState;
    }

    // IsCountdownComplete, PS3 0x23CBA4: the countdown has run below zero. X360 inline at
    // 0x82397D28..0x82397D44: `lfs f13,0x248 ; lfs f0,flt_82001CC0 (0.0) ; fcmpu ; blt -> 1`, so a
    // NaN timer is NOT complete (the `<` below is false for NaN, exactly like the blt).
    bool
    PaybackManager::IsCountdownComplete()
    {
        return mfCountdownTimer < 0.0f;
    }

    // RemoveCountdown, PS3 0x26903C: cancel the countdown and tell the HUD with a -1.0 record.
    // X360 inline at 0x82397D8C..0x82397DB8 (and 0x82397EAC..0x82397ED8):
    //     lfs f0, flt_820037C8 (-1.0) ; stfs f0, 0x248(r31) ; stfs f0, var_4C
    //     GetOutputGuiEventQueue ; AddEvent(&var_4C, 0xEB, 4)
    void
    PaybackManager::RemoveCountdown()
    {
        mfCountdownTimer = -1.0f;

        PaybackCountdownEvent lCountdown;
        lCountdown.mfRemaining = -1.0f;
        mpGameStateModule->GetOutputGuiEventQueue()->AddEvent(
            &lCountdown, KI_GUI_EVENT_PAYBACK_COUNTDOWN, sizeof(f32));
    }

    // DirtyTrickTriggered, PS3 0x2592B8: the GUI "dirty trick triggered" record. X360 inline at
    // HandleTriggeringPayback 0x82397C58..0x82397C74: `bl 0x8231D8A8` (the write-locked
    // GetGameStateToGuiInterface) ; addi r3,r3,0x40 ; stw {aggressor, victim, type} ;
    // bl GameStateToGuiTriggeredDirtyTrick AddEvent 0x82368940.
    void
    PaybackManager::DirtyTrickTriggered(GameStateModuleIO::OutputBuffer* lpOutput,
                                        ::EActiveRaceCarIndex leAggressorRaceCarIndex,
                                        ::EActiveRaceCarIndex leVictimRaceCarIndex,
                                        BrnNetwork::EPaybackType leDirtyTrickType)
    {
        lpOutput->GetGameStateToGuiInterface()->AddDirtyTrickTriggered(
            leAggressorRaceCarIndex, leVictimRaceCarIndex, leDirtyTrickType);
    }

    // DirtyTrickEnding, PS3 0x2584D8: the GUI "dirty trick ended" record. X360 inline at
    // 0x82397DD0..0x82397DF4 (survived byte 0) and 0x82397EF0..0x82397F14 (survived byte 1):
    // `bl 0x8231D8A8` ; addi r3,r3,0x7C ; {aggressor, victim, type, survived} ;
    // bl GameStateToGuiEndingDirtyTrick AddEvent 0x82368A98.
    void
    PaybackManager::DirtyTrickEnding(GameStateModuleIO::OutputBuffer* lpOutput,
                                     ::EActiveRaceCarIndex leAggressorRaceCarIndex,
                                     ::EActiveRaceCarIndex leVictimRaceCarIndex,
                                     BrnNetwork::EPaybackType leDirtyTrickType,
                                     bool lbSurvived)
    {
        lpOutput->GetGameStateToGuiInterface()->AddDirtyTrickEnding(
            leAggressorRaceCarIndex, leVictimRaceCarIndex, leDirtyTrickType, lbSurvived);
    }

    // -----------------------------------------------------------------------------------
    // ProcessPaybackTriggerableEvent  @ 0x82397FC8
    // The player has chosen to trigger their earned dirty trick: hide the "payback ready" HUD,
    // advance the aggressor FSM to YOU_TRIGGERED_DT, cancel the aggressor timer and show the
    // "triggering" HUD.
    // -----------------------------------------------------------------------------------
    void
    PaybackManager::ProcessPaybackTriggerableEvent()
    {
        // X360 @0x82397FC8: `v2 = mePaybackAggressorState; if (v2 != 3 && v2)` -- the compiled check
        // is against literal 3 (== READY_TO_TRIGGER) and 0 (== IDLE), NOT AWARD_DT (== 2). The binary's
        // embedded assert string names AWARD_DT (a stale name from the original enum); reproduced
        // verbatim, but the condition matches the binary's actual value-3 comparison.
        CGS_ASSERT((mePaybackAggressorState == E_PAYBACK_AGGRESSOR_STATE_READY_TO_TRIGGER) ||
                   (mePaybackAggressorState == E_PAYBACK_AGGRESSOR_STATE_IDLE),
                   "( mePaybackAggressorState == E_PAYBACK_AGGRESSOR_STATE_AWARD_DT ) || "
                   "( mePaybackAggressorState == E_PAYBACK_AGGRESSOR_STATE_IDLE )");
        CGS_ASSERT(meAwardedDirtyTrick != KE_NO_DIRTY_TRICK,
                   "meAwardedDirtyTrick != BrnNetwork::E_PAYBACK_TYPE_COUNT");

        PaybackStateChangeEvent lHide;
        lHide.miShow = 0;
        mpGameStateModule->GetOutputGuiEventQueue()->AddEvent(
            &lHide, KI_GUI_EVENT_PAYBACK_STATE_CHANGE, sizeof(s32));

        mbPaybackAwarded        = false;
        mfPaybackAggTimer       = -1.0f;
        mePaybackAggressorState = E_PAYBACK_AGGRESSOR_STATE_YOU_TRIGGERED_DT;

        PaybackStateChangeEvent lShow;
        lShow.miShow = 1;
        mpGameStateModule->GetOutputGuiEventQueue()->AddEvent(
            &lShow, KI_GUI_EVENT_PAYBACK_STATE_CHANGE, sizeof(s32));
    }

    // -----------------------------------------------------------------------------------
    // SendNetworkDirtyTrickMessage  @ 0x8236D1B0
    // Queue an outbound dirty-trick network event describing aggressor/victim/type/status.
    // -----------------------------------------------------------------------------------
    void
    PaybackManager::SendNetworkDirtyTrickMessage(::EActiveRaceCarIndex leAggressorRaceCarIndex,
                                                 ::EActiveRaceCarIndex leVictimRaceCarIndex,
                                                 BrnNetwork::EPaybackType leDirtyTrickType,
                                                 BrnNetwork::EDirtyTrickStatus leDirtyTrickStatus)
    {
        CGS_ASSERT(leAggressorRaceCarIndex >= ::E_ACTIVE_RACE_CAR_INDEX_0,
                   "leAggressorRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");
        CGS_ASSERT(leVictimRaceCarIndex >= ::E_ACTIVE_RACE_CAR_INDEX_0,
                   "leVictimRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");
        CGS_ASSERT(leAggressorRaceCarIndex < ::E_ACTIVE_RACE_CAR_INDEX_COUNT,
                   "leAggressorRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");
        CGS_ASSERT(leVictimRaceCarIndex < ::E_ACTIVE_RACE_CAR_INDEX_COUNT,
                   "leVictimRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");

        BrnNetwork::BrnNetworkModuleIO::DirtyTrickEvent lDirtyTrickEvent;
        lDirtyTrickEvent.meAggressorActiveRaceCarIndex = leAggressorRaceCarIndex;
        lDirtyTrickEvent.meVictimActiveRaceCarIndex    = leVictimRaceCarIndex;
        lDirtyTrickEvent.meDirtyTrickType              = leDirtyTrickType;
        lDirtyTrickEvent.meDirtyTrickStatus            = leDirtyTrickStatus;

        mDirtyTrickOutputQueue.AddEvent(lDirtyTrickEvent);
    }

    // -----------------------------------------------------------------------------------
    // HandleWaitForPaybackAggressorToCrash  @ 0x823977F0
    // Aggressor side, WAIT_AWARD_PAYBACK -> waiting for the player (who was just taken down) to crash.
    // Snapshot the per-car "is crashing" flags from this frame's vehicle output; if the LOCAL player's
    // race car is now crashing, the payback is earned: cancel the aggressor timer, advance the FSM to
    // AWARD_DT, clear the awarded flag and show the payback HUD.
    //     0x8239780C  assert "lpVehicleOutputInterface" (non-gating, line 355)
    //     0x8239783C  bl  CrashingRaceCarInterface::SetFromVehicleOutputInterface (stack copy)
    //     0x82397844  bl  GameStateModule::GetPlayerActiveRaceCarIndex
    //     0x82397848  addi r11, r1, var_20 ; lbzx r11, r3, r11   -- IsCrashing(player), no assert
    //     0x82397854  beq -> return; else flt_820037C8 (-1.0) -> +0x24C, `li r10, 2` -> +0x25C,
    //                 0 -> +0x266, AddEvent(outGui, &1, 0xB0, 4)
    // [FX-GS2 G12-D1] The test was a pinned `false` (FLAG placeholder) while IsCrashing had no body;
    // it has its inline body now (b5 27ad7089), so the console's own read is back.
    // -----------------------------------------------------------------------------------
    void
    PaybackManager::HandleWaitForPaybackAggressorToCrash(
        const BrnPhysics::Vehicle::VehicleOutputInterface* lpVehicleOutputInterface)
    {
        CGS_ASSERT(lpVehicleOutputInterface, "lpVehicleOutputInterface");

        BrnPhysics::Vehicle::CrashingRaceCarInterface lCrashingRaceCars;
        lCrashingRaceCars.SetFromVehicleOutputInterface(lpVehicleOutputInterface);

        const s32 liPlayerRaceCarIndex =
            static_cast<s32>(mpGameStateModule->GetPlayerActiveRaceCarIndex());

        if (lCrashingRaceCars.IsCrashing(liPlayerRaceCarIndex))
        {
            mfPaybackAggTimer       = -1.0f;                                 // +588
            mePaybackAggressorState = E_PAYBACK_AGGRESSOR_STATE_AWARD_DT;    // +604 = 2
            mbPaybackAwarded        = false;                                 // +614 = 0

            PaybackStateChangeEvent lStateChange;
            lStateChange.miShow = 1;
            mpGameStateModule->GetOutputGuiEventQueue()->AddEvent(
                &lStateChange, KI_GUI_EVENT_PAYBACK_STATE_CHANGE, sizeof(s32));
        }
    }

    // -----------------------------------------------------------------------------------
    // HandleWaitingToAwardPayback  @ 0x823978B0 (Update jump table 0x8239AC2C[2] = 0x8239AC54,
    // `mr r4, r21` = the vehicle output interface)
    // Aggressor side, value 2 -- the taken-down player has crashed; wait for the crash to END. Once
    // the player's car is no longer crashing, move to value 3 (the awarding arm).
    //     0x823978CC  assert "lpVehicleOutputInterface" (non-gating, line 378)
    //     0x823978FC  bl  CrashingRaceCarInterface::SetFromVehicleOutputInterface (stack copy)
    //     0x8239790C  lbzx r11, r3, r11 ; 0x82397914 bne -> return   (IsCrashing(player), no assert)
    //     0x82397918..0x82397950  ChangeState(3) inlined: flt_820037C8 (-1.0) -> +0x24C,
    //                 `li r10, 3` -> +0x25C, 0 -> +0x266, AddEvent(outGui, &1, 0xB0, 4)
    // The PS3 twin (DecFIGS 0x268080) calls ChangeState by name, so the handler does too.
    // NAMING: the X360 inserted value 1 (HandleWaitForPaybackAggressorToCrash) ahead of the DWARF's
    // states, so this header's PS3 enumerator for value 3 is READY_TO_TRIGGER while the X360 source
    // calls value 3 AWARD_DT (its own assert text in ProcessPaybackTriggerableEvent). The value
    // stored is the console's 3.
    // -----------------------------------------------------------------------------------
    void
    PaybackManager::HandleWaitingToAwardPayback(
        const BrnPhysics::Vehicle::VehicleOutputInterface* lpVehicleOutputInterface)
    {
        CGS_ASSERT(lpVehicleOutputInterface, "lpVehicleOutputInterface");

        BrnPhysics::Vehicle::CrashingRaceCarInterface lCrashingRaceCars;
        lCrashingRaceCars.SetFromVehicleOutputInterface(lpVehicleOutputInterface);

        const s32 liPlayerRaceCarIndex =
            static_cast<s32>(mpGameStateModule->GetPlayerActiveRaceCarIndex());

        if (!lCrashingRaceCars.IsCrashing(liPlayerRaceCarIndex))
        {
            ChangeState(E_PAYBACK_AGGRESSOR_STATE_READY_TO_TRIGGER);   // raw 3, `li r10, 3` @0x82397920
        }
    }

    // -----------------------------------------------------------------------------------
    // DirtyTrickAwarded  (DWARF BrnPaybackManager.h:198; PS3 DecFIGS 0x258614)
    // No out-of-line X360 body: HandleAwardingPayback inlines it at 0x82397A40..0x82397A64:
    //     bl  GetPlayerActiveRaceCarIndex ; bl 0x8231D8A8 (OutputBuffer::GetGameStateToGuiInterface,
    //     write lock) ; addi r3, r3, 4 (mNewDirtyTrickQueue) ; stw {aggressor, victim, type} ;
    //     bl  GameStateToGuiNewDirtyTrick AddEvent 0x823687E8 (a 12-byte copy, length++)
    // The PS3 twin writes the same record into mNewDirtyTrickQueue through the GUI interface.
    // -----------------------------------------------------------------------------------
    void
    PaybackManager::DirtyTrickAwarded(GameStateModuleIO::OutputBuffer* lpOutput,
                                      ::EActiveRaceCarIndex leDTAggressorRaceCarIndex,
                                      ::EActiveRaceCarIndex leDTVictimRaceCarIndex,
                                      BrnNetwork::EPaybackType leDirtyTrickType)
    {
        lpOutput->GetGameStateToGuiInterface()->AddNewDirtyTrick(
            leDTAggressorRaceCarIndex, leDTVictimRaceCarIndex, leDirtyTrickType);
    }

    // -----------------------------------------------------------------------------------
    // HandleAwardingPayback  @ 0x82397970 (Update jump table 0x8239AC2C[3] = 0x8239AC64:
    // r4 = lpOutput (r22), r5 = the vehicle output (r21), r6 = the game-mode type (r29))
    // Aggressor side, value 3 -- the player survived their crash. After a second in this state
    // the payback is awarded: a random dirty trick, the GUI "new dirty trick" record and the
    // network AVAILABLE message. Independently, if the player is crashing again the payback is
    // lost: PaybackLostAction, the awarded trick and victim are dropped and the FSM goes idle.
    //     0x82397998  assert "lpVehicleOutputInterface" (non-gating, line 0x194 = 404)
    //     0x823979BC  bl  CrashingRaceCarInterface::SetFromVehicleOutputInterface (stack copy)
    //     0x823979C0  fcmpu +0x24C, flt_82001C98 (1.0) ; blt -> skip   (a NaN timer falls through)
    //     0x823979D8  lbz +0x266 ; bne -> skip
    //     0x823979E8  stb 1, +0x266
    //     0x823979E4..0x82397A3C  +0x258 = RandomInt(0, 2): the OLD seed's high word % 3
    //                 (srdi 32 ; mulhwu 0xAAAAAAAB ; srwi 1 ; *3 ; subf), seed = old * 0x5851F42D4C957F2D + 1
    //     0x82397A40..0x82397A64  DirtyTrickAwarded(lpOutput, player, +0x244, +0x258)  (inlined)
    //     0x82397A68..0x82397A8C  SendNetworkDirtyTrickMessage(player, +0x244, +0x258, `li r7, 1`)
    //     0x82397A90  lbzx crashing[player] ; beq -> return        (NOT an else: both can run)
    //     0x82397AA8  assert "lpOutput" (non-gating, line 0x1B5 = 437)
    //     0x82397AD0..0x82397AE0  GetGameActionQueue()->AddEvent(&<uninitialised byte>, 0xD3, 1)
    //     0x82397AE4..0x82397B24  +0x258 = 3, +0x244 = -1, ChangeState(0) inlined (-1.0 -> +0x24C,
    //                 0 -> +0x25C, 0 -> +0x266, AddEvent(outGui, &1, 0xB0, 4))
    // r6 (the game-mode type) is never read on the X360. The PS3 twin (DecFIGS 0x29D508) has the
    // same shape and calls DirtyTrickAwarded / ChangeState(IDLE) by name.
    // -----------------------------------------------------------------------------------
    void
    PaybackManager::HandleAwardingPayback(GameStateModuleIO::OutputBuffer* lpOutput,
                                          const BrnPhysics::Vehicle::VehicleOutputInterface* lpVehicleOutputInterface,
                                          GameStateModuleIO::EGameModeType /*leGameModeType*/)
    {
        CGS_ASSERT(lpVehicleOutputInterface, "lpVehicleOutputInterface");

        BrnPhysics::Vehicle::CrashingRaceCarInterface lCrashingRaceCars;
        lCrashingRaceCars.SetFromVehicleOutputInterface(lpVehicleOutputInterface);

        if (!(mfPaybackAggTimer < 1.0f) && !mbPaybackAwarded)   // fcmpu/blt vs flt_82001C98
        {
            mbPaybackAwarded = true;

            // The DWARF's RandomInt draw over the three X360 trick types (E_PAYBACK_TYPE_COUNT == 3
            // in this build, see the header note on KE_NO_DIRTY_TRICK).
            meAwardedDirtyTrick = static_cast<BrnNetwork::EPaybackType>(mRdmNumGenerator.RandomInt(0, 2));

            DirtyTrickAwarded(lpOutput, mpGameStateModule->GetPlayerActiveRaceCarIndex(),
                              mePaybackVictimRaceCarIndex, meAwardedDirtyTrick);

            SendNetworkDirtyTrickMessage(mpGameStateModule->GetPlayerActiveRaceCarIndex(),
                                         mePaybackVictimRaceCarIndex, meAwardedDirtyTrick,
                                         KE_DIRTY_TRICK_STATUS_AVAILABLE);
        }

        if (lCrashingRaceCars.IsCrashing(static_cast<s32>(mpGameStateModule->GetPlayerActiveRaceCarIndex())))
        {
            CGS_ASSERT(lpOutput, "lpOutput");

            PaybackLostAction lPaybackLost;
            lpOutput->GetGuiOutputQueue()->AddEvent(
                &lPaybackLost, KI_ACTION_PAYBACK_LOST, sizeof(PaybackLostAction));

            meAwardedDirtyTrick         = KE_NO_DIRTY_TRICK;                   // +0x258 = 3
            mePaybackVictimRaceCarIndex = ::E_ACTIVE_RACE_CAR_INDEX_INVALID;   // +0x244 = -1
            ChangeState(E_PAYBACK_AGGRESSOR_STATE_IDLE);                       // +0x24C/+0x25C/+0x266, GUI 176
        }
    }

    // -----------------------------------------------------------------------------------
    // HandleHavingPayback  @ 0x82397B30
    // Aggressor side, READY_TO_TRIGGER -> waiting for the victim car. While the victim's race car is
    // still present (resolved from the module's active-car table by mePaybackVictimRaceCarIndex and
    // holding a live car slot), keep waiting. Once it has gone, abandon the earned payback: reset the
    // aggressor FSM to idle, clear the awarded trick, hide the payback HUD and post the
    // "payback victim left the game" game action.
    // -----------------------------------------------------------------------------------
    void
    PaybackManager::HandleHavingPayback(GameStateModuleIO::OutputBuffer* lpOutput)
    {
        if (mpGameStateModule->IsActiveRaceCarStillPresent(mePaybackVictimRaceCarIndex))
            return;   // victim car still in play -- keep waiting

        mfPaybackAggTimer       = -1.0f;
        meAwardedDirtyTrick     = KE_NO_DIRTY_TRICK;   // 3
        mePaybackAggressorState = E_PAYBACK_AGGRESSOR_STATE_IDLE;
        mbPaybackAwarded        = false;

        PaybackStateChangeEvent lHide;
        lHide.miShow = 1;
        mpGameStateModule->GetOutputGuiEventQueue()->AddEvent(
            &lHide, KI_GUI_EVENT_PAYBACK_STATE_CHANGE, sizeof(s32));

        CGS_ASSERT(lpOutput, "lpOutput");

        // PaybackVictimLeftGameAction (X360 game-action tag 0xD4, 1-byte payload).
        HavePaybackEvent lVictimLeft;
        lVictimLeft.mu8Flag = 1;
        lpOutput->GetGuiOutputQueue()->AddEvent(
            &lVictimLeft, KI_GUI_EVENT_HAVE_PAYBACK, 1);
    }

    // -----------------------------------------------------------------------------------
    // HandleTriggeringPayback  @ 0x82397C08
    // Aggressor side -- the player just triggered their earned dirty trick on the victim. Broadcast
    // the network trigger message, tell the GUI a dirty trick was triggered, then reset the
    // aggressor FSM to idle and hide the "ready to trigger" HUD.
    // -----------------------------------------------------------------------------------
    void
    PaybackManager::HandleTriggeringPayback(GameStateModuleIO::OutputBuffer* lpOutput)
    {
        const BrnNetwork::EPaybackType leAwarded     = meAwardedDirtyTrick;
        const ::EActiveRaceCarIndex    leVictimIndex = mePaybackVictimRaceCarIndex;
        const ::EActiveRaceCarIndex    lePlayerIndex = mpGameStateModule->GetPlayerActiveRaceCarIndex();

        SendNetworkDirtyTrickMessage(lePlayerIndex, leVictimIndex, leAwarded,
                                     static_cast<BrnNetwork::EDirtyTrickStatus>(2));

        // X360 0x82397C44..0x82397C74 re-reads the three values (+0x258, +0x244, the player) and
        // writes the GUI "dirty trick triggered" record {player, victim, trick} into the output
        // buffer's GameStateToGuiInterface (+0x40 queue) -- the PS3 twin calls
        // DirtyTrickTriggered(lpOutput, player, victim, trick) here, which the X360 inlines.
        const BrnNetwork::EPaybackType leAwarded2     = meAwardedDirtyTrick;
        const ::EActiveRaceCarIndex    leVictimIndex2 = mePaybackVictimRaceCarIndex;
        const ::EActiveRaceCarIndex    lePlayerIndex2 = mpGameStateModule->GetPlayerActiveRaceCarIndex();

        DirtyTrickTriggered(lpOutput, lePlayerIndex2, leVictimIndex2, leAwarded2);

        // X360 @0x82397C08 end-stores (asm order): timer off, clear the awarded trick, drop the victim
        // index, return the aggressor FSM to IDLE, clear the awarded flag. The earlier reconstruction
        // wrongly left the state at READY_TO_TRIGGER and dropped the meAwardedDirtyTrick reset.
        mfPaybackAggTimer              = -1.0f;                              // +588
        meAwardedDirtyTrick            = KE_NO_DIRTY_TRICK;                  // +600 = 3
        mePaybackVictimRaceCarIndex    = ::E_ACTIVE_RACE_CAR_INDEX_INVALID; // +580 = -1
        mePaybackAggressorState        = E_PAYBACK_AGGRESSOR_STATE_IDLE;     // +604 = 0
        mbPaybackAwarded               = false;                             // +614 = 0

        PaybackStateChangeEvent lHide;
        lHide.miShow = 1;
        mpGameStateModule->GetOutputGuiEventQueue()->AddEvent(
            &lHide, KI_GUI_EVENT_PAYBACK_STATE_CHANGE, sizeof(s32));
    }

    // -----------------------------------------------------------------------------------
    // StartCountdown  (DWARF BrnPaybackManager.h:223; PS3 DecFIGS 0x23CB90: `mfCountdownTimer = 10.0`)
    // No out-of-line X360 body: HandleReceivingPayback inlines it at 0x82383C8C..0x82383C98
    // (`lfs f0, flt_8202AC38` -- the image reads 0x41200000 = 10.0 -- `stfs f0, 0x248(r31)`).
    // -----------------------------------------------------------------------------------
    void
    PaybackManager::StartCountdown()
    {
        mfCountdownTimer = 10.0f;   // flt_8202AC38
    }

    // -----------------------------------------------------------------------------------
    // HandleReceivingPayback  @ 0x82383B40 (Update victim jump table 0x8239AD24[1] = 0x8239AD38:
    // r4 = lpOutput (r22), r5 = the vehicle output (r21))
    // Victim side, TRIGGERED_ON_YOU -- a dirty trick was triggered on the local player. It starts
    // as soon as the player is not crashing: take the trick type from the event copy, tell the GUI
    // it was triggered, post PaybackActivatedAction, start the 10 s countdown, go ACTIVE.
    //     0x82383B68  assert "lpVehicleOutputInterface" (non-gating, line 0x207 = 519)
    //     0x82383B8C  bl  CrashingRaceCarInterface::SetFromVehicleOutputInterface (stack copy)
    //     0x82383B9C  lbzx crashing[player] ; bne -> return
    //     0x82383BA8  lwz +0x204 (mEvent.meDirtyTrickType) ; stw +0x254 ; assert != 3 (non-gating, 0x211)
    //     0x82383BD4..0x82383C04  DirtyTrickTriggered(+0x240, player, +0x254) inlined:
    //                 gui+0x40 record {aggressor, victim = player, type}
    //     0x82383C10/0x82383C3C  asserts "lpOutput" / "lpOutput->GetGameActionQueue()" (non-gating)
    //     0x82383C58..0x82383C88  GetGameActionQueue()->AddEvent({+0x254, +0x240, player}, 0xD7, 0xC)
    //     0x82383C8C..0x82383C98  StartCountdown() inlined (10.0 -> +0x248)
    //     0x82383C9C  `li r10, 2 ; stw r10, 0x260(r31)`   victim ChangeState(ACTIVE) inlined
    // The PS3 twin (DecFIGS 0x29C9B0) calls DirtyTrickTriggered / StartCountdown / ChangeState by name.
    // -----------------------------------------------------------------------------------
    void
    PaybackManager::HandleReceivingPayback(GameStateModuleIO::OutputBuffer* lpOutput,
                                           const BrnPhysics::Vehicle::VehicleOutputInterface* lpVehicleOutputInterface)
    {
        CGS_ASSERT(lpVehicleOutputInterface, "lpVehicleOutputInterface");

        BrnPhysics::Vehicle::CrashingRaceCarInterface lCrashingRaceCars;
        lCrashingRaceCars.SetFromVehicleOutputInterface(lpVehicleOutputInterface);

        if (lCrashingRaceCars.IsCrashing(static_cast<s32>(mpGameStateModule->GetPlayerActiveRaceCarIndex())))
            return;

        meActiveDirtyTrickType = mEvent.meDirtyTrickType;
        CGS_ASSERT(meActiveDirtyTrickType != KE_NO_DIRTY_TRICK,
                   "BrnNetwork::E_PAYBACK_TYPE_COUNT != meActiveDirtyTrickType");

        DirtyTrickTriggered(lpOutput, mePaybackAggressorRaceCarIndex,
                            mpGameStateModule->GetPlayerActiveRaceCarIndex(), meActiveDirtyTrickType);

        CGS_ASSERT(lpOutput, "lpOutput");
        CGS_ASSERT(lpOutput->GetGameActionQueue(), "lpOutput->GetGameActionQueue()");

        // PaybackActivatedAction (DWARF record, BrnGameActions.h): {type, aggressor, victim} in
        // THIS order (`stw +0x254, var_50 ; stw +0x240, var_4C ; stw player, var_48`), 12 bytes.
        GameStateModuleIO::PaybackActivatedAction lActivated;
        lActivated.mePaybackType           = meActiveDirtyTrickType;
        lActivated.mePaybackAggressorIndex = mePaybackAggressorRaceCarIndex;
        lActivated.mePaybackVictimIndex    = mpGameStateModule->GetPlayerActiveRaceCarIndex();
        lpOutput->GetGuiOutputQueue()->AddEvent(&lActivated, GameStateModuleIO::E_ACTION_PAYBACK_ACTIVATED);

        StartCountdown();                            // +0x248 = 10.0
        ChangeState(E_PAYBACK_VICTIM_STATE_ACTIVE);  // +0x260 = 2
    }

    // -----------------------------------------------------------------------------------
    // HandleActivePayback  @ 0x82397CC8 (Update victim jump table 0x8239AD24[2] = 0x8239AD4C)
    // Victim side, ACTIVE -- a dirty trick is running on the local player. Publish it to the
    // output buffer every frame, then resolve it: the player crashing ends it as YOU_CRASHED, the
    // countdown running out ends it as YOU_SURVIVED, otherwise the countdown ticks on.
    //     0x82397CEC  OutputBuffer::SetActivePaybackType(lpOutput, +0x254)
    //     0x82397CF8  OutputBuffer::SetActivePaybackAggressor(lpOutput, +0x240)
    //     0x82397D0C  GameStateModule::IsRaceCarCrashing(player) -> +0x260 = 3
    //     0x82397D28  else fcmpu +0x248, 0.0 ; blt              -> +0x260 = 4
    //     0x82397D60  else UpdateCountdown
    // The crash test reads the MODULE's cached per-slot flag (IsRaceCarCrashing), not a
    // CrashingRaceCarInterface -- the PS3 twin (DecFIGS 0x268BFC) reads the same member.
    // -----------------------------------------------------------------------------------
    void
    PaybackManager::HandleActivePayback(GameStateModuleIO::OutputBuffer* lpOutput)
    {
        lpOutput->SetActivePaybackType(meActiveDirtyTrickType);
        lpOutput->SetActivePaybackAggressor(mePaybackAggressorRaceCarIndex);

        if (mpGameStateModule->IsRaceCarCrashing(mpGameStateModule->GetPlayerActiveRaceCarIndex()))
        {
            ChangeState(E_PAYBACK_VICTIM_STATE_YOU_CRASHED);
        }
        else if (IsCountdownComplete())
        {
            ChangeState(E_PAYBACK_VICTIM_STATE_YOU_SURVIVED);
        }
        else
        {
            UpdateCountdown();
        }
    }

    // -----------------------------------------------------------------------------------
    // HandleCrashDueToPayback  @ 0x82397D80 (Update victim jump table 0x8239AD24[3] = 0x8239AD5C)
    // Victim side, YOU_CRASHED -- the dirty trick did its job. Cancel the countdown, tell the GUI
    // the trick ended (not survived), broadcast status 4, post PaybackOverAction, go idle.
    //     0x82397D8C..0x82397DB8  RemoveCountdown()          (-1.0 -> +0x248, record 235 = -1.0)
    //     0x82397DBC..0x82397DF4  DirtyTrickEnding(+0x240, player, +0x254, survived 0)
    //     0x82397DF8..0x82397E18  SendNetworkDirtyTrickMessage(+0x240, player, +0x254, 4)
    //     0x82397E1C..0x82397E70  asserts "lpOutput" / "lpOutput->GetGameActionQueue()" (non-gating)
    //     0x82397E74..0x82397E88  GetGameActionQueue()->AddEvent(&<byte>, 0xD8, 1)
    //     0x82397E8C..0x82397E94  +0x260 = 0, +0x254 = 3
    // -----------------------------------------------------------------------------------
    void
    PaybackManager::HandleCrashDueToPayback(GameStateModuleIO::OutputBuffer* lpOutput)
    {
        RemoveCountdown();

        DirtyTrickEnding(lpOutput, mePaybackAggressorRaceCarIndex,
                         mpGameStateModule->GetPlayerActiveRaceCarIndex(), meActiveDirtyTrickType,
                         /*lbSurvived=*/false);

        SendNetworkDirtyTrickMessage(mePaybackAggressorRaceCarIndex,
                                     mpGameStateModule->GetPlayerActiveRaceCarIndex(),
                                     meActiveDirtyTrickType, KE_DIRTY_TRICK_STATUS_CRASHED);

        CGS_ASSERT(lpOutput, "lpOutput");
        CGS_ASSERT(lpOutput->GetGameActionQueue(), "lpOutput->GetGameActionQueue()");

        PaybackOverAction lPaybackOver;
        lpOutput->GetGuiOutputQueue()->AddEvent(
            &lPaybackOver, KI_ACTION_PAYBACK_OVER, sizeof(PaybackOverAction));

        meActiveDirtyTrickType = KE_NO_DIRTY_TRICK;     // +0x254 = 3
        ChangeState(E_PAYBACK_VICTIM_STATE_IDLE);       // +0x260 = 0
    }

    // -----------------------------------------------------------------------------------
    // HandleSurvivingPayback  @ 0x82397EA0 (Update victim jump table 0x8239AD24[4] = 0x8239AD6C)
    // Victim side, YOU_SURVIVED -- the countdown ran out first. The same body as the crash arm
    // with the two values that say "survived": the GUI record's survived byte is 1
    // (`li r11,1` @0x82397EF8) and the network status is 3 (`li r7,3` @0x82397F34). The
    // PaybackOverAction is the SAME id (0xD8, size 1, @0x82397FA0). The end stores run +0x254 = 3
    // then +0x260 = 0 (0x82397FB4/0x82397FB8).
    // -----------------------------------------------------------------------------------
    void
    PaybackManager::HandleSurvivingPayback(GameStateModuleIO::OutputBuffer* lpOutput)
    {
        RemoveCountdown();

        DirtyTrickEnding(lpOutput, mePaybackAggressorRaceCarIndex,
                         mpGameStateModule->GetPlayerActiveRaceCarIndex(), meActiveDirtyTrickType,
                         /*lbSurvived=*/true);

        SendNetworkDirtyTrickMessage(mePaybackAggressorRaceCarIndex,
                                     mpGameStateModule->GetPlayerActiveRaceCarIndex(),
                                     meActiveDirtyTrickType, KE_DIRTY_TRICK_STATUS_SURVIVED);

        CGS_ASSERT(lpOutput, "lpOutput");
        CGS_ASSERT(lpOutput->GetGameActionQueue(), "lpOutput->GetGameActionQueue()");

        PaybackOverAction lPaybackOver;
        lpOutput->GetGuiOutputQueue()->AddEvent(
            &lPaybackOver, KI_ACTION_PAYBACK_OVER, sizeof(PaybackOverAction));

        meActiveDirtyTrickType = KE_NO_DIRTY_TRICK;     // +0x254 = 3
        ChangeState(E_PAYBACK_VICTIM_STATE_IDLE);       // +0x260 = 0
    }

    // -----------------------------------------------------------------------------------
    // ProcessTakedownEvents  @ 0x82397658
    // Walk this frame's takedown queue; for each marked-man takedown where the LOCAL player is the
    // victim, arm the aggressor FSM: remember who took you down, drop into WAIT_AWARD_PAYBACK, and
    // show the payback HUD.
    // -----------------------------------------------------------------------------------
    void
    PaybackManager::ProcessTakedownEvents(const GameStateModuleIO::PreWorldInputBuffer* /*lpInput*/,
                                          GameStateModuleIO::OutputBuffer* /*lpOutput*/,
                                          const CgsModule::EventQueue<TakedownEvent, 8>* lpQueue,
                                          GameStateModuleIO::EGameModeType /*leGameModeType*/)
    {
        for (s32 liIndex = 0; liIndex < lpQueue->GetLength(); ++liIndex)
        {
            const TakedownEvent& lTakedownEvent = lpQueue->GetEvent(liIndex);

            const ::EActiveRaceCarIndex leTakedownAggressorRaceCarIndex = lTakedownEvent.meAggressorIndex;
            const ::EActiveRaceCarIndex leTakedownVictimRaceCarIndex    = lTakedownEvent.meVictimIndex;

            CGS_ASSERT(leTakedownAggressorRaceCarIndex != ::E_ACTIVE_RACE_CAR_INDEX_INVALID,
                       "leTakedownAggressorRaceCarIndex != E_ACTIVE_RACE_CAR_INDEX_INVALID");
            CGS_ASSERT(leTakedownAggressorRaceCarIndex >= ::E_ACTIVE_RACE_CAR_INDEX_0 &&
                       leTakedownAggressorRaceCarIndex < ::E_ACTIVE_RACE_CAR_INDEX_COUNT,
                       "leTakedownAggressorRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0 && "
                       "leTakedownAggressorRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");
            CGS_ASSERT(leTakedownVictimRaceCarIndex != ::E_ACTIVE_RACE_CAR_INDEX_INVALID,
                       "leTakedownVictimRaceCarIndex != E_ACTIVE_RACE_CAR_INDEX_INVALID");
            CGS_ASSERT(leTakedownVictimRaceCarIndex >= ::E_ACTIVE_RACE_CAR_INDEX_0 &&
                       leTakedownVictimRaceCarIndex < ::E_ACTIVE_RACE_CAR_INDEX_COUNT,
                       "leTakedownVictimRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0 && "
                       "leTakedownVictimRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");

            if (!lTakedownEvent.mbMarkedManTakeDown)
                continue;

            // Only react when the local player is the one who was taken down.
            if (mpGameStateModule->GetPlayerActiveRaceCarIndex() != leTakedownVictimRaceCarIndex)
                continue;

            // [payback] PC witness (NOT in the console), first 8 only: the point where this
            // manager REGISTERS a takedown -- the marked-man and local-victim filters have both
            // passed and the aggressor FSM is about to be armed. [FLAG PC witness]
            // DELETE-WHEN: the organic takedown case goes green and the payback registration is
            // confirmed from a scenario run.
            {
                static s32 siPaybackWitnessed = 0;
                if (siPaybackWitnessed < 8 && CgsDev::Log::gpDebugPrint != 0)
                {
                    ++siPaybackWitnessed;
                    *CgsDev::Log::gpDebugPrint << "[payback] takedown registered attacker "
                                               << static_cast<s32>(leTakedownAggressorRaceCarIndex)
                                               << " -> victim " << static_cast<s32>(leTakedownVictimRaceCarIndex)
                                               << " markedMan=" << (lTakedownEvent.mbMarkedManTakeDown ? 1 : 0)
                                               << " [FLAG PC witness]\n";
                }
            }

            mfPaybackAggTimer              = -1.0f;
            mePaybackAggressorRaceCarIndex = leTakedownVictimRaceCarIndex;
            mePaybackVictimRaceCarIndex    = leTakedownAggressorRaceCarIndex;
            mePaybackAggressorState        = E_PAYBACK_AGGRESSOR_STATE_WAIT_AWARD_PAYBACK;
            mbPaybackAwarded               = false;

            PaybackStateChangeEvent lShow;
            lShow.miShow = 1;
            mpGameStateModule->GetOutputGuiEventQueue()->AddEvent(
                &lShow, KI_GUI_EVENT_PAYBACK_STATE_CHANGE, sizeof(s32));
        }
    }

    // -----------------------------------------------------------------------------------
    // ProcessDirtyTrickEventQueue  @ 0x82383CA8
    // Drain the inbound (network) dirty-trick queue and apply each event to the victim FSM / GUI:
    //   status 2 (TRIGGERED)  -> if it targets the local player, enter TRIGGERED_ON_YOU + remember it
    //   status 3 (ENDED, survived) -> notify the GUI the dirty trick is ending
    //   status 4 (ENDED, crashed)  -> notify the GUI + (if it was on you) post the "ended on you" HUD
    // -----------------------------------------------------------------------------------
    void
    PaybackManager::ProcessDirtyTrickEventQueue(const GameStateModuleIO::PreWorldInputBuffer* lpInput,
                                                GameStateModuleIO::OutputBuffer* lpOutput)
    {
        CGS_ASSERT(lpInput, "lpInput");
        CGS_ASSERT(lpInput->GetNetworkToGameStateInterface(),
                   "lpInput->GetNetworkToGameStateInterface()");
        CGS_ASSERT(lpInput->GetNetworkToGameStateInterface()->GetDirtyTrickQueue(),
                   "lpInput->GetNetworkToGameStateInterface()->GetDirtyTrickQueue()");

        // The inbound queue is the network interface's own dirty-trick queue (console +0x2268 of
        // the interface). The loop re-reads its length every pass, as the console does.
        const GameStateModuleIO::NetworkToGameStateInterface::DirtyTrickQueue* lpDirtyTrickQueue =
            lpInput->GetNetworkToGameStateInterface()->GetDirtyTrickQueue();

        for (s32 liIndex = 0; liIndex < lpDirtyTrickQueue->GetLength(); ++liIndex)
        {
            const BrnNetwork::BrnNetworkModuleIO::DirtyTrickEvent& lEvent = lpDirtyTrickQueue->GetEvent(liIndex);

            const ::EActiveRaceCarIndex      leAggressor = lEvent.meAggressorActiveRaceCarIndex;
            const ::EActiveRaceCarIndex      leVictim    = lEvent.meVictimActiveRaceCarIndex;
            const BrnNetwork::EPaybackType   leType      = lEvent.meDirtyTrickType;
            const BrnNetwork::EDirtyTrickStatus leStatus = lEvent.meDirtyTrickStatus;

            switch (static_cast<s32>(leStatus))
            {
                case 1:   // AWARDED -- no local FSM/GUI action
                    break;

                case 2:   // TRIGGERED on a car -- if it is you, become the victim
                    if (leVictim == mpGameStateModule->GetPlayerActiveRaceCarIndex())
                    {
                        // X360 @0x82383CA8 case 2: a1[152]=1 (victim state) then a1[144]=aggressor.
                        // a1[144] == +576 == mePaybackAggressorRaceCarIndex (NOT the victim index at
                        // +580): on the victim side this records who triggered the dirty trick on you.
                        mePaybackVictimState           = E_PAYBACK_VICTIM_STATE_TRIGGERED_ON_YOU;
                        mePaybackAggressorRaceCarIndex = leAggressor;

                        mEvent.meAggressorActiveRaceCarIndex = leAggressor;
                        mEvent.meVictimActiveRaceCarIndex    = leVictim;
                        mEvent.meDirtyTrickType              = leType;
                        mEvent.meDirtyTrickStatus            = leStatus;
                    }
                    break;

                case 3:   // ENDED -- the victim survived
                    lpOutput->GetGameStateToGuiInterface()->AddDirtyTrickEnding(
                        leAggressor, leVictim, leType, /*lbSurvived=*/true);
                    break;

                case 4:   // ENDED -- the victim crashed
                    lpOutput->GetGameStateToGuiInterface()->AddDirtyTrickEnding(
                        leAggressor, leVictim, leType, /*lbSurvived=*/false);
                    if (leAggressor == mpGameStateModule->GetPlayerActiveRaceCarIndex())
                    {
                        DirtyTrickEndedOnYouEvent lEndedOnYou;
                        lEndedOnYou.meAggressorRaceCarIndex = leAggressor;
                        lEndedOnYou.meVictimRaceCarIndex    = leVictim;
                        lpOutput->GetGuiOutputQueue()->AddEvent(
                            &lEndedOnYou, KI_GUI_EVENT_DT_ENDED_ON_YOU, 2 * static_cast<s32>(sizeof(s32)));
                    }
                    break;

                default:
                    CGS_ASSERT(false, "Dirty Trick Status not handled");
                    break;
            }
        }
    }

    // -----------------------------------------------------------------------------------
    // Update  @ 0x8239AB78
    // The per-frame entry point: drain the takedown + network dirty-trick queues, advance the
    // aggressor-side payback timer, step both FSMs, then merge the outbound dirty-trick queue into
    // the world action queue and re-clear the manager's per-frame state.
    // -----------------------------------------------------------------------------------
    void
    PaybackManager::Update(const GameStateModuleIO::PreWorldInputBuffer* lpInput,
                           GameStateModuleIO::OutputBuffer* lpOutput,
                           const BrnPhysics::Vehicle::VehicleOutputInterface* lpVehicleOutputInterface,
                           const CgsModule::EventQueue<TakedownEvent, 8>* lpQueue,
                           GameStateModuleIO::EGameModeType leGameModeType)
    {
        ProcessTakedownEvents(lpInput, lpOutput, lpQueue, leGameModeType);
        ProcessDirtyTrickEventQueue(lpInput, lpOutput);

        // Advance the aggressor FSM's running timer (held disabled at -1.0 while idle).
        if (mfPaybackAggTimer == -1.0f)
        {
            mfPaybackAggTimer = 0.0f;
        }
        else
        {
            mfPaybackAggTimer += mTimerStatusInterface.GetGameTimerStatus()->GetCurrentTimeStep();
        }

        switch (mePaybackAggressorState)
        {
            case E_PAYBACK_AGGRESSOR_STATE_IDLE:
                break;
            case E_PAYBACK_AGGRESSOR_STATE_WAIT_AWARD_PAYBACK:
                HandleWaitForPaybackAggressorToCrash(lpVehicleOutputInterface);
                break;
            case E_PAYBACK_AGGRESSOR_STATE_AWARD_DT:
                // Jump table 0x8239AC2C[2] = 0x8239AC54: `mr r4, r21` (the vehicle output).
                HandleWaitingToAwardPayback(lpVehicleOutputInterface);
                break;
            case E_PAYBACK_AGGRESSOR_STATE_READY_TO_TRIGGER:
                // Jump table 0x8239AC2C[3] = 0x8239AC64: r6 = mode (r29), r5 = the vehicle output
                // (r21), r4 = lpOutput (r22).
                HandleAwardingPayback(lpOutput, lpVehicleOutputInterface, leGameModeType);
                break;
            case E_PAYBACK_AGGRESSOR_STATE_YOU_TRIGGERED_DT:
                HandleHavingPayback(lpOutput);
                break;
            case E_PAYBACK_AGGRESSOR_STATE_TRIGGER_DT:
                // The X360 jump-table's explicit case 5 (Update @0x8239AB78).
                HandleTriggeringPayback(lpOutput);
                break;
            default:
                // X360 default arm: FireAssert("Unknown payback aggressor state: " + state, line 286).
                CGS_ASSERT(false, "Unknown payback aggressor state");
                break;
        }

        switch (mePaybackVictimState)
        {
            case E_PAYBACK_VICTIM_STATE_IDLE:
                break;
            case E_PAYBACK_VICTIM_STATE_TRIGGERED_ON_YOU:
                // Jump table 0x8239AD24[1] = 0x8239AD38: r5 = the vehicle output (r21), r4 = lpOutput.
                HandleReceivingPayback(lpOutput, lpVehicleOutputInterface);
                break;
            case E_PAYBACK_VICTIM_STATE_ACTIVE:
                HandleActivePayback(lpOutput);        // jump table [2] 0x8239AD4C, r4 = lpOutput
                break;
            case E_PAYBACK_VICTIM_STATE_YOU_CRASHED:
                HandleCrashDueToPayback(lpOutput);    // jump table [3] 0x8239AD5C, r4 = lpOutput
                break;
            case E_PAYBACK_VICTIM_STATE_YOU_SURVIVED:
                HandleSurvivingPayback(lpOutput);     // jump table [4] 0x8239AD6C, r4 = lpOutput
                break;
            default:
                CGS_ASSERT(false, "Unknown payback victim state");
                break;
        }

        // Publish this frame's outbound dirty-trick events onto the GameState->Network interface's
        // dirty-trick queue, then clear the manager's per-frame queue.
        lpOutput->GetGameStateToNetworkInterface()->GetDirtyTrickQueue()->Append(mDirtyTrickOutputQueue);
        mDirtyTrickOutputQueue.Clear();

        // [FX-FLOW 2026-09-24, NEW-PAYBACK-WIRING part 4 -- verified, deliberately not emitted] X360
        // tail 0x8239ADF0..0x8239AE04: `lwz r11, 0x26C(this)` (the embedded component's vtable),
        // `lwz r11, 0(r11)`, `bctrl` with r3 = &mDebugComponent -- slot 0, DebugComponent::Update.
        // PaybackDebugComponent's vtable @0x820CDDCC reads (image) {0x8284CB38, 0x8284CB38,
        // 0x8284CB38, 0x82357B88 GetName, 0x827E2F38, 0x827E2F38, 0x82357B98 OnActivate}: slot 0 is
        // 0x8284CB38, the ICF-folded bare `blr` (the same target Destruct's DebugComponent::Destruct
        // call lands on). The call does nothing on the console, and CgsDev::DebugComponent::Update
        // is the same empty body here, so leaving it out is behaviour-identical.
    }
}
