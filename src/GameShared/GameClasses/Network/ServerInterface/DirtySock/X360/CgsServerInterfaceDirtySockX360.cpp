#include "CgsServerInterfaceDirtySockX360.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsNetwork::ServerInterfaceDirtySockX360::Destruct @ 0x8288C950
//   CgsNetwork::ServerInterfaceDirtySockX360::Prepare  @ 0x828997C0
//   CgsNetwork::ServerInterfaceDirtySockX360::Resume   @ 0x8288C958
//   CgsNetwork::ServerInterfaceDirtySockX360::Suspend  @ 0x828940E0
//   CgsNetwork::ServerInterfaceDirtySockX360::Update   @ 0x828997C8
//
// Each function is a single tail-call branch to its ServerInterfaceDirtySock base
// counterpart, e.g.:
//     0x8288C950  b  CgsNetwork__ServerInterfaceDirtySock__Destruct
// i.e. the X360 leaf override is behaviourally identical to the base; MSVC emits a
// plain `b` (tail call) that re-uses the incoming `this` and (for Prepare/Suspend)
// the incoming argument register unchanged. Reproduced as explicit base-qualified
// forwarding calls.

namespace CgsNetwork
{
    ServerInterfaceDirtySockX360::ServerInterfaceDirtySockX360()
    {
    }

    void ServerInterfaceDirtySockX360::Destruct()
    {
        ServerInterfaceDirtySock::Destruct();
    }

    bool ServerInterfaceDirtySockX360::Prepare(ServerInterfacePrepareParams* lpParams)
    {
        return ServerInterfaceDirtySock::Prepare(lpParams);
    }

    void ServerInterfaceDirtySockX360::Resume()
    {
        ServerInterfaceDirtySock::Resume();
    }

    void ServerInterfaceDirtySockX360::Suspend(s32 liUpdateFlags)
    {
        ServerInterfaceDirtySock::Suspend(liUpdateFlags);
    }

    void ServerInterfaceDirtySockX360::Update()
    {
        ServerInterfaceDirtySock::Update();
    }
}
