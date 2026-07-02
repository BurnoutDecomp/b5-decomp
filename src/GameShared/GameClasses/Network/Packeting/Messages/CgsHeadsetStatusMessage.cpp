#include "GameShared/GameClasses/Network/Packeting/Messages/CgsHeadsetStatusMessage.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   (CgsNetwork::HeadsetStatusMessage)
//
//   GetPackedMessageSize @ 0x82882748:
//       *(this + 0x20) = 0;   // stb -- byte store, one byte only
//       return CgsNetwork::Message::GetPackedMessageSize(this);
//
// sizeof(CgsNetwork::Message) rounds up to 0x20 (the base's last field,
// mu16Frame, ends at 0x1C+2 == 0x1E; the base's 4-byte alignment pads it out
// to 0x20). That makes offset 0x20 the derived class's own first byte field
// -- mu8HeadsetStatus (DWARF CgsHeadsetStatusMessage.h:73) -- not a Message
// field. GetPackedMessageSize resets it to 0 before delegating to the base.

namespace CgsNetwork
{
    s32 HeadsetStatusMessage::GetPackedMessageSize()
    {
        mu8HeadsetStatus = 0;
        return Message::GetPackedMessageSize();
    }
}
