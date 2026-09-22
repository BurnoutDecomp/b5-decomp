#include "types.hpp"

#include "GameShared/GameClasses/Network/Packeting/Messages/CgsTestConnectionMessage.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

// CgsNetwork::TestConnectionMessage: GetPackedMessageSize, PrepareForSend,
// PackOrUnpack.
// (GetName is inline in the header.)

namespace CgsNetwork
{
    // The console folds this with ReliableMessage::GetPackedMessageSize: the same three
    // zero stores (frame, sending id, receiving id) and a tail call of the base size.
    s32 TestConnectionMessage::GetPackedMessageSize()
    {
        return ReliableMessage::GetPackedMessageSize();
    }

    // Stamp a reliable test-connection message (message type 4) for lu16Frame. The
    // console first writes "Preparing TestConnectionMessage\n" to the global debug-print
    // stream; that stream has no home in the tree, so the log line is dropped.
    void TestConnectionMessage::PrepareForSend(u16 lu16Frame)
    {
        CGS_ASSERT(lu16Frame != KU16_INVALID_FRAME, "lu16Frame != KU16_INVALID_FRAME");
        ReliableMessage::PrepareForSend(4, lu16Frame);
    }

    // No fields of its own: an inlined copy of ReliableMessage::PackOrUnpack (the reliable
    // id in the inherited frame word, [0, 65534]).
    PackOrUnpackResult TestConnectionMessage::PackOrUnpack()
    {
        return ReliableMessage::PackOrUnpack();
    }
}
