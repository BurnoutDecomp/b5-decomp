#pragma once

// ===================================================================================
// CgsNetwork::TimeManager -- minimal owning header
//   b5-decomp/src/GameShared/GameClasses/Network/Time/CgsTimeManager.h
//
// The session time-synchronisation manager. The full type (start-time sync, the sync-time
// message pump, per-frame clock) is large and reconstructed in its own TU; this header is
// an INTENTIONALLY MINIMAL home declaring only what the consumers reconstructed so far need.
//
// The single field a consumer reads here is the running network frame counter
// (BrnNetworkSelectedRoutesManager::SendRouteData @ 0x8255C168 reads it: `lwz r11,0x388(tm)`
// then reduces it modulo 0xFFFF -- the 16-bit network frame-wrap -- via the 0x80008001
// reciprocal-multiply idiom). Modelled here as a named u32 plus a GetCurrentFrameNumber()
// accessor that returns the wrapped 16-bit value, accessed BY NAME. The X360 +0x388 byte
// offset is documentation only -- the reconstruction follows the project's semantic-parity
// rule (native x64 layout, name access), so a leading reserved block keeps the named field
// present without claiming the X360 absolute offset.
// ===================================================================================

#include "types.hpp"

namespace CgsNetwork
{
    struct TimeManager
    {
        // The current network frame number, returned wrapped into the 16-bit window the wire
        // protocol uses (the X360 `frame % 0xFFFF` reduction). 0xFFFF is the wrap modulus.
        u16 GetCurrentFrameNumber() const
        {
            return static_cast<u16>(mu32CurrentFrameNumber % 0xFFFFu);
        }

    private:
        // Reserved storage standing in for the un-reconstructed leading members (the X360
        // frame counter sits at +0x388). Honest placeholder: the full TimeManager TU will
        // replace this with the real typed members; only the frame counter is named here
        // because it is the only field a reconstructed consumer reads.
        u8  maReserved[0x388];          // un-reconstructed leading members
        u32 mu32CurrentFrameNumber;     // +0x388 -- running network frame counter
    };
}
