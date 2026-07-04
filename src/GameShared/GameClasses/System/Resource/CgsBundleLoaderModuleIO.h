// ============================================================================
// b5-decomp/src/GameShared/GameClasses/System/Resource/CgsBundleLoaderModuleIO.h
//
// Canonical (DWARF) home for the BundleLoaderModule's IO payload buffers
// (CgsBundleLoaderModuleIO.h). The bundle loader exchanges two of these per frame
// through CgsModule::IOBufferStack:
//   CgsResource::BundleLoaderIO::InputBuffer_Record   (the per-frame input record)
//   CgsResource::BundleLoaderIO::OutputBuffer         (the loader's output, incl. its pool span)
// Both derive from CgsModule::IOBuffer (1-byte FlagSet status; the producing module write-locks
// the buffer while filling it). The buffers' first payload member sits at +4, after the status
// byte's +1..+3 padding (the getters return this + 4).
//
// MINIMAL-COMPLETE slice: this header models ONLY the write-locked accessors the
// cgs-resource-pool group owns:
//   InputBuffer_Record::GetRecord()  @ 0x828E2090  write-lock (bit 3) -> +4   (DWARF :113)
//   OutputBuffer::GetPool()          @ 0x828E21E0  write-lock (bit 3) -> +4   (DWARF :143)
//
// FLAG (foreign types): the actual payload member types (the input record body and the output
// pool span) have their own owning homes elsewhere and are NOT reconstructed here; each is
// modelled as a correctly-placed named byte span so the X360 return offset (+4) is exact. Adopt
// the named types additively when their homes land. Member offset/placement is read from the
// X360 getter bodies (lbz 0(this) status test + addi r3,this,4 return); we do NOT byte-match.
#pragma once

#include "types.hpp"

#include "GameShared/GameClasses/Module/CgsIOBuffer.h"   // CgsModule::IOBuffer

namespace CgsResource
{
namespace BundleLoaderIO
{
    // The per-frame input buffer. Read-locked by the consumer; the event-queue payload
    // begins at +4. (CgsBundleLoaderModuleIO.h; created/destroyed via
    // CgsModule::IOBufferStack::Create/DestroyIOBuffer<InputBuffer>.) The only accessor
    // this slice bodies is the read-locked GetEventQueue() @0x828E1D48; consumed by
    // CgsResource::BundleLoaderModule::ProcessBundleLoadRequests.
    struct InputBuffer : public CgsModule::IOBuffer
    {
        // FLAG: foreign payload type (the bundle-loader event queue). Opaque, correctly
        // placed at +4.
        struct EventQueueStorage
        {
            unsigned char maBytes[4];
        };

        // @ 0x828E1D48: assert read-locked, then return &mEventQueue (this + 4).
        const EventQueueStorage* GetEventQueue() const;

        static void _AssertLayout();

    private:
        u8               maStatusPad[3];   // +1..+3 (force +4 placement)
        EventQueueStorage mEventQueue;     // +4
    };

    // The per-frame input record buffer. Write-locked by the producer; the consumer reads the
    // record body that begins at +4. (CgsBundleLoaderModuleIO.h, created/destroyed via
    // CgsModule::IOBufferStack::Create/DestroyIOBuffer<InputBuffer_Record>.)
    struct InputBuffer_Record : public CgsModule::IOBuffer
    {
        // FLAG: foreign payload type (the bundle-loader input record body). Opaque, correctly
        // placed at +4.
        struct RecordStorage
        {
            unsigned char maBytes[4];
        };

        // @ 0x828E1FE8: assert read-locked, then return &mRecord (this + 4). (DWARF :112)
        const RecordStorage* GetRecord() const;
        // @ 0x828E2090: assert write-locked, then return &mRecord (this + 4). (DWARF :113)
        RecordStorage* GetRecord();

        static void _AssertLayout();

    private:
        u8            maStatusPad[3];   // +1..+3 (force +4 placement)
        RecordStorage mRecord;          // +4
    };

    // The bundle loader's output buffer. Write-locked by the loader while it fills the output;
    // GetPool() hands back the pool span that begins at +4, and the read-locked GetStream()
    // @0x828E2528 hands back a second payload span at +0x1342C.
    struct OutputBuffer : public CgsModule::IOBuffer
    {
        // FLAG: foreign payload type (the output pool span). Opaque, sized so the second
        // payload lands at its X360 offset (+4 .. +0x1342C == 0x13428 bytes).
        struct PoolStorage
        {
            unsigned char maBytes[0x13428];
        };

        // FLAG: foreign payload type (the output stream span). Opaque, correctly placed at
        // +0x1342C.
        struct StreamStorage
        {
            unsigned char maBytes[4];
        };

        // @ 0x828E21E0: assert write-locked, then return &mPool (this + 4). (DWARF :143)
        PoolStorage* GetPool();
        // @ 0x828E2528: assert read-locked, then return &mStream (this + 0x1342C). (DWARF :151)
        const StreamStorage* GetStream() const;

        static void _AssertLayout();

    private:
        u8            maStatusPad[3];   // +1..+3 (force +4 placement)
        PoolStorage   mPool;            // +4
        StreamStorage mStream;          // +0x1342C
    };
}
}
