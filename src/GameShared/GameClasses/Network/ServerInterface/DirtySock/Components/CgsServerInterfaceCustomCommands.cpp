#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceCustomCommands.h"

// The platform-shared custom-commands component base. Every lifecycle step here is
// trivial: the Burnout leaf (BrnNetwork::ServerInterfaceCustomCommands) owns the state.
// On the console these bodies survive only as identical-code folds shared with other
// trivial functions:
//   Construct -- the component-base reset (the leaf's Construct opens with it);
//   Prepare / Release -- the shared "return true" body the leaf gates on;
//   Destruct / OnEvent -- the shared empty body (the leaf's vtable OnEvent slot and the
//                         call BrnServerInterfaceBase::Destruct makes after the leaf reset).

namespace CgsNetwork
{
    void ServerInterfaceCustomCommands::Construct()
    {
        ServerInterfaceComponent::Construct();
    }

    bool ServerInterfaceCustomCommands::Prepare()
    {
        return true;
    }

    bool ServerInterfaceCustomCommands::Release()
    {
        return true;
    }

    void ServerInterfaceCustomCommands::Destruct()
    {
    }

    void ServerInterfaceCustomCommands::OnEvent(EServerInterfaceEvent /*leEvent*/, void* /*lpData*/)
    {
    }
}
