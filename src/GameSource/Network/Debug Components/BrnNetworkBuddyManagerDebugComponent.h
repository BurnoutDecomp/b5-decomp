#ifndef BRN_NETWORK_BUDDY_MANAGER_DEBUG_COMPONENT_H
#define BRN_NETWORK_BUDDY_MANAGER_DEBUG_COMPONENT_H

#include "types.hpp"
#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugComponent.h"  // CgsDev::DebugComponent (real base)
#include "GameSource/Network/BrnNetworkModuleIO.h"                                  // BrnNetworkModuleIO::NetworkEventQueue
#include "GameSource/Network/SharedIO/BrnNetworkSharedIO.h"                         // BrnNetwork::PlayerName (16B), BrnNetwork::Event

// ===========================================================================
// BrnNetwork::BuddyManagerDebugComponent
//   Home: GameSource/Network/Debug Components/BrnNetworkBuddyManagerDebugComponent.{h,cpp}
//
// The in-game debug-menu component for the buddy / friends-list subsystem. It derives from the
// real CgsDev::DebugComponent and is embedded BY VALUE in BrnNetwork::BuddyManagerBase
// (mDebugComponent, DWARF BrnNetworkBuddyManagerBase.h:156), so this header must give the
// complete object layout.
//
// Each "action" menu item is wired to a static debug callback (DebugCallbackFunction ==
// void(*)(void*); the void* user-data IS this component). Every callback builds a small
// network-IN event and pushes it onto the buddy manager's NetworkEventQueue; the manager's
// ProcessEvent later dispatches it (the network-IN event tags are the DWARF NetworkEvent<N>
// leaves in BrnNetworkInEventTypeDefs.h -- see the cpp).
//
// LAYOUT (X360-AUTHORITATIVE; offsets from `this`; the base CgsDev::DebugComponent sub-object
// occupies +0x00..+0x0B). Pinned from the X360 ARTIST Construct stores (@0x82585648) and every
// member access in this TU's bodies (the BuddyInformation stride is 0x84 == 132, the index/name
// members land exactly after the 5-element array):
//   mBuddyInformation[5]  +0x000C  BuddyInformation (DWARF h:101; stride 132; the JoinBuddy/
//                                   SendInvite/etc memcpys read &mBuddyInformation[i].mPlayerName,
//                                   i.e. this+0x18 == array base +12 == the PlayerName slot).
//   miBuddyIndex          +0x02A0  s32  (a1[168]; debug-menu variable; selects the array slot).
//   miMessageIndex        +0x02A4  s32  (a1[169]; PrintMessage stores it in the chat-get event).
//   miFeedbackIndex       +0x02A8  s32  (a1[170]; LeaveFeedback stores it as the feedback type).
//   mInviteBuddy          +0x02AC  PlayerName (16B; AcceptInvite asserts !IsEmpty + memcpys it).
//   mpEventQueue          +0x02BC  NetworkEventQueue*  (a1[175]; the manager's IN-event queue).
//   mpBuddyManager        +0x02C0  BrnNetwork::BuddyManagerBase*  (a2 stored in Construct).
//
// The three index members are registered with the debug menu as s32 variables; they are read as
// raw s32 at the call sites (same idiom as the sibling network debug components).
// ===========================================================================

namespace BrnNetwork
{
    class BuddyManagerBase;   // back-pointer member only (pointer-only use; its full header embeds
                              // this component by value, so including it would cycle).

    class BuddyManagerDebugComponent : public CgsDev::DebugComponent
    {
    public:
        // @ 0x82585648 -- base Destruct (reset) then store the manager back-pointer + clear members.
        void Construct(BuddyManagerBase* lpBuddyManager);
        bool Prepare();    // @ 0x825856A8 -- clear members, return true.
        bool Release();    // drop the menu surface, clear members, free the event queue.
        void Destruct();   // @ 0x825856D0 -- clear members + manager back-pointer, base Destruct.

        // The manager's IN-event queue the menu actions post to (header-inline on the console:
        // the buddy manager reads the pointer straight off the component).
        BrnNetworkModuleIO::NetworkEventQueue* GetEventQueue() { return mpEventQueue; }

        // Log the buddy events the manager has just produced (the buddy count, the buddy details,
        // message and invite notifications) and pick up the latest buddy details for the menu.
        void ProcessOutgoingEvents(BrnNetworkModuleIO::NetworkEventQueue* lpEventQueue);

        // SetGameInviteBuddy(const PlayerName*), GetPath() and OnActivate() also belong to this
        // class; they are not reconstructed here.

    protected:
        const char* GetName() const override;   // @ 0x825856F0 -> "Buddies"

    private:
        // ---- static debug-menu action callbacks (registered with the debug menu; lpData is this) ----
        // Each posts one network-IN event onto mpEventQueue (event tag + byte size are X360-authoritative,
        // taken from the `li r5,<type>` / `li r6,<size>` immediates -- see the cpp).
        static void GetBuddyCount(void* lpData);             // @ 0x82594780  (NetworkEvent<1>,  size 1)
        static void PrintBuddyInfo(void* lpData);            // @ 0x825947B0  (NetworkEvent<2>,  size 1)
        static void PrintMessage(void* lpData);              // @ 0x825947E0  (NetworkEvent<3>,  size 20)
        static void PrintNextUnreadMessage(void* lpData);    // @ 0x82594840  (NetworkEvent<5>,  size 16)
        static void SendTestMessage(void* lpData);           // @ 0x82594898  (NetworkEvent<4>,  size 216)
        static void SendInvite(void* lpData);                // @ 0x82594908  (NetworkEvent<8>,  size 16)
        static void AcceptInvite(void* lpData);              // @ 0x82594960  (NetworkEvent<10>, size 16)
        static void JoinBuddy(void* lpData);                 // @ 0x825949F8  (NetworkEvent<12>, size 16)
        static void LeaveFeedback(void* lpData);             // @ 0x82594A58  (NetworkEvent<7>,  size 20)
        static void ForceServerFriendsOverwrite(void* lpData); // @ 0x82591B78

        // ---- member layout (see header comment) ----
        // BuddyInformation is the network-IO buddy record (DWARF BrnNetworkSharedIO.h:599; mInviteStatus,
        // miTotalMessages, miUnreadMesssages, PlayerName mPlayerName, 4 bools, char[100] presence; 132B).
        // Its full home is BrnNetworkSharedIO.h; only the byte stride + the embedded PlayerName slot are
        // load-bearing for this TU, so the record is reserved as a fixed-stride buffer and the one field
        // this TU reads (the embedded PlayerName at +12 of each record) is reached via GetBuddyName().
        struct BuddyInformation
        {
            s32        miInviteStatus;     // +0x00  EInviteStatus
            s32        miTotalMessages;    // +0x04
            s32        miUnreadMessages;   // +0x08
            PlayerName mPlayerName;        // +0x0C  (the slot the action callbacks memcpy, this+0x18)
            bool       mbIsFullBuddy;      // +0x1C
            bool       mbIsOnline;         // +0x1D
            bool       mbIsInSameLobby;    // +0x1E
            bool       mbIsJoinable;       // +0x1F
            char       macPresenceData[100]; // +0x20 .. +0x84
        };

        static const s32 KI_MAX_BUDDIES = 5;   // DWARF h:101 array bound; pins miBuddyIndex @ +0x2A0.

        BuddyInformation   mBuddyInformation[KI_MAX_BUDDIES];   // +0x000C
        s32                miBuddyIndex;                        // +0x02A0
        s32                miMessageIndex;                      // +0x02A4
        s32                miFeedbackIndex;                     // +0x02A8
        PlayerName         mInviteBuddy;                        // +0x02AC
        BrnNetworkModuleIO::NetworkEventQueue* mpEventQueue;    // +0x02BC
        BuddyManagerBase*  mpBuddyManager;                      // +0x02C0
    };
}

#endif // BRN_NETWORK_BUDDY_MANAGER_DEBUG_COMPONENT_H
