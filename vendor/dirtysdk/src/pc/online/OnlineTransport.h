#pragma once

#include "../lan/pclan.h"

// FLAG PC-platform leaf: encrypted transport to the self-hosted Rust service. The
// original game's messages, reliability and session state machines stay above it.
namespace B5Online
{
    bool Enabled();
    bool Configure(uintptr_t luSocket, uint32_t luIdent, PcLanPeerT* lpSelf);
    void Shutdown();
    void Update();
    int32_t Send(uint32_t luTarget, const uint8_t* lpPacket, int32_t liLength);
    // Consumes a server datagram. Returns a complete BPL1 packet length, or 0.
    int32_t Receive(const PcLanPeerT* lpFrom, const uint8_t* lpPacket, int32_t liLength,
                    uint8_t* lpInner, int32_t liCapacity, PcLanPeerT* lpPeer);
    uint16_t MaxPlayers();
}
