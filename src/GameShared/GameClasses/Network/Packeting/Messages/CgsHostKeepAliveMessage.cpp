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

    // Stamp the frame, set the type and mark the message valid (the base PrepareForSend,
    // inlined with its constant type).
    void HostKeepAliveMessage::PrepareForSend(u16 lu16Frame)
    {
        Message::PrepareForSend(KI_E_MESSAGE_TYPE_HOST_KEEP_ALIVE, lu16Frame);
    }

    // A keep-alive carries no payload: nothing to serialise, success (the console
    // folds this `return 0` leaf with every other identical leaf in the image).
    PackOrUnpackResult HostKeepAliveMessage::PackOrUnpack()
    {
        return KX_PACK_OR_UNPACK_SUCCESS;
    }
}
