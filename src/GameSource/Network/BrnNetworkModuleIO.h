// ============================================================================
// b5-decomp/src/GameSource/Network/BrnNetworkModuleIO.h
// ============================================================================
// BrnNetwork::BrnNetworkModuleIO is a NAMESPACE. It holds the three per-frame IO buffers the
// network module exchanges with the rest of the game (PreSimulationInputBuffer,
// PostSimulationInputBuffer, OutputBuffer, all CgsModule::IOBuffer), the module's
// NetworkEventQueue, and the player-results interface.
//
// Every lock-guarded accessor asserts the buffer lock first: a const accessor asserts
// IsBufferLockedForReading() ("Not locked for reading\n"), a non-const one asserts
// IsBufferLockedForWriting() ("Not locked for writing\n"), then returns &member.
//
// LAYOUTS. Members are listed in declaration order with their CONSOLE offsets, recovered from
// each buffer's Construct / Destruct (which construct every member at its offset) and from the
// accessors (each returns this + offset). Most members hold pointers (event-queue headers), so
// the host offsets are larger; reach members by name only, never by the console numbers.
//
// The accessor bodies' assert line numbers run in blocks of about seven lines in member-function
// order, which is how the unnamed accessors were matched to the reference declaration list; the
// declarations with no out-of-line console body fall exactly in the gaps of that sequence.
//
// One member cannot be typed here: OutputBuffer::mGameEventQueue is a
// BrnGameState::GameStateModuleIO::GameEventQueue, which is defined in BrnGameStateModuleIO.h --
// and that header includes this one (for PlayerResultsInterface). It is held as storage of the
// queue's exact size (GameEventQueue adds nothing to VariableEventQueue<1536,16>) and the two
// accessors hand it out as the real type.

#pragma once

#include <cstddef>
#include "types.hpp"
#include "GameShared/GameClasses/Module/CgsIOBuffer.h"                          // CgsModule::IOBuffer (base; 1-byte status flags @ +0)
#include "GameShared/GameClasses/Core/CgsAssert.h"                              // CGS_ASSERT (PlayerResultsInterface::GetPlayerResultsData)
#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"                     // CgsModule::BaseEventQueue<T>
#include "GameShared/GameClasses/Module/CgsEventQueue.h"                         // CgsModule::EventQueue<T,N>
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"                 // CgsModule::VariableEventQueue<N,16>
#include "GameShared/GameClasses/System/Timer/CgsTime.h"                        // CgsSystem::Time (PlayerResultsData::mFinishTime)
#include "GameShared/GameClasses/System/Timer/CgsTimerStatusInterface.h"        // CgsSystem::TimerStatusInterface (PreSim +8)
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                             // CgsGui::GuiEventQueueSmall (OutputBuffer +174576)
#include "GameSource/Network/SharedIO/BrnNetworkSharedIO.h"                     // PlayerName, NetworkPlayerID, EActiveRaceCarIndex, DirtyTrickEvent
#include "GameSource/Network/SharedIO/BrnNetworkModuleInGamePlayerStatusInterface.h" // InGamePlayerStatusInterface (OutputBuffer +180864)
#include "GameSource/Network/SharedIO/BrnNetworkModuleGameStateIOInterfaces.h"  // GameStateToNetworkInterface / NetworkToGameStateInterface
#include "GameSource/Network/SharedIO/BrnNetworkModuleOnlineLobbyPlayerStatusInterface.h" // OnlineLobbyPlayerStatusInterface (OutputBuffer +183408)
#include "GameSource/Network/SharedIO/BrnNetworkModuleStatsIOInterface.h"       // StatsInputInterface / StatsOutputInterface
#include "GameSource/Network/SharedIO/BrnNetworkToGuiIOInterfaces.h"            // NetworkToGuiInterface (OutputBuffer +172376)
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.h"        // BrnPhysics::Vehicle::ImpactEvent / PhysicalTrafficState
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleOutputInterface.h"      // VehicleOutputInterface (PostSim +16)
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleInputInterface.h"       // VehicleInputInterface (OutputBuffer +5312)
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleDriverInputInterface.h" // VehicleDriverInputInterface (OutputBuffer +16)
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarEntityModuleOutputInterface.h" // RCEntityActiveRaceCarOutputInterface (PostSim +27680)
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnPlayerVehicleControls.h"            // BrnWorld::PlayerVehicleControls (PostSim +40240)
#include "GameSource/World/CrashModule/SharedIO/BrnCrashModuleNetworkIOInterfaces.h"  // CrashIO::NetworkInputInterface / NetworkOutputInterface
#include "GameSource/World/EntityModules/TrafficEntityModule/SharedIO/BrnTrafficNetworkInterfaces.h" // TrafficNetworkInputInterface / TrafficNetworkOutputInterface
#include "GameSource/GameState/TakedownManager/BrnTakedownManagerTypes.h"       // BrnGameState::TakedownEvent (40-byte queue element)
#include "GameSource/Resource/SharedIO/BrnGameDataRequestQueue.h"               // BrnResource::GameDataIO::RequestInterface<N>
#include "GameSource/GameState/StreetData/BrnChallengeHighScoreEntry.h"     // BrnStreetData::ChallengeHighScoreEntry (NetworkOutRecvRoadRulesPBEvent)

// Defined in BrnGameStateModuleIO.h, which includes this header (see the banner).
namespace BrnGameState { namespace GameStateModuleIO { class GameEventQueue; } }

namespace BrnNetwork
{
namespace BrnNetworkModuleIO
{
    // The lifecycle event a freeburn challenge broadcasts over the network (original home
    // BrnNetworkSharedIO.h). COUNT is the count, not a valid event. Used by
    // BrnNetwork::FreeburnChallengeMessage and the ChallengeManager.
    //
    // The console carries one enumerator more than the reference list, ahead of ENDED, so ENDED
    // is 5, RESULTS_FINISHED 6 and COUNT 7. Attested by the producers and consumers:
    // ChallengeManager::EndChallenge stores 5, UpdateResults stores 6, the arbitration reset
    // of every action slot stores 4; BridgeGameStateToNetwork tests 5 for the "challenge
    // finished" telemetry and filters 6 out of the network event; FreeburnChallengeMessage's
    // constructor seeds its "no event" value as 7.
    // FLAG: the name of value 4 is unrecovered; it is named after its one producer.
    enum EChallengeEventType
    {
        E_CHALLENGE_EVENT_SELECTED         = 0,
        E_CHALLENGE_EVENT_TRIGGERED        = 1,
        E_CHALLENGE_EVENT_ACTION_SUCCESS   = 2,
        E_CHALLENGE_EVENT_RESET            = 3,
        E_CHALLENGE_EVENT_RESET_ACTIONS    = 4,   // FLAG name
        E_CHALLENGE_EVENT_ENDED            = 5,
        E_CHALLENGE_EVENT_RESULTS_FINISHED = 6,
        E_CHALLENGE_EVENT_COUNT            = 7,
    };

    // ========================================================================
    // NetworkEventQueue (original home BrnNetworkEvent.h, beside NetworkEvent<N>)
    //   The module's variable-size network event queue. It adds nothing to its base.
    //   Embedded at PostSimulationInputBuffer +73284, OutputBuffer +184080 and
    //   BrnNetworkModule +852392 (console offsets).
    // ========================================================================
    const s32 KI_NETWORK_EVENT_QUEUE_SIZE = 14000;

    struct NetworkEventQueue : public CgsModule::VariableEventQueue<KI_NETWORK_EVENT_QUEUE_SIZE, 16>
    {
    };

    // ========================================================================
    // PlayerResultsData (original home SharedIO/BrnNetworkModulePlayerResultsInterface.h)
    // ========================================================================
    // Per-player end-of-round result record. The member set is the reference one; the field
    // ORDER and offsets are the console's, recovered from ScoringSystem::UpdateNetworkPlayerResults,
    // which walks this array at a 28-byte stride:
    //   +0x00 (8) Time mFinishTime    : miSeconds + mfFraction
    //   +0x08 (4) meActiveRaceCarIndex: the record's car slot (checked != -1)
    //   +0x0C (4) meEliminatorIndex   : eliminator car slot
    //   +0x10 (4) mfDistanceToFinish  : captured finish distance
    //   +0x14 (4) miEliminations      : eliminations
    //   +0x18 (1) mbValid             : record-present guard (gates the whole write)
    //   +0x19 (1) mbTimedOut          : timed-out flag
    //   +0x1A (1) mbEliminated        : eliminated flag
    //   +0x1B (1) padding to the 28-byte stride
    // The reference member list names only one bool (mbTimedOut); the console reads three
    // distinct bytes, so mbValid and mbEliminated are named from their use.
    struct PlayerResultsData
    {
        CgsSystem::Time     mFinishTime;            // +0x00 (8)
        EActiveRaceCarIndex meActiveRaceCarIndex;   // +0x08
        EActiveRaceCarIndex meEliminatorIndex;      // +0x0C
        f32                 mfDistanceToFinish;     // +0x10
        s32                 miEliminations;         // +0x14
        bool                mbValid;                // +0x18 (named from use)
        bool                mbTimedOut;             // +0x19
        bool                mbEliminated;           // +0x1A (named from use)
        u8                  maPad1B[1];             // +0x1B pad to the 28-byte stride

        void Clear();   // no standalone console body (inlined into PlayerResultsInterface::Clear)
    };
    static_assert(sizeof(PlayerResultsData) == 28, "PlayerResultsData on-disk stride (0x1C)");

    // ========================================================================
    // PlayerResultsInterface (original home SharedIO/BrnNetworkModulePlayerResultsInterface.h)
    //   maPlayerResultsData[8]; OutputBuffer +183856, PreWorldInputBuffer +0x36B8.
    // ========================================================================
    struct PlayerResultsInterface
    {
        // Member-wise copy of the 8 records (body in BrnNetworkModuleIO.cpp).
        PlayerResultsInterface& operator=(const PlayerResultsInterface& lOther);

        // Const element accessor, header-inline (the consumer indexes the array directly).
        const PlayerResultsData* GetPlayerResultsData(s32 liIndex) const
        {
            CGS_ASSERT(liIndex >= 0, "liIndex >= 0");
            return &maPlayerResultsData[liIndex];
        }
        // Mutable element accessor, header-inline (the writer, BrnNetworkManager::
        // OutputPlayerResultsInfo, carries the one assert and the 28-byte stride itself).
        PlayerResultsData* GetPlayerResultsDataForWriting(s32 liIndex)
        {
            CGS_ASSERT(liIndex >= 0, "liIndex >= 0");
            return &maPlayerResultsData[liIndex];
        }

        // Resets all 8 records (body in BrnNetworkModuleIO.cpp).
        void Clear();

    private:
        PlayerResultsData maPlayerResultsData[8];   // 8 records, 28-byte stride
    };
    static_assert(sizeof(PlayerResultsInterface) == 8 * 28, "PlayerResultsInterface layout (8 x 0x1C)");

    // ========================================================================
    // PreSimulationInputBuffer : IOBuffer
    //   console layout (Construct: +0 status = 1, +1 = 0, +2 = 1, +4 = -1, then
    //   TimerStatusInterface::Clear on +8):
    //     +1  bool mbSysMenuOnScreen
    //     +2  bool mbPadIdle
    //     +4  s32  miControllerPort
    //     +8  CgsSystem::TimerStatusInterface mTimerInterface (48 bytes)
    //   All four are pointer-free, so the host offsets equal the console ones (asserted).
    // ========================================================================
    struct PreSimulationInputBuffer : public CgsModule::IOBuffer
    {
        void Construct();
        void Destruct();

        const CgsSystem::TimerStatusInterface* GetTimerStatusInterface() const;
        void SetTimerStatusInterface(const CgsSystem::TimerStatusInterface* lpTimerStatusInterface);
        void SetPadIdle(bool lbPadIdle);
        bool IsPadIdle() const;
        s32  GetControllerPort() const;
        void SetControllerPort(s32 liControllerPort);
        void SetSysMenuOnScreen(bool lbSysMenuOnScreen);
        bool IsSysMenuOnScreen() const;

    private:
        bool                            mbSysMenuOnScreen;   // +1
        bool                            mbPadIdle;           // +2
        s32                             miControllerPort;    // +4
        CgsSystem::TimerStatusInterface mTimerInterface;     // +8

        static void _AssertLayout()
        {
            static_assert(offsetof(PreSimulationInputBuffer, mbSysMenuOnScreen) == 1, "PreSim mbSysMenuOnScreen @ +1");
            static_assert(offsetof(PreSimulationInputBuffer, mbPadIdle)         == 2, "PreSim mbPadIdle @ +2");
            static_assert(offsetof(PreSimulationInputBuffer, miControllerPort)  == 4, "PreSim miControllerPort @ +4");
            static_assert(offsetof(PreSimulationInputBuffer, mTimerInterface)   == 8, "PreSim mTimerInterface @ +8");
            static_assert(sizeof(CgsSystem::TimerStatusInterface) == 48, "TimerStatusInterface is 48 bytes");
        }
    };

    // ========================================================================
    // PostSimulationInputBuffer : IOBuffer   (console sizeof 87300)
    //   console layout (Construct / Destruct):
    //     +16     VehicleOutputInterface               mVehicleOutputInterface
    //     +27680  RCEntityActiveRaceCarOutputInterface mActiveRaceCarInterface         (10480)
    //     +38160  TrafficNetworkOutputInterface        mTrafficNetworkOutputInterface  (144)
    //     +38304  CrashIO::NetworkOutputInterface      mCrashNetworkOutputInterface    (1936)
    //     +40240  BrnWorld::PlayerVehicleControls      mPlayerVehicleControls          (60)
    //     +40300  GameStateModuleIO::GameActionQueue   mGameActionQueue                (VariableEventQueue<13312,16>)
    //     +53628  GameStateToNetworkInterface          mGameStateToNetworkInterface    (540)
    //     +54168  TakedownEventQueue                   mTakedownEventInputQueue        (EventQueue<TakedownEvent,8>, 336)
    //     +54504  GuiEventQueue                        mGuiEventQueue                  (VariableEventQueue<18432,16>)
    //     +72952  StatsInputInterface                  mStatsInputInterface            (332)
    //     +73284  NetworkEventQueue                    mNetworkEventQueue
    // ========================================================================
    struct PostSimulationInputBuffer : public CgsModule::IOBuffer
    {
        typedef CgsModule::EventQueue<BrnGameState::TakedownEvent, 8> TakedownEventQueue;
        // The reference spells this InputBuffer::GuiEventQueue; the committed GUI IO header
        // (CgsGuiModuleIO.h) resolves that name to this instantiation, and Construct / Destruct
        // call the VariableEventQueue<18432,16> members on it by name.
        typedef CgsModule::VariableEventQueue<18432, 16> GuiEventQueue;

        void Construct();
        void Destruct();

        TakedownEventQueue*                                    GetTakedownEventInputQueue();
        const TakedownEventQueue*                              GetTakedownEventInputQueue() const;
        GuiEventQueue*                                         GetGuiEventQueue();
        const GuiEventQueue*                                   GetGuiEventQueue() const;
        BrnGameState::GameStateModuleIO::GameActionQueue*       GetGameActionQueue();
        const BrnGameState::GameStateModuleIO::GameActionQueue* GetGameActionQueue() const;
        const BrnPhysics::Vehicle::VehicleOutputInterface*      GetVehicleOutputInterface() const;
        const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* GetActiveRaceCarInterface() const;
        const BrnWorld::CrashIO::NetworkOutputInterface*        GetCrashNetworkOutputInterface() const;
        const BrnTraffic::BrnTrafficIO::TrafficNetworkOutputInterface* GetTrafficNetworkOutputInterface() const;
        const GameStateToNetworkInterface*                      GetGameStateToNetworkInterface() const;
        BrnWorld::PlayerVehicleControls*                        GetPlayerVehicleControls();
        const BrnWorld::PlayerVehicleControls*                  GetPlayerVehicleControls() const;
        const StatsInputInterface*                              GetStatsInputInterface() const;
        StatsInputInterface*                                    GetStatsInputInterface();

        void AppendVehicleOutputInterface(const BrnPhysics::Vehicle::VehicleOutputInterface* lpVehicleOutputInterface);
        void SetActiveRaceCarInterface(const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpInterface);
        void SetTrafficOutputInterface(const BrnTraffic::BrnTrafficIO::TrafficNetworkOutputInterface* lpInterface);
        void SetCrashOutputInterface(const BrnWorld::CrashIO::NetworkOutputInterface* lpInterface);
        void AppendTakedownQueue(const TakedownEventQueue* lpQueue);
        void AppendGameStateToNetworkInterface(const GameStateToNetworkInterface* lpInterface);

        NetworkEventQueue*       GetNetworkEventQueue();
        const NetworkEventQueue* GetNetworkEventQueue() const;

    private:
        BrnPhysics::Vehicle::VehicleOutputInterface                         mVehicleOutputInterface;        // console +16
        BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface mActiveRaceCarInterface;      // console +27680
        BrnTraffic::BrnTrafficIO::TrafficNetworkOutputInterface             mTrafficNetworkOutputInterface; // console +38160
        BrnWorld::CrashIO::NetworkOutputInterface                           mCrashNetworkOutputInterface;   // console +38304
        BrnWorld::PlayerVehicleControls                                     mPlayerVehicleControls;         // console +40240
        BrnGameState::GameStateModuleIO::GameActionQueue                    mGameActionQueue;               // console +40300
        GameStateToNetworkInterface                                         mGameStateToNetworkInterface;   // console +53628
        TakedownEventQueue                                                  mTakedownEventInputQueue;       // console +54168
        GuiEventQueue                                                       mGuiEventQueue;                 // console +54504
        StatsInputInterface                                                 mStatsInputInterface;           // console +72952
        NetworkEventQueue                                                   mNetworkEventQueue;             // console +73284
    };

    // ========================================================================
    // OutputBuffer : IOBuffer   (console sizeof 198096)
    //   console layout (Construct / Destruct):
    //     +4       f32  mfStandingsReceivedCountDownTime   (Construct stores -1.0f)
    //     +16      VehicleDriverInputInterface     mVehicleDriverInputInterface   (5296)
    //     +5312    VehicleInputInterface           mVehicleInputInterface         (142176)
    //     +147488  CrashIO::NetworkInputInterface  mCrashNetworkInputInterface    (15504)
    //     +162992  TrafficNetworkInputInterface    mTrafficNetworkInputInterface  (112)
    //     +163104  NetworkToGameStateInterface     mNetworkToGameStateInterface   (9272)
    //     +172376  NetworkToGuiInterface           mNetworkToGuiInterface         (76)
    //     +172452  StatsOutputInterface            mStatsOutputInterface          (2124)
    //     +174576  CgsGui::GuiEventQueueSmall      mGuiEventQueue                 (VariableEventQueue<4096,16>)
    //     +178688  EActiveRaceCarIndex             mePlayerActiveRaceCarIndex     (Construct stores -1)
    //     +178692  GameStateModuleIO::GameEventQueue mGameEventQueue              (VariableEventQueue<1536,16>)
    //     +180248  TakedownEventQueue              mTakedownEventOutputQueue      (336)
    //     +180584  bool mbIsPlaying / +180585 mbIsConnected / +180586 mbIsInInvite / +180587 mbInvitesOpen
    //     +180588  RequestInterface<256>           mGameDataRequestInterface
    //     +180864  InGamePlayerStatusInterface     mInGamePlayerStatusInterface   (2544)
    //     +183408  OnlineLobbyPlayerStatusInterface mOnlineLobbyPlayerStatusInterface (448)
    //     +183856  PlayerResultsInterface          mPlayerResultsInterface        (224)
    //     +184080  NetworkEventQueue               mNetworkEventQueue
    // ========================================================================
    struct OutputBuffer : public CgsModule::IOBuffer
    {
        typedef CgsModule::EventQueue<BrnGameState::TakedownEvent, 8> TakedownEventQueue;
        typedef BrnResource::GameDataIO::RequestInterface<256>        NetworkGameDataRequestInterface;

        void Construct();
        void Destruct();

        EActiveRaceCarIndex GetPlayerActiveRaceCarIndex() const;
        void                SetPlayerActiveRaceCarIndex(EActiveRaceCarIndex leActiveRaceCarIndex);

        BrnGameState::GameStateModuleIO::GameEventQueue*       GetGameEventQueue();
        const BrnGameState::GameStateModuleIO::GameEventQueue* GetGameEventQueue() const;
        CgsGui::GuiEventQueueSmall*                            GetGuiEventQueue();
        const CgsGui::GuiEventQueueSmall*                      GetGuiEventQueue() const;
        TakedownEventQueue*                                    GetTakedownEventOutputQueue();
        const TakedownEventQueue*                              GetTakedownEventOutputQueue() const;

        void SetIsPlaying(bool lbIsPlaying);
        bool IsPlaying() const;
        void SetConnected(bool lbIsConnected);
        bool IsConnected() const;
        void SetIsInInvite(bool lbIsInInvite);
        bool IsInInvite() const;

        const BrnPhysics::Vehicle::VehicleInputInterface*       GetVehicleInputInterface() const;
        const BrnPhysics::Vehicle::VehicleDriverInputInterface* GetVehicleDriverInputInterface() const;
        BrnPhysics::Vehicle::VehicleInputInterface*             GetVehicleInputInterface();
        BrnPhysics::Vehicle::VehicleDriverInputInterface*       GetVehicleDriverInputInterface();
        const BrnWorld::CrashIO::NetworkInputInterface*         GetCrashNetworkInputInterface() const;
        BrnWorld::CrashIO::NetworkInputInterface*               GetCrashNetworkInputInterface();
        const BrnTraffic::BrnTrafficIO::TrafficNetworkInputInterface* GetTrafficNetworkInputInterface() const;
        BrnTraffic::BrnTrafficIO::TrafficNetworkInputInterface*       GetTrafficNetworkInputInterface();
        NetworkToGameStateInterface*                            GetNetworkToGameStateInterface();
        const NetworkToGameStateInterface*                      GetNetworkToGameStateInterface() const;

        void SetStandingsReceivedCountDownTime(f32 lfTime);

        const NetworkGameDataRequestInterface*  GetGameDataRequestInterface() const;
        NetworkGameDataRequestInterface*        GetGameDataRequestInterface();
        const StatsOutputInterface*             GetStatsOutputInterface() const;
        StatsOutputInterface*                   GetStatsOutputInterface();
        const InGamePlayerStatusInterface*      GetInGamePlayerStatusInterface() const;
        InGamePlayerStatusInterface*            GetInGamePlayerStatusInterface();
        const NetworkToGuiInterface*            GetNetworkToGuiInterface() const;
        NetworkToGuiInterface*                  GetNetworkToGuiInterface();
        const PlayerResultsInterface*           GetPlayerResultsInterface() const;
        PlayerResultsInterface*                 GetPlayerResultsInterface();
        OnlineLobbyPlayerStatusInterface*       GetOnlineLobbyPlayerStatusInterface();
        const OnlineLobbyPlayerStatusInterface* GetOnlineLobbyPlayerStatusInterface() const;
        NetworkEventQueue*                      GetNetworkEventQueue();
        const NetworkEventQueue*                GetNetworkEventQueue() const;

        // Inline on the console: BridgeNetworkToGameState reads the byte at +0x2C16B directly,
        // with no lock assert.
        bool AreInvitesOpen() const { return mbInvitesOpen; }
        // Also inline: ProcessBeforeSimulation stores the byte straight into the buffer.
        void SetInvitesOpen(bool lbInvitesOpen) { mbInvitesOpen = lbInvitesOpen; }

    private:
        f32                                                 mfStandingsReceivedCountDownTime;   // console +4
        BrnPhysics::Vehicle::VehicleDriverInputInterface    mVehicleDriverInputInterface;       // console +16
        BrnPhysics::Vehicle::VehicleInputInterface          mVehicleInputInterface;             // console +5312
        BrnWorld::CrashIO::NetworkInputInterface            mCrashNetworkInputInterface;        // console +147488
        BrnTraffic::BrnTrafficIO::TrafficNetworkInputInterface mTrafficNetworkInputInterface;   // console +162992
        NetworkToGameStateInterface                         mNetworkToGameStateInterface;       // console +163104
        NetworkToGuiInterface                               mNetworkToGuiInterface;             // console +172376
        StatsOutputInterface                                mStatsOutputInterface;              // console +172452
        CgsGui::GuiEventQueueSmall                          mGuiEventQueue;                     // console +174576
        EActiveRaceCarIndex                                 mePlayerActiveRaceCarIndex;         // console +178688
        // GameStateModuleIO::GameEventQueue (include cycle, see the banner): storage of the
        // queue's exact size; the accessors return it as the real type.
        alignas(4) u8 maGameEventQueueStorage[sizeof(CgsModule::VariableEventQueue<1536, 16>)]; // console +178692
        TakedownEventQueue                                  mTakedownEventOutputQueue;          // console +180248
        bool                                                mbIsPlaying;                        // console +180584
        bool                                                mbIsConnected;                      // console +180585
        bool                                                mbIsInInvite;                       // console +180586
        bool                                                mbInvitesOpen;                      // console +180587
        NetworkGameDataRequestInterface                     mGameDataRequestInterface;          // console +180588
        InGamePlayerStatusInterface                         mInGamePlayerStatusInterface;       // console +180864
        OnlineLobbyPlayerStatusInterface                    mOnlineLobbyPlayerStatusInterface;  // console +183408
        PlayerResultsInterface                              mPlayerResultsInterface;            // console +183856
        NetworkEventQueue                                   mNetworkEventQueue;                 // console +184080
    };

    // ========================================================================
    // NetworkOutRecvRoadRulesPBEvent -- network-out tag 35, 72 bytes: a road-rules personal
    // best arrived. NetworkRoadRulesManager queues it straight onto the network event queue,
    // or buffers it in its FifoQueue<...,14> (the element stride is the same 72 bytes).
    // Pointer-free, so the host layout is the console layout.
    // ========================================================================
    struct NetworkOutRecvRoadRulesPBEvent : public NetworkEvent<35>
    {
        BrnStreetData::ChallengeHighScoreEntry mPersonalBestScore;           // +0x00 (56)
        NetworkPlayerID                        mPersonalBestPlayerID;        // +0x38
        Road::ChallengeIndex                   mPersonalBestChallengeIndex;  // +0x3C
        bool                                   mbWasPBByFriend;              // +0x40

        static void _AssertLayout()
        {
            static_assert(offsetof(NetworkOutRecvRoadRulesPBEvent, mPersonalBestPlayerID) == 0x38, "tag 35 +0x38");
            static_assert(offsetof(NetworkOutRecvRoadRulesPBEvent, mPersonalBestChallengeIndex) == 0x3C, "tag 35 +0x3C");
            static_assert(offsetof(NetworkOutRecvRoadRulesPBEvent, mbWasPBByFriend) == 0x40, "tag 35 +0x40");
            static_assert(sizeof(NetworkOutRecvRoadRulesPBEvent) == 72, "tag 35 is queued as 72 bytes");
        }
    };
}
}
