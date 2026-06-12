#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   (CgsNetwork::HostKeepAliveMessage)
//
//   GetPackedMessageSize @ 0x82882400:
//       return CgsNetwork::Message::GetPackedMessageSize(this);   // pure delegate
//   GetName @ 0x827DBC48:
//       return "Keep Host Alive Message";
//
// Both functions of this class are reconstructed together.

namespace CgsNetwork
{
    struct Message
    {
        int GetPackedMessageSize();
    };

    struct HostKeepAliveMessage : Message
    {
        int          GetPackedMessageSize();
        const char*  GetName();
    };

    int HostKeepAliveMessage::GetPackedMessageSize()
    {
        return Message::GetPackedMessageSize();
    }

    const char* HostKeepAliveMessage::GetName()
    {
        return "Keep Host Alive Message";
    }
}
