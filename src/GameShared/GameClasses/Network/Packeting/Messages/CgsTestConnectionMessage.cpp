#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x827DE358
//   (CgsNetwork::TestConnectionMessage::GetName)  ->  return "Test Connection Message";

namespace CgsNetwork
{
    struct TestConnectionMessage
    {
        const char* GetName();
    };

    const char* TestConnectionMessage::GetName()
    {
        return "Test Connection Message";
    }
}
