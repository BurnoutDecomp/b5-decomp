#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"   // CGS_ASSERT at ARTIST locations

#include "GameShared/GameClasses/Network/Packeting/Messages/CgsTestConnectionMessage.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/CgsServerInterfaceDirtySock.h"
#define CGS_ARTIST_FILE "..\\..\\..\\GameShared\\GameClasses\\Network\\Packeting\\Messages\\CgsTestConnectionMessage.cpp"

// CgsNetwork::TestConnectionMessage: PrepareForSend and PackOrUnpack.
// (GetName is inline in the header.)

namespace CgsNetwork
{
    // Stamp a reliable test-connection message (message type 4) for lu16Frame.
    void TestConnectionMessage::PrepareForSend(u16 lu16Frame)
    {
        ServerInterfaceDirtySock::mNetStreamLogChannelOutput << "Preparing TestConnectionMessage\n";
        CGS_ASSERT(lu16Frame != KU16_INVALID_FRAME, CGS_ASSERT_MESSAGE_AT(
            "lu16Frame != KU16_INVALID_FRAME",
            CGS_ARTIST_FILE, 69 /* ARTIST 0x82882380 */));
        ReliableMessage::PrepareForSend(4, lu16Frame);
    }

    // No fields of its own: an inlined copy of ReliableMessage::PackOrUnpack (the reliable
    // id in the inherited frame word, [0, 65534]).
    PackOrUnpackResult TestConnectionMessage::PackOrUnpack()
    {
        return ReliableMessage::PackOrUnpack();
    }
}
