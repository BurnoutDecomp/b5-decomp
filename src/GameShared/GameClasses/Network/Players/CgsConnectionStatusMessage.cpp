#include "GameShared/GameClasses/Network/Players/CgsConnectionStatusMessage.h"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/CgsServerInterfaceDirtySock.h" // mNetStreamLogChannelOutput (0x82F335C8)
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"   // CGS_ASSERT at ARTIST locations
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"   // CgsDev::Log::gpDebugPrint

// CgsNetwork::ConnectionStatusMessage -- the send stamp and the receive copy-out. GetName is
// header-inline; the packing virtuals live with the rest of the message hierarchy.

namespace CgsNetwork
{
    namespace
    {
        // The message type this class sends (E_MESSAGE_TYPE_CONNECTION_STATUS).
        const s32 KI_CONNECTION_STATUS_MESSAGE_TYPE = 5;
    }

    // Copy the seven records in and queue the message as a reliable send for lu16Frame.
    void ConnectionStatusMessage::PrepareForSend(const PlayerConnectionData* lpaConnectionData,
                                                 u16 lu16Frame)
    {
        CgsNetwork::ServerInterfaceDirtySock::mNetStreamLogChannelOutput << "Preparing ConnectionStatusMessage\n";
        CGS_ASSERT_AT(lu16Frame != KU16_INVALID_FRAME,
            "lu16Frame != KU16_INVALID_FRAME",
            __FILE__, 80 /* ARTIST 0x82883194 */);

        for (s32 liIndex = 0; liIndex < KI_CONNECTION_STATUS_PLAYER_COUNT; ++liIndex)
        {
            maConnectionData[liIndex] = lpaConnectionData[liIndex];
        }

        ReliableMessage::PrepareForSend(KI_CONNECTION_STATUS_MESSAGE_TYPE, lu16Frame);
    }

    // A received message: copy the seven records out, consume it and report true; false when
    // nothing arrived.
    bool ConnectionStatusMessage::Retrieve(PlayerConnectionData* lpaConnectionData)
    {
        // ARTIST 0x82883210 branches over the whole body when the message is invalid.
        if (IsMessageValid())
        {
            CgsNetwork::ServerInterfaceDirtySock::mNetStreamLogChannelOutput << "Retrieving ConnectionStatusMessage\n";
            for (s32 liIndex = 0; liIndex < KI_CONNECTION_STATUS_PLAYER_COUNT; ++liIndex)
            {
                lpaConnectionData[liIndex] = maConnectionData[liIndex];
            }

            SetMessageInvalid();
            CGS_ASSERT_AT(!IsMessageValid(),
                "!IsMessageValid()",
                __FILE__, 121 /* ARTIST 0x82883294 */);
            return true;
        }

        return false;
    }
}
