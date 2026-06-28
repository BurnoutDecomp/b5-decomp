#pragma once

// ===================================================================================
// BrnGameState::PaybackManager -- owning header
//   b5-decomp/src/GameSource/GameState/PaybackManager/BrnPaybackManager.h
//
// The online "payback" / dirty-trick manager: a two-headed finite-state machine (an
// AGGRESSOR side that earns + triggers a dirty trick on the player who took you down, and a
// VICTIM side that receives + survives/crashes from a dirty trick the player triggered on
// you). It drains the per-frame takedown + network dirty-trick event queues, drives both
// FSMs, publishes GUI notifications, and broadcasts dirty-trick network messages.
//
// SHAPE authoritative from the DecFIGS DWARF
// (references/DecFIGS/dwarfdump/GameSource/GameState/PaybackManager/BrnPaybackManager.h:64),
// member byte offsets gated against the X360 binary (Construct @0x82377238 / OnRoundEnd
// @0x8236D4D0 init every scalar; the committed BrnPaybackDebugComponent reads
// meActiveDirtyTrickType @ +596 and meAwardedDirtyTrick @ +600). FULL named layout -- this is
// the TU the placeholder predecessor said MUST replace the maReserved_0x000 blob in place:
//
//   +0    TimerStatusInterface             mTimerStatusInterface     (48B; CgsTimerStatusInterface.h)
//   +48   GameStateToNetworkInterface::DirtyTrickQueue mDirtyTrickOutputQueue (EventQueue<DirtyTrickEvent,28> == 460B -> +508)
//   +508  DirtyTrickEvent                  mEvent                    (16B; Construct sets {-1,-1,3,5})
//   +524  (4B alignment pad before the 8-byte-aligned Random)
//   +528  Random                           mRdmNumGenerator          (48B; CgsRandom.h, LCG ring -> +576)
//   +576  EActiveRaceCarIndex              mePaybackAggressorRaceCarIndex (-1)
//   +580  EActiveRaceCarIndex              mePaybackVictimRaceCarIndex    (-1)
//   +584  f32                              mfCountdownTimer          (-1.0)
//   +588  f32                              mfPaybackAggTimer         (-1.0)
//   +592  f32                              mfPaybackVictimTimer
//   +596  BrnNetwork::EPaybackType         meActiveDirtyTrickType    (3)
//   +600  BrnNetwork::EPaybackType         meAwardedDirtyTrick       (3)
//   +604  EPaybackAggressorState           mePaybackAggressorState   (0)
//   +608  EPaybackVictimState              mePaybackVictimState      (0)
//   +612  bool                             mbDirtyTrickButtonDown    (0)
//   +613  bool                             mbDirtyTrickButtonWasDown (0)
//   +614  bool                             mbPaybackAwarded          (0)
//   +615  (1B pad)
//   +616  GameStateModule*                 mpGameStateModule
//   +620  PaybackDebugComponent            mDebugComponent           (back-ptr mpPaybackManager @ +632 == +620+12)
//
// NOTE on the dirty-trick sentinel: Construct/ChangeState/asserts use the raw value 3 as the
// "no dirty trick" sentinel for meActiveDirtyTrickType/meAwardedDirtyTrick and the
// ProcessPaybackTriggerableEvent assert spells it `!= BrnNetwork::E_PAYBACK_TYPE_COUNT`. In
// THIS X360 build E_PAYBACK_TYPE_COUNT == 3 (3 real trick types: 0,1,2). The committed
// BrnNetworkSharedIO.h currently enumerates a 4th type with COUNT==4; that is a cross-TU
// discrepancy flagged for the reviewer -- this TU preserves X360 behaviour via the local
// KE_NO_DIRTY_TRICK sentinel (== 3) rather than editing the shared enum.
// ===================================================================================

#include "types.hpp"

#include "GameShared/GameClasses/System/Timer/CgsTimerStatusInterface.h"        // CgsSystem::TimerStatusInterface
#include "GameShared/GameClasses/Numeric/CgsRandom.h"                           // CgsNumeric::Random
#include "GameSource/Network/SharedIO/BrnNetworkSharedIO.h"                     // BrnNetwork::EPaybackType, ::EActiveRaceCarIndex, DirtyTrickEvent
#include "GameSource/GameState/ModeManager/Scoring/BrnScoringSystemEventQueues.h" // BrnGameState::GameStateToNetworkInterface::DirtyTrickQueue (== EventQueue<DirtyTrickEvent,28>)
#include "GameSource/GameState/PaybackManager/BrnPaybackDebugComponent.h"        // BrnGameState::PaybackDebugComponent (embedded by value @ +620)
#include "GameSource/GameState/BrnGameStateSharedIO.h"                          // GameStateModuleIO::EGameModeType

// The Update / Handle* vehicle-output param (DWARF spells it unqualified `VehicleOutputInterface`,
// which resolves to BrnPhysics::Vehicle::VehicleOutputInterface). Pointer-only in this header --
// forward-declared so we need no deep physics include. NOTE: do NOT alias it into BrnGameState as a
// typedef -- BrnGameState already carries a `namespace VehicleOutputInterface` (BrnScoringSystem.h
// keystone), so the fully-qualified physics type is used in the signatures below to avoid the clash.
namespace BrnPhysics { namespace Vehicle { struct VehicleOutputInterface; } }

namespace BrnGameState
{
    // Forward-declares for the method signatures (full layouts included by the .cpp).
    class GameStateModule;
    namespace GameStateModuleIO
    {
        struct PreWorldInputBuffer;
        struct OutputBuffer;
    }
    namespace InputBuffer { struct TakedownEventQueue; }  // BrnScoringSystemEventQueues.h completes it

    struct PaybackManager
    {
        // DWARF BrnPaybackManager.h:121 -- the AGGRESSOR-side FSM (you earn + spend payback on the
        // player who took you down).
        enum EPaybackAggressorState
        {
            E_PAYBACK_AGGRESSOR_STATE_IDLE              = 0,
            E_PAYBACK_AGGRESSOR_STATE_WAIT_AWARD_PAYBACK = 1,
            E_PAYBACK_AGGRESSOR_STATE_AWARD_DT          = 2,
            E_PAYBACK_AGGRESSOR_STATE_READY_TO_TRIGGER  = 3,
            E_PAYBACK_AGGRESSOR_STATE_YOU_TRIGGERED_DT  = 4,
            // X360-only 6th state: Update @0x8239AB78 has an explicit jump-table case 5 ->
            // HandleTriggeringPayback. Absent from the DecFIGS DWARF enum (which names 0..4 only) --
            // a merge-window delta / X360-side addition. Named here from its handler; X360-attested
            // by the jump table, so it exists per the ledger rule.
            E_PAYBACK_AGGRESSOR_STATE_TRIGGER_DT        = 5,
        };

        // DWARF BrnPaybackManager.h:139 -- the VICTIM-side FSM (a dirty trick was triggered on you).
        enum EPaybackVictimState
        {
            E_PAYBACK_VICTIM_STATE_IDLE             = 0,
            E_PAYBACK_VICTIM_STATE_TRIGGERED_ON_YOU = 1,
            E_PAYBACK_VICTIM_STATE_ACTIVE           = 2,
            E_PAYBACK_VICTIM_STATE_YOU_CRASHED      = 3,
            E_PAYBACK_VICTIM_STATE_YOU_SURVIVED     = 4,
        };

        // X360-attested "no dirty trick" sentinel for the two EPaybackType members (Construct stores
        // 3, ProcessPaybackTriggerableEvent asserts against it). See the header note above.
        static constexpr BrnNetwork::EPaybackType KE_NO_DIRTY_TRICK =
            static_cast<BrnNetwork::EPaybackType>(3);

        // ------------------------------------------------------------------ public API
        void Construct(GameStateModule* lpGameStateModule);                         // :70
        bool Prepare();                                                             // :74  (own TU)
        bool Release();                                                             // :78  (own TU)
        void Destruct();                                                           // :82  (own TU)

        void Update(const GameStateModuleIO::PreWorldInputBuffer* lpInput,          // :90
                    GameStateModuleIO::OutputBuffer* lpOutput,
                    const BrnPhysics::Vehicle::VehicleOutputInterface* lpVehicleOutputInterface,
                    const InputBuffer::TakedownEventQueue* lpQueue,
                    GameStateModuleIO::EGameModeType leGameModeType);

        void OnRoundStart();                                                        // :94  (own TU)
        void OnRoundEnd(bool lbResetState);                                         // :99
        void ProcessPaybackTriggerableEvent();                                     // :103
        void SetDirtyTrickButtonState(bool lbButtonPressed);                       // :106 (own TU)
        void SetTimerInterface(const CgsSystem::TimerStatusInterface* lpTimerStatusInterface); // :109 (own TU)

    private:
        // ------------------------------------------------------------------ internals
        void ResetState();                                                         // :170 (own TU)

        void ProcessTakedownEvents(const GameStateModuleIO::PreWorldInputBuffer* lpInput, // :177
                                   GameStateModuleIO::OutputBuffer* lpOutput,
                                   const InputBuffer::TakedownEventQueue* lpQueue,
                                   GameStateModuleIO::EGameModeType leGameModeType);

        void ProcessDirtyTrickEventQueue(const GameStateModuleIO::PreWorldInputBuffer* lpInput, // :182
                                         GameStateModuleIO::OutputBuffer* lpOutput);

        void SendNetworkDirtyTrickMessage(::EActiveRaceCarIndex leAggressorRaceCarIndex, // :190
                                          ::EActiveRaceCarIndex leVictimRaceCarIndex,
                                          BrnNetwork::EPaybackType leDirtyTrickType,
                                          BrnNetwork::EDirtyTrickStatus leDirtyTrickStatus);

        void DirtyTrickAwarded(GameStateModuleIO::OutputBuffer* lpOutput,           // :198 (own TU)
                               ::EActiveRaceCarIndex leDTAggressorRaceCarIndex,
                               ::EActiveRaceCarIndex leDTVictimRaceCarIndex,
                               BrnNetwork::EPaybackType leDirtyTrickType);

        void DirtyTrickTriggered(GameStateModuleIO::OutputBuffer* lpOutput,        // :206 (own TU)
                                 ::EActiveRaceCarIndex leAggressorRaceCarIndex,
                                 ::EActiveRaceCarIndex leVictimRaceCarIndex,
                                 BrnNetwork::EPaybackType leDirtyTrickType);

        void DirtyTrickEnding(GameStateModuleIO::OutputBuffer* lpOutput,           // :215 (own TU)
                              ::EActiveRaceCarIndex leAggressorRaceCarIndex,
                              ::EActiveRaceCarIndex leVictimRaceCarIndex,
                              BrnNetwork::EPaybackType leDirtyTrickType, bool lbSurvived);

        void ShowDTAvailableHudNotification(bool lbShowMessage);                   // :220 (own TU)

        void StartCountdown();                                                     // :223 (own TU)
        void UpdateCountdown();                                                    // :226
        bool IsCountdownComplete();                                               // :229 (own TU)
        void RemoveCountdown();                                                    // :232 (own TU)

        void ChangeState(EPaybackAggressorState leNewPaybackAggState);             // :237
        void ChangeState(EPaybackVictimState leNewPaybackVictimState);             // :242 (own TU)

        void UpdateFSMTimers();                                                    // :246 (own TU)

        void HandleWaitForPaybackAggressorToCrash(const BrnPhysics::Vehicle::VehicleOutputInterface* lpVehicleOutputInterface); // (own TU)
        void HandleWaitingToAwardPayback(const BrnPhysics::Vehicle::VehicleOutputInterface* lpVehicleOutputInterface);          // :255 (own TU)
        void HandleAwardingPayback(GameStateModuleIO::OutputBuffer* lpOutput,      // :262 (own TU)
                                   const BrnPhysics::Vehicle::VehicleOutputInterface* lpVehicleOutputInterface,
                                   GameStateModuleIO::EGameModeType leGameModeType);
        void HandleHavingPayback(GameStateModuleIO::OutputBuffer* lpOutput);       // :267
        void HandleTriggeringPayback(GameStateModuleIO::OutputBuffer* lpOutput);   // :272
        void HandleReceivingPayback(GameStateModuleIO::OutputBuffer* lpOutput,     // :278 (own TU)
                                    const BrnPhysics::Vehicle::VehicleOutputInterface* lpVehicleOutputInterface);
        void HandleActivePayback(GameStateModuleIO::OutputBuffer* lpOutput);       // :283 (own TU)
        void HandleCrashDueToPayback(GameStateModuleIO::OutputBuffer* lpOutput);   // :288 (own TU)
        void HandleSurvivingPayback(GameStateModuleIO::OutputBuffer* lpOutput);    // :293 (own TU)

        // ------------------------------------------------------------------ data members
        CgsSystem::TimerStatusInterface               mTimerStatusInterface;     // :150  @ +0
        GameStateToNetworkInterface::DirtyTrickQueue  mDirtyTrickOutputQueue;    // :151  @ +48
        BrnNetwork::BrnNetworkModuleIO::DirtyTrickEvent mEvent;                  // :152  @ +508
        CgsNumeric::Random                            mRdmNumGenerator;          // :153  @ +528
        ::EActiveRaceCarIndex                         mePaybackAggressorRaceCarIndex; // :154 @ +576
        ::EActiveRaceCarIndex                         mePaybackVictimRaceCarIndex;    // :155 @ +580
        f32                                           mfCountdownTimer;          // :156  @ +584
        f32                                           mfPaybackAggTimer;         // :157  @ +588
        f32                                           mfPaybackVictimTimer;      // :158  @ +592

    public:
        // The two dirty-trick enum fields are read directly by the embedded PaybackDebugComponent
        // (mpPaybackManager->meActiveDirtyTrickType / ->meAwardedDirtyTrick), so they stay public.
        BrnNetwork::EPaybackType                      meActiveDirtyTrickType;    // :159  @ +596
        BrnNetwork::EPaybackType                      meAwardedDirtyTrick;       // :160  @ +600

    private:
        EPaybackAggressorState                        mePaybackAggressorState;   // :161  @ +604
        EPaybackVictimState                           mePaybackVictimState;      // :162  @ +608
        bool                                          mbDirtyTrickButtonDown;    // :163  @ +612
        bool                                          mbDirtyTrickButtonWasDown; // :164  @ +613
        bool                                          mbPaybackAwarded;          // :165  @ +614
        GameStateModule*                              mpGameStateModule;         // :166  @ +616
        PaybackDebugComponent                         mDebugComponent;           // embedded @ +620
    };
}
