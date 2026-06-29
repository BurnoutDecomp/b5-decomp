#ifndef BRN_NETWORK_BUDDY_MANAGER_BASE_H
#define BRN_NETWORK_BUDDY_MANAGER_BASE_H

#include "types.hpp"
#include <cstddef>   // offsetof
#include "GameShared/GameClasses/Network/Buddies/DirtySock/X360/CgsBuddyManagerDirtySockX360.h"  // CgsNetwork::BuddyManagerX360 (real base)
#include "GameShared/GameClasses/System/Timer/CgsTime.h"                                          // CgsSystem::Time (embedded by value)
#include "GameSource/Network/Debug Components/BrnNetworkBuddyManagerDebugComponent.h"             // BrnNetwork::BuddyManagerDebugComponent (embedded by value), NetworkEventQueue
#include "GameSource/GameState/BrnCgsPlayerName.h"                                                // CgsNetwork::PlayerName (16B; the base buddy-query type)

// ===========================================================================
// BrnNetwork::BuddyManagerBase
//   Home: GameSource/Network/Managers/BrnNetworkBuddyManagerBase.{h,cpp}
//
// The Burnout (game-side) buddy manager. It extends the platform buddy manager
// CgsNetwork::BuddyManagerX360 (the DirtySock/HLB wrapper specialised for the
// Xbox 360 friends list) with the *server*-side buddy mirroring: it downloads
// the player's server friends list, diffs it against the live Xbox friends
// list, and uploads the add/remove deltas through the BrnNetwork server-
// interface custom-commands component. It also owns the in-game debug component
// and feeds buddy notifications into the network module's IN-event queue.
//
// Reconstructed from the X360 ARTIST asm spine (Construct @0x82553FD0 etc.)
// gated against the DecFIGS DWARF (references/DecFIGS/dwarfdump/.../
// BrnNetworkBuddyManagerBase.h). The DWARF prints the base as
// CgsNetwork::BuddyManagerPS3 -- that is the PS3 build's platform leaf; on the
// X360 ledger the platform leaf is CgsNetwork::BuddyManagerX360 (Construct
// calls CgsNetwork::BuddyManagerX360::Construct, Prepare/Release/Destruct/Update
// forward to the X360 leaf), so we derive from the X360 leaf here.
//
// MEMBER LAYOUT (X360-authoritative; offsets from `this`). The base X360 leaf
// ends at +0x13920 (80160); the derived members begin there. Every offset below
// is taken from a store/load immediate in the asm (Construct stores 0x14EB4..
// 0x14ECC; the body accessors index a1[21017..21426]). _AssertLayout() (never
// called) pins them so the gate keeps them honest.
//   +80160  mDebugComponent                         (BuddyManagerDebugComponent, 708B)
//   +80868  maServerBuddyList[100]                   (PlayerName, 16B each)
//   +82468  maBuddyListAtUpload[100]
//   +84068  maCachedBuddyListAtLastChange[100]
//   +85668  mTimeUntilRetryAfterFailedBuddyUpload    (CgsSystem::Time, 8B)
//   +85676  miNumServerBuddies                       (s32)
//   +85680  miNumBuddiesAtUpload                     (s32)
//   +85684  miCachedNumBuddiesAtLastChange           (s32; Construct stores 0)
//   +85688  meState                                  (EBuddyManagerState; Construct stores 0)
//   +85692  mpNetworkModule                          (BrnNetworkModule*; Construct stores a2)
//   +85696  mfTimeUntilRetryGetServerBuddies         (f32; Construct stores -1.0)
//   +85700  mfBuddyListChangedTimer                  (f32; Construct stores -1.0)
//   +85704  miNextBuddyToSend                        (s32; Construct stores 100)
//   +85708  mbOverwriteFriendsList                   (bool; Construct stores 0)
// ===========================================================================

namespace BrnNetwork
{
    class BrnNetworkModule;    // pointer-only member (full header pulls the network chain)
    class BrnServerInterface;  // Construct param + the mpServerInterface concrete view

    class BuddyManagerBase : public CgsNetwork::BuddyManagerX360
    {
    public:
        // DWARF BrnNetworkBuddyManagerBase.h:140 -- the server-mirroring state machine.
        enum EBuddyManagerState
        {
            E_BUDDY_MANAGER_STATE_FAILED                     = -1,
            E_BUDDY_MANAGER_STATE_START                      = 0,
            E_BUDDY_MANAGER_STATE_START_GET_SERVER_BUDDIES   = 1,
            E_BUDDY_MANAGER_STATE_GETTING_SERVER_BUDDIES     = 2,
            E_BUDDY_MANAGER_STATE_START_UPLOAD_SERVER_BUDDIES = 3,
            E_BUDDY_MANAGER_STATE_UPLOADING_SERVER_BUDDIES   = 4,
            E_BUDDY_MANAGER_STATE_SERVER_BUDDIES_UPDATED     = 5,
            E_BUDDY_MANAGER_STATE_COUNT                      = 6,
        };

        // The server buddy lists are sized by the shared CgsNetwork::KI_MAX_BUDDIES
        // (asserts "... < CgsNetwork::KI_MAX_BUDDIES" compare against 100).
        static const s32 KI_MAX_BUDDIES = 100;

        // ---- lifecycle ------------------------------------------------------
        void Construct(BrnNetworkModule* lpNetworkModule, BrnServerInterface* lpServerInterface);
        bool Prepare();
        bool Release();
        void Destruct();

        virtual CgsNetwork::EBuddyErrorCodes Disconnect();
        void Update(bool lbCanBlock, f32 lfTimeStep);

    protected:
        virtual void BuddyListHasChanged();

        // DWARF BrnNetworkBuddyManagerBase.h:99 (.cpp:910) -- drive the inbound post-sim
        // network-event queue (read events from the first queue, mirror them onto the
        // module's IN-event queue). VIRTUAL: the X360 leaf (BuddyManagerX360) overrides it
        // to additionally route the gamer-card "show profile" events through the Xbox Guide.
        // ADDITIVE GROW (BrnNetworkBuddyManagerX360 TU): the body is homed in this base's own
        // TU; declared here so the X360 override (which calls through this slot via the vtable)
        // matches the base virtual signature.
        virtual void ProcessNetworkQueue(const NetworkEventQueue* lpInQueue,
                                         NetworkEventQueue* lpOutQueue);

        BrnNetworkModule* GetNetworkModule() { return mpNetworkModule; }

        void ProcessEvent(const CgsModule::Event* lpEvent, s32 liEventType);
        void ProcessDebugEvents();
        void GameInviteAvailable(const PlayerName* lpBuddyName);

        // DWARF BrnNetworkBuddyManagerBase.h:90 (.cpp:335) -- begin a Live invite/join with the
        // launch parameters (taken by value, the 0x9C-byte InviteOrJoinParams block). The X360
        // leaf's DoInvite forwards the same params block here. ADDITIVE GROW (BrnNetworkBuddy-
        // ManagerX360 TU): declared-only here; the body is homed in this base's own TU.
        void StartInvite(BrnNetworkModuleIO::InviteOrJoinParams lInviteOrJoinParams);

    private:
        void SendEmptyBuddyInformation(NetworkEventQueue* lpEventQueue);
        void SendBuddyInformation(NetworkEventQueue* lpEventQueue);
        void GetServerFriends();
        void UploadServerFriends();
        void GetBuddiesToAddAndRemoveFromServer(CgsNetwork::PlayerName* lpaPlayerNamesToAdd,
                                                CgsNetwork::PlayerName* lpaPlayerNamesToRemove,
                                                s32* lpiNumToAdd, s32* lpiNumToRemove,
                                                bool* lpbOverwriteRequired);
        void HandleBuddyListChange();
        bool IsLocalPlayersBuddyListAlphabetical();

        void PopulateServerBuddyList(char* lpcServerData);

        // The DirtySock custom-commands completion callbacks (registered with the
        // server-interface; signature is CgsNetwork::CustomCommandCallback ==
        // void(*)(void* lpData, void* lpResult, bool lbSuccess); lpData is this).
        static void _GetFriendsListFromServerComplete(void* lpBuddyManager, void* lpResult, bool lbSuccess);
        static void _UploadFriendsToServerComplete(void* lpBuddyManager, void* lpResult, bool lbSuccess);

        // qsort comparator over PlayerName (X360 sub_8287DCF0 == LobbyNameCmp wrapper).
        static int _SortPlayerNames(const void* lpA, const void* lpB);

        // ---- members (offsets pinned by the asm; see header comment) ---------
        BuddyManagerDebugComponent mDebugComponent;                  // +80160
        CgsNetwork::PlayerName     maServerBuddyList[KI_MAX_BUDDIES]; // +80868
        CgsNetwork::PlayerName     maBuddyListAtUpload[KI_MAX_BUDDIES]; // +82468
        CgsNetwork::PlayerName     maCachedBuddyListAtLastChange[KI_MAX_BUDDIES]; // +84068
        CgsSystem::Time            mTimeUntilRetryAfterFailedBuddyUpload; // +85668 (8B)
        s32                        miNumServerBuddies;               // +85676
        s32                        miNumBuddiesAtUpload;             // +85680
        s32                        miCachedNumBuddiesAtLastChange;   // +85684
        EBuddyManagerState         meState;                          // +85688
        BrnNetworkModule*          mpNetworkModule;                  // +85692
        f32                        mfTimeUntilRetryGetServerBuddies; // +85696
        f32                        mfBuddyListChangedTimer;          // +85700
        s32                        miNextBuddyToSend;                // +85704
        bool                       mbOverwriteFriendsList;           // +85708

        static void _AssertLayout();
    };
}

#endif // BRN_NETWORK_BUDDY_MANAGER_BASE_H
