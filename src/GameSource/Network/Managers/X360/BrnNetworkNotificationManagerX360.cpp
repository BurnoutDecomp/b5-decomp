#include "GameSource/Network/Managers/X360/BrnNetworkNotificationManagerX360.h"
#include "GameSource/Network/BrnNetworkModule.h"    // BrnNetworkModule::GetNetworkManager
#include "GameSource/Network/BrnNetworkManager.h"   // GetLocalUserControllerPort / ClearLocalUserSignedInFlag
#include "GameShared/GameClasses/Core/CgsAssert.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnNetwork::NetworkNotificationManagerX360::Prepare              @ 0x8254C6E0
//   BrnNetwork::NetworkNotificationManagerX360::Release              @ 0x8254C740
//   BrnNetwork::NetworkNotificationManagerX360::ProcessNotifications @ 0x8254C780
//
// The Xbox XNotify listener plumbing. The XNotify* / CloseHandle entry points are the Xbox 360
// XDK C-API; declared extern "C" at file scope with plain types (void* / int / unsigned long),
// mirroring the committed BuddyManagerX360.cpp / GamerCardManagerX360.cpp precedent -- the Xbox
// HANDLE / DWORD / ULONG_PTR typedefs are NOT modelled in types.hpp.
extern "C"
{
    void* XNotifyCreateListener(unsigned long long luqwAreas);
    int   XNotifyGetNext(void* lhListener, unsigned long ludwMsgFilter,
                         unsigned long* lpdwId, unsigned long* lpParam);
    int   CloseHandle(void* lhObject);
}

namespace BrnNetwork
{
    // XN_SYS_SIGNINCHANGED notification id (asm compares the XNotifyGetNext id against 14).
    static const u32 KU_XN_SYS_SIGNINCHANGED = 14;

    // X360 0x8254C6E0 -- create the Xbox notification listener for the system areas
    // (qwAreas = 1, XNOTIFY_SYSTEM) and stash the handle. On failure the create is asserted.
    bool NetworkNotificationManagerX360::Prepare()
    {
        mhNotifier = XNotifyCreateListener(1);   // li r3, 1 ; bl XNotifyCreateListener ; stw r3, 8(this)
        CGS_ASSERT(mhNotifier != nullptr, "mhNotifier");
        return true;                             // li r3, 1
    }

    // X360 0x8254C740 -- close the notification listener and clear the handle.
    bool NetworkNotificationManagerX360::Release()
    {
        CloseHandle(mhNotifier);   // lwz r3, 8(this) ; bl CloseHandle
        mhNotifier = nullptr;      // li r11, 0 ; stw r11, 8(this)
        return true;               // li r3, 1
    }

    // X360 0x8254C780 -- drain the Xbox notification queue. The only notification handled is the
    // system sign-in change (XN_SYS_SIGNINCHANGED, id 14): its parameter is a bitmask of the user
    // indices whose sign-in state changed; if the bit for the local user's controller port is set,
    // the manager's cached local-user sign-in flag (byte @ manager +0x3D0CC) is cleared.
    void NetworkNotificationManagerX360::ProcessNotifications()
    {
        unsigned long luId    = 0;   // var_2C (pdwId)
        unsigned long luParam = 0;   // var_30 (pParam)
        while (XNotifyGetNext(mhNotifier, 0, &luId, &luParam))
        {
            if (luId == KU_XN_SYS_SIGNINCHANGED)
            {
                BrnNetworkManager* lpNetworkManager = mpNetworkModule->GetNetworkManager();
                if (((1u << lpNetworkManager->GetLocalUserControllerPort()) & luParam) != 0)
                {
                    // stbx r29(=0), r3, r30(=0x3D0CC) -- second GetNetworkManager() call in the asm.
                    mpNetworkModule->GetNetworkManager()->ClearLocalUserSignedInFlag();
                }
            }
        }
    }
}
