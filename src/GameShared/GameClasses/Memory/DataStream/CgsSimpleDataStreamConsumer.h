#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Memory/DataStream/CgsDataStreamCommandReader.h"
#include "GameShared/GameClasses/Memory/DataStream/CgsSimpleDataStreamProducer.h" // SimpleDataStreamProducer::SharedData (BY VALUE at +0x00)

// CgsMemory::SimpleDataStreamConsumer
// ---------------------------------------------------------------------------
// The consumer / read-side dual of SimpleDataStreamProducer: a convenience
// front-end that drains a producer's command stream through an embedded
// DataStreamCommandReader, and posts answers back into the producer's own
// result buffer.
//
// X360 homes:
//   Construct  @ 0x82868508   (15)
//   AddResult  @ 0x82868550   (6)   -- EXPORT HOLE, lifted from the image
//   Destruct   @ 0x82868548   (2)
//   ReadCo     @ 0x82916FD8   (15)
//
// ⭐⭐ LAYOUT RETIRED 2026-08-10 (fill-worker wave 2). The leading region was an
// honest-but-unmodelled `u8 maReserved0[0x80]`, written blind by a Construct
// that was not in the tree. Construct @0x82868508 names the whole head:
//
//   0x82868524  bl  XMemCpy(this, lpProducer, 0x20)   <- the producer's SharedData
//   0x82868530  stw r30, 0x20(this)                   <- lpvResultDestination
//   0x82868534  stw r29, 0x24(this)                   <- liResultDestinationSize
//   0x8286852C  lwz r4, 0x18(this)                    <- mShared.mpPoster (COPIED above)
//   0x82868538  bl  DataStreamCommandReader::Construct(this + 0x80, r4)
//
// and `SimpleDataStreamProducer::SharedData` is EXACTLY those 32 X360 bytes
// (5 x s32 = 0x14, mpResultBuffer @0x14, mpPoster @0x18, muPad @0x1C) -- which
// is why the `lwz 0x18` lands on the poster. So the head is three named members,
// not an opaque block.
//
// ⚠️ THE 0x20 IS A CONSOLE SIZE LITERAL AND IS NOT REPRODUCED AS ONE. SharedData
// is a runtime-carved (stack) struct that WIDENS on x64 (two 4-byte pointers
// become 8), so the copy is a struct assignment, per the standing "a count is
// not a size" / "carved at runtime => widen 4->8" rules. Everything below the
// head keeps mReader at the same relative seat by NAME, never by console offset.
//
// ⚠️ FLAG (unchanged, honest): the span between the three named head members and
// mReader is still unattested -- nothing in any of the four bodies touches it.
// It is kept as a sized pad so the reader keeps its console-relative seat, and
// it is named as UNATTESTED rather than as an interpreted member.
namespace CgsMemory
{
    struct SimpleDataStreamConsumer
    {
        // Construct @0x82868508. Snapshot the producer's shared geometry, remember
        // where this consumer's own results go, and bind the embedded command
        // reader to the producer's poster.
        //
        // PS3 DWARF types every parameter:
        //   _ZN9CgsMemory24SimpleDataStreamConsumer9ConstructE
        //     PNS_24SimpleDataStreamProducerEPvi                     @0xBC29E4 (12)
        void Construct(SimpleDataStreamProducer* lpProducer,
                       void*                     lpvResultDestination,
                       s32                       liResultDestinationSize);

        // Destruct @0x82868548 -- `addi r3, r3, 0x80 ; b DataStreamCommandReader::Destruct`,
        // i.e. a pure tail-call onto the embedded reader (which unregisters this
        // reader as a poster user). Nothing else is torn down.
        void Destruct();

        // ReadCo (ReadCommand) @0x82916FD8. Forwards to the embedded command
        // reader: claims the next command record and copies it into lpDest,
        // writing the slot index through lpuOutIndex when non-NULL.
        // Returns 0 on success, 1 when the stream is drained.
        s32 ReadCo(void* lpDest, u32* lpuOutIndex);

        // AddResult @0x82868550 -- ⚠️ X360 EXPORT HOLE (no 0x82868550.json among
        // the 30,084). Six instructions lifted from the image with ppcdis.py and
        // reproduced store-for-store:
        //   lwz   r11, 16(r3)    ; mShared.miAlignedResultSize
        //   lwz   r9,  20(r3)    ; mShared.mpResultBuffer
        //   mullw r10, r11, r5   ; stride * index
        //   add   r3,  r10, r9
        //   mr    r5,  r11
        //   b     XMemCpy        ; tail-call
        // Signature settled by the PS3 mangle
        //   _ZN9CgsMemory24SimpleDataStreamConsumer9AddResultEPvi     @0xBC1C04 (16).
        //
        // ⭐ NOTE WHERE THIS WRITES: mShared.mpResultBuffer is the PRODUCER's
        // result buffer, copied in by Construct. That is the same buffer
        // TriangleCacheManager::EndUpdateTriangleCaches walks through
        // SimpleDataStreamProducer::GetResultIterator() -- i.e. the drain for this
        // producer already exists and is already mounted. (`xrefs_to` on the
        // consumer side checked before writing, per the standing rule.)
        void AddResult(const void* lpvResult, s32 liResultIndex);

    private:
        // --- members ---------------------------------------------------------
        SimpleDataStreamProducer::SharedData mShared;   // +0x00  copied from the producer
        void* mpvResultDestination;                     // X360 +0x20
        s32   miResultDestinationSize;                  // X360 +0x24

        // ⚠️ UNATTESTED span. No body in this class touches anything between the
        // three members above and the reader; the pad exists only to keep mReader
        // at the console's relative seat. Sized from the X360 gap (0x80 - 0x28).
        u8    mauUnattested28[0x80 - 0x28];

        DataStreamCommandReader mReader;                // X360 +0x80
    };
}
