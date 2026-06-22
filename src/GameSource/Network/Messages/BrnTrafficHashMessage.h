#pragma once

// ===================================================================================
// BrnNetwork::TrafficHashMessage -- owning header
//   b5-decomp/src/GameSource/Network/Messages/BrnTrafficHashMessage.h
//
// SHAPE from DecFIGS DWARF (BrnTrafficHashMessage.h:45) gated against the X360 binary.
// Construct @ 0x8257A3B8 stores three halfwords at +0x20/+0x22/+0x24 (a1[16..18] as a
// _WORD*), directly after the 0x20-byte CgsNetwork::Message base:
//   +0x00  (CgsNetwork::Message base)
//   +0x20  u16 mu16SyncedFrameSinceStart
//   +0x22  u16 mu16TrafficHash
//   +0x24  u16 mu16Update10HzFrame
//
// This message is embedded by value inside BrnNetwork::TrafficManager::TrafficSyncData
// (send/recv copies), so the layout must be exact.
// ===================================================================================

#include "types.hpp"
#include "GameShared/GameClasses/Network/Packeting/Messages/CgsMessage.h"

namespace BrnNetwork
{
    // The traffic-determinism hash broadcast each 10 Hz update so peers can detect a
    // diverged traffic simulation. Unreliable (asserts !IsReliable on send).
    struct TrafficHashMessage : CgsNetwork::Message
    {
        u16 mu16SyncedFrameSinceStart;   // +0x20
        u16 mu16TrafficHash;             // +0x22
        u16 mu16Update10HzFrame;         // +0x24

        void                           Construct();
        void                           PrepareForSend(u16 lu16FrameCount,
                                                       u16 lu16SyncedFrameSinceStart,
                                                       u16 lu16Update10HzFrame,
                                                       u16 lu16TrafficHash);
        bool                           Retrieve(u16* lpu16SyncedFrameSinceStart,
                                                u16* lpu16Update10HzFrame,
                                                u16* lpu16TrafficHash);
        s32                            GetPackedMessageSize();
        CgsNetwork::PackOrUnpackResult PackOrUnpack();
    };
} // namespace BrnNetwork
