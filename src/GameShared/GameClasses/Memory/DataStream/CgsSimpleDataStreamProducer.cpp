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

    // X360 0x825B29A0.
    // Returns a pointer to the result record the iterator currently points at.
    // The record address is mpResultBuffer + miAlignedResultSize * miResultIndex
    // (results are stored at the aligned stride). If the cursor has reached the
    // end (miResultIndex >= parent->miNumAddedCommands) the cursor is clamped to
    // the count and a null pointer is returned.
    //
    // Asm: a1[1]=mpParent checked non-null ('No parent'); mpParent+0x100=mbIsStreaming
    // checked false ('Parent is streaming'); then compares miResultIndex (a1[0]) with
    // mpParent[0x104]=miNumAddedCommands; else-branch computes
    // miAlignedResultSize(0x30)*miResultIndex + mpResultBuffer(0x34).
    const void* SimpleDataStreamResultIterator::GetCurrent()
    {
        CGS_ASSERT(mpParent != nullptr, "No parent\n");
        CGS_ASSERT(!mpParent->mbIsStreaming, "Parent is streaming\n");

        SimpleDataStreamProducer* const lpParent = mpParent;
        const s32 liNumAddedCommands = lpParent->miNumAddedCommands;

        if (miResultIndex >= liNumAddedCommands)
        {
            miResultIndex = liNumAddedCommands;
            return nullptr;
        }

        return reinterpret_cast<const char*>(lpParent->mpResultBuffer)
             + lpParent->miAlignedResultSize * miResultIndex;
    }
}
