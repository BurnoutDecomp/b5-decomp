#include "GameSource/Network/Managers/BrnNetworkNotificationManagerBase.h"

// @ 0x825487C8  int __fastcall BrnNetwork::NetworkNotificationManagerBase::Update(int a1)
//
// X360 body is a single tail-call through the vtable:
//     return (*(*a1 + 8))(a1);
//   lwz r11, 0(r3) ; lwz r11, 8(r11) ; mtctr r11 ; bctr
// Slot +8 is virtual index 2 == ProcessNotifications (declaration order Connect, Disconnect,
// ProcessNotifications). So Update is just a forward to the virtual per-frame drain. The
// reconstruction issues the C++ virtual call by name rather than indexing the vtable, which is
// the faithful, ABI-portable equivalent of the X360 tail dispatch.
//
// Called by: BrnNetwork::BrnNetworkManager::ProcessAfterSimulation.

namespace BrnNetwork
{
    void NetworkNotificationManagerBase::Update()
    {
        ProcessNotifications();
    }
}
