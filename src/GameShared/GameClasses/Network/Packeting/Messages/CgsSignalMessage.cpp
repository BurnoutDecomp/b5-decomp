#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x827DDF40
//   (CgsNetwork::SignalMessage::GetName)  ->  return "Signal Message";

namespace CgsNetwork
{
    struct SignalMessage
    {
        const char* GetName();
    };

    const char* SignalMessage::GetName()
    {
        return "Signal Message";
    }
}
