#pragma once

// ===================================================================================
// BrnGameState::GameStateModuleIO::GameStateToGuiInterface -- owning header
//   b5-decomp/src/GameSource/GameState/SharedIO/BrnGameStateToGuiIOInterfaces.h
//
// The GameState -> GUI half of the per-frame output buffer: a bundle of small
// fixed-capacity EventQueue<T,N> notification queues (new/triggered/ending dirty
// trick, overtakes, race finished, took-lead/last, on-tail) plus the cross-module
// race-car-crash queue and the cached player race-car slot index.
//
// SHAPE authoritative from the DecFIGS DWARF
// (references/DecFIGS/dwarfdump/GameSource/GameState/SharedIO/BrnGameStateToGuiIOInterfaces.h:60),
// member offsets gated against the X360 binary: the BrnPaybackManager bodies inline
// AddDirtyTrickTriggered / AddDirtyTrickEnding as direct AddEvent calls onto the
// embedded sub-queues, pinning mDirtyTrickTriggeredQueue @ +0x40 (64) and
// mDirtyTrickEndingQueue @ +0x7C (124) -- which exactly matches:
//   +0    s32                       miPlayerRaceCarIndex
//   +4    NewDirtyTrickQueue        mNewDirtyTrickQueue        (EventQueue<...,4>: base 12B + 4*12B == 60B -> ends +64)
//   +64   DirtyTrickTriggeredQueue  mDirtyTrickTriggeredQueue  (12 + 4*12 == 60B -> ends +124)
//   +124  DirtyTrickEndingQueue     mDirtyTrickEndingQueue     (EndingDirtyTrick is 16B: 12 + 4*16 == 76B)
//   ...   the remaining notification queues, then the crash queue.
//
// The Add* publishers are bodied by this interface's own TU (BrnGameStateToGuiIOInterfaces.cpp,
// not yet reconstructed); declared-only here so callers (PaybackManager, ModeManager scoring)
// compile against the de-inlined named API rather than raw sub-queue offset writes.
// ===================================================================================

#include "types.hpp"
#include "BrnCommonTypes.h"                                         // CgsID
#include "GameSource/BurnoutConstants.h"                            // ::EActiveRaceCarIndex
#include "GameSource/Network/SharedIO/BrnNetworkSharedIO.h"         // BrnNetwork::EPaybackType
#include "GameShared/GameClasses/Module/CgsEventQueue.h"            // CgsModule::EventQueue<T,N>
#include "GameSource/GameState/SharedIO/BrnGameStateToGuiEvents.h"  // the GameStateToGui* event records (+ BrnGui::EFinishType)

namespace BrnGameState
{
namespace GameStateModuleIO
{
    // DWARF BrnGameStateToGuiIOInterfaces.h:36 -- shared capacity of the small notification queues.
    const s32 KI_MAX_GAME_STATE_TO_GUI_QUEUE_LENGTH = 4;

    struct GameStateToGuiInterface
    {
        // ---- queue element typedefs (DWARF :42-49) ----
        typedef CgsModule::EventQueue<GameStateToGuiNewDirtyTrick,       4> NewDirtyTrickQueue;
        typedef CgsModule::EventQueue<GameStateToGuiTriggeredDirtyTrick, 4> DirtyTrickTriggeredQueue;
        typedef CgsModule::EventQueue<GameStateToGuiEndingDirtyTrick,    4> DirtyTrickEndingQueue;
        typedef CgsModule::EventQueue<GameStateToGuiOvertakeEvent,       4> OvertakeEventQueue;
        typedef CgsModule::EventQueue<GameStateToGuiFinishedRaceEvent,   4> FinishedRaceEventQueue;
        typedef CgsModule::EventQueue<GameStateToGuiTookLeadEvent,       1> TookLeadEventQueue;
        typedef CgsModule::EventQueue<GameStateToGuiTookLastEvent,       1> TookLastEventQueue;
        typedef CgsModule::EventQueue<GameStateToGuiOnTailEvent,         7> OnTailEventQueue;

        // ---- bodied by the BrnGameStateToGuiIOInterfaces TU (declared-only here) ----
        void Construct();                                                   // DWARF :65
        void Clear();                                                       // DWARF :69

        // The de-inlined dirty-trick publishers. The X360 PaybackManager bodies inline these as a
        // direct write of the three/four fields into the matching sub-queue followed by AddEvent.
        void AddNewDirtyTrick(::EActiveRaceCarIndex leAggressor, ::EActiveRaceCarIndex leVictim,
                              BrnNetwork::EPaybackType leTrickType);                          // DWARF :76
        void AddDirtyTrickTriggered(::EActiveRaceCarIndex leAggressor, ::EActiveRaceCarIndex leVictim,
                                    BrnNetwork::EPaybackType leTrickType);                     // DWARF :83
        void AddDirtyTrickEnding(::EActiveRaceCarIndex leAggressor, ::EActiveRaceCarIndex leVictim,
                                 BrnNetwork::EPaybackType leTrickType, bool lbSurvived);       // DWARF :91

        void AddOvertakeEvent(u8 lu8NewPosition, ::EActiveRaceCarIndex leActiveRaceCarIndex);    // DWARF :97
        void AddFinishedRaceEvent(BrnGui::EFinishType leFinishType, ::EActiveRaceCarIndex leActiveRaceCarIndex); // DWARF :103
        void AddTookLeadEvent(CgsID lOfflineRivalCarID, ::EActiveRaceCarIndex leActiveRaceCarIndex);  // DWARF :109
        void AddTookLastEvent(CgsID lOfflineRivalCarID, ::EActiveRaceCarIndex leActiveRaceCarIndex);  // DWARF :115
        void AddOnTailEvent(CgsID lOfflineRivalCarID, ::EActiveRaceCarIndex leActiveRaceCarIndex);    // DWARF :121

        const NewDirtyTrickQueue*       GetNewDirtyTrickQueue() const;        // DWARF :128
        const DirtyTrickTriggeredQueue* GetDirtyTrickTriggeredQueue() const;  // DWARF :129
        const DirtyTrickEndingQueue*    GetDirtyTrickEndingQueue() const;     // DWARF :130
        const OvertakeEventQueue*       GetOvertakeEventQueue() const;        // DWARF :131
        const FinishedRaceEventQueue*   GetFinishedRaceEventQueue() const;    // DWARF :132
        const TookLeadEventQueue*       GetTookLeadEventQueue() const;        // DWARF :133
        const TookLastEventQueue*       GetTookLastEventQueue() const;        // DWARF :134
        const OnTailEventQueue*         GetOnTailEventQueue() const;          // DWARF :135

        void SetPlayerRaceCarIndex(s32 liPlayerRaceCarIndex);                 // DWARF :137
        s32  GetPlayerRaceCarIndex() const;                                  // DWARF :138

    private:
        s32                      miPlayerRaceCarIndex;       // DWARF :142  @ +0
        NewDirtyTrickQueue       mNewDirtyTrickQueue;        // DWARF :143  @ +4
        DirtyTrickTriggeredQueue mDirtyTrickTriggeredQueue;  // DWARF :144  @ +64
        DirtyTrickEndingQueue    mDirtyTrickEndingQueue;     // DWARF :145  @ +124
        OvertakeEventQueue       mOvertakeEventQueue;        // DWARF :146
        FinishedRaceEventQueue   mFinishedRaceEventQueue;    // DWARF :147
        TookLeadEventQueue       mTookLeadEventQueue;        // DWARF :148
        TookLastEventQueue       mTookLastEventQueue;        // DWARF :149
        OnTailEventQueue         mOnTailEventQueue;          // DWARF :150

        // DWARF :151 mRaceCarCrashEventQueue ==
        // VehicleManagerOutputInterface::RaceCarCrashEventQueue == EventQueue<BrnPhysics::Vehicle::
        // RaceCarCrashEvent,8>. Its element type (a 64-byte Matrix44Affine-carrying crash record) is
        // owned by the VehicleManager / RaceCarEntityModule TUs; modelled here as a documented opaque
        // tail so this interface is a complete type without forking that physics type. Sized base(12B)
        // + 8 * 64B == 524B (the trailing crash queue is never touched by the PaybackManager bodies).
        u8                       maRaceCarCrashEventQueueStorage[524];   // DWARF :151
    };
} // namespace GameStateModuleIO
} // namespace BrnGameState
