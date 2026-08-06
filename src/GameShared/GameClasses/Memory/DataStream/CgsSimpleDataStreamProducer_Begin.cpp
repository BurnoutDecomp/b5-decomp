// ============================================================================
// GameShared/GameClasses/Memory/DataStream/CgsSimpleDataStreamProducer_Begin.cpp
//
// ⭐ SLICE TU (big-five #2 wave, 2026-08-06): SimpleDataStreamProducer::Begin.
// The home TU (CgsSimpleDataStreamProducer.cpp) stays UNMOUNTED -- its Construct calls the
// still-declared-only DataStreamCommandPoster::Construct -- so the one method the mounted
// contact-generation chain needs lives here. Fold back when the home mounts.
//
// The X360 has no out-of-line emission: VehicleManager::StartVehicleContactGeneration
// @0x8262AEE8 INLINES this body three times (asm 0x8262BE10.., one per stream producer):
//   * publish the private stream geometry into the shared snapshot
//     (mShared.{miMaxCommands,miCommandSize,miMaxResults,miResultSize,miAlignedResultSize,
//     mpResultBuffer} <- the same-named private members -- the six lwz/stw pairs
//     +32..+52 -> +0..+20),
//   * bind the shared poster pointer to the embedded poster (+24 <- this+128),
//   * raise mbIsStreaming (+256 <- 1),
//   * then DataStreamCommandPoster::Begin @0x82867AE8.
// The PS3 DecFIGS keeps it out-of-line behind its thunks (sub_4BFEC8 family), same shape.
// ============================================================================

#include "GameShared/GameClasses/Memory/DataStream/CgsSimpleDataStreamProducer.h"

namespace CgsMemory
{
    void SimpleDataStreamProducer::Begin()
    {
        mShared.miMaxCommands       = miMaxCommands;
        mShared.miCommandSize       = miCommandSize;
        mShared.miMaxResults        = miMaxResults;
        mShared.miResultSize        = miResultSize;
        mShared.miAlignedResultSize = miAlignedResultSize;
        mShared.mpResultBuffer      = mpResultBuffer;
        mShared.mpPoster            = &mCommandPoster;

        mbIsStreaming = true;

        mCommandPoster.Begin();
    }
}
