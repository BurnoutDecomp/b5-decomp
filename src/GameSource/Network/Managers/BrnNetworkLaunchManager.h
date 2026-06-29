#ifndef BRN_NETWORK_LAUNCH_MANAGER_H
#define BRN_NETWORK_LAUNCH_MANAGER_H

#include "types.hpp"
#include "GameShared/GameClasses/System/Timer/CgsTime.h"   // CgsSystem::Time (mTimeToStopWaiting, by value)

// ===========================================================================
// BrnNetwork::LaunchManager
//   Home: GameSource/Network/Managers/BrnNetworkLaunchManager.{h,cpp}
//
// The online pre-launch state machine: once the host (or a joining client) starts
// the launch sequence it walks through locking the game, setting every player to
// "playing", suspending the server interface, waiting out a short settle window and
// finally launching. Each ESubState owns an Update* handler; the handlers are wired
// into maUpdateFunctions by Construct and dispatched by Update().
//
// LAYOUT (X360, grounded against the Construct / Update / ServerInterfaceIdle asm;
// 32-bit-pointer offsets, NOT static_asserted on the 64-bit host):
//   +0x00  maUpdateFunctions[10]   ten UpdateFn slots (one per ESubState)
//   +0x28  meSubState              (ESubState; ServerInterfaceIdle stores 9 @ +0x28)
//   +0x2C  mpNetworkManager        (BrnNetworkManager*; Construct stores a2 @ +0x2C)
//   +0x30  mTimeToStopWaiting      (Time, 8 bytes @ +0x30/+0x34)
//   +0x38  miNumPlayersSetPlaying  (s32; the per-player walk counter, a1[14])
//
// EStatus is the value the manager reports up to the StateManager each Update; the
// ESubState values double as the maUpdateFunctions index (the dispatcher does
// maUpdateFunctions[meSubState](this)). Both enums and their member values are from
// the DecFIGS DWARF (BrnNetworkLaunchManager.h:49 / :97).
// ===========================================================================

namespace BrnNetwork
{
    class BrnNetworkManager;

    class LaunchManager
    {
    public:
        // DWARF BrnNetworkLaunchManager.h:49 -- the status reported to the StateManager.
        enum EStatus
        {
            E_STATUS_IDLE              = 0,
            E_STATUS_BUSY              = 1,
            E_STATUS_WAITING_FOR_LAUNCH= 2,
            E_STATUS_LAUNCHING         = 3,
            E_STATUS_LAUNCHED          = 4,
            E_STATUS_ERROR             = 5,
            E_STATUS_COUNT             = 6,
        };

        void Construct(BrnNetworkManager* lpNetworkManager);
        bool Prepare();
        bool Release();
        void Destruct();

        EStatus Update();

        void Start();
        void Stop();
        void Reset();

        void Disconnected();
        void OnLeaveGame();

    private:
        // DWARF BrnNetworkLaunchManager.h:97 -- the launch sub-state; also the
        // maUpdateFunctions dispatch index.
        enum ESubState
        {
            E_SUBSTATE_NONE                  = 0,
            E_SUBSTATE_RUNNING               = 1,
            E_SUBSTATE_LOCKING               = 2,
            E_SUBSTATE_SET_PLAYERS_TO_PLAYING= 3,
            E_SUBSTATE_UNLOCK_FROM_ERROR     = 4,
            E_SUBSTATE_DOWNLOADING_STATS     = 5,
            E_SUBSTATE_LAUNCHING             = 6,
            E_SUBSTATE_SUSPENDING            = 7,
            E_SUBSTATE_WAITING               = 8,
            E_SUBSTATE_ERROR                 = 9,
            E_SUBSTATE_COUNT                 = 10,
        };

        // One handler per ESubState (member function pointer; the X360 array holds the
        // ten Update* thunks, dispatched as maUpdateFunctions[meSubState](this)).
        typedef EStatus (LaunchManager::*UpdateFn)();

        EStatus UpdateNone();
        EStatus UpdateRunning();
        EStatus UpdateLocking();
        EStatus UpdateSetPlayersToPlaying();
        EStatus UpdateUnlockFromError();
        EStatus UpdateDownloadingStats();
        EStatus UpdateLaunching();
        EStatus UpdateWaiting();
        EStatus UpdateSuspending();
        EStatus UpdateError();

        bool ServerInterfaceIdle();

        // Server-interface suspension-finished callback (BrnNetworkSuspensionManager
        // Suspend() invokes it once the in-game suspend completes). lpUserData is the
        // owning LaunchManager.
        static void SuspensionFinishedCallback(bool lbSuccess, void* lpUserData);

        UpdateFn          maUpdateFunctions[E_SUBSTATE_COUNT];   // +0x00 .. +0x24
        ESubState         meSubState;                            // +0x28
        BrnNetworkManager* mpNetworkManager;                     // +0x2C
        CgsSystem::Time   mTimeToStopWaiting;                    // +0x30
        s32               miNumPlayersSetPlaying;                // +0x38
    };
}

#endif // BRN_NETWORK_LAUNCH_MANAGER_H
