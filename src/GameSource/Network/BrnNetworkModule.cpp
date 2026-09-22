// ============================================================================
// b5-decomp/src/GameSource/Network/BrnNetworkModule.cpp
// ============================================================================
// BrnNetwork::BrnNetworkModule -- constructor, the staged lifecycle (Construct / Prepare / Release /
// Destruct), the player-ID cache pair, the two mapping-table forwards, the lock-guarded sub-object
// accessors, and the compile-time layout pins. The per-frame update (ProcessBeforeSimulation /
// ProcessAfterSimulation) is declared in the header and not bodied yet.
//
// Every accessor asserts mbIsUpdating ("Can not use this function unless module is updating") and
// then returns the address of its embedded member.

#include "GameSource/Network/BrnNetworkModule.h"

#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"                         // CgsDev::PerfMonCpu::AddMonitor
#include "GameShared/GameClasses/Development/DebugSystem/Interface/CgsDebugInterface.h"            // CgsDev::DebugInterface (the two update-message toggles)
#include "GameShared/GameClasses/Network/Utilities/CgsNetworkImageConverter.h"                     // CgsNetwork::NetworkImageConverter::SetupPerfmons
#include "GameShared/GameClasses/Network/Players/CgsPlayerManager.h"                               // CgsNetwork::PlayerManager::GetNextPlayerID
#include "GameShared/GameClasses/Network/Players/CgsNetworkPlayer.h"                               // CgsNetwork::KI_INVALID_PLAYER_ID
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceGames.h" // CgsNetwork::ServerInterfaceGames::IsPlayerInGameByID
#include "GameSource/Resource/SharedIO/BrnGameDataAllocatorList.h"                                 // AllocatorList::GetHeapAllocator

#include <cstddef>       // offsetof
#include <type_traits>   // std::is_same

namespace BrnNetwork
{
    // The console constructor has no stores of its own past the base and the embedded manager:
    // every store it makes is an inlined member constructor.
    //   - the four module-owned VariableEventQueues and the two queues nested in the vehicle
    //     driver-input / vehicle output interfaces: BaseVariableEventQueue() clears
    //     mbIsConstructed (one byte store at each queue's first byte);
    //   - mActiveRaceCarInterface's maCarsInTheRace array: its count word takes the unconstructed
    //     sentinel (-1 at interface +0x200);
    //   - mTimerStatusOutputInterface: the two embedded CgsSystem::Time members zero themselves
    //     (their own constructor does that here too).
    // The tree's BaseVariableEventQueue and Array have no constructors, so the same stores are made
    // here through the members' MarkUnconstructed(), the precedent the GUI and particle module
    // constructors follow. Everything else (mbIsUpdating, the stages, the cache, the perf-monitor
    // ids) is left for Construct(bool), exactly as on the console.
    BrnNetworkModule::BrnNetworkModule()
    {
        mVehicleDriverInputInterface.GetUpdateDriverQueue()->MarkUnconstructed();   // +614704
        mVehicleOutputInterface.GetGameEventQueue()->MarkUnconstructed();           // +788272
        mActiveRaceCarInterface.maCarsInTheRace.MarkUnconstructed();                // +790352
        mGameEventQueue.MarkUnconstructed();                                        // +827940
        mInputGuiEventQueue.MarkUnconstructed();                                    // +829832
        mOutputGuiEventQueue.MarkUnconstructed();                                   // +848280
        mNetworkEventQueue.MarkUnconstructed();                                     // +852392
    }

    // The memory-map bank Prepare carves the manager's heap from ("Network Memory", bank 27).
    static const s32 KI_NETWORK_HEAP_BANK = 27;

    // Slot 16. Constructs the base and the embedded manager, resets the two stages, constructs
    // every owned queue and interface (in the console's call order), clears the player-ID cache,
    // then registers the ten perf monitors and the image converter's own monitors.
    void BrnNetworkModule::Construct(bool lbEnableJuice)
    {
        mbIsUpdating = true;
        ModuleSingleBuffered::Construct();
        mNetworkManager.Construct(this, lbEnableJuice);

        meReleaseStage = E_RELEASESTAGE_DONE;
        mePrepareStage = E_PREPARESTAGE_START;

        GetGameEventQueue()->Construct();
        mInputGuiEventQueue.Construct();
        mOutputGuiEventQueue.Construct();
        mVehicleDriverInputInterface.Construct();
        mVehicleInputInterface.Construct();
        mTrafficNetworkInputInterface.Construct();       // inlined: queue Construct + mbDiverged = false
        mCrashNetworkInputInterface.Construct();
        mNetworkToGameStateInterface.Construct();        // inlined: the five queue Constructs + frame counter 0
        mCrashNetworkOutputInterface.Construct();        // the queue Construct...
        mCrashNetworkOutputInterface.Clear();            // ...then a second length reset
        mVehicleOutputInterface.Construct();             // inlined: three queues, used-cars bitset, flags
        mActiveRaceCarInterface.Clear();
        mGameStateToNetworkInterface.Construct();        // inlined: dirty-trick queue Construct + Clear()
        mTakedownEventInputQueue.Construct();
        mTrafficNetworkOutputInterface.Construct();
        mNetworkEventQueue.Construct();
        mTimerStatusOutputInterface.Clear();

        ClearCachedNetworkPlayerIDsAtGameEnd();

        mbIsUpdating  = false;
        mbIsNewModule = true;
        mbPadIdle     = true;

        miNetworkBeforeSimPM = CgsDev::PerfMonCpu::AddMonitor("Module - Process before",        CgsDev::E_PMP_8, false, 1.0f, true);
        miNetworkAfterSimPM  = CgsDev::PerfMonCpu::AddMonitor("Module - Process after",         CgsDev::E_PMP_8, false, 1.0f, true);
        miNetworkAfterSim1PM = CgsDev::PerfMonCpu::AddMonitor("Module - Process after 1",       CgsDev::E_PMP_9, false, 1.0f, true);
        miNetworkAfterSim2PM = CgsDev::PerfMonCpu::AddMonitor("Module - Process after 2",       CgsDev::E_PMP_9, false, 1.0f, true);
        miNetworkAfterSim3PM = CgsDev::PerfMonCpu::AddMonitor("Module - Process after 3",       CgsDev::E_PMP_9, false, 1.0f, true);
        miNetworkAfterSim4PM = CgsDev::PerfMonCpu::AddMonitor("Module - Process after 4",       CgsDev::E_PMP_9, false, 1.0f, true);
        miNetworkAfterSim5PM = CgsDev::PerfMonCpu::AddMonitor("Module - Process after 5",       CgsDev::E_PMP_9, false, 1.0f, true);
        miNetworkAfterSim6PM = CgsDev::PerfMonCpu::AddMonitor("Man - ProcessAfterSimulation()", CgsDev::E_PMP_9, false, 1.0f, true);
        miNetworkAfterSim7PM = CgsDev::PerfMonCpu::AddMonitor("Module - Process after 7",       CgsDev::E_PMP_9, false, 1.0f, true);
        miNetworkAfterSim8PM = CgsDev::PerfMonCpu::AddMonitor("Module - Process after 8",       CgsDev::E_PMP_9, false, 1.0f, true);

        CGS_ASSERT(miNetworkBeforeSimPM >= 0, "miNetworkBeforeSimPM >= 0");
        CGS_ASSERT(miNetworkAfterSimPM  >= 0, "miNetworkAfterSimPM >= 0");
        CGS_ASSERT(miNetworkAfterSim1PM >= 0, "miNetworkAfterSim1PM >= 0");
        CGS_ASSERT(miNetworkAfterSim2PM >= 0, "miNetworkAfterSim2PM >= 0");
        CGS_ASSERT(miNetworkAfterSim3PM >= 0, "miNetworkAfterSim3PM >= 0");
        CGS_ASSERT(miNetworkAfterSim4PM >= 0, "miNetworkAfterSim4PM >= 0");
        CGS_ASSERT(miNetworkAfterSim5PM >= 0, "miNetworkAfterSim5PM >= 0");
        CGS_ASSERT(miNetworkAfterSim6PM >= 0, "miNetworkAfterSim6PM >= 0");
        CGS_ASSERT(miNetworkAfterSim7PM >= 0, "miNetworkAfterSim7PM >= 0");
        CGS_ASSERT(miNetworkAfterSim8PM >= 0, "miNetworkAfterSim8PM >= 0");

        CgsNetwork::NetworkImageConverter::SetupPerfmons();
    }

    // Slot 17. Each stage stores its own number and falls through to the next; a failing stage
    // breaks out with false so the next call resumes there. The output buffer stays write-locked
    // for the whole call and the heap is fetched before the switch.
    bool BrnNetworkModule::Prepare(BrnNetworkModuleIO::OutputBuffer*             lpOutputBuffer,
                                   CgsSystem::EFrameRate                         leFrameRate,
                                   const BrnHW::LaunchData*                      lpLaunchData,
                                   const BrnResource::GameDataIO::AllocatorList* lpAllocatorList)
    {
        mbIsUpdating = true;
        lpOutputBuffer->LockForWrite();

        CgsMemory::HeapMalloc* lpHeapMalloc = lpAllocatorList->GetHeapAllocator(KI_NETWORK_HEAP_BANK);

        switch (mePrepareStage)
        {
        case E_PREPARESTAGE_START:
            mePrepareStage = E_PREPARESTAGE_START;
            // fall through
        case E_PREPARESTAGE_MODULE:
            mePrepareStage = E_PREPARESTAGE_MODULE;
            if (!ModuleSingleBuffered::Prepare())
            {
                break;
            }
            // FLAG: the console calls BrnResource::PrintConsoleMemory("Network prepare start") here.
            // That function has no declaration in the tree yet, so the call is not made.
            // fall through
        case E_PREPARESTAGE_NETWORK_MANAGER:
            mePrepareStage = E_PREPARESTAGE_NETWORK_MANAGER;
            if (!mNetworkManager.Prepare(leFrameRate, lpLaunchData, lpHeapMalloc))
            {
                break;
            }
            // fall through
        case E_PREPARESTAGE_DONE:
        {
            mePrepareStage = E_PREPARESTAGE_DONE;
            meReleaseStage = E_RELEASESTAGE_START;

            CGS_ASSERT(mInputGuiEventQueue.Prepare(),  "mInputGuiEventQueue.Prepare()");
            CGS_ASSERT(mOutputGuiEventQueue.Prepare(), "mOutputGuiEventQueue.Prepare()");
            CGS_ASSERT(mNetworkEventQueue.Prepare(),   "mNetworkEventQueue.Prepare()");

            CgsDev::DebugInterface lDebugInterface;
            mbOutputSendUpdateMessages = false;
            mbOutputRecvUpdateMessages = false;
            lDebugInterface.RegisterVariable(&mbOutputSendUpdateMessages, "Network", "Output SendUpdateMessages");
            lDebugInterface.RegisterVariable(&mbOutputRecvUpdateMessages, "Network", "Output RecvUpdateMessages");

            mbPadIdle = true;
            lpOutputBuffer->UnlockForWrite();
            mbIsUpdating = false;
            return true;
        }
        default:
            CGS_ASSERT(false, "unknown prepare stage");
            break;
        }

        lpOutputBuffer->UnlockForWrite();
        mbIsUpdating = false;
        return false;
    }

    // Slot 2. The mirror of Prepare: drop the debug toggles and release the network-event queue,
    // then the manager, then the base; the last stage clears the game-event queue and re-arms the
    // prepare stages.
    bool BrnNetworkModule::Release()
    {
        mbIsUpdating = true;

        switch (meReleaseStage)
        {
        case E_RELEASESTAGE_START:
        {
            meReleaseStage = E_RELEASESTAGE_START;
            CgsDev::DebugInterface lDebugInterface;
            lDebugInterface.UnregisterVariable(&mbOutputSendUpdateMessages);
            lDebugInterface.UnregisterVariable(&mbOutputRecvUpdateMessages);
            CGS_ASSERT(mNetworkEventQueue.Release(), "mNetworkEventQueue.Release()");
        }
            // fall through
        case E_RELEASESTAGE_NETWORK_MANAGER:
            meReleaseStage = E_RELEASESTAGE_NETWORK_MANAGER;
            if (!mNetworkManager.Release())
            {
                break;
            }
            // fall through
        case E_RELEASESTAGE_MODULE:
            meReleaseStage = E_RELEASESTAGE_MODULE;
            if (!ModuleSingleBuffered::Release())
            {
                break;
            }
            // fall through
        case E_RELEASESTAGE_DONE:
            GetGameEventQueue()->Clear();
            mbIsUpdating   = false;
            meReleaseStage = E_RELEASESTAGE_DONE;
            mePrepareStage = E_PREPARESTAGE_START;
            mbPadIdle      = true;
            return true;
        default:
            CGS_ASSERT(false, "unknown release stage");
            break;
        }

        mbIsUpdating = false;
        return false;
    }

    // Slot 3.
    void BrnNetworkModule::Destruct()
    {
        mbIsUpdating = true;
        mNetworkManager.Destruct();
        mGameEventQueue.Destruct();
        ClearCachedNetworkPlayerIDsAtGameEnd();
        ModuleSingleBuffered::Destruct();
        mbIsUpdating = false;
    }

    // Records, in registry order, every player the games component reports as in the game.
    void BrnNetworkModule::CacheNetworkPlayerIDsAtGameStart()
    {
        CGS_ASSERT(mNetworkManager.GetServerInterface(), "mNetworkManager.GetServerInterface()");

        CgsNetwork::ServerInterfaceGames* lpServerInterfaceGames = mNetworkManager.GetServerInterface()->GetGameComponent();
        CgsNetwork::PlayerManager*        lpPlayerManager        = mNetworkManager.GetPlayerManager();
        NetworkPlayerID                   lPlayerID              = CgsNetwork::KI_INVALID_PLAYER_ID;

        CGS_ASSERT(lpServerInterfaceGames, "lpServerInterfaceGames");
        CGS_ASSERT(lpPlayerManager,        "lpPlayerManager");

        miCachedPlayersInGame = 0;
        while (lpPlayerManager->GetNextPlayerID(&lPlayerID, CgsNetwork::PlayerManager::E_CONSIDER_PLAYERS_WHO_HAVE_FINALISED))
        {
            if (lpServerInterfaceGames->IsPlayerInGameByID(lPlayerID))
            {
                maCachedPlayerIDsInGame[miCachedPlayersInGame] = lPlayerID;
                ++miCachedPlayersInGame;
            }
        }
    }

    void BrnNetworkModule::ClearCachedNetworkPlayerIDsAtGameEnd()
    {
        for (s32 liIndex = 0; liIndex < KI_MAX_CACHED_PLAYERS_IN_GAME; ++liIndex)
        {
            maCachedPlayerIDsInGame[liIndex] = CgsNetwork::KI_INVALID_PLAYER_ID;
        }
        miCachedPlayersInGame = 0;
    }

    // Header-inline on the console (no out-of-line symbol): the callers read the module's own
    // mGameStateToNetworkInterface (+818064) directly, with no updating assert, and call the
    // mapping-table lookups on it.
    EActiveRaceCarIndex BrnNetworkModule::GetActiveRaceCarIndex(NetworkPlayerID lNetworkPlayerID)
    {
        return mGameStateToNetworkInterface.GetActiveRaceCarIndex(lNetworkPlayerID);
    }

    NetworkPlayerID BrnNetworkModule::GetNetworkPlayerID(EActiveRaceCarIndex leActiveRaceCarIndex)
    {
        return mGameStateToNetworkInterface.GetNetworkPlayerID(leActiveRaceCarIndex);
    }

    // -> &mVehicleDriverInputInterface (+614704).
    BrnPhysics::Vehicle::VehicleDriverInputInterface* BrnNetworkModule::GetVehicleDriverInputInterface()
    {
        CGS_ASSERT(mbIsUpdating, "Can not use this function unless module is updating\n");
        return &mVehicleDriverInputInterface;
    }

    // -> &mTrafficNetworkInputInterface (+800320).
    BrnTraffic::BrnTrafficIO::TrafficNetworkInputInterface* BrnNetworkModule::GetTrafficInputInterface()
    {
        CGS_ASSERT(mbIsUpdating, "Can not use this function unless module is updating\n");
        return &mTrafficNetworkInputInterface;
    }

    // -> &mTrafficNetworkOutputInterface (+800432).
    const BrnTraffic::BrnTrafficIO::TrafficNetworkOutputInterface* BrnNetworkModule::GetTrafficOutputInterface() const
    {
        CGS_ASSERT(mbIsUpdating, "Can not use this function unless module is updating\n");
        return &mTrafficNetworkOutputInterface;
    }

    // -> &mCrashNetworkInputInterface (+800576).
    BrnWorld::CrashIO::NetworkInputInterface* BrnNetworkModule::GetCrashInputInterface()
    {
        CGS_ASSERT(mbIsUpdating, "Can not use this function unless module is updating\n");
        return &mCrashNetworkInputInterface;
    }

    // -> &mCrashNetworkOutputInterface (+816080).
    const BrnWorld::CrashIO::NetworkOutputInterface* BrnNetworkModule::GetCrashOutputInterface() const
    {
        CGS_ASSERT(mbIsUpdating, "Can not use this function unless module is updating\n");
        return &mCrashNetworkOutputInterface;
    }

    // -> &mGameStateToNetworkInterface (+818064).
    BrnNetworkModuleIO::GameStateToNetworkInterface* BrnNetworkModule::GetGameStateToNetworkInterface()
    {
        CGS_ASSERT(mbIsUpdating, "Can not use this function unless module is updating\n");
        return &mGameStateToNetworkInterface;
    }

    // -> &mNetworkToGameStateInterface (+818608).
    BrnNetworkModuleIO::NetworkToGameStateInterface* BrnNetworkModule::GetNetworkToGameStateInterface()
    {
        CGS_ASSERT(mbIsUpdating, "Can not use this function unless module is updating\n");
        return &mNetworkToGameStateInterface;
    }

    // -> &mPlayerVehicleControls (+827880).
    BrnWorld::PlayerVehicleControls* BrnNetworkModule::GetPlayerVehicleControls()
    {
        CGS_ASSERT(mbIsUpdating, "Can not use this function unless module is updating\n");
        return &mPlayerVehicleControls;
    }

    // -> &mTakedownEventInputQueue (+829496).
    const CgsModule::EventQueue<BrnGameState::TakedownEvent, 8>* BrnNetworkModule::GetTakedownEventInputQueue() const
    {
        CGS_ASSERT(mbIsUpdating, "Can not use this function unless module is updating\n");
        return &mTakedownEventInputQueue;
    }

    // -> &mInputGuiEventQueue (+829832).
    CgsModule::VariableEventQueue<18432, 16>* BrnNetworkModule::GetInputGuiEventQueue()
    {
        CGS_ASSERT(mbIsUpdating, "Can not use this function unless module is updating\n");
        return &mInputGuiEventQueue;
    }

    // -> &mNetworkEventQueue (+852392). BrnNetworkModuleIO.h still spells NetworkEventQueue as a
    // pointer-only class rather than the VariableEventQueue<14000,16> typedef, so the address is
    // handed back under that name; the cast becomes an identity cast once the typedef lands there.
    BrnNetworkModuleIO::NetworkEventQueue* BrnNetworkModule::GetNetworkEventQueue()
    {
        CGS_ASSERT(mbIsUpdating, "Can not use this function unless module is updating\n");
        return reinterpret_cast<BrnNetworkModuleIO::NetworkEventQueue*>(&mNetworkEventQueue);
    }

    // -> mbOutputSendUpdateMessages (+614664).
    bool BrnNetworkModule::IsSendUpdateMessageToBeShown() const
    {
        CGS_ASSERT(mbIsUpdating, "Can not use this function unless module is updating\n");
        return mbOutputSendUpdateMessages;
    }

    // -> mbOutputRecvUpdateMessages (+614665).
    bool BrnNetworkModule::IsRecvUpdateMessageToBeShown() const
    {
        CGS_ASSERT(mbIsUpdating, "Can not use this function unless module is updating\n");
        return mbOutputRecvUpdateMessages;
    }

    // Never called. The object starts with a vtable pointer, so no member has a meaningful ABSOLUTE
    // host offset; what is pinned is (a) that every sub-object is the real type, never a byte blob,
    // and (b) the pointer-free runs, as console deltas from the run's first member.
    void BrnNetworkModule::_AssertLayout()
    {
        static_assert(std::is_same<decltype(BrnNetworkModule::mNetworkManager), BrnNetworkManager>::value,
                      "mNetworkManager must be the real BrnNetworkManager");
        static_assert(std::is_same<decltype(BrnNetworkModule::mVehicleDriverInputInterface),
                                   BrnPhysics::Vehicle::VehicleDriverInputInterface>::value,
                      "mVehicleDriverInputInterface must be the real type");
        static_assert(std::is_same<decltype(BrnNetworkModule::mVehicleInputInterface),
                                   BrnPhysics::Vehicle::VehicleInputInterface>::value,
                      "mVehicleInputInterface must be the real type");
        static_assert(std::is_same<decltype(BrnNetworkModule::mVehicleOutputInterface),
                                   BrnPhysics::Vehicle::VehicleOutputInterface>::value,
                      "mVehicleOutputInterface must be the real type");
        static_assert(std::is_same<decltype(BrnNetworkModule::mGameEventQueue),
                                   BrnGameState::GameStateModuleIO::GameEventQueue>::value,
                      "mGameEventQueue must be the real GameStateModuleIO::GameEventQueue");

        // The stage/flag/cache run after the manager: +614656 .. +614704 on the console.
        static_assert(offsetof(BrnNetworkModule, meReleaseStage)             - offsetof(BrnNetworkModule, mePrepareStage) == 4,  "meReleaseStage @ +4");
        static_assert(offsetof(BrnNetworkModule, mbOutputSendUpdateMessages) - offsetof(BrnNetworkModule, mePrepareStage) == 8,  "mbOutputSendUpdateMessages @ +8");
        static_assert(offsetof(BrnNetworkModule, mbOutputRecvUpdateMessages) - offsetof(BrnNetworkModule, mePrepareStage) == 9,  "mbOutputRecvUpdateMessages @ +9");
        static_assert(offsetof(BrnNetworkModule, mbPadIdle)                  - offsetof(BrnNetworkModule, mePrepareStage) == 10, "mbPadIdle @ +10");
        static_assert(offsetof(BrnNetworkModule, maCachedPlayerIDsInGame)    - offsetof(BrnNetworkModule, mePrepareStage) == 12, "maCachedPlayerIDsInGame @ +12");
        static_assert(offsetof(BrnNetworkModule, miCachedPlayersInGame)      - offsetof(BrnNetworkModule, mePrepareStage) == 44, "miCachedPlayersInGame @ +44");

        // The player controls and the game-event queue are pointer-free: +827880 -> +827940.
        static_assert(sizeof(BrnWorld::PlayerVehicleControls) == 60, "PlayerVehicleControls console span 60");
        static_assert(offsetof(BrnNetworkModule, mGameEventQueue) - offsetof(BrnNetworkModule, mPlayerVehicleControls) == 60,
                      "mGameEventQueue @ mPlayerVehicleControls + 60");

        // The three module-owned GUI/network queues and the perf-monitor ids are pointer-free:
        // +829832 -> +848280 -> +852392 -> +866408 .. +866444.
        static_assert(offsetof(BrnNetworkModule, mOutputGuiEventQueue)  - offsetof(BrnNetworkModule, mInputGuiEventQueue) == 848280 - 829832, "mOutputGuiEventQueue delta");
        static_assert(offsetof(BrnNetworkModule, mNetworkEventQueue)    - offsetof(BrnNetworkModule, mInputGuiEventQueue) == 852392 - 829832, "mNetworkEventQueue delta");
        static_assert(offsetof(BrnNetworkModule, miNetworkBeforeSimPM)  - offsetof(BrnNetworkModule, mInputGuiEventQueue) == 866408 - 829832, "miNetworkBeforeSimPM delta");
        static_assert(offsetof(BrnNetworkModule, miNetworkAfterSim8PM)  - offsetof(BrnNetworkModule, miNetworkBeforeSimPM) == 36,            "the ten perf-monitor ids are contiguous");
    }
}
