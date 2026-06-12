#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   (CgsNetwork::HeadsetStatusMessage)
//
//   GetPackedMessageSize @ 0x82882748:
//       *(this + 32) = 0;                                  // reset a base field
//       return CgsNetwork::Message::GetPackedMessageSize(this);
//   GetName @ 0x827DBC78:
//       return "Headset Status Message";
//
// Both functions of this class are reconstructed together. GetPackedMessageSize
// zeroes a 32-bit field in the Message base (offset 0x20) and delegates to the
// base implementation.

namespace CgsNetwork
{
    struct Message
    {
        u8   mPad[32];          // [0x00] opaque base fields
        u32  muField20;         // [0x20] zeroed before packing

        int GetPackedMessageSize();
    };

    struct HeadsetStatusMessage : Message
    {
        int          GetPackedMessageSize();
        const char*  GetName();
    };

    int HeadsetStatusMessage::GetPackedMessageSize()
    {
        muField20 = 0;
        return Message::GetPackedMessageSize();
    }

    const char* HeadsetStatusMessage::GetName()
    {
        return "Headset Status Message";
    }
}
