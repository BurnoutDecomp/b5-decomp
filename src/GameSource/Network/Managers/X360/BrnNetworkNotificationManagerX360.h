#pragma once

// ===================================================================================
// BrnNetwork::NetworkNotificationManagerX360 -- owning header
//   b5-decomp/src/GameSource/Network/Managers/X360/BrnNetworkNotificationManagerX360.{h,cpp}
//
// The Xbox-360 concrete online notification manager owned by BrnNetworkManager. It is the
// platform leaf of the notification-manager hierarchy:
//
//   BrnNetwork::NetworkNotificationManagerBase  (BrnNetworkNotificationManagerBase.h)
//     <- BrnNetwork::NetworkNotificationManagerX360  (this type)
//
// The base supplies the Construct/Prepare/Release/Destruct/Update lifecycle and the virtual
// Connect/Disconnect/ProcessNotifications set; each concrete leaf redeclares its own non-virtual
// Prepare()/Release() (distinct X360 addresses, called directly by BrnNetworkManager) and
// overrides the private virtual ProcessNotifications() to drain its platform notification source.
// On X360 that source is the Xbox XNotify listener. This mirrors the committed LoginManagerX360
// / BuddyManagerX360 / GamerCardManagerX360 X360 leaves.
//
// MEMBER LAYOUT (X360-authoritative). The base ends at +0x08 (vptr @ +0x00, mpNetworkModule
// @ +0x04); this leaf adds a single member just past it:
//   +0x08  mhNotifier  void*   (the XNotifyCreateListener listener handle)
// Grounded: Prepare stores XNotifyCreateListener's result at *(this+8); Release CloseHandles
// *(this+8) and zeroes it; ProcessNotifications reads *(this+8) as the XNotifyGetNext handle.
// The X360 byte offset is a 32-bit-pointer/Xenon layout fact; on a 64-bit host the base
// sub-object and its pointer member widen, so the absolute offset is NOT static_asserted --
// members are accessed BY NAME.
//
// Original home (from the Prepare assert-string rodata):
//   ..\..\..\GameSource\Network/Managers/X360/BrnNetworkNotificationManagerX360.cpp
// ===================================================================================

#include "types.hpp"
#include "GameSource/Network/Managers/BrnNetworkNotificationManagerBase.h"  // real base

namespace BrnNetwork
{
    class BrnNetworkModule;

    class NetworkNotificationManagerX360 : public NetworkNotificationManagerBase
    {
    public:
        // ---- lifecycle (non-virtual; called directly by BrnNetworkManager) -----------------
        // X360 0x8254C6E0 -- create the Xbox notification listener (qwAreas = 1) and stash it.
        bool Prepare();
        // X360 0x8254C740 -- CloseHandle the listener and clear the handle.
        bool Release();

    private:
        // X360 0x8254C780 -- drain the XNotify queue; on a system sign-in change affecting the
        // local user's controller port, clear the manager's cached sign-in flag. Overrides the
        // base pure hook (vtable idx 2). Matches the base's private virtual declaration.
        virtual void ProcessNotifications() /*override*/;

        // ---- data members (X360 offset in the note) ---------------------------------------
        // +0x08 -- XNotifyCreateListener listener handle. Corpus convention is a plain void*
        // (mirrors BuddyManagerX360::mhNotifier / GamerCardManagerX360::mNotifyListener); the
        // Xbox HANDLE typedef is not modelled in types.hpp.
        void* mhNotifier;   // +0x08
    };
}
