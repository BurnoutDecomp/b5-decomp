// ⭐ THIS TU MOUNTS as of 2026-08-10 (ground wave). It was held off the link since 2026-08-06 by a
// single unresolved edge -- its Construct calls DataStreamCommandPoster::Construct @0x82869E08,
// which is an export-set hole -- and that edge is now carried by the loud dead trap in
// CgsDataStreamCommandPoster_LinkStub.cpp, exactly as CgsCollisionGenerator_StreamStubs.cpp carries
// the collide-stream family's. Nothing here is reachable yet; the mount is for LINK CLOSURE.
// GetCurrent/GetNext live in the sibling slice CgsSimpleDataStreamProducer_ResultIterator.cpp.

#include "GameShared/GameClasses/Memory/DataStream/CgsSimpleDataStreamProducer.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"  // CGS_ASSERT

namespace CgsMemory
{
    // X360 0x8286A3B0.
    // Sizes and constructs the embedded command poster over the caller's command
    // buffer, then caches the command/result geometry the producer needs. The
    // command buffer is rounded up to a 128-byte multiple ((maxCommands*commandSize
    // + 127) & ~127); the per-result stride is likewise 128-byte aligned and cached
    // as miAlignedResultSize.
    //
    // Observed in the X360 store sequence:
    //   mCommandPoster.Construct(lpCommandBuffer, alignedCmdBufSize, liCommandSize,
    //                            0, nullptr, 0, 0)
    //   miCommandSize       = liCommandSize
    //   miMaxCommands       = liMaxCommands
    //   miResultSize        = liResultSize
    //   miAlignedResultSize = (liResultSize + 127) & ~127
    //   mpResultBuffer      = lpResultBuffer
    //   miMaxResults        = 0   (NOTE: the liMaxResults parameter is NOT consumed
    //                              by Construct in the X360 code -- the field is
    //                              zero-initialised here and populated elsewhere)
    //   mResultIterator.mpParent = this
    //   mbIsStreaming       = false
    //   miNumAddedCommands  = 0
    void SimpleDataStreamProducer::Construct(s32 liMaxCommands, s32 liCommandSize,
                                             void* lpCommandBuffer, s32 liMaxResults,
                                             s32 liResultSize, void* lpResultBuffer)
    {
        (void)liMaxResults;  // unused in the X360 Construct (see header note above)

        const s32 liCommandBufferSize = (liMaxCommands * liCommandSize + 127) & ~127;

        mCommandPoster.Construct(lpCommandBuffer, liCommandBufferSize, liCommandSize,
                                 0, nullptr, 0, 0);

        miMaxCommands            = liMaxCommands;
        miCommandSize            = liCommandSize;
        miResultSize             = liResultSize;
        miAlignedResultSize      = (liResultSize + 127) & ~127;
        mpResultBuffer           = lpResultBuffer;
        miMaxResults             = 0;
        mResultIterator.mpParent = this;
        mbIsStreaming            = false;
        miNumAddedCommands       = 0;
    }

    // X360 0x8280FFF0.
    // Computes the two backing-buffer sizes a producer needs for the requested
    // geometry, both rounded up to a 128-byte boundary:
    //   *lpuOutCommandBufferSize = round128(liMaxCommands * liCommandSize)
    //   *lpuOutResultBufferSize  = round128(round128(liResultSize) * liMaxResults)
    // The command size must be a positive multiple of 16; otherwise the assert
    // tripwire fires ("Invalid command size:" + the offending value).
    //
    // Asm: r29=liCommandSize validated as (liCommandSize % 16 == 0 && liCommandSize > 0);
    // command size = ((liMaxCommands*liCommandSize)+0x7F) & ~0x7F stored to r24;
    // result size = (((liResultSize+0x7F)&~0x7F)*liMaxResults + 0x7F) & ~0x7F stored to r23.
    void SimpleDataStreamProducer::GetRequiredBufferSizes(
            s32 liMaxCommands, s32 liCommandSize,
            s32 liMaxResults, s32 liResultSize,
            u32* lpuOutCommandBufferSize, u32* lpuOutResultBufferSize)
    {
        CGS_ASSERT(liCommandSize % 16 == 0 && liCommandSize > 0,
                   "Invalid command size: \n");

        const u32 luAlignedResultSize =
            (static_cast<u32>(liResultSize) + 0x7Fu) & ~0x7Fu;

        *lpuOutCommandBufferSize =
            (static_cast<u32>(liMaxCommands * liCommandSize) + 0x7Fu) & ~0x7Fu;
        *lpuOutResultBufferSize =
            (luAlignedResultSize * static_cast<u32>(liMaxResults) + 0x7Fu) & ~0x7Fu;
    }

    // ⭐ ADDED 2026-08-10 (cache-fill wave). Both are INLINED by the X360 at every call
    // site, so neither carries a symbol; the shapes are read off the sites the triangle-cache
    // fill uses.
    //
    // AllocateCommand -- TriangleCacheManager::StartUpdateTriangleCaches @0x828BEE84..0x828BEE8C:
    //     addi r3, producer, 0x80          <- &mCommandPoster
    //     bl   DataStreamCommandPoster::AllocateCommand
    //     stw  r3, 0x104(producer)         <- miNumAddedCommands = <the poster's new count>
    // ⚠️ FLAG: the producer-level RETURN value is not observed at that site (the console drops
    // r3 after the store). Modelled as the freshly stored count, mirroring the sibling
    // AddCommand shape; nothing in this tree reads it yet.
    s32 SimpleDataStreamProducer::AllocateCommand(void** lppOutBuffer)
    {
        miNumAddedCommands = mCommandPoster.AllocateCommand(lppOutBuffer);
        return miNumAddedCommands;
    }

    // End -- TriangleCacheManager::EndUpdateTriangleCaches @0x828BF1AC..0x828BF1B8:
    //     lwz r31, 0x30(this) ; addi r3, r31, 0x80 ; bl DataStreamCommandPoster::End
    //     li  r20, 0          ; stb r20, 0x100(r31)
    // i.e. the exact mirror of Begin() (which is CgsSimpleDataStreamProducer_Begin.cpp): close the
    // embedded poster, then drop the streaming flag. Unlike Begin it publishes nothing -- the
    // shared snapshot is left as the streaming side last saw it, which is what lets the result
    // iterator still size itself afterwards.
    void SimpleDataStreamProducer::End()
    {
        mCommandPoster.End();
        mbIsStreaming = false;
    }

    // GetResultIterator -- TriangleCacheManager::EndUpdateTriangleCaches @0x828BF1F0:
    //     ld  r11, 0x38(producer) ; std r11, <local>
    // i.e. an 8-byte BY-VALUE copy of mResultIterator (s32 miResultIndex + the 4-byte console
    // parent pointer) into the caller's frame. The caller then walks the COPY, so the
    // producer's own cursor is left untouched.
    SimpleDataStreamResultIterator SimpleDataStreamProducer::GetResultIterator() const
    {
        return mResultIterator;
    }

    // ⭐ MOVED OUT 2026-08-10 (ground wave): SimpleDataStreamResultIterator::GetCurrent
    // (X360 0x825B29A0) now lives in the mountable slice
    // CgsSimpleDataStreamProducer_ResultIterator.cpp, alongside GetNext, because the traction-line
    // harvest needs it and THIS home TU is still unmountable (its Construct calls the
    // declared-only DataStreamCommandPoster::Construct). Same reason CgsSimpleDataStream-
    // Producer_Begin.cpp exists. It was MOVED, not copied -- one definition, no LNK2005 --
    // and folds back here when this TU mounts.
}
