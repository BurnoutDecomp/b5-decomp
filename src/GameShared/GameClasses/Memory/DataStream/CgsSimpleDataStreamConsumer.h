#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Memory/DataStream/CgsDataStreamCommandReader.h"

// CgsMemory::SimpleDataStreamConsumer
// ---------------------------------------------------------------------------
// The consumer / read-side dual of SimpleDataStreamProducer: a convenience
// front-end that drains a producer's command stream through an embedded
// DataStreamCommandReader. ReadCo() forwards straight to the embedded reader.
//
// X360 home (reconstructed in this pass):
//   ReadCo (ReadCommand) @ 0x82916FD8
//
// Layout note (FLAG: leading region opaque): ReadCo @ 0x82916FD8 only ever
// touches the reader subobject, which the asm addresses at `this + 0x80`
// (addi r3, this, 0x80 before the bl to DataStreamCommandReader::ReadCom).
// Nothing in this TU references any field below +0x80, so the consumer's
// leading 128-byte region is not recoverable here and is declared as an honest
// opaque block sized only to place mReader at +0x80. Member access is by name.
namespace CgsMemory
{
    struct SimpleDataStreamConsumer
    {
        // Reconstructed in this pass (X360 0x82916FD8). Forwards to the embedded
        // command reader: claims the next command record and copies it into
        // lpDest, writing the slot index through lpuOutIndex when non-NULL.
        // Returns 0 on success, 1 when the stream is drained.
        s32 ReadCo(void* lpDest, u32* lpuOutIndex);

    private:
        // --- members ---------------------------------------------------------
        // +0x00: not referenced by ReadCo; opaque leading region sized so the
        //        embedded reader lands at +0x80 (matches the `this + 0x80`
        //        reader address the asm forms). (FLAG: layout opaque.)
        u8                       maReserved0[0x80]; // +0x00 opaque (see note)
        DataStreamCommandReader  mReader;           // +0x80 embedded command reader
    };
}
