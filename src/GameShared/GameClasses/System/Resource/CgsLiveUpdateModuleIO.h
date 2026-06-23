// ============================================================================
// b5-decomp/src/GameShared/GameClasses/System/Resource/CgsLiveUpdateModuleIO.h
//
// Canonical (DWARF) home for CgsResource::LiveUpdateIO::OutputBuffer (CgsLiveUpdateModuleIO.h).
// The live-update IO output payload buffer the resource live-update module fills each frame.
// It derives from CgsModule::IOBuffer and exposes its queues through read-locked const getters
// (each tests the read-lock bit (((*a1 >> 4) & 1)) and fires "Not locked for reading" on failure).
//
// X360-emitted accessors:
//   GetOutputQueue() const  @ 0x826642D8  read-lock (bit 4) -> +4    (DWARF :111)  [this group]
//   GetStatusQueue() const  @ 0x82664230  read-lock (bit 4) -> +0x214(DWARF :108)  [sibling]
//
// LAYOUT (X360 getter return-offsets, authoritative):
//   base  CgsModule::IOBuffer                 (1-byte status FlagSet; +1..+3 pad)
//   +4    OutputQueue   mOutputQueue           (528 bytes up to the next member)
//   +0x214 StatusQueue  mStatusQueue           (foreign/sibling type, opaque-but-placed)
//
// This group bodies ONLY GetOutputQueue() (+4). The +0x214 status queue and its GetStatusQueue()
// getter belong to a sibling TU; they are modelled here as correctly-placed opaque byte storage
// so the X360 +4 offset is exact. Adopt the named queue type additively when its home lands.
#pragma once

#include "types.hpp"

#include "GameShared/GameClasses/Module/CgsIOBuffer.h"  // CgsModule::IOBuffer

namespace CgsResource
{
namespace LiveUpdateIO
{
    struct OutputBuffer : public CgsModule::IOBuffer
    {
        // The live-update output queue at +4. Foreign queue type (own home elsewhere); modelled
        // as correctly-sized byte storage (offset +4 .. +0x214 = 528 bytes) so the X360 return
        // offset is exact.
        struct OutputQueueStorage
        {
            unsigned char maBytes[528];
        };

        const OutputQueueStorage* GetOutputQueue() const;   // +4, read-lock

        static void _AssertLayout();

    private:
        u8                 maStatusPad[3];  // +1..+3 (force +4 placement)
        OutputQueueStorage mOutputQueue;    // +4
        // +0x214 mStatusQueue (sibling TU's GetStatusQueue() target) lives beyond this slice.
    };
}
}
