#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Memory/DataStream/CgsDataStreamCommandPoster.h"

// CgsMemory::DataStreamCommandReader
// ---------------------------------------------------------------------------
// The reading side of a lock-free command stream: it plays back the fixed-stride
// command records a DataStreamCommandPoster appended. The reader is a thin
// wrapper that holds a pointer to the poster it drains (mpPoster @ +4); the
// command buffer and the packed 64-bit lock-free status word live on the poster.
// Each ReadCom() atomically advances the poster's packed nextCommand field (low
// 24 bits of mEncodedStatus) via a compare-and-swap retry loop, then block-copies
// the reserved record out.
//
// X360 home (reconstructed in this pass):
//   ReadCom (ReadCommand) @ 0x82867920
//
// Layout: the X360 ReadCom reads the poster pointer from *(this+4); the leading
// 4-byte word at +0 is NOT touched by ReadCom, so its meaning is not recoverable
// from this TU and it is declared as an honest opaque first word sized to keep
// mpPoster at +4. (FLAG: muReserved0 layout opaque.) The reader is embedded at
// offset +128 inside a SimpleDataStreamConsumer (proven by ReadCo @ 0x82916FD8,
// which calls ReadCom(this+128, ...)). Member access is by name throughout.
namespace CgsMemory
{
    struct DataStreamCommandReader
    {
        // Reconstructed in this pass (X360 0x82867920). Atomically reserves the
        // next command slot and copies it into lpDest; writes the slot index
        // through lpuOutIndex when non-NULL. Returns 0 on success, 1 when the
        // stream is drained (nextCommand has reached the poster's posted count).
        s32 ReadCom(void* lpDest, u32* lpuOutIndex);

    private:
        // --- members ---------------------------------------------------------
        // +0: not referenced by ReadCom; opaque first word sized so mpPoster
        //     lands at +4 (matches the *(this+4) poster load). (FLAG)
        u32                      muReserved0; // +0  opaque (see header note)
        DataStreamCommandPoster* mpPoster;    // +4  the poster this reader drains
    };
}
