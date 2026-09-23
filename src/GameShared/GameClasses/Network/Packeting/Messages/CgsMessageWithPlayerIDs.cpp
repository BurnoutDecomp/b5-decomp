#include "GameShared/GameClasses/Network/Packeting/Messages/CgsMessageWithPlayerIDs.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"   // CgsDev::Log::gpDebugPrint

// CgsNetwork::MessageWithPlayerIDs -- the two out-of-line id getters and the packed-size
// override. Each getter logs an unset (negative) id together with the message type before
// asserting it is valid.

namespace CgsNetwork
{

MessageWithPlayerIDs::NetworkPlayerID MessageWithPlayerIDs::GetSendingPlayerID() const
{
    if (mSendingPlayerID < 0)
    {
        *CgsDev::Log::gpDebugPrint << "We have received a message with sending player index "
                                   << mSendingPlayerID << ", the message is of type "
                                   << static_cast<s32>(mi8Type) << "\n";
    }
    CGS_ASSERT(mSendingPlayerID >= 0, "mSendingPlayerID >= 0");
    return mSendingPlayerID;
}

MessageWithPlayerIDs::NetworkPlayerID MessageWithPlayerIDs::GetRecvingPlayerID() const
{
    if (mRecvingPlayerID < 0)
    {
        *CgsDev::Log::gpDebugPrint << "We have received a message with recv player index "
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
    mSendingPlayerID = 0;
    mRecvingPlayerID = 0;
    return Message::GetPackedMessageSize();
}

}
