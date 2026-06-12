#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x827DE368
//   (CgsNetwork::ConnectionStatusMessage::GetName)  ->  return "Connection Status Message";

namespace CgsNetwork
{
    struct ConnectionStatusMessage
    {
        const char* GetName();
    };

    const char* ConnectionStatusMessage::GetName()
    {
        return "Connection Status Message";
    }
}
