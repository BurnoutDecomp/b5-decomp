// ============================================================================
// GameShared/GameClasses/Memory/DataStream/CgsDataStreamCommandPoster_LinkStub.cpp
//
// ⚠️⚠️ TRAP-STUB TU (closure enforcement, ground wave 2026-08-10), same precedent and same rules as
// CgsCollisionGenerator_StreamStubs.cpp: ONE symbol, loud, dead, and named with its address.
//
//   CgsMemory::DataStreamCommandPoster::Construct @0x82869E08
//
// It is the ONE unresolved edge that has kept the whole SimpleDataStreamProducer home TU -- and
// with it every BaseCollisionGenerator stream factory -- off the link since 2026-08-06. It is an
// EXPORT-SET HOLE, verified both ways rather than assumed: there is no JSON at 0x82869E08 (the
// export dir goes 0x82869C00 -> 0x8286A0D0), and nothing of that name is anywhere in a name index
// over all 30,084 exports -- yet the address itself is certain, because
// SimpleDataStreamProducer::Construct @0x8286A3B0's own xrefs_from names it there. Its body is
// image-only and this wave did not have a PPC decoder able to lift a whole store sequence.
//
// ⛔ IT IS A TRAP, NOT A NO-OP, ON PURPOSE. A silent empty Construct would leave a poster with a
// null command buffer and a zero stride, and the first AllocateCommand would hand out a plausible
// pointer into nothing -- the silent-drop shape this project keeps paying for. Anything that
// reaches it says so and stops.
//
// DEAD TODAY: its only caller is SimpleDataStreamProducer::Construct, whose only callers are the
// BaseCollisionGenerator stream factories, whose only callers are the still-gated contact and
// traction-line generation legs. RECONSTRUCT AND DELETE THIS FILE -- LNK2005 is the tripwire.
// ============================================================================

#include "GameShared/GameClasses/Memory/DataStream/CgsDataStreamCommandPoster.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

namespace CgsMemory
{
    void DataStreamCommandPoster::Construct(void* /*lpCommandBuffer*/, s32 /*liCommandBufferSize*/,
                                            s32 /*liCommandSize*/, s32 /*liInitialCommandCount*/,
                                            void* /*lpDataBuffer*/, s32 /*liDataBufferSize*/,
                                            s32 /*liInitialDataBufferUsed*/)
    {
        CGS_ASSERT(false,
                   "TRAP: CgsMemory::DataStreamCommandPoster::Construct @0x82869E08 is not "
                   "reconstructed (export-set hole, image-only). A stream was constructed over an "
                   "un-initialised poster.\n");
    }
}
