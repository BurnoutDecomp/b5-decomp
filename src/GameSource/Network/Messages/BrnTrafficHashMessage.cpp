#include "types.hpp"

#include "GameSource/Network/Messages/BrnTrafficHashMessage.h"
#include "GameSource/World/DebugComponents/BrnPVSDebugComponent.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnNetwork::TrafficHashMessage::Construct            @ 0x8257A3B8
//   BrnNetwork::TrafficHashMessage::GetPackedMessageSize @ 0x8257A3F8
//   BrnNetwork::TrafficHashMessage::PackOrUnpack         @ 0x8257A410
//   BrnNetwork::TrafficHashMessage::PrepareForSend       @ 0x8257DEB8
//   BrnNetwork::TrafficHashMessage::Retrieve             @ 0x8257DF50
//
// Three 16-bit fields serialised through the shared u16 primitive. The first field's
// pack/unpack status is OR-ed with the PVS "is simple" flag (always false here) exactly
// as the X360 build does. The VALID flag (inherited Message::mx8Flags bit) gates
// send/receive as for the other unreliable messages.

namespace BrnNetwork
{
    void TrafficHashMessage::Construct()
    {
        CgsNetwork::Message::Construct();
        mu16SyncedFrameSinceStart = 0;
        mu16TrafficHash           = 0;
        mu16Update10HzFrame       = 0;
    }

    s32 TrafficHashMessage::GetPackedMessageSize()
    {
        mu16SyncedFrameSinceStart = 0;
        mu16TrafficHash           = 0;
        mu16Update10HzFrame       = 0;
        return CgsNetwork::Message::GetPackedMessageSize();
    }

    CgsNetwork::PackOrUnpackResult TrafficHashMessage::PackOrUnpack()
    {
        const CgsNetwork::PackOrUnpackResult lxIsSimple =
            reinterpret_cast<BrnWorld::PVSDebugComponent*>(this)->IsSimple() ? 1 : 0;

        const CgsNetwork::PackOrUnpackResult lxSynced =
            CgsNetwork::PackOrUnpackU16(this, &mu16SyncedFrameSinceStart, 0, 0xFFFF) | lxIsSimple;
        const CgsNetwork::PackOrUnpackResult lxUpdate =
            CgsNetwork::PackOrUnpackU16(this, &mu16Update10HzFrame, 0, 0xFFFF);
        return CgsNetwork::PackOrUnpackU16(this, &mu16TrafficHash, 0, 0xFFFF) | (lxUpdate | lxSynced);
    }

    void TrafficHashMessage::PrepareForSend(u16 lu16FrameCount,
                                            u16 lu16SyncedFrameSinceStart,
                                            u16 lu16Update10HzFrame,
                                            u16 lu16TrafficHash)
    {
        if ((mx8Flags & CgsNetwork::KX8_FLAGS_VALID) == 0)
        {
            mu16SyncedFrameSinceStart = lu16SyncedFrameSinceStart;
            mu16TrafficHash           = lu16TrafficHash;
            mu16Update10HzFrame       = lu16Update10HzFrame;
            CgsNetwork::Message::PrepareForSend(14, lu16FrameCount);
            CGS_ASSERT(!IsReliable(), "!IsReliable()");
        }
    }

    bool TrafficHashMessage::Retrieve(u16* lpu16SyncedFrameSinceStart,
                                      u16* lpu16Update10HzFrame,
                                      u16* lpu16TrafficHash)
    {
        if ((mx8Flags & CgsNetwork::KX8_FLAGS_VALID) == 0)
            return false;

        *lpu16SyncedFrameSinceStart = mu16SyncedFrameSinceStart;
        *lpu16Update10HzFrame       = mu16Update10HzFrame;
        *lpu16TrafficHash           = mu16TrafficHash;
        mx8Flags &= ~CgsNetwork::KX8_FLAGS_VALID;
        return true;
    }
}
