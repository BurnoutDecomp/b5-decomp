#pragma once
#include "GameShared/GameClasses/Network/CgsNetworkVersionDisplay.h"

namespace CgsPC::Debug
{
    // FLAG PC-platform leaf: this recovered component intentionally has no
    // OnActivate controls; its RenderHUD is the network build banner. Keep
    // both automatic and explicit activation available without inventing a menu.
    inline bool HasHudWithoutMenu(const CgsDev::DebugComponent* lpComponent)
    {
        return dynamic_cast<const CgsNetwork::VersionDisplay*>(lpComponent) != nullptr;
    }
}
