// ============================================================================
// GameShared/GameClasses/Memory/DataStream/CgsSimpleDataStreamProducer_ResultIterator.cpp
//
// ⭐ SLICE TU (ground wave, 2026-08-10): the SimpleDataStreamResultIterator read cursor.
// Same reason CgsSimpleDataStreamProducer_Begin.cpp exists -- the home TU
// (CgsSimpleDataStreamProducer.cpp) is still unmountable because its Construct calls the
// declared-only DataStreamCommandPoster::Construct, and the traction-line harvest
// (VehicleManager::ReadRaceCarTractionLineTestResults @0x82618058) needs the cursor.
// GetCurrent was MOVED here from the home TU, not copied; fold both back when the home mounts.
//
//   SimpleDataStreamResultIterator::GetCurrent  @ X360 0x825B29A0 (out-of-line, exported)
//   SimpleDataStreamResultIterator::GetNext     -- inlined by the console at its call sites
// ============================================================================

#include "GameShared/GameClasses/Memory/DataStream/CgsSimpleDataStreamProducer.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"  // CGS_ASSERT

namespace CgsMemory
{
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

    // No out-of-line X360 emission -- the console inlines this. The shape is read off one of the
    // inline sites, VehicleManager::ReadRaceCarTractionLineTestResults @0x82618204..0x82618210:
    //     lwz r11, 0(r30) ; addi r11, r11, 1 ; stw r11, 0(r30)   <- ++miResultIndex
    //     bl  SimpleDataStreamResultIterator::GetCurrent          <- and return its answer
    // i.e. the bump happens FIRST and GetCurrent then re-validates the new cursor against the
    // parent's command count (so a walk past the end yields null rather than an out-of-range read).
    const void* SimpleDataStreamResultIterator::GetNext()
    {
        ++miResultIndex;
        return GetCurrent();
    }
}
