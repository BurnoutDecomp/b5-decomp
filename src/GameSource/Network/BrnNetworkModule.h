#ifndef BRN_NETWORK_MODULE_H
#define BRN_NETWORK_MODULE_H

// ============================================================================
// b5-decomp/src/GameSource/Network/BrnNetworkModule.h
// ============================================================================
// BrnNetwork::BrnNetworkModule -- the engine's networking module. BrnGame::BrnGameModule owns it
// by value, constructs it, and drives it every frame through DoUpdate_NetworkPreSim
// (ProcessBeforeSimulation) and DoUpdate_NetworkPostSim (ProcessAfterSimulation).
//
// It is a full CgsModule::ModuleSingleBuffered. The console constructor stores the base vtable,
// builds the base's two RWMutexes (+0x10 / +0x118), then stores this class's vtable and builds the
// embedded BrnNetworkManager at +0x280.
//
// VTABLE (console, 18 slots). Slots 0..15 are the base's; this class overrides two of them and
// appends two new virtuals:
//   0 ModuleSingleBuffered::Construct()      1 ModuleSingleBuffered::Prepare()
//   2 BrnNetworkModule::Release()            3 BrnNetworkModule::Destruct()
//   4 ModuleSingleBuffered::Update()         5 ModuleSingleBuffered::SetMultiThreaded(bool)
//   6..9 the four Lock/Unlock hooks          10..13 the four DataStructure hooks
//   14 CreateInputDataStructure()            15 CreateOutputDataStructure()
//   16 BrnNetworkModule::Construct(bool)     17 BrnNetworkModule::Prepare(OutputBuffer*, ...)
// Construct(bool) and Prepare(...) are NEW virtuals that hide the base's argument-less
// Construct()/Prepare(); Release()/Destruct() are true overrides (marked `override`). The tree's
// CgsModule::Module adds a virtual destructor ahead of these (see CgsModule.h), so host slot
// numbers are one higher; every call is made by name, so the order stays self-consistent.
//
// LAYOUT (console byte offsets; the object is 866,560 bytes on the console). Every member below is
// the REAL typed member in declaration order. Host pointer widths move everything after the
// vtable pointer, so only pointer-free runs are pinned (relative offsets) in _AssertLayout().
//   +0x228   mbIsUpdating                    (every accessor asserts it)
//   +0x280   mNetworkManager                 BrnNetworkManager
//   +614656  mePrepareStage / +614660 meReleaseStage
//   +614664  mbOutputSendUpdateMessages / +614665 mbOutputRecvUpdateMessages / +614666 mbPadIdle
//   +614668  maCachedPlayerIDsInGame[8] / +614700 miCachedPlayersInGame
//   +614704  mVehicleDriverInputInterface    +620000 mVehicleInputInterface
//   +762176  mVehicleOutputInterface         +789840 mActiveRaceCarInterface
//   +800320  mTrafficNetworkInputInterface   +800432 mTrafficNetworkOutputInterface
//   +800576  mCrashNetworkInputInterface     +816080 mCrashNetworkOutputInterface
//   +818016  mTimerStatusOutputInterface     +818064 mGameStateToNetworkInterface
//   +818608  mNetworkToGameStateInterface    +827880 mPlayerVehicleControls
//   +827940  mGameEventQueue                 +829496 mTakedownEventInputQueue
//   +829832  mInputGuiEventQueue             +848280 mOutputGuiEventQueue
//   +852392  mNetworkEventQueue              +866408..+866444 the ten perf-monitor ids
// (+866448 .. +866560 is tail padding.)

#include "types.hpp"
#include "SharedClasses/BrnSharedConstants.h"                                   // BrnUpdateSet
#include "GameShared/GameClasses/Core/CgsAssert.h"                              // CGS_ASSERT
#include "GameShared/GameClasses/Module/CgsModuleSingleBuffered.h"              // CgsModule::ModuleSingleBuffered (base)
#include "GameShared/GameClasses/Module/CgsIOBufferStack.h"                     // CgsModule::IOBufferStack (the update legs' stacks)
#include "GameShared/GameClasses/Module/CgsEventQueue.h"                        // CgsModule::EventQueue<T,N> (takedown input queue)
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"                // CgsModule::VariableEventQueue<N,16> (the three module-owned queues)
#include "GameShared/GameClasses/System/Timer/CgsFrameRate.h"                   // CgsSystem::EFrameRate (Prepare)
#include "GameShared/GameClasses/System/Timer/CgsTimerStatusInterface.h"        // CgsSystem::TimerStatusInterface (+818016)
#include "GameSource/Network/BrnNetworkManager.h"                               // BrnNetwork::BrnNetworkManager (+0x280)
#include "GameSource/Network/BrnNetworkModuleIO.h"                              // PreSimulationInputBuffer / PostSimulationInputBuffer / OutputBuffer
#include "GameSource/Network/SharedIO/BrnNetworkSharedIO.h"                     // NetworkPlayerID, EActiveRaceCarIndex
#include "GameSource/Network/SharedIO/BrnNetworkModuleGameStateIOInterfaces.h"  // GameStateToNetworkInterface / NetworkToGameStateInterface
#include "GameSource/GameState/BrnGameStateModuleIO.h"                          // BrnGameState::GameStateModuleIO::GameEventQueue (+827940)
#include "GameSource/GameState/TakedownManager/BrnTakedownManagerTypes.h"       // BrnGameState::TakedownEvent
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleDriverInputInterface.h" // +614704
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleInputInterface.h"       // +620000
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleOutputInterface.h"      // +762176
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarEntityModuleOutputInterface.h" // +789840
#include "GameSource/World/EntityModules/TrafficEntityModule/SharedIO/BrnTrafficNetworkInterfaces.h"          // +800320 / +800432
#include "GameSource/World/CrashModule/SharedIO/BrnCrashModuleNetworkIOInterfaces.h"                          // +800576 / +816080
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnPlayerVehicleControls.h"             // +827880

// Prepare's two pointer-only arguments. Neither is dereferenced by this header.
namespace BrnHW { struct LaunchData; }
namespace BrnResource { namespace GameDataIO { class AllocatorList; } }

namespace BrnNetwork
{
    // The module's output GUI event queue (+848280). Every AddOutputGuiEvent<T> instance publishes
    // through GetOutputGuiEventQueue()->AddEvent(event, T::GetEventType(), sizeof(T)).
    typedef CgsModule::VariableEventQueue<4096, 16> GuiEventQueueSmall;

    class BrnNetworkModule : public CgsModule::ModuleSingleBuffered
    {
    public:
        enum EPrepareStage
        {
            E_PREPARESTAGE_START           = 0,
            E_PREPARESTAGE_MODULE          = 1,
            E_PREPARESTAGE_NETWORK_MANAGER = 2,
            E_PREPARESTAGE_DONE            = 3,
        };

        enum EReleaseStage
        {
            E_RELEASESTAGE_START           = 0,
            E_RELEASESTAGE_NETWORK_MANAGER = 1,
            E_RELEASESTAGE_MODULE          = 2,
            E_RELEASESTAGE_DONE            = 3,
        };

        BrnNetworkModule();

        // ---- lifecycle -------------------------------------------------------------------------
        // Slot 16. Sets mbIsUpdating, runs the base Construct(), constructs the manager (handing it
        // this module and the juice flag), resets the stages, constructs every queue/interface,
        // clears the player-ID cache and registers the ten perf monitors.
        virtual void Construct(bool lbEnableJuice);

        // Slot 17. The staged prepare (EPrepareStage): base Prepare(), then the manager's Prepare
        // with the frame rate, the launch data and the network heap from the allocator list, then
        // the three module-owned queues' Prepare and the two update-message debug toggles. Returns
        // true once every stage is done. Holds the output buffer's write lock throughout.
        virtual bool Prepare(BrnNetworkModuleIO::OutputBuffer*                lpOutputBuffer,
                             CgsSystem::EFrameRate                            leFrameRate,
                             const BrnHW::LaunchData*                         lpLaunchData,
                             const BrnResource::GameDataIO::AllocatorList*    lpAllocatorList);

        // Slot 2. The staged release (EReleaseStage): the network-event queue and the two debug
        // toggles, then the manager, then the base Release().
        bool Release() override;

        // Slot 3. Destructs the manager and the game-event queue, clears the player-ID cache, then
        // the base Destruct().
        void Destruct() override;

        // ---- per-frame update (non-virtual; called by the game module's network legs) ----------
        // Returns whether the local player is in a game (DoUpdate_NetworkPreSim stores it).
        bool ProcessBeforeSimulation(CgsModule::IOBufferStack*                          lpInputBufferStack,
                                     CgsModule::IOBufferStack*                          lpOutputBufferStack,
                                     const BrnNetworkModuleIO::PreSimulationInputBuffer* lpInputBuffer,
                                     BrnNetworkModuleIO::OutputBuffer*                  lpOutputBuffer,
                                     BrnUpdateSet                                       lUpdateSet);

        void ProcessAfterSimulation(CgsModule::IOBufferStack*                           lpInputBufferStack,
                                    CgsModule::IOBufferStack*                           lpOutputBufferStack,
                                    const BrnNetworkModuleIO::PostSimulationInputBuffer* lpInputBuffer,
                                    BrnUpdateSet                                        lUpdateSet);

        void CacheNetworkPlayerIDsAtGameStart();
        void ClearCachedNetworkPlayerIDsAtGameEnd();

        // ---- accessors (each asserts the module is updating, then hands back its member) --------
        BrnGameState::GameStateModuleIO::GameEventQueue* GetGameEventQueue()
        {
            CGS_ASSERT(mbIsUpdating, "Can not use this function unless module is updating\n");
            return &mGameEventQueue;
        }

        BrnNetworkManager* GetNetworkManager()
        {
            CGS_ASSERT(mbIsUpdating, "Can not use this function unless module is updating\n");
            return &mNetworkManager;
        }

        GuiEventQueueSmall* GetOutputGuiEventQueue()
        {
            CGS_ASSERT(mbIsUpdating, "Can not use this function unless module is updating\n");
            return &mOutputGuiEventQueue;
        }

        // Publish one GUI event onto the output GUI event queue. The event-type id and byte size fall
        // out of TEvent, so one body reproduces every per-T instance; the explicit instantiations
        // live in BrnNetworkModule_AddOutputGuiEvent_Inst.cpp.
        template<typename TEvent>
        int AddOutputGuiEvent(const TEvent& lrEvent)
        {
            CGS_ASSERT(mbIsUpdating, "Can not use this function unless module is updating\n");
            return GetOutputGuiEventQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lrEvent),
                lrEvent.GetEventType(), static_cast<s32>(sizeof(TEvent)));
        }

        BrnNetworkModuleIO::GameStateToNetworkInterface* GetGameStateToNetworkInterface();

        // Convenience forwards onto the GameState->Network mapping table (the network managers call
        // them directly). Declared-only; bodies land with the mapping-table TU.
        EActiveRaceCarIndex GetActiveRaceCarIndex(NetworkPlayerID lNetworkPlayerID);
        NetworkPlayerID     GetNetworkPlayerID(EActiveRaceCarIndex leActiveRaceCarIndex);

        BrnPhysics::Vehicle::VehicleDriverInputInterface*                 GetVehicleDriverInputInterface();
        BrnTraffic::BrnTrafficIO::TrafficNetworkInputInterface*           GetTrafficInputInterface();
        const BrnTraffic::BrnTrafficIO::TrafficNetworkOutputInterface*    GetTrafficOutputInterface() const;
        BrnWorld::CrashIO::NetworkInputInterface*                         GetCrashInputInterface();
        const BrnWorld::CrashIO::NetworkOutputInterface*                  GetCrashOutputInterface() const;
        BrnNetworkModuleIO::NetworkToGameStateInterface*                  GetNetworkToGameStateInterface();
        BrnWorld::PlayerVehicleControls*                                  GetPlayerVehicleControls();
        const CgsModule::EventQueue<BrnGameState::TakedownEvent, 8>*      GetTakedownEventInputQueue() const;
        CgsModule::VariableEventQueue<18432, 16>*                         GetInputGuiEventQueue();

        // Returns the tree-wide pointer-only name BrnNetworkModuleIO::NetworkEventQueue (declared in
        // BrnNetworkModuleIO.h); the member itself is the real VariableEventQueue<14000,16>.
        BrnNetworkModuleIO::NetworkEventQueue*                            GetNetworkEventQueue();

        bool IsSendUpdateMessageToBeShown() const;
        bool IsRecvUpdateMessageToBeShown() const;

    private:
        // Never called: compile-time pins of the pointer-free member runs (BrnNetworkModule.cpp).
        static void _AssertLayout();

        static const s32 KI_MAX_CACHED_PLAYERS_IN_GAME = 8;

        bool                                                    mbIsUpdating;                     // +0x228
        BrnNetworkManager                                       mNetworkManager;                  // +0x280
        EPrepareStage                                           mePrepareStage;                   // +614656
        EReleaseStage                                           meReleaseStage;                   // +614660
        bool                                                    mbOutputSendUpdateMessages;       // +614664
        bool                                                    mbOutputRecvUpdateMessages;       // +614665
        bool                                                    mbPadIdle;                        // +614666
        NetworkPlayerID                                         maCachedPlayerIDsInGame[KI_MAX_CACHED_PLAYERS_IN_GAME]; // +614668
        s32                                                     miCachedPlayersInGame;            // +614700
        BrnPhysics::Vehicle::VehicleDriverInputInterface        mVehicleDriverInputInterface;     // +614704
        BrnPhysics::Vehicle::VehicleInputInterface              mVehicleInputInterface;           // +620000
        BrnPhysics::Vehicle::VehicleOutputInterface             mVehicleOutputInterface;          // +762176
        BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface mActiveRaceCarInterface; // +789840
        BrnTraffic::BrnTrafficIO::TrafficNetworkInputInterface  mTrafficNetworkInputInterface;    // +800320
        BrnTraffic::BrnTrafficIO::TrafficNetworkOutputInterface mTrafficNetworkOutputInterface;   // +800432
        BrnWorld::CrashIO::NetworkInputInterface                mCrashNetworkInputInterface;      // +800576
        BrnWorld::CrashIO::NetworkOutputInterface               mCrashNetworkOutputInterface;     // +816080
        CgsSystem::TimerStatusInterface                         mTimerStatusOutputInterface;      // +818016
        BrnNetworkModuleIO::GameStateToNetworkInterface         mGameStateToNetworkInterface;     // +818064
        BrnNetworkModuleIO::NetworkToGameStateInterface         mNetworkToGameStateInterface;     // +818608
        BrnWorld::PlayerVehicleControls                         mPlayerVehicleControls;           // +827880
        BrnGameState::GameStateModuleIO::GameEventQueue         mGameEventQueue;                  // +827940
        CgsModule::EventQueue<BrnGameState::TakedownEvent, 8>   mTakedownEventInputQueue;         // +829496
        CgsModule::VariableEventQueue<18432, 16>                mInputGuiEventQueue;              // +829832
        GuiEventQueueSmall                                      mOutputGuiEventQueue;             // +848280
        CgsModule::VariableEventQueue<14000, 16>                mNetworkEventQueue;               // +852392

        // Perf-monitor ids registered by Construct ("Module - Process before", "Module - Process
        // after", "... after 1".."... after 5", "Man - ProcessAfterSimulation()", "... after 7",
        // "... after 8").
        s32                                                     miNetworkBeforeSimPM;             // +866408
        s32                                                     miNetworkAfterSimPM;              // +866412
        s32                                                     miNetworkAfterSim1PM;             // +866416
        s32                                                     miNetworkAfterSim2PM;             // +866420
        s32                                                     miNetworkAfterSim3PM;             // +866424
        s32                                                     miNetworkAfterSim4PM;             // +866428
        s32                                                     miNetworkAfterSim5PM;             // +866432
        s32                                                     miNetworkAfterSim6PM;             // +866436
        s32                                                     miNetworkAfterSim7PM;             // +866440
        s32                                                     miNetworkAfterSim8PM;             // +866444
    };
}

#endif
