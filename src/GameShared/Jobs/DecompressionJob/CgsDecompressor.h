#pragma once

#include "types.hpp"
#include "zlib.h"   // z_stream + inflateInit_/inflate/inflateEnd (vendor zlib 1.1.3 interface)

// CgsResource::Decompressor -- the zlib inflate worker that the decompression job runs over a
// list of compressed resource entries. Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   Execute                     @ 0x82ACCB48  (called by CgsResource::DecompressionJobEntry)
//   BeginDecompressingFirstEntry @ 0x82ACC9E0
//   BeginDecompressingNextEntry  @ 0x82ACCA90
//   CompressorAllocateCallback   @ 0x82ACC9C0  (zlib zalloc -> CgsMemory::HeapMalloc::Malloc)
//   CompressorFreeCallback       @ 0x82ACC9D8  (zlib zfree  -> CgsMemory::HeapMalloc::Free)
//
// Supporting structs follow the DecFIGS DWARF (GameShared/Jobs/DecompressionJob): CompressedData
// (StatusClasses.h:43, one per packed entry) and DecompressionJobData (DecompressionJob.h:46, the
// job descriptor Execute consumes). The job descriptor's malloc handle is read as a
// CgsMemory::HeapMalloc* (the X360 ARTIST CompressorAllocateCallback dispatches HeapMalloc::Malloc
// through it; the PS3 DWARF spells the same word mpcMallocBuffer -- the X360 spine is authoritative).

namespace CgsMemory { class HeapMalloc; }

namespace CgsResource
{
    // StatusClasses.h:43 -- one compressed entry: a source (compressed) span and the destination
    // (uncompressed) span. Mapped onto zlib's {next_in, avail_in, next_out, avail_out}.
    struct CompressedData
    {
        void* mpSourceBuffer;        // +0  next_in
        u32   muSourceSize;          // +4  avail_in
        void* mpDestinationBuffer;   // +8  next_out
        u32   muDestinationSize;     // +12 avail_out
    };

    // StatusClasses.h:64 -- the per-stream saved state the decompression job carries across
    // job invocations. On X360 this is the 128-byte snapshot Decompressor::Execute memcpy's in
    // and out (mpStatus): the working zlib stream plus the running read/write counters and the
    // last inflate result. The DecFIGS (PS3) DecompressionJobStatus additionally embeds an
    // IndexedNodeHeap + a 128-byte cache; on the X360 ARTIST spine the malloc heap is the
    // separate CgsMemory::HeapMalloc owned by DecompressionJobInterface (not inside the status),
    // so the X360 status is exactly the 128-byte stream snapshot. Layout proven by
    // DecompressionJobInterface::BeginStream @ 0x828DB1E8 (zeros 128 bytes, then sets the three
    // counters at +0x38/+0x3C/+0x40) and Decompressor::Execute @ 0x82ACCB48 (memcpy 128 bytes).
    struct DecompressionJobStatus
    {
        z_stream mDecompressionStream;   // +0x00 StatusClasses.h:66 (zlib stream, 56B)
        u32      muAmountRead;           // +0x38 StatusClasses.h:67
        u32      muAmountWritten;        // +0x3C StatusClasses.h:68
        s32      miLastInflateResult;    // +0x40 StatusClasses.h:69
        u8       maPad0[60];             // +0x44 pad the saved snapshot to 128 bytes
    };

    // DecompressionJob.h:46 -- the per-job descriptor. Execute restores the saved zlib stream from
    // mpStatus, walks mpEntries[0..muNumEntries), and allocates zlib internal state via mpHeapMalloc.
    struct DecompressionJobData
    {
        DecompressionJobStatus* mpStatus;       // +0  saved 128-byte zlib-stream snapshot src/dst
        CompressedData*         mpEntries;       // +4  compressed entry array
        u32                     muNumEntries;    // +8  entry count
        CgsMemory::HeapMalloc*  mpHeapMalloc;    // +12 heap the zlib callbacks allocate through
    };

    // The inflate worker. It keeps a working zlib stream (mStream) into which it copies the saved
    // stream snapshot, runs inflate per entry with Z_SYNC_FLUSH, and copies the stream back out.
    // Layout follows the X360 ARTIST object (mStream embedded at +0x80; the entry cursor + heap
    // handle trail it); members are accessed by name (the callbacks recover `this` from
    // z_stream::opaque). Reconstructed at semantic parity, not byte-matched.
    class Decompressor
    {
    public:
        // 0x82ACCB48 -- restore the saved stream, inflate every entry, copy the stream back.
        // Returns the destination of the final snapshot copy (the X360 returns memcpy's result).
        void* Execute(DecompressionJobData* lpJobData);

    private:
        // 0x82ACC9E0 -- (re)initialise mStream for the current entry and inflateInit_ it.
        s32 BeginDecompressingFirstEntry();
        // 0x82ACCA90 -- advance to the next entry, then (re)initialise + inflateInit_.
        s32 BeginDecompressingNextEntry();

        // 0x82ACC9C0 / 0x82ACC9D8 -- zlib zalloc/zfree trampolines onto the job's HeapMalloc.
        static void* CompressorAllocateCallback(void* lpOpaque, uInt luItems, uInt luSize);
        static void  CompressorFreeCallback(void* lpOpaque, void* lpAddress);

        DecompressionJobData* mpJobData;       // +0x000  the job being run (set by Execute)
        u8                    maPad0[0x7C];    // +0x004  (unused by the inflate path)
        z_stream              mStream;         // +0x080  the working zlib stream (56B)
        u32                   muAmountRead;    // +0x0B8
        u32                   muAmountWritten; // +0x0BC
        s32                   miLastInflateResult; // +0x0C0  zlib status of the last inflate
        u8                    maPad1[0x3C];    // +0x0C4  (heap cache, untouched by inflate)
        CompressedData*       mpEntries;       // +0x100  current entry array
        u32                   muNumEntries;    // +0x104  entry count
        u32                   muCurrentEntry;  // +0x108  current entry index
        CgsMemory::HeapMalloc* mpHeapMalloc;   // +0x10C  heap the zlib callbacks allocate through
    };

    // 0x82ACCCA0 -- the EA::Jobs local-job entry point the decompression job runs. The
    // scheduler invokes it on a worker thread with the job's data pointer; it builds a
    // Decompressor on the worker stack and runs it over the DecompressionJobData. The
    // address is handed to EA::Jobs::EntryPoint::SetCode by DecompressionJobInterface::
    // RunFlushJobs. Declared with the job's data pointer as a plain argument (the EA
    // local-job ABI passes it in the first parameter slot).
    void DecompressionJobEntry(void* lpvJobData);
}
