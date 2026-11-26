#include "GameShared/GameClasses/Network/Packeting/Messages/CgsMessageWithPlayerIDs.h"
#include "GameShared/GameClasses/Network/Packeting/Messages/CgsReliableMessage.h"
#include "GameShared/GameClasses/Network/Packeting/Messages/CgsTestConnectionMessage.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/CgsServerInterfaceDirtySock.h"

// CgsNetwork::MessageWithPlayerIDs -- the two out-of-line id getters and the packed-size
// override. Each getter logs an unset (negative) id together with the message type before
// asserting it is valid.

namespace CgsNetwork
{

// ARTIST 0x82541050 reports an unset sender through the network log.
MessageWithPlayerIDs::NetworkPlayerID MessageWithPlayerIDs::GetSendingPlayerID() const
{
    if (mSendingPlayerID < 0)
    {
        const EMessageType leMessageType = GetType();
        ServerInterfaceDirtySock::mNetStreamLogChannelOutput
            << "We have received a message with sending player index "
            << mSendingPlayerID << ", the message is of type "
            << static_cast<s32>(leMessageType) << "\n";
    }
    CGS_ASSERT(mSendingPlayerID >= 0, CGS_ASSERT_MESSAGE_AT(
        "mSendingPlayerID >= 0",
        "..\\..\\..\\GameShared\\GameClasses\\Network/Packeting/Messages/CgsMessageWithPlayerIDs.h", 147));
    return mSendingPlayerID;
}

MessageWithPlayerIDs::NetworkPlayerID MessageWithPlayerIDs::GetRecvingPlayerID() const
{
    if (mRecvingPlayerID < 0)
    {
        CgsNetwork::ServerInterfaceDirtySock::mNetStreamLogChannelOutput << "We have received a message with recv player index "
                                   << mRecvingPlayerID << ", the message is of type "
                                   << static_cast<s32>(mi8Type) << "\n";
    }
    CGS_ASSERT(mRecvingPlayerID >= 0, "mRecvingPlayerID >= 0");
    return mRecvingPlayerID;
}

// Zero both player ids (their packed width is fixed, so the value does not matter) and
// size the message through the base. The shipping build only has this inlined inside
// the ReliableMessage / TestConnectionMessage::GetPackedMessageSize fold.
s32 MessageWithPlayerIDs::GetPackedMessageSize()
{
    SetSendingPlayerID(0);
    SetRecvingPlayerID(0);
    return Message::GetPackedMessageSize();
}

// The three size overrides share the CgsNetworkUnity compilation scope in
// DecFIGS. The two derived bodies fold at ARTIST 0x828821F8.
s32 ReliableMessage::GetPackedMessageSize()
{
    SetU16Frame(0);
    return MessageWithPlayerIDs::GetPackedMessageSize();
}

s32 TestConnectionMessage::GetPackedMessageSize()
{
    return ReliableMessage::GetPackedMessageSize();
}

}
