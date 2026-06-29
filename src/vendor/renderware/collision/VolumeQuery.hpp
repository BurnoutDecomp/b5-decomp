#pragma once

// rw::collision::VolumeVolumeQuery / VolumeLineQuery — the two RenderWare collision
// query objects the CgsSceneManager FineIntersectionTestModule embeds. This header
// only declares the construction-time entry points the module calls
// (GetResourceDescriptor / Initialize) plus the runtime entry points the fine query
// loops use. The full query layouts are reconstructed in their own vendor TUs
// (e.g. SDKs/EATech/rwcollision/volumelinequery.cpp); this declaration-only header
// lets dependent TUs call the entry points without pulling the whole implementation.
//
// Ground truth (X360 BURNOUT_X360_ARTIST.XEX):
//   rw::collision::VolumeVolumeQuery::GetResourceDescriptor (called @ Construct 0x828B0C44)
//   rw::collision::VolumeVolumeQuery::Initialize            (called @ Construct 0x828B0CA8)
//   rw::collision::VolumeLineQuery::GetResourceDescriptor   @ 0x82BB3838
//   rw::collision::VolumeLineQuery::Initialize              @ 0x82BB3888
//   rw::collision::VolumeLineQuery::GetAllIntersections     @ 0x82BB3820
//
// At the FineIntersectionTestModule::Construct call sites the descriptor/initialize
// entry points are invoked with the descriptor-output / buffer-table pointer in r3 and
// the volume/result counts in r4/r5 (no implicit `this`): they behave as static
// factory entry points that partition the caller-provided backing buffer. The buffer
// table (`void** ppBuffer`) holds the backing-store base at [0] with [1..4] zeroed.

#include "types.hpp"

namespace rw
{
namespace collision
{
    // The query objects are constructed in place inside a caller-owned backing buffer;
    // the module keeps the returned handle (the buffer base interpreted as the query).
    class VolumeVolumeQuery
    {
    public:
        // Fills the 5-entry rw::ResourceDescriptor block at lpOut with the backing-buffer
        // size/alignment needed for liVolumes volumes and liResults results.
        static void* GetResourceDescriptor(void* lpOut, int liVolumes, int liResults);

        // Partitions the backing buffer (*lppBuffer at [0]) into the query's working
        // arrays and returns the constructed query handle.
        static void* Initialize(void** lppBuffer, int liVolumes, int liResults);
    };

    class VolumeLineQuery
    {
    public:
        static void* GetResourceDescriptor(void* lpOut, int liVolumes, int liResults);
        static void* Initialize(void** lppBuffer, int liVolumes, int liResults);
    };
}
}
