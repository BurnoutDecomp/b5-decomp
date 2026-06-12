#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x827DBC68
//   (CgsNetwork::SyncTimeMessage::GetName)  ->  return "Sync Time Message";

namespace CgsNetwork
{
    struct SyncTimeMessage
    {
        const char* GetName();
    };

    const char* SyncTimeMessage::GetName()
    {
        return "Sync Time Message";
    }
}
