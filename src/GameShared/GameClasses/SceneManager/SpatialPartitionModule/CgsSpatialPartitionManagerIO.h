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
#ifdef GetFreeSpace
#undef GetFreeSpace
#endif
#include "GameShared/GameClasses/SceneManager/SpatialPartitionModule/CgsCoarseQueryResultBuffer.h" // CoarseQueryResultBuffer<16384>

namespace CgsSceneManager
{
namespace SpatialPartitionIO
{
    // ------------------------------------------------------------------------
    // The four inbound update-queue RECORDS (the variable-event-queue payloads
    // SpatialPartitionManager::ProcessUpdateQueue @0x828BAE50 switches on). Their
    // field offsets are the producer's stores, read back by the drain:
    //   ADD    : the bounding-sphere CENTRE in the leading 16-byte vmx lane, then
    //            the entity index / type-flag mask / radius scalars;
    //   REMOVE : the entity index alone;
    //   SETPOS : the leading vmx lane + the entity index;
    //   SETRAD : the entity index + the radius.
    // Record type ids are the queue's switch keys (see ESpatialPartitionUpdateEvent).
    // ------------------------------------------------------------------------
    enum ESpatialPartitionUpdateEvent
    {
        E_SPATIAL_PARTITION_UPDATE_ADD_ENTITY          = 0,
        E_SPATIAL_PARTITION_UPDATE_REMOVE_ENTITY       = 1,
        E_SPATIAL_PARTITION_UPDATE_SET_ENTITY_POSITION = 2,
        E_SPATIAL_PARTITION_UPDATE_SET_ENTITY_RADIUS   = 3,
    };

    struct alignas(16) InEventAddEntity : public CgsModule::Event
    {
        Vector3 mPosition;      // +0x00 (16-byte vmx lane)
        u16     mu16EntityId;   // +0x10
        u16     mu16Pad12;      // +0x12
        u32     mx32TypeFlags;  // +0x14
        f32     mfRadius;       // +0x18
    };

    struct InEventRemoveEntity : public CgsModule::Event
    {
        u16 mu16EntityId;       // +0x00
        u16 mu16Pad02;          // +0x02
    };

    struct alignas(16) InEventSetEntityPosition : public CgsModule::Event
    {
        Vector3 mPosition;      // +0x00 (16-byte vmx lane)
        u16     mu16EntityId;   // +0x10
        u16     mu16Pad12;      // +0x12
    };

    struct InEventSetEntityRadius : public CgsModule::Event
    {
        u16 mu16EntityId;       // +0x00
        u16 mu16Pad02;          // +0x02
        f32 mfRadius;           // +0x04
    };

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

        // ADOPTED 2026-07-28 (culling wave): the real home landed, so the member is the
        // named CgsSceneManager::CoarseQueryResultBuffer<16384> the DWARF declares
        // (honest size 32920 on the console, unchanged here). The old opaque stand-in
        // typedef is kept as an alias so existing spellings still resolve.
        typedef CgsSceneManager::CoarseQueryResultBuffer<16384> CoarseQueryResultBufferDefault;

        // Lifecycle (DWARF-attested). CreateIOBuffer<T> runs T::Construct (2026-08-15) -- on the
        // PC as well as the console -- so this body is what the stack template invokes: raise the
        // IOBuffer status base, bring up the query-result ring and clear the coarse-result
        // buffer's counters.
        void Construct()
        {
            CgsModule::IOBuffer::Construct();
            mQueryResultQueue.Construct();
            mCoarseResultBuffer.Construct();
        }
        // BODIED 2026-08-15 (IO-buffer zero-fill removal audit): CgsIOBufferStack.h's
        // DestroyIOBuffer<T> is the console's mirror now and calls T::Destruct, so this could no
        // longer be declaration-only (CgsSceneManagerModule.cpp:1174 destroys this buffer).
        // X360 0x828BB088, four acts in this order:
        //   six zero stores at +53396/+53400/+53404/+53408/+53412/+53416 -- which are exactly
        //     CoarseQueryResultBuffer<16384>::Clear()'s six member stores at the buffer's own
        //     +32896..+32916 (mCoarseResultBuffer sits at +20500), i.e. an INLINED
        //     mCoarseResultBuffer.Clear()
        //   VariableEventQueue<20480,16>::Clear(this+4)     -> mQueryResultQueue
        //   VariableEventQueue<20480,16>::Destruct(this+4)  -> mQueryResultQueue
        //   CgsModule::IOBuffer::Destruct(this)
        void Destruct()
        {
            mCoarseResultBuffer.Clear();
            mQueryResultQueue.Clear();
            mQueryResultQueue.Destruct();
            CgsModule::IOBuffer::Destruct();
        }

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
