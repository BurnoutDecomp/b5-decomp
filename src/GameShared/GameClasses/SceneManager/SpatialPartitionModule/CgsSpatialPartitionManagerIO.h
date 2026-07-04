// ============================================================================
// b5-decomp/src/GameShared/GameClasses/SceneManager/SpatialPartitionModule/
//   CgsSpatialPartitionManagerIO.h
//
// Canonical (DWARF) home for the CgsSceneManager::SpatialPartitionIO per-frame IO
// payload buffers the coarse spatial-partition module exchanges with its clients.
// This is a MINIMAL-COMPLETE slice covering ONLY the X360-emitted read/write-lock
// queue accessors owned by this group:
//   InputBuffer_Update::GetSpatialPartitionUpdateQueue() const @ 0x828AFD98
//   OutputBuffer::GetCoarseResultBuffer()                      @ 0x828AFE40
//
// Each buffer derives the shared CgsModule::IOBuffer (1-byte FlagSet status; +1..+3
// pad) and embeds a variable-size event queue by value at this+4, following the
// recurring *ModuleIO pattern (see the committed CgsGuiModuleIO.h / CgsMemoryModuleIO.h
// siblings: maStatusPad[3] forces the queue to +4).
//
// LAYOUT (DWARF CgsSpatialPartitionManagerIO.h + X360 getter return-offsets, authoritative):
//   InputBuffer_Update:
//     base  CgsModule::IOBuffer                       (1-byte status; +1..+3 pad)
//     +4    InSpatialPartitionUpdateQueue mSpatialPartitionUpdateQueue
//                                                     (VariableEventQueue<135168,16>; DWARF :194)
//   OutputBuffer:
//     base  CgsModule::IOBuffer                       (1-byte status; +1..+3 pad)
//     +4    OutQueryResultsQueue mQueryResultQueue    (VariableEventQueue<20480,16>; DWARF :226)
//     +0x5014 (20500) CoarseQueryResultBufferDefault mCoarseResultBuffer  (DWARF :227)
// The getter return-offsets pin +4 (mSpatialPartitionUpdateQueue, read-lock bit 4) and
// +20500 (mCoarseResultBuffer, write-lock bit 3 -- a getter that guards on the write lock,
// faithful to the asm).
//
// FLAG (foreign type): the DWARF real type of mCoarseResultBuffer is
// CgsSceneManager::CoarseQueryResultBuffer<16384> (nested in SpatialPartition::
// LineTestRecursiveFuncParams); its owning home is CgsCoarseQueryResultBuffer.h. It is
// NOT reconstructed here -- modelled as a correctly-sized opaque POD stand-in
// (CoarseQueryResultBufferDefault, honest size 32920 from the X360) so its +20500 offset
// (fully pinned by the preceding queue) is exact. When the real home lands this header
// should adopt the named type additively.
#pragma once

#include "types.hpp"
#include "BrnCommonTypes.h"                                       // Vector3 (AddEntity / SetEntityPosition params)

#include "GameShared/GameClasses/Module/CgsIOBuffer.h"            // CgsModule::IOBuffer base + lock-state queries
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"  // CgsModule::VariableEventQueue<BUFSIZE,ALIGN>

namespace CgsSceneManager
{
namespace SpatialPartitionIO
{
    // CgsSpatialPartitionManagerIO.h:149 (DWARF) -- inbound spatial-partition update payload buffer.
    // A producer write-locks it and fills the update queue (AddEntity / RemoveEntity /
    // SetEntityPosition / SetEntityRadius); the partition module read-locks it and consumes them.
    struct InputBuffer_Update : public CgsModule::IOBuffer
    {
        // CgsSpatialPartitionManagerIO.h:115 (DWARF): the update queue is a 135168-byte, 16-aligned VEQ.
        typedef CgsModule::VariableEventQueue<135168, 16> InSpatialPartitionUpdateQueue;

        // Lifecycle (DWARF-attested) -- DECLARE-ONLY (defined in their own TUs).
        void Construct();
        void Destruct();
        void AddEntity(u16 lu16Id, u32 luFlags, Vector3 lPosition, f32 lfRadius);
        void RemoveEntity(u16 lu16Id);
        void SetEntityPosition(u16 lu16Id, Vector3 lPosition);
        void SetEntityRadius(u16 lu16Id, f32 lfRadius);

        // Accessors (DWARF :189/:190). The const overload asserts the buffer is read-locked and is
        // bodied in this TU's .cpp (X360 0x828AFD98); the non-const overload is declare-only.
        const InSpatialPartitionUpdateQueue* GetSpatialPartitionUpdateQueue() const;
        InSpatialPartitionUpdateQueue*       GetSpatialPartitionUpdateQueue();

    private:
        u8                            maStatusPad[3];               // +1..+3 (force +4 placement)
        InSpatialPartitionUpdateQueue mSpatialPartitionUpdateQueue; // +4 (DWARF :194)
    };

    // CgsSpatialPartitionManagerIO.h:206 (DWARF) -- outbound coarse-query-result payload buffer.
    // The coarse queries append their results here; the client read-locks it and reads them back.
    struct OutputBuffer : public CgsModule::IOBuffer
    {
        // CgsSpatialPartitionManagerIO.h:118 (DWARF): the query-result queue is a 20480-byte, 16-aligned VEQ.
        typedef CgsModule::VariableEventQueue<20480, 16> OutQueryResultsQueue;

        // FLAG: foreign type (see file header). Honest size 32920 (X360); its +20500 offset is
        // fully pinned by the preceding queue, so its internal layout is owned by the real home.
        struct CoarseQueryResultBufferDefault
        {
            unsigned char maBytes[32920];
        };

        // Lifecycle (DWARF-attested) -- DECLARE-ONLY.
        void Construct();
        void Destruct();

        // Accessors (DWARF :218/:219/:221/:222). GetCoarseResultBuffer() (non-const, X360
        // 0x828AFE40) asserts the buffer is write-locked and is bodied in this TU's .cpp; the
        // remaining overloads are declare-only.
        const OutQueryResultsQueue*           GetQueryResultQueue() const;
        OutQueryResultsQueue*                 GetQueryResultQueue();
        const CoarseQueryResultBufferDefault* GetCoarseResultBuffer() const;
        CoarseQueryResultBufferDefault*       GetCoarseResultBuffer();

    private:
        u8                             maStatusPad[3];      // +1..+3 (force +4 placement)
        OutQueryResultsQueue           mQueryResultQueue;   // +4 (DWARF :226)
        CoarseQueryResultBufferDefault mCoarseResultBuffer; // +0x5014 (20500) (DWARF :227)
    };
}
}
