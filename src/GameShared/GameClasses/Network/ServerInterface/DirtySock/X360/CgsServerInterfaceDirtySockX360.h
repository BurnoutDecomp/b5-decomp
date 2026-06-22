#ifndef CGS_SERVER_INTERFACE_DIRTY_SOCK_X360_H
#define CGS_SERVER_INTERFACE_DIRTY_SOCK_X360_H

#include "../CgsServerInterfaceDirtySock.h"

// ===========================================================================
// CgsNetwork::ServerInterfaceDirtySockX360
//   Home: GameShared/GameClasses/Network/ServerInterface/DirtySock/X360/
//         CgsServerInterfaceDirtySockX360.{h,cpp}
//
// The X360 (and PC-remaster) platform leaf of the DirtySock server-interface
// facade. CgsNetwork::ServerInterface derives from this; this derives from
// ServerInterfaceDirtySock (the PS3 build substitutes ServerInterfaceDirtySockPS3
// in the same slot -- confirmed by the CgsServerInterface.h DWARF
// `ServerInterface : public ServerInterfaceDirtySockPS3`).
//
// On X360 the five lifecycle virtuals are pure forwarding overrides: each is a
// tail-call branch (`b`) straight to the corresponding ServerInterfaceDirtySock
// base method (asm @ 0x8288C950 / 0x828997C0 / 0x8288C958 / 0x828940E0 /
// 0x828997C8). They exist only so the X360 leaf occupies its own vtable; they add
// no behaviour.
// ===========================================================================

namespace CgsNetwork
{
    struct ServerInterfacePrepareParams;

    struct ServerInterfaceDirtySockX360 : public ServerInterfaceDirtySock
    {
    public:
        ServerInterfaceDirtySockX360();

        virtual void Destruct();
        virtual bool Prepare(ServerInterfacePrepareParams* lpParams);
        virtual void Resume();
        virtual void Suspend(s32 liUpdateFlags);
        virtual void Update();
    };
}

#endif // CGS_SERVER_INTERFACE_DIRTY_SOCK_X360_H
