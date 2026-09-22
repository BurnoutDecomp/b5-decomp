// ============================================================================
// b5-decomp/src/GameSource/Network/BrnNetworkModuleIO.cpp
// ============================================================================
// Bodies for the BrnNetwork::BrnNetworkModuleIO IO buffers' lock-guarded accessors and
// producer setters, plus the InGamePlayerStatusInterface / PlayerResultsInterface helpers.
//
// Every buffer accessor is the same console shape: test the lock bit (read = status bit 0x10,
// write = status bit 0x08), fire the "Not locked for reading\n" / "Not locked for writing\n"
// assert when it is clear, then return &member. The assert's streamed file/line is reduced to
// CGS_ASSERT on the base predicate, as everywhere in the tree.
//
// The three buffers' Construct / Destruct run each member's own Construct / Clear in the
// console's emission order, which is not member order and constructs a few queues twice
// (kept as shipped).
//
// Not bodied here (declared in the header, no body yet): PostSimulationInputBuffer::
// AppendVehicleOutputInterface (its whole body is the inlined VehicleOutputInterface::Append,
// which that interface does not declare yet), and the declarations with no out-of-line console
// body (header inlines on the console).
//
// InGamePlayerStatusData::operator= / Clear live in the SharedIO .cpp beside the struct.

#include "GameSource/Network/BrnNetworkModuleIO.h"
#include "GameSource/Network/SharedIO/BrnNetworkModuleInGamePlayerStatusInterface.h"
#include "GameSource/GameState/BrnGameStateModuleIO.h"   // BrnGameState::GameStateModuleIO::GameEventQueue (OutputBuffer storage)
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include <cstring>   // std::memcpy (SetTimerStatusInterface, SetActiveRaceCarInterface) / std::strncpy (SetGameName)

namespace BrnNetwork
{
namespace BrnNetworkModuleIO
{
    // ========================================================================
    // PreSimulationInputBuffer
    // ========================================================================

    // The status byte to "constructed", the three scalars to their idle values (menu off, pad
    // idle, no controller port), then the timer-status pair cleared.
    void PreSimulationInputBuffer::Construct()
    {
        IOBuffer::Construct();
        mbSysMenuOnScreen = false;
        mbPadIdle         = true;
        miControllerPort  = -1;
        mTimerInterface.Clear();
    }

    // The same reset as Construct, then IOBuffer::Destruct.
    void PreSimulationInputBuffer::Destruct()
    {
        mbSysMenuOnScreen = false;
        mbPadIdle         = true;
        miControllerPort  = -1;
        mTimerInterface.Clear();
        IOBuffer::Destruct();
    }

    const CgsSystem::TimerStatusInterface* PreSimulationInputBuffer::GetTimerStatusInterface() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return &mTimerInterface;
    }

    // The console body is the flat field-for-field copy of the two 24-byte TimerStatus blocks
    // (the inlined TimerStatusInterface assignment). CgsSystem::TimerStatusInterface::operator=
    // has no body in the tree yet and the type is pointer-free (48 bytes on both targets), so
    // the copy is the same bytes as a block copy.
    void PreSimulationInputBuffer::SetTimerStatusInterface(const CgsSystem::TimerStatusInterface* lpTimerStatusInterface)
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        std::memcpy(&mTimerInterface, lpTimerStatusInterface, sizeof(mTimerInterface));
    }

    void PreSimulationInputBuffer::SetPadIdle(bool lbPadIdle)
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        mbPadIdle = lbPadIdle;
    }

    bool PreSimulationInputBuffer::IsPadIdle() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return mbPadIdle;
    }

    s32 PreSimulationInputBuffer::GetControllerPort() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return miControllerPort;
    }

    void PreSimulationInputBuffer::SetControllerPort(s32 liControllerPort)
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        miControllerPort = liControllerPort;
    }

    void PreSimulationInputBuffer::SetSysMenuOnScreen(bool lbSysMenuOnScreen)
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        mbSysMenuOnScreen = lbSysMenuOnScreen;
    }

    bool PreSimulationInputBuffer::IsSysMenuOnScreen() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return mbSysMenuOnScreen;
    }

    // ========================================================================
    // PostSimulationInputBuffer -- Construct / Destruct
    // ========================================================================

    // The two variable queues are constructed and prepared first, then every interface in
    // member order, then the GUI and game-action queues are constructed a second time and the
    // network event queue last. The crash and stats interfaces each follow their Construct with
    // a Clear (their queue length reset); the game-state interface's Construct ends in its own.
    void PostSimulationInputBuffer::Construct()
    {
        IOBuffer::Construct();

        mGuiEventQueue.Construct();
        mGuiEventQueue.Prepare();
        mGameActionQueue.Construct();
        mGameActionQueue.Prepare();

        mVehicleOutputInterface.Construct();
        mActiveRaceCarInterface.Clear();
        mTrafficNetworkOutputInterface.Construct();
        mCrashNetworkOutputInterface.Construct();
        mCrashNetworkOutputInterface.Clear();
        mGameStateToNetworkInterface.Construct();
        mStatsInputInterface.Construct();
        mStatsInputInterface.Clear();
        mTakedownEventInputQueue.Construct();

        mGuiEventQueue.Construct();
        mGameActionQueue.Construct();
        mNetworkEventQueue.Construct();
    }

    // Only the game-action queue is released; the network event queue is destructed without a
    // release, and the base status byte goes last.
    void PostSimulationInputBuffer::Destruct()
    {
        mGameActionQueue.Release();
        mGameActionQueue.Destruct();
        mNetworkEventQueue.Destruct();
        IOBuffer::Destruct();
    }

    // ========================================================================
    // PostSimulationInputBuffer -- accessors
    // ========================================================================

    const PostSimulationInputBuffer::TakedownEventQueue* PostSimulationInputBuffer::GetTakedownEventInputQueue() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return &mTakedownEventInputQueue;
    }

    PostSimulationInputBuffer::GuiEventQueue* PostSimulationInputBuffer::GetGuiEventQueue()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        return &mGuiEventQueue;
    }

    const PostSimulationInputBuffer::GuiEventQueue* PostSimulationInputBuffer::GetGuiEventQueue() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return &mGuiEventQueue;
    }

    BrnGameState::GameStateModuleIO::GameActionQueue* PostSimulationInputBuffer::GetGameActionQueue()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        return &mGameActionQueue;
    }

    const BrnGameState::GameStateModuleIO::GameActionQueue* PostSimulationInputBuffer::GetGameActionQueue() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return &mGameActionQueue;
    }

    const BrnPhysics::Vehicle::VehicleOutputInterface* PostSimulationInputBuffer::GetVehicleOutputInterface() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return &mVehicleOutputInterface;
    }

    const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface*
    PostSimulationInputBuffer::GetActiveRaceCarInterface() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return &mActiveRaceCarInterface;
    }

    const BrnWorld::CrashIO::NetworkOutputInterface* PostSimulationInputBuffer::GetCrashNetworkOutputInterface() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return &mCrashNetworkOutputInterface;
    }

    const BrnTraffic::BrnTrafficIO::TrafficNetworkOutputInterface*
    PostSimulationInputBuffer::GetTrafficNetworkOutputInterface() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return &mTrafficNetworkOutputInterface;
    }

    const GameStateToNetworkInterface* PostSimulationInputBuffer::GetGameStateToNetworkInterface() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return &mGameStateToNetworkInterface;
    }

    BrnWorld::PlayerVehicleControls* PostSimulationInputBuffer::GetPlayerVehicleControls()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        return &mPlayerVehicleControls;
    }

    const BrnWorld::PlayerVehicleControls* PostSimulationInputBuffer::GetPlayerVehicleControls() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return &mPlayerVehicleControls;
    }

    const StatsInputInterface* PostSimulationInputBuffer::GetStatsInputInterface() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return &mStatsInputInterface;
    }

    NetworkEventQueue* PostSimulationInputBuffer::GetNetworkEventQueue()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        return &mNetworkEventQueue;
    }

    const NetworkEventQueue* PostSimulationInputBuffer::GetNetworkEventQueue() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return &mNetworkEventQueue;
    }

    // ========================================================================
    // PostSimulationInputBuffer -- producer setters (BrnGameModule's world/game-state bridges)
    // ========================================================================

    // Asserts the source (direct assert, no stream) and the write lock, then XMemCpy's the
    // whole 10480-byte interface. The console assignment is a plain block copy; the committed
    // interface declares a user operator= with no body in the tree, so the block copy is kept.
    void PostSimulationInputBuffer::SetActiveRaceCarInterface(
            const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpInterface)
    {
        CGS_ASSERT(lpInterface != nullptr, "lpInterface");
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        std::memcpy(static_cast<void*>(&mActiveRaceCarInterface), lpInterface, sizeof(mActiveRaceCarInterface));
    }

    // The console runs TrafficNetworkOutputInterface::operator= BEFORE the lock assert; the
    // order is kept.
    void PostSimulationInputBuffer::SetTrafficOutputInterface(
            const BrnTraffic::BrnTrafficIO::TrafficNetworkOutputInterface* lpInterface)
    {
        mTrafficNetworkOutputInterface = *lpInterface;
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
    }

    // Write lock, then the interface's replace-merge: the queue length reset and the
    // EventQueue<CrashingTrafficUpdateEvent,24>::Append of the source queue
    // (NetworkOutputInterface::Append is exactly that pair).
    void PostSimulationInputBuffer::SetCrashOutputInterface(const BrnWorld::CrashIO::NetworkOutputInterface* lpInterface)
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        mCrashNetworkOutputInterface.Append(lpInterface);
    }

    // Write lock, then BaseEventQueue<TakedownEvent>::Append (no length reset).
    void PostSimulationInputBuffer::AppendTakedownQueue(const TakedownEventQueue* lpQueue)
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        mTakedownEventInputQueue.Append(*lpQueue);
    }

    // Write lock, then GameStateToNetworkInterface::Append.
    void PostSimulationInputBuffer::AppendGameStateToNetworkInterface(const GameStateToNetworkInterface* lpInterface)
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        mGameStateToNetworkInterface.Append(lpInterface);
    }

    // ========================================================================
    // OutputBuffer -- Construct / Destruct
    // ========================================================================

    // The GUI, game-event and takedown queues are constructed and prepared first (the
    // takedown queue's prepare is its length reset), the four state bytes cleared, then every
    // interface; the GUI and takedown queues are constructed a second time on the way. The
    // online-lobby interface is not touched. The countdown and the player's race-car slot are
    // stored last.
    void OutputBuffer::Construct()
    {
        // The game-event queue lives in size-exact storage (see the header banner).
        static_assert(sizeof(BrnGameState::GameStateModuleIO::GameEventQueue) == sizeof(maGameEventQueueStorage),
                      "GameEventQueue must fill its storage exactly");
        BrnGameState::GameStateModuleIO::GameEventQueue& lGameEventQueue =
            *reinterpret_cast<BrnGameState::GameStateModuleIO::GameEventQueue*>(&maGameEventQueueStorage[0]);

        IOBuffer::Construct();

        mGuiEventQueue.Construct();
        lGameEventQueue.Construct();
        mTakedownEventOutputQueue.Construct();
        // GuiEventQueueBase adds nothing to its base; the console calls the base Prepare.
        mGuiEventQueue.CgsModule::VariableEventQueue<4096, 16>::Prepare();
        lGameEventQueue.Prepare();
        mTakedownEventOutputQueue.Clear();

        mbIsPlaying    = false;
        mbIsConnected  = false;
        mbIsInInvite   = false;
        mbInvitesOpen  = false;

        mVehicleDriverInputInterface.Construct();
        mVehicleInputInterface.Construct();
        mCrashNetworkInputInterface.Construct();
        mTrafficNetworkInputInterface.Construct();
        mGuiEventQueue.Construct();
        mTakedownEventOutputQueue.Construct();
        mNetworkToGameStateInterface.Construct();
        mGameDataRequestInterface.Construct();
        mStatsOutputInterface.Construct();
        mStatsOutputInterface.Clear();
        mNetworkToGuiInterface.Construct();
        mNetworkToGuiInterface.Clear();
        mPlayerResultsInterface.Clear();
        mNetworkEventQueue.Construct();
        mInGamePlayerStatusInterface.Clear();

        mfStandingsReceivedCountDownTime = -1.0f;
        mePlayerActiveRaceCarIndex       = ::E_ACTIVE_RACE_CAR_INDEX_INVALID;
    }

    // The base status byte first, then the network event queue.
    void OutputBuffer::Destruct()
    {
        IOBuffer::Destruct();
        mNetworkEventQueue.Destruct();
    }

    // ========================================================================
    // OutputBuffer -- accessors
    // ========================================================================

    EActiveRaceCarIndex OutputBuffer::GetPlayerActiveRaceCarIndex() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return mePlayerActiveRaceCarIndex;
    }

    BrnGameState::GameStateModuleIO::GameEventQueue* OutputBuffer::GetGameEventQueue()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        return reinterpret_cast<BrnGameState::GameStateModuleIO::GameEventQueue*>(&maGameEventQueueStorage[0]);
    }

    const BrnGameState::GameStateModuleIO::GameEventQueue* OutputBuffer::GetGameEventQueue() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return reinterpret_cast<const BrnGameState::GameStateModuleIO::GameEventQueue*>(&maGameEventQueueStorage[0]);
    }

    CgsGui::GuiEventQueueSmall* OutputBuffer::GetGuiEventQueue()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        return &mGuiEventQueue;
    }

    const CgsGui::GuiEventQueueSmall* OutputBuffer::GetGuiEventQueue() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return &mGuiEventQueue;
    }

    OutputBuffer::TakedownEventQueue* OutputBuffer::GetTakedownEventOutputQueue()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        return &mTakedownEventOutputQueue;
    }

    const OutputBuffer::TakedownEventQueue* OutputBuffer::GetTakedownEventOutputQueue() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return &mTakedownEventOutputQueue;
    }

    void OutputBuffer::SetIsPlaying(bool lbIsPlaying)
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        mbIsPlaying = lbIsPlaying;
    }

    bool OutputBuffer::IsPlaying() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return mbIsPlaying;
    }

    void OutputBuffer::SetConnected(bool lbIsConnected)
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        mbIsConnected = lbIsConnected;
    }

    bool OutputBuffer::IsConnected() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return mbIsConnected;
    }

    void OutputBuffer::SetIsInInvite(bool lbIsInInvite)
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        mbIsInInvite = lbIsInInvite;
    }

    bool OutputBuffer::IsInInvite() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return mbIsInInvite;
    }

    const BrnPhysics::Vehicle::VehicleInputInterface* OutputBuffer::GetVehicleInputInterface() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return &mVehicleInputInterface;
    }

    const BrnPhysics::Vehicle::VehicleDriverInputInterface* OutputBuffer::GetVehicleDriverInputInterface() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return &mVehicleDriverInputInterface;
    }

    BrnPhysics::Vehicle::VehicleInputInterface* OutputBuffer::GetVehicleInputInterface()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        return &mVehicleInputInterface;
    }

    BrnPhysics::Vehicle::VehicleDriverInputInterface* OutputBuffer::GetVehicleDriverInputInterface()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        return &mVehicleDriverInputInterface;
    }

    const BrnWorld::CrashIO::NetworkInputInterface* OutputBuffer::GetCrashNetworkInputInterface() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return &mCrashNetworkInputInterface;
    }

    BrnWorld::CrashIO::NetworkInputInterface* OutputBuffer::GetCrashNetworkInputInterface()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        return &mCrashNetworkInputInterface;
    }

    const BrnTraffic::BrnTrafficIO::TrafficNetworkInputInterface* OutputBuffer::GetTrafficNetworkInputInterface() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return &mTrafficNetworkInputInterface;
    }

    BrnTraffic::BrnTrafficIO::TrafficNetworkInputInterface* OutputBuffer::GetTrafficNetworkInputInterface()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        return &mTrafficNetworkInputInterface;
    }

    NetworkToGameStateInterface* OutputBuffer::GetNetworkToGameStateInterface()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        return &mNetworkToGameStateInterface;
    }

    const NetworkToGameStateInterface* OutputBuffer::GetNetworkToGameStateInterface() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return &mNetworkToGameStateInterface;
    }

    const OutputBuffer::NetworkGameDataRequestInterface* OutputBuffer::GetGameDataRequestInterface() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return &mGameDataRequestInterface;
    }

    StatsOutputInterface* OutputBuffer::GetStatsOutputInterface()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        return &mStatsOutputInterface;
    }

    const InGamePlayerStatusInterface* OutputBuffer::GetInGamePlayerStatusInterface() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return &mInGamePlayerStatusInterface;
    }

    InGamePlayerStatusInterface* OutputBuffer::GetInGamePlayerStatusInterface()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        return &mInGamePlayerStatusInterface;
    }

    const PlayerResultsInterface* OutputBuffer::GetPlayerResultsInterface() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return &mPlayerResultsInterface;
    }

    PlayerResultsInterface* OutputBuffer::GetPlayerResultsInterface()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        return &mPlayerResultsInterface;
    }

    const NetworkToGuiInterface* OutputBuffer::GetNetworkToGuiInterface() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return &mNetworkToGuiInterface;
    }

    NetworkToGuiInterface* OutputBuffer::GetNetworkToGuiInterface()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        return &mNetworkToGuiInterface;
    }

    OnlineLobbyPlayerStatusInterface* OutputBuffer::GetOnlineLobbyPlayerStatusInterface()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        return &mOnlineLobbyPlayerStatusInterface;
    }

    const OnlineLobbyPlayerStatusInterface* OutputBuffer::GetOnlineLobbyPlayerStatusInterface() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return &mOnlineLobbyPlayerStatusInterface;
    }

    NetworkEventQueue* OutputBuffer::GetNetworkEventQueue()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        return &mNetworkEventQueue;
    }

    const NetworkEventQueue* OutputBuffer::GetNetworkEventQueue() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return &mNetworkEventQueue;
    }

    // ========================================================================
    // InGamePlayerStatusInterface helpers
    // ========================================================================

    // Asserts the index is in [0, miNumPlayers), then returns the indexed 312-byte record.
    InGamePlayerStatusData* InGamePlayerStatusInterface::GetPlayerStatusDataForWriting(s32 liIndex)
    {
        CGS_ASSERT(liIndex >= 0, "liIndex >= 0");
        CGS_ASSERT(liIndex < miNumPlayers, "liIndex < miNumPlayers");
        return &maInGamePlayerData[liIndex];
    }

    // Member-wise copy: the 8 records through InGamePlayerStatusData::operator= (312-byte
    // stride), the 36-byte game name, then the word at +2532, the word at +2536 and the byte at
    // +2540.
    InGamePlayerStatusInterface& InGamePlayerStatusInterface::operator=(const InGamePlayerStatusInterface& lOther)
    {
        for (s32 liIndex = 0; liIndex < 8; ++liIndex)
        {
            maInGamePlayerData[liIndex] = lOther.maInGamePlayerData[liIndex];
        }

        for (s32 liChar = 0; liChar < 36; ++liChar)
        {
            macGameName[liChar] = lOther.macGameName[liChar];
        }

        miNumPlayers         = lOther.miNumPlayers;           // +2532
        miTotalNumberPlayers = lOther.miTotalNumberPlayers;   // +2536
        mbLocalPlayerIsHost  = lOther.mbLocalPlayerIsHost;    // +2540
        return *this;
    }

    // Computes strlen(lpcName) inline and asserts it is < 36 ("String too long"), then
    // strncpy's into the 36-char macGameName.
    void InGamePlayerStatusInterface::SetGameName(const char* lpcName)
    {
        CGS_ASSERT(std::strlen(lpcName) < 36, "String too long");
        std::strncpy(macGameName, lpcName, 36);
    }

    // ========================================================================
    // PlayerResultsInterface helpers
    // ========================================================================

    // Member-wise copy of the 8 PlayerResultsData records (28-byte stride).
    PlayerResultsInterface& PlayerResultsInterface::operator=(const PlayerResultsInterface& lOther)
    {
        for (s32 liIndex = 0; liIndex < 8; ++liIndex)
        {
            maPlayerResultsData[liIndex] = lOther.maPlayerResultsData[liIndex];
        }
        return *this;
    }

    // Resets all 8 records: PlayerResultsData::Clear inlined into an 8-iteration loop. Per
    // record, mFinishTime goes through the out-of-line CgsSystem::Time::SetFloatVal(0.0f), both
    // race-car slots become -1, the finish distance -1.0f, eliminations 0, the three bytes false.
    void PlayerResultsInterface::Clear()
    {
        for (s32 liIndex = 0; liIndex < 8; ++liIndex)
        {
            PlayerResultsData& lRecord = maPlayerResultsData[liIndex];

            lRecord.mFinishTime.SetFloatVal(0.0f);
            lRecord.meActiveRaceCarIndex = ::E_ACTIVE_RACE_CAR_INDEX_INVALID;
            lRecord.meEliminatorIndex    = ::E_ACTIVE_RACE_CAR_INDEX_INVALID;
            lRecord.mfDistanceToFinish   = -1.0f;
            lRecord.miEliminations       = 0;
            lRecord.mbValid              = false;
            lRecord.mbTimedOut           = false;
            lRecord.mbEliminated         = false;
        }
    }
}
}
