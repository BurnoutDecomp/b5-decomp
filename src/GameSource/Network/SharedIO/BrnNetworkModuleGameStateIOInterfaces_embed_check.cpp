// Embed check: the owning header must compile standalone and the interface must be embeddable
// by value (verifies the queue/mapping/flags layout closes cleanly with no incomplete types).
#include "GameSource/Network/SharedIO/BrnNetworkModuleGameStateIOInterfaces.h"

namespace
{
    struct EmbedCheck
    {
        BrnNetwork::BrnNetworkModuleIO::GameStateToNetworkInterface mInterface;
        BrnNetwork::BrnNetworkModuleIO::NetworkPlayerMappingData    mRow;
    };

    EmbedCheck gEmbedCheck;
}
