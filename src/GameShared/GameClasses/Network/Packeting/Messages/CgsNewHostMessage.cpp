#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"   // CGS_ASSERT at ARTIST locations

#include "GameShared/GameClasses/Network/Packeting/Messages/CgsNewHostMessage.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

// CgsNetwork::NewHostMessage: PrepareForSend, Retrieve, PackOrUnpack,
// GetPackedMessageSize.
// (GetName is inline in the header.)
//
// An unreliable host-migration broadcast: the id of the player becoming host plus the
// KI_MAX_RECEIVED_CLIENTS client ids whose acknowledgements the sender has seen.

namespace CgsNetwork
{
    // Copy the payload in, then stamp the message (type 9) for lu16CurrentFrame and flag
    // it VALID (the base PrepareForSend, inlined).
    void NewHostMessage::PrepareForSend(u16 lu16CurrentFrame, NetworkPlayerID liNewHostID,
                                        NetworkPlayerID* lpReceivedClientsIDs)
    {
        mNewHostID = liNewHostID;
        for (s32 liIndex = 0; liIndex < KI_MAX_RECEIVED_CLIENTS; ++liIndex)
        {
            maReceivedClientsIDs[liIndex] = lpReceivedClientsIDs[liIndex];
        }

        Message::PrepareForSend(9, lu16CurrentFrame);
    }

    // When the slot is VALID, copy the payload out, mark the message dealt with and
    // return true; otherwise return false and leave the outputs untouched.
    bool NewHostMessage::Retrieve(NetworkPlayerID* lpNewHostID, NetworkPlayerID* lpReceivedClientsIDs)
    {
        // ARTIST 0x828824E0 branches to a trailing `return false` (0x82882578): the copy-out is the body
        // of the valid test.
        if (IsMessageValid())
        {
            *lpNewHostID = mNewHostID;
            for (s32 liIndex = 0; liIndex < KI_MAX_RECEIVED_CLIENTS; ++liIndex)
            {
                lpReceivedClientsIDs[liIndex] = maReceivedClientsIDs[liIndex];
            }

            SetMessageInvalid();
            CGS_ASSERT(!IsMessageValid(), CGS_ASSERT_MESSAGE_AT(
                "!IsMessageValid()",
                "..\\..\\..\\GameShared\\GameClasses\\Network\\Packeting\\Messages\\CgsNewHostMessage.cpp", 118 /* ARTIST 0x8288255C */));
            return true;
        }
        return false;
    }

    // New host id, then every received-client id, each in [-1, 0x7FFFFFFF]; the
    // per-field statuses are OR-accumulated.
    PackOrUnpackResult NewHostMessage::PackOrUnpack()
    {
        PackOrUnpackResult lxResult = CgsNetwork::Message::PackOrUnpack(&mNewHostID, -1, 0x7FFFFFFF);
        for (s32 liIndex = 0; liIndex < KI_MAX_RECEIVED_CLIENTS; ++liIndex)
        {
            lxResult = CgsNetwork::Message::PackOrUnpack(&maReceivedClientsIDs[liIndex], -1, 0x7FFFFFFF)
                       | lxResult;
        }
        return lxResult;
    }

    // Zero the payload (fixed packed widths) and size the message through the base.
    s32 NewHostMessage::GetPackedMessageSize()
    {
        mNewHostID = 0;
        for (s32 liIndex = 0; liIndex < KI_MAX_RECEIVED_CLIENTS; ++liIndex)
        {
            maReceivedClientsIDs[liIndex] = 0;
        }
        return Message::GetPackedMessageSize();
    }
}
