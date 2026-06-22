#include "GameShared/GameClasses/Memory/DataStream/CgsSimpleDataStreamProducer.h"

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
}
