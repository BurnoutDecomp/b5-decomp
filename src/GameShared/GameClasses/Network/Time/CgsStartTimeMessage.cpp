#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x827DE0E8
//   (CgsNetwork::StartTimeMessage::GetName)  ->  return "Start Time Message";

namespace CgsNetwork
{
    struct StartTimeMessage
    {
        const char* GetName();
    };

    const char* StartTimeMessage::GetName()
    {
        return "Start Time Message";
    }
}
