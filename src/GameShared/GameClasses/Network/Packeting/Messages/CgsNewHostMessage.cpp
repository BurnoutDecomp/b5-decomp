#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x827DBC58
//   (CgsNetwork::NewHostMessage::GetName)
//
// Behaviour-faithful to the X360 pseudocode:
//     return "New Host Message";

namespace CgsNetwork
{
    struct NewHostMessage
    {
        const char* GetName();
    };

    const char* NewHostMessage::GetName()
    {
        return "New Host Message";
    }
}
