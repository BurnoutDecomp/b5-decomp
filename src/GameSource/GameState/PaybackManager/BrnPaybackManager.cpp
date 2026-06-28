// ===================================================================================
// b5-decomp/src/GameSource/GameState/PaybackManager/BrnPaybackManager.cpp
//
// The online payback / dirty-trick manager. Reconstructed from BURNOUT_X360_ARTIST.XEX
// (semantic parity, not byte-matching). Eleven of the manager's functions live here; the
// remaining declared members (Prepare/Release/Destruct/OnRoundStart/the Handle* helpers/
// DirtyTrick{Awarded,Triggered,Ending}/Show*/StartCountdown/IsCountdownComplete/RemoveCountdown/
// the victim-side ChangeState/UpdateFSMTimers/SetDirtyTrickButtonState/SetTimerInterface) are
// other passes -- they are called by name here and trap-stub linked.
//
// Source-of-truth: X360 ASM (behaviour + calling convention) > DecFIGS DWARF (shape) > none.
// ===================================================================================

#include "GameSource/GameState/PaybackManager/BrnPaybackManager.h"

#include "GameSource/GameState/BrnGameStateModule.h"                     // GameStateModule::Get{Player...,OutputGuiEventQueue}
#include "GameSource/GameState/BrnGameStateModuleIO.h"                   // PreWorldInputBuffer / OutputBuffer accessors
#include "GameSource/GameState/SharedIO/BrnGameStateToGuiIOInterfaces.h" // GameStateToGuiInterface (complete)
#include "GameSource/Network/SharedIO/BrnNetworkModuleGameStateIOInterfaces.h" // GameStateToNetworkInterface (complete; ->GetDirtyTrickQueue)
#include "GameSource/GameState/TakedownManager/BrnTakedownManagerTypes.h" // BrnGameState::TakedownEvent
#include "GameShared/GameClasses/Core/CgsAssert.h"                       // CGS_ASSERT

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

        // The engine carries three layout-identical (all `: s32`) active-race-car-index enums in
        // different scopes -- global ::EActiveRaceCarIndex (the PaybackManager's own index space),
        // BrnNetwork::EActiveRaceCarIndex (DirtyTrickEvent fields), and BrnGameState::
        // EActiveRaceCarIndex (TakedownEvent fields). They are NOT implicitly interconvertible, so
        // bridge at the boundaries with these explicit same-value converters.
        inline ::EActiveRaceCarIndex ToGlobalRCI(BrnNetwork::EActiveRaceCarIndex le)
        { return static_cast<::EActiveRaceCarIndex>(static_cast<s32>(le)); }
        inline ::EActiveRaceCarIndex ToGlobalRCI(BrnGameState::EActiveRaceCarIndex le)
        { return static_cast<::EActiveRaceCarIndex>(static_cast<s32>(le)); }
        inline BrnNetwork::EActiveRaceCarIndex ToNetworkRCI(::EActiveRaceCarIndex le)
        { return static_cast<BrnNetwork::EActiveRaceCarIndex>(static_cast<s32>(le)); }
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
        mEvent.meAggressorActiveRaceCarIndex = BrnNetwork::E_ACTIVE_RACE_CAR_NONE;
        mEvent.meVictimActiveRaceCarIndex    = BrnNetwork::E_ACTIVE_RACE_CAR_NONE;
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
    // OnRoundEnd  @ 0x8236D4D0
    // When asked to reset, return both FSMs to their post-Construct idle state and re-prime the RNG.
    // (When not asked to reset, the X360 is a no-op.)
    // -----------------------------------------------------------------------------------
    void
    PaybackManager::OnRoundEnd(bool lbResetState)
    {
        if (!lbResetState)
            return;

        mePaybackAggressorState = E_PAYBACK_AGGRESSOR_STATE_IDLE;
        mePaybackVictimState    = E_PAYBACK_VICTIM_STATE_IDLE;

        mePaybackAggressorRaceCarIndex = ::E_ACTIVE_RACE_CAR_INDEX_INVALID;
        mePaybackVictimRaceCarIndex    = ::E_ACTIVE_RACE_CAR_INDEX_INVALID;

        mfCountdownTimer  = -1.0f;
        mfPaybackAggTimer = -1.0f;

        meActiveDirtyTrickType = KE_NO_DIRTY_TRICK;   // 3
        meAwardedDirtyTrick    = KE_NO_DIRTY_TRICK;   // 3

        mbDirtyTrickButtonDown    = false;
        mbDirtyTrickButtonWasDown = false;
        mbPaybackAwarded          = false;

        mEvent.meAggressorActiveRaceCarIndex = BrnNetwork::E_ACTIVE_RACE_CAR_NONE;
        mEvent.meVictimActiveRaceCarIndex    = BrnNetwork::E_ACTIVE_RACE_CAR_NONE;
        mEvent.meDirtyTrickType              = KE_NO_DIRTY_TRICK;                       // 3
        mEvent.meDirtyTrickStatus            = static_cast<BrnNetwork::EDirtyTrickStatus>(5);

        mRdmNumGenerator.Construct();
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
        lDirtyTrickEvent.meAggressorActiveRaceCarIndex = ToNetworkRCI(leAggressorRaceCarIndex);
        lDirtyTrickEvent.meVictimActiveRaceCarIndex    = ToNetworkRCI(leVictimRaceCarIndex);
        lDirtyTrickEvent.meDirtyTrickType              = leDirtyTrickType;
        lDirtyTrickEvent.meDirtyTrickStatus            = leDirtyTrickStatus;

        mDirtyTrickOutputQueue.AddEvent(lDirtyTrickEvent);
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
        const ::EActiveRaceCarIndex    lePlayerIndex = ToGlobalRCI(mpGameStateModule->GetPlayerActiveRaceCarIndex());

        SendNetworkDirtyTrickMessage(lePlayerIndex, leVictimIndex, leAwarded,
                                     static_cast<BrnNetwork::EDirtyTrickStatus>(2));

        const BrnNetwork::EPaybackType leAwarded2     = meAwardedDirtyTrick;
        const ::EActiveRaceCarIndex    leVictimIndex2 = mePaybackVictimRaceCarIndex;
        const ::EActiveRaceCarIndex    lePlayerIndex2 = ToGlobalRCI(mpGameStateModule->GetPlayerActiveRaceCarIndex());

        lpOutput->GetGameStateToGuiInterface()->AddDirtyTrickTriggered(
            lePlayerIndex2, leVictimIndex2, leAwarded2);

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
    // ProcessTakedownEvents  @ 0x82397658
    // Walk this frame's takedown queue; for each marked-man takedown where the LOCAL player is the
    // victim, arm the aggressor FSM: remember who took you down, drop into WAIT_AWARD_PAYBACK, and
    // show the payback HUD.
    // -----------------------------------------------------------------------------------
    void
    PaybackManager::ProcessTakedownEvents(const GameStateModuleIO::PreWorldInputBuffer* /*lpInput*/,
                                          GameStateModuleIO::OutputBuffer* /*lpOutput*/,
                                          const InputBuffer::TakedownEventQueue* lpQueue,
                                          GameStateModuleIO::EGameModeType /*leGameModeType*/)
    {
        for (s32 liIndex = 0; liIndex < lpQueue->GetLength(); ++liIndex)
        {
            const TakedownEvent& lTakedownEvent = lpQueue->GetEvent(liIndex);

            const ::EActiveRaceCarIndex leTakedownAggressorRaceCarIndex = ToGlobalRCI(lTakedownEvent.meAggressorIndex);
            const ::EActiveRaceCarIndex leTakedownVictimRaceCarIndex    = ToGlobalRCI(lTakedownEvent.meVictimIndex);

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
            if (ToGlobalRCI(mpGameStateModule->GetPlayerActiveRaceCarIndex()) != leTakedownVictimRaceCarIndex)
                continue;

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

        const CgsModule::EventQueue<BrnNetwork::BrnNetworkModuleIO::DirtyTrickEvent, 28>* lpDirtyTrickQueue =
            lpInput->GetNetworkToGameStateInterface()->GetDirtyTrickQueue();

        for (s32 liIndex = 0; liIndex < lpDirtyTrickQueue->GetLength(); ++liIndex)
        {
            const BrnNetwork::BrnNetworkModuleIO::DirtyTrickEvent& lEvent = lpDirtyTrickQueue->GetEvent(liIndex);

            const BrnNetwork::EActiveRaceCarIndex leNetAggressor = lEvent.meAggressorActiveRaceCarIndex;
            const BrnNetwork::EActiveRaceCarIndex leNetVictim    = lEvent.meVictimActiveRaceCarIndex;
            const ::EActiveRaceCarIndex      leAggressor = ToGlobalRCI(leNetAggressor);
            const ::EActiveRaceCarIndex      leVictim    = ToGlobalRCI(leNetVictim);
            const BrnNetwork::EPaybackType   leType      = lEvent.meDirtyTrickType;
            const BrnNetwork::EDirtyTrickStatus leStatus = lEvent.meDirtyTrickStatus;

            switch (static_cast<s32>(leStatus))
            {
                case 1:   // AWARDED -- no local FSM/GUI action
                    break;

                case 2:   // TRIGGERED on a car -- if it is you, become the victim
                    if (leVictim == ToGlobalRCI(mpGameStateModule->GetPlayerActiveRaceCarIndex()))
                    {
                        // X360 @0x82383CA8 case 2: a1[152]=1 (victim state) then a1[144]=aggressor.
                        // a1[144] == +576 == mePaybackAggressorRaceCarIndex (NOT the victim index at
                        // +580): on the victim side this records who triggered the dirty trick on you.
                        mePaybackVictimState           = E_PAYBACK_VICTIM_STATE_TRIGGERED_ON_YOU;
                        mePaybackAggressorRaceCarIndex = leAggressor;

                        mEvent.meAggressorActiveRaceCarIndex = leNetAggressor;
                        mEvent.meVictimActiveRaceCarIndex    = leNetVictim;
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
                    if (leAggressor == ToGlobalRCI(mpGameStateModule->GetPlayerActiveRaceCarIndex()))
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
                           const InputBuffer::TakedownEventQueue* lpQueue,
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
                HandleWaitingToAwardPayback(lpVehicleOutputInterface);
                break;
            case E_PAYBACK_AGGRESSOR_STATE_READY_TO_TRIGGER:
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
                HandleReceivingPayback(lpOutput, lpVehicleOutputInterface);
                break;
            case E_PAYBACK_VICTIM_STATE_ACTIVE:
                HandleActivePayback(lpOutput);
                break;
            case E_PAYBACK_VICTIM_STATE_YOU_CRASHED:
                HandleCrashDueToPayback(lpOutput);
                break;
            case E_PAYBACK_VICTIM_STATE_YOU_SURVIVED:
                HandleSurvivingPayback(lpOutput);
                break;
            default:
                CGS_ASSERT(false, "Unknown payback victim state");
                break;
        }

        // Publish this frame's outbound dirty-trick events onto the GameState->Network interface's
        // dirty-trick queue, then clear the manager's per-frame queue (X360 *(this+56) = 0).
        lpOutput->GetGameStateToNetworkInterface()->GetDirtyTrickQueue()->Append(mDirtyTrickOutputQueue);
        mDirtyTrickOutputQueue.Clear();

        // (X360 tail: a virtual hook on the embedded debug component -- a debug-only per-frame record
        // call -- is omitted here; it has no retail-observable effect. See packet uncertainties.)
    }
}
