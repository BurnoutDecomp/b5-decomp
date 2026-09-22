// ===================================================================================
// BrnNetwork::NetworkAggressiveDrivingManager -- owning header
//   b5-decomp/src/GameSource/Network/Managers/BrnNetworkAggressiveDrivingManager.h
//
// The online aggressive-driving relay. It keeps a small fixed table of per-player slots
// (KI_MAX_NETWORK_PLAYERS == 7), each holding a send/receive AggressiveDrivingMessage plus
// a priority-ordered buffer of the most "interesting" aggressive moves (takedowns / slams /
// shunts / trading-paint / battling) that local cars performed against that player. Each
// frame it scans the local physics ImpactEvent / TakedownEvent queues, classifies the move,
// buffers it (replacing the least-interesting buffered move when full), and round-robins the
// buffered moves out over the reliable-message channel. On the receive side it turns inbound
// aggressive moves back into local TakedownEvent / ImpactEvent / battling-gui events.
//
// SHAPE authoritative from the DecFIGS DWARF
//   (references/DecFIGS/dwarfdump/.../BrnNetworkAggressiveDrivingManager.h:73/130), gated
// against the X360 binary. The X360 manager-`this` member offsets pin the layout exactly:
//   maAggressiveDrivingData[7]   @ +0      (7 * 1728-byte AggressiveDrivingData stride == 12096)
//   mfNoImpactTime               @ +12096  (f32; OnRoundStart sets 8.0/-1.0, ProcessBefore decays)
//   mLastVictimNetworkPlayerID   @ +12100  (s32; reset to -1)
//   miConsecutiveImpactsOnSamePlayer @ +12104 (s32; reset to 0)
//   mpNetworkModule              @ +12108
//   mpPlayerManager              @ +12112
//   mpTimeManager               @ +12116
//   mbAreWeInOnlineGame          @ +12120  (the "are we live" gate ProcessBefore/After check)
// (Console stride proof, Construct / AddPlayer: AggressiveDrivingData == mPlayerID(4) + pad to the
//  16-byte-aligned send message @ +0x10, recv message @ +0x1D0 (0x1C0 bytes each), the
//  AggressiveMoveData maBuffer[10] (80B stride, @ +912 within the entry) and miBufferCount
//  (@ +1712 within the entry), padded to 1728; 7 * 1728 == 12096 places mfNoImpactTime at
//  +12096; the object is 0x2F60 bytes.) Members are accessed strictly BY NAME; the console
//  strides are pinned in a 32-bit build only (_AssertLayout).
//
// FUNCTION OWNERSHIP: all 17 functions in this TU are bodied in the sibling .cpp.
// ===================================================================================
#pragma once

#include <cstddef>                                                 // offsetof (_AssertLayout)

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameSource/Network/SharedIO/BrnNetworkSharedIO.h"          // BrnNetwork::NetworkPlayerID, EActiveRaceCarIndex
#include "GameSource/CompilerDefines/gameshared_network_defines.h"       // KI_MAX_NETWORK_PLAYERS
#include "GameSource/Network/Messages/BrnAggressiveDrivingMessage.h" // AggressiveDrivingMessage, AggressiveMoveData
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.h"  // BrnPhysics::Vehicle::ImpactEvent
#include "GameSource/GameState/TakedownManager/BrnTakedownManagerTypes.h" // BrnGameState::TakedownEvent

namespace CgsNetwork
{
    struct PlayerManager;   // pointer-only
    struct TimeManager;     // pointer-only
}

namespace BrnNetwork
{
    class BrnNetworkModule; // pointer-only (DWARF h:144)

    namespace BrnNetworkModuleIO
    {
        struct OutputBuffer;               // ProcessBeforeSimulation / HandleReceivingMessages param
        struct PostSimulationInputBuffer;  // ProcessAfterSimulation / ProcessInputQueue param
    }

    // DWARF BrnNetworkAggressiveDrivingManager.h:58
    const s32 KI_MAX_PLAYER_AGGRESSIVE_MOVES_BUFFER = 10;
    // The per-frame send budget (DWARF .cpp:33) and the consecutive-impact battling threshold
    // (.cpp:37). KF_BATTLING_IMPACT_DELAY (.cpp:36) is the no-impact decay window used by
    // UpdateOnlineBattling/ProcessBeforeSimulation (X360 sets mfNoImpactTime to 8.0 on a fresh
    // impact and decays it by the frame step; the >= 0 test is the "still battling" window).
    const s32 KI_MAX_NUMBER_OF_AGGRESSIVE_MOVE_SENDS = 3;
    const f32 KF_BATTLING_IMPACT_DELAY               = 8.0f;
    const s32 KI_CONSECUTIVE_IMPACTS_FOR_BATTLING    = 5;

    struct NetworkAggressiveDrivingManager
    {
        // DWARF BrnNetworkAggressiveDrivingManager.h:130 -- one player's send/receive slot pair
        // plus the priority-ordered buffer of aggressive moves queued for that player.
        struct AggressiveDrivingData
        {
            NetworkPlayerID          mPlayerID;                    // +0x000 (DWARF :132)
            AggressiveDrivingMessage mAggressiveDrivingMessageSend; // +0x010
            AggressiveDrivingMessage mAggressiveDrivingMessageRecv; // +0x1D0
            AggressiveMoveData       maBuffer[KI_MAX_PLAYER_AGGRESSIVE_MOVES_BUFFER]; // X360 @ +912 (DWARF :136)
            s32                      miBufferCount;                // X360 @ +1712 (DWARF :137)
        };

        // The C++ constructor (called from BrnNetworkManager's constructor): member construction
        // only (the per-slot messages and their move arrays). Distinct from Construct().
        NetworkAggressiveDrivingManager();

        // ---- lifecycle / per-frame API (bodied in this TU) -------------------------
        void Construct(BrnNetworkModule* lpNetworkModule,
                       CgsNetwork::PlayerManager* lpPlayerManager,
                       CgsNetwork::TimeManager* lpTimeManager);
        bool Prepare();
        bool Release();
        void Destruct();
        void ProcessBeforeSimulation(BrnNetworkModuleIO::OutputBuffer* lpOutput, f32 lfTimeStep);
        void ProcessAfterSimulation(const BrnNetworkModuleIO::PostSimulationInputBuffer* lpInput);
        void AddPlayer(NetworkPlayerID lPlayerID);
        void RemovePlayer(NetworkPlayerID lPlayerID);
        void Disconnected();
        void OnRoundStart();
        void OnRoundFinish();

    private:
        // ---- internals (bodied in this TU) -----------------------------------------
        AggressiveDrivingData* GetAggressiveDrivingDataEntry(NetworkPlayerID lPlayerID);
        void ProcessInputQueue(const BrnNetworkModuleIO::PostSimulationInputBuffer* lpInput);
        void HandleSendingMessages();
        void HandleReceivingMessages(BrnNetworkModuleIO::OutputBuffer* lpOutput);
        bool AddImpactEvent(const BrnPhysics::Vehicle::ImpactEvent* lpImpactEvent);
        bool AddTakedownEvent(const BrnGameState::TakedownEvent* lpTakedownEvent);
        void RemoveAggressiveMove(s32 liPlayerIndex, s32 liMoveIndex);
        bool UpdateMarkedPlayersOnMarkedManTakeDown(EActiveRaceCarIndex leAggressorActiveRaceCarIndex);
        void UpdateOnlineBattling(const BrnPhysics::Vehicle::ImpactEvent* lpImpactEvent);
        s32  CompareMove(const AggressiveMoveData* lpMove0, const AggressiveMoveData* lpMove1);
        bool AddNewMove(AggressiveDrivingData* lpData, const AggressiveMoveData* lpNewMove);

        // ---- data layout (DWARF h:140-147) -----------------------------------------
        AggressiveDrivingData maAggressiveDrivingData[KI_MAX_NETWORK_PLAYERS]; // +0x000 (X360: 7 x 1728B stride)
        f32                   mfNoImpactTime;                                  // X360 @ +12096
        NetworkPlayerID       mLastVictimNetworkPlayerID;                      // X360 @ +12100
        s32                   miConsecutiveImpactsOnSamePlayer;                // X360 @ +12104
        BrnNetworkModule*          mpNetworkModule;                            // X360 @ +12108
        CgsNetwork::PlayerManager* mpPlayerManager;                            // X360 @ +12112
        CgsNetwork::TimeManager*   mpTimeManager;                              // X360 @ +12116
        bool                  mbAreWeInOnlineGame;                             // X360 @ +12120

        // Console layout, pinned in a 32-bit build; inert on the x64 host. The record stride is
        // 0x6C0 once AggressiveDrivingMessage reproduces its 0x1C0 console bytes; until then the
        // records are pinned relative to the message size.
        static void _AssertLayout();
    };

    inline void NetworkAggressiveDrivingManager::_AssertLayout()
    {
        static_assert(sizeof(void*) != 4 || offsetof(AggressiveDrivingData, mAggressiveDrivingMessageSend) == 0x10, "AggressiveDrivingData::mAggressiveDrivingMessageSend @ +0x10");
        static_assert(sizeof(void*) != 4 || sizeof(AggressiveMoveData) == 0x50, "AggressiveMoveData stride is 0x50");
        static_assert(sizeof(void*) != 4 || offsetof(AggressiveDrivingData, maBuffer) == 0x10 + 2 * sizeof(AggressiveDrivingMessage), "AggressiveDrivingData::maBuffer follows the messages");
        static_assert(sizeof(void*) != 4 || sizeof(AggressiveDrivingData) == 0x10 + 2 * sizeof(AggressiveDrivingMessage) + 0x330, "AggressiveDrivingData tail is 0x330 bytes");
        static_assert(sizeof(void*) != 4 || sizeof(NetworkAggressiveDrivingManager) == KI_MAX_NETWORK_PLAYERS * sizeof(AggressiveDrivingData) + 0x20, "NetworkAggressiveDrivingManager tail is 0x20 bytes");
    }
} // namespace BrnNetwork
