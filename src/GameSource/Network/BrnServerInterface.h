#ifndef BRN_SERVER_INTERFACE_H
#define BRN_SERVER_INTERFACE_H

#include "types.hpp"
#include "GameSource/Network/BrnServerInterfaceBase.h"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceComponent.h"

namespace CgsNetwork
{
    class ServerInterfaceConnection;   // GetConnectionComponent() return type (pointer only)
    class ServerInterfaceGames;        // GetGameComponent() return type (pointer only)
}

// ===========================================================================
// BrnNetwork::BrnServerInterface
//   Home: GameSource/Network/BrnServerInterface.{h,cpp}
//
// The most-derived Burnout server-interface aggregate the game actually embeds
// (BrnNetworkManager::mServerInterface). It is BrnServerInterfaceBase plus ONE
// additional embedded server-interface component, proven by the X360 scalar
// deleting destructor @ 0x827E4090:
//
//   result[779] = &off_820CDBF8;   // 0xC2C  the EXTRA component this class adds
//   result[746] = &off_820CDBF8;   // 0xBA8 ] these eleven restores are
//   result[738] = &off_820CDBF8;   // 0xB88 ]   IDENTICAL to the Base dtor
//   ... (the eight further Base restores) ...
//   result[ 48] = &off_820CDBF8;   // 0x0C0 ]
//   *result     =  off_820CDBD0;   // the (shared) Base primary vtable slot
//   if ( flag & 1 ) operator delete(result);
//
// i.e. the derived teardown is exactly the Base teardown (@ 0x827E1710) with one
// extra component-vtable restore prepended at +0xC2C -- the by-value destructor of
// the one component declared here, which (being the last-declared member) is torn
// down first and so reinstalls its shared CgsNetwork::ServerInterfaceComponent base
// vtable (off_820CDBF8) before the inlined Base destructor runs. The compiler
// synthesises that whole walk + the conditional free from the trivial out-of-line
// virtual destructor; only the (empty) body is hand-written, matching the
// established deleting-destructor convention (see BrnServerInterfaceBase.cpp and
// BrnServerInterfaceDownloadableConfig.cpp). Defining the destructor out-of-line
// here anchors this class's deleting-destructor thunk to this TU.
//
// FLAGGED: the concrete game-specific TYPE/NAME of the one extra component is not
// named by any available dossier or DWARF -- only its existence and shared
// component-base vtable (off_820CDBF8 @ +0xC2C) are recovered from the asm. It is
// therefore modelled honestly as one embedded CgsNetwork::ServerInterfaceComponent
// (the exact polymorphic base whose vtable the destructor restores); no constant
// value is fabricated. If the concrete leaf type is recovered later, swap the
// member type for it -- the layout/teardown contract is unchanged.
//
// The X360 +0xC2C member offset is a 32-bit-pointer layout fact; it is NOT
// reproduced or static_asserted on a 64-bit host (vptr + pointers widen there, so
// the embedded slot lands at a different byte offset while the by-name member walk
// the compiler emits is identical).
//
// EStatus / GetStatus / IsSuspended / GetConnectionComponent are the public surface
// the committed callers drive (BrnNetworkManager / SuspensionManager); their bodies
// belong to this class's own (not-yet-homed) behavioural TUs and are declared-only
// here. Suspend/Resume are inherited virtuals from BrnServerInterfaceBase.
// ===========================================================================

namespace BrnNetwork
{
    class BrnServerInterface : public BrnServerInterfaceBase
    {
    public:
        // Mirrors CgsNetwork::ServerInterfaceDirtySock::EStatus (BUSY/ERROR/IDLE/COUNT);
        // GetStatus returns one of these for the queried component.
        enum EStatus
        {
            E_STATUS_BUSY = 0,
            E_STATUS_ERROR,
            E_STATUS_IDLE,
            E_STATUS_COUNT
        };

        BrnServerInterface();

        // Scalar deleting destructor @ 0x827E4090 (bodied in BrnServerInterface.cpp).
        virtual ~BrnServerInterface();

        // Public surface driven by the committed callers (bodies in this class's own TUs).
        EStatus GetStatus( s32 liComponent ) const;
        bool IsSuspended() const;
        CgsNetwork::ServerInterfaceConnection* GetConnectionComponent();

        // ADDITIVE GROW (BrnNetworkLaunchManager TU): the launch state machine drives the
        // games component and the per-component error state of the underlying DirtySock
        // interface. The X360 reaches these through the embedded ServerInterfaceDirtySock
        // (this+0x38E8/0x38F4 from the network manager): GetGameComponent returns the games
        // component pointer; GetLastError/ClearLastError read/clear the per-component last
        // error (liComponent == E_COMPONENTS_GAMES == 1 at every launch-manager call site).
        // Declared-only here; bodies live in this class's own (DirtySock) TUs.
        CgsNetwork::ServerInterfaceGames* GetGameComponent();
        s32  GetLastError( s32 liComponent ) const;
        void ClearLastError( s32 liComponent );

    private:
        // The one extra server-interface component this most-derived class adds over
        // BrnServerInterfaceBase (the +0xC2C off_820CDBF8 restore). See header note --
        // its concrete game leaf type is unrecovered, so it is modelled as the shared
        // component base whose vtable the destructor reinstalls.
        CgsNetwork::ServerInterfaceComponent mExtraComponent;
    };
}

#endif // BRN_SERVER_INTERFACE_H
