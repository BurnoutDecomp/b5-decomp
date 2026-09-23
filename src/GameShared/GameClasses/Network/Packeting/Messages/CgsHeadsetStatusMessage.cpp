#include "GameShared/GameClasses/Network/Packeting/Messages/CgsHeadsetStatusMessage.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

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
    namespace
    {
        // The headset status message type id.
        const s32 KI_E_MESSAGE_TYPE_HEADSET_STATUS = 10;
    }

    // The message base fields and no headset. (The voice manager's Construct inlines it.)
    Message* HeadsetStatusMessage::Construct()
    {
        Message::Construct();
        mu8HeadsetStatus = E_NETWORK_HEADSET_PLAYER_STATUS_NONE;
        return this;
    }

    // Stamp the status for sending on the frame; the message is unreliable.
    // (The voice manager's status sender inlines it.)
    void HeadsetStatusMessage::PrepareForSend(u16 lu16CurrentFrame, u8 lu8HeadsetStatus)
    {
        mu8HeadsetStatus = lu8HeadsetStatus;
        Message::PrepareForSend(KI_E_MESSAGE_TYPE_HEADSET_STATUS, lu16CurrentFrame);
        CGS_ASSERT(!IsReliable(), "!IsReliable()");
    }

    // Hand out a received status once and consume the message; false when none arrived.
    // (The voice manager's status receiver inlines it.)
    bool HeadsetStatusMessage::Retrieve(u8* lpu8HeadsetStatus)
    {
        if (!IsMessageValid())
        {
            return false;
        }

        SetMessageInvalid();
        *lpu8HeadsetStatus = mu8HeadsetStatus;
        CGS_ASSERT(!Message::IsMessageValid(), "!CgsNetwork::Message::IsMessageValid()");
        return true;
    }

    s32 HeadsetStatusMessage::GetPackedMessageSize()
    {
        mu8HeadsetStatus = 0;
        return Message::GetPackedMessageSize();
    }

    // The status byte travels as a u8 in [0, 3].
    PackOrUnpackResult HeadsetStatusMessage::PackOrUnpack()
    {
        return PackOrUnpackU8(this, &mu8HeadsetStatus, 0, 3);
    }
}
