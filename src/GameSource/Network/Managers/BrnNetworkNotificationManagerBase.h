// ---- GameSource/Network/Managers/BrnNetworkNotificationManagerBase.h ----
// Member names, ordering, virtual set and accessor signatures recovered from the DecFIGS
// DWARF for this exact header
// (references/DecFIGS/dwarfdump/GameSource/Network/Managers/BrnNetworkNotificationManagerBase.h),
// gated against the X360 binary (BrnNetwork::NetworkNotificationManagerBase::Update @ 0x825487C8).
//
// Layout / dispatch proof for Update (the only function homed in this TU):
//   X360:  return (*(*a1 + 8))(a1);
//          0x825487C8  lwz r11, 0(r3)    ; r11 = vptr (object +0)
//          0x825487CC  lwz r11, 8(r11)   ; r11 = vtable slot at byte +8 (4-byte X360 slots
//                                        ;       -> virtual index 2)
//          0x825487D0  mtctr r11
//          0x825487D4  bctr              ; tail-call that virtual with `this` (a1)
//   Virtual declaration order in this class is Connect (idx 0), Disconnect (idx 1),
//   ProcessNotifications (idx 2). Slot +8 == index 2 == ProcessNotifications, so Update is a
//   plain forward to the virtual ProcessNotifications(). The X360 4-byte slot stride is an
//   ABI detail; on the host the compiler lays out its own vtable, and the C++ virtual call
//   `ProcessNotifications()` is the faithful, ABI-portable reconstruction (no raw vtable
//   indexing). The +8 fact is documentation, not asserted.
#pragma once

#include "types.hpp"

namespace BrnNetwork
{
    // Uncommitted dependency -- declared-only (used only in declared-only signatures and as a
    // stored pointer; never dereferenced in this header). Promote to its real home when the
    // BrnNetworkModule TU is reconstructed.
    class BrnNetworkModule;

    // BrnNetworkNotificationManagerBase.h:43 (DWARF).
    // Abstract base for the online notification managers: holds a back-pointer to the network
    // module and routes per-frame work through the virtual ProcessNotifications() hook.
    class NetworkNotificationManagerBase
    {
    public:
        // BrnNetworkNotificationManagerBase.cpp:54 / :71 / :88 / :102 -- bodies link from this
        // base class's own (class-sourced) TU.
        void Construct( BrnNetworkModule* lpNetworkModule );
        bool Prepare();
        bool Release();
        void Destruct();

        // Virtual set, in vtable order (idx 0,1,2). Connect/Disconnect are overridable
        // connect-state hooks; ProcessNotifications is the per-frame drain implemented by each
        // concrete manager. Declared here, bodied in the derived/own TUs.
        virtual void Connect();              // vtable idx 0
        virtual void Disconnect();           // vtable idx 1

        // === Reconstructed in this TU ===
        // @ 0x825487C8 -- per-frame entry: forward to the virtual ProcessNotifications().
        void Update();

    protected:
        // BrnNetworkNotificationManagerBase.cpp:129 -- own TU.
        void OnConnectionSignOut();

        // BrnNetworkNotificationManagerBase.h:81 (DWARF). 32-bit pointer on X360; widened to a
        // host pointer here (access by name only; no absolute offset is relied upon).
        BrnNetworkModule* mpNetworkModule;

    private:
        // BrnNetworkNotificationManagerBase.cpp:143 -- vtable idx 2. Pure per-derived hook;
        // the base leaves it abstract (each concrete notification manager drains its own queue).
        virtual void ProcessNotifications() = 0;
    };
}
