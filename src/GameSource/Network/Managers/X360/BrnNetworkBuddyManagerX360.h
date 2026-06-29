#pragma once

// ===================================================================================
// BrnNetwork::BuddyManagerX360 -- owning header
//   b5-decomp/src/GameSource/Network/Managers/X360/BrnNetworkBuddyManagerX360.{h,cpp}
//
// The GAME-side (BrnNetwork) Xbox-360 buddy manager owned by BrnNetworkManager. It is the
// X360 leaf of the game-side buddy-manager chain:
//
//   CgsNetwork::BuddyManagerBase        (DirtySock wrapper, CgsBuddyManagerDirtySock.h)
//     <- CgsNetwork::BuddyManagerX360   (platform leaf,     CgsBuddyManagerDirtySockX360.h)
//        <- BrnNetwork::BuddyManagerBase (game server-mirroring, BrnNetworkBuddyManagerBase.h)
//           <- BrnNetwork::BuddyManagerX360 (this type)
//
// On top of the game-side server-buddy mirroring done by BrnNetwork::BuddyManagerBase, this
// leaf adds the Xbox-LIVE cross-title invite/join handling: it owns a Live notification
// listener (XNotifyCreateListener), watches it for accepted-invite notifications
// (XNotifyGetNext / XInviteGetAcceptedInfo), and -- when the local console is invited into a
// remote Burnout session it is not already in -- launches the invite/join through the Xbox
// Guide (XSetLaunchData) and the game-side invite pipeline (BuddyManagerBase::StartInvite).
// It also routes the network-queue "show profile" events through the platform leaf's
// ShowProfile (CgsNetwork::BuddyManagerX360::ShowProfile) by overriding ProcessNetworkQueue.
//
// MEMBER LAYOUT (X360-authoritative; absolute byte offsets from `this`). The game base
// BrnNetwork::BuddyManagerBase ends at +85709 (its mbOverwriteFriendsList @ +85708); the X360
// leaf members begin just past it. Every offset below is a store/load immediate in the asm
// (Construct stores 0x14ED0/0x14ED4/0x14ED8; Prepare/Update access the same three).
//   +85712 (0x14ED0)  mhNotifier              void*  (XNotifyCreateListener HANDLE)
//   +85716 (0x14ED4)  miPendingInviteUserPort s32    (the requesting controller port latched by
//                                                     Prepare from soft-reboot data; 0 == none)
//   +85720 (0x14ED8)  macPendingSessionID[128] char  (the latched soft-reboot session ID; its
//                                                     leading byte doubles as the "pending" flag)
//
// The X360 byte offsets are 32-bit-pointer/Xenon layout facts; on a 64-bit host the pointer +
// the base sub-object widen, so the absolute offsets are NOT static_asserted -- members are
// accessed BY NAME. _AssertLayout() pins the recovered RELATIONSHIPS (the three leaf members'
// order/spacing), mirroring the sibling CgsBuddyManagerDirtySockX360::_AssertLayout.
//
// Original home (from the asm assert strings):
//   ..\..\..\GameSource\Network/Managers/X360/BrnNetworkBuddyManagerX360.cpp
// ===================================================================================

#include "types.hpp"
#include <cstddef>   // offsetof
#include "GameSource/Network/Managers/BrnNetworkBuddyManagerBase.h"  // BrnNetwork::BuddyManagerBase (real base)
#include "GameSource/Network/SharedIO/BrnNetworkSharedIO.h"          // PlayerName, BrnNetworkModuleIO::InviteOrJoinParams

namespace BrnNetwork
{
    namespace BrnNetworkModuleIO { struct PostSimulationInputBuffer; class NetworkEventQueue; }

    // The launch / soft-reboot data block handed to Prepare. Only the two fields the X360
    // Prepare reads are modelled (the requesting controller port @ +0x00 and the session-ID
    // string @ +0x14); the remainder of the block is owned by its own (not-yet-homed) type, so
    // the unread span is reserved as opaque storage rather than fabricated. FLAGGED: the block's
    // full layout/home is unrecovered in this TU; only the two grounded fields are named.
    struct BuddySoftRebootData
    {
        s32  miUserControllerPort;   // +0x00 (Prepare: this->miPendingInviteUserPort = lpData[0])
        u8   maPad04[0x14 - 0x04];   // +0x04 .. +0x14 (opaque)
        char macSessionID[128];      // +0x14 (Prepare: strncpy(macPendingSessionID, &lpData[0x14], 128))
    };

    class BrnServerInterface;

    class BuddyManagerX360 : public BuddyManagerBase
    {
    public:
        // X360 0x825579F0 -- chain the game base Construct (forwarding the module + server
        // interface, which pass straight through in r4/r5), create the Live notification listener
        // (XNotifyCreateListener for the system + 'live' areas), and clear the pending-invite
        // latch. Called by BrnNetworkManager::Construct.
        void Construct(BrnNetworkModule* lpNetworkModule, BrnServerInterface* lpServerInterface);

        // X360 0x82557A78 -- close the Live notification listener, clear the pending-invite latch,
        // then chain the game base Destruct. Called by BrnNetworkManager::Destruct.
        void Destruct();

        // X360 0x8254C818 -- chain the game base Prepare; on success latch the soft-reboot
        // invite (controller port + session ID) so the next Update can re-issue it. Returns the
        // base Prepare result. Called by BrnNetworkManager::Prepare.
        bool Prepare(const BuddySoftRebootData* lpSoftRebootData);

        // X360 0x8254C978 -- chain the game base Release. Called by BrnNetworkManager::Release.
        bool Release();

        // X360 0x82573A40 -- the per-frame post-simulation tick. Chains the game base Update,
        // pumps the inbound network queue through ProcessNetworkQueue, processes debug events,
        // then (when invites may be acted on) either re-issues a latched soft-reboot invite or
        // polls the Live notification listener for a freshly accepted cross-title invite and acts
        // on it. Called by BrnNetworkManager::ProcessAfterSimulation. lbProcessInvites (r6) gates
        // the invite handling; lbCanBlock (r7) + lfTimeStep (f1) forward to the base Update.
        u32 Update(BrnNetworkModuleIO::PostSimulationInputBuffer* lpInputBuffer,
                   bool lbProcessInvites, bool lbCanBlock, f32 lfTimeStep);

    protected:
        // X360 0x8256C8E0 -- override: walk the inbound network-event queue, routing the buddy
        // "show profile" event (type 13) through the platform leaf's ShowProfile and the
        // online/offline/join events (types 8/9/11) into a "buddy list updated" poke, then defer
        // to the game base ProcessEvent for every event.
        virtual void ProcessNetworkQueue(const NetworkEventQueue* lpInQueue,
                                         NetworkEventQueue* lpOutQueue) override;

    private:
        // X360 0x825700A8 -- begin a Live invite/join for the given launch parameters. If the
        // target user is not already in the named game session (or is, but is not the host),
        // optionally stamp the Guide launch data (when the invite targets the local user) and
        // forward to BuddyManagerBase::StartInvite; otherwise post the appropriate
        // already-in-session network event. Takes the params by value (the 0x9C-byte block).
        void DoInvite(BrnNetworkModuleIO::InviteOrJoinParams lInviteOrJoinParams);

        // X360 0x8256C718 -- true if the local game's current session ID matches lpcSessionID
        // (read the game's params off the games server-interface component, strnicmp the session
        // strings). Returns false when not in a game.
        bool IsUserInGameSession(const char* lpcSessionID);

        // -- members (offsets pinned in the header note) -------------------------------------
        void* mhNotifier;                  // +85712 (0x14ED0)
        s32   miPendingInviteUserPort;     // +85716 (0x14ED4)
        char  macPendingSessionID[128];    // +85720 (0x14ED8)

        static void _AssertLayout();
    };
}
