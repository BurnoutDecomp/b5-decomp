#include "types.hpp"
#include "GameShared/GameClasses/Network/Packeting/Messages/CgsHostKeepAliveMessage.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   (CgsNetwork::HostKeepAliveMessage)
//
//   GetPackedMessageSize @ 0x82882400:
//       return CgsNetwork::Message::GetPackedMessageSize(this);   // pure delegate
//
// GetName @ 0x827DBC48 is header-homed (inline) in CgsHostKeepAliveMessage.h.

namespace CgsNetwork
{
    s32 HostKeepAliveMessage::GetPackedMessageSize()
    {
        return Message::GetPackedMessageSize();
    }
}
