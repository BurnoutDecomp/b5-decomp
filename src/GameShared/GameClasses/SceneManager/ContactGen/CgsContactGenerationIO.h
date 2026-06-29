#pragma once

// ===========================================================================
// Contact-generation / spatial-partition IO buffer payloads referenced by
// CgsSceneManager::SceneManagerModule's per-frame passes (UpdateContactGeneration,
// ProcessFrustumTestJobResults).
//
// These buffers are pushed/popped on a CgsModule::IOBufferStack via the
// CreateIOBuffer<T>/DestroyIOBuffer<T> member templates, which placement-construct
// and sizeof() the payload -- so the template needs a COMPLETE type. Each payload's
// full layout has a real home in its own module-IO TU (SpatialPartitionManagerIO /
// OverlapCullingModuleIO / OverlapGenerationModuleIO / ContactGenerator); those TUs
// are not reconstructed yet, and pulling their full event-queue trees in here would
// cascade most of the SceneManager into this TU's compile.
//
// Per the AGENTS.md forward-declaration exception (a complete type is needed but no
// reconstructable reference exists without a whole-program header cascade), each is
// modelled here as a documented, sized POD stand-in: enough to placement-new and
// size on the stack allocator, with the real layout owned by its IO TU. Sizes are
// the DWARF/X360 buffer budgets where known, else a conservative cache-line block.
// This mirrors the existing SceneFineLineTestQueue sized-blob precedent in
// CgsSceneManagerModuleIO.h. FLAG: these sizes are layout stand-ins, not pinned.
// ===========================================================================

#include "types.hpp"
#include "GameShared/GameClasses/Module/CgsIOBuffer.h"  // CgsModule::IOBuffer (base)

namespace CgsSceneManager
{

namespace SpatialPartitionIO
{
    // The coarse-query result buffer the loose-octree frustum jobs fill. Carries the
    // CoarseQueryResultBuffer<16384> the SceneManagerModule reads job results out of.
    struct alignas(16) OutputBuffer : public CgsModule::IOBuffer
    {
        u8 maReserved[65536];   // stand-in (real home: SpatialPartitionManagerIO TU)
    };
}

namespace OverlapCullingIO
{
    struct alignas(16) InputBuffer  : public CgsModule::IOBuffer { u8 maReserved[4096]; };
    struct alignas(16) OutputBuffer : public CgsModule::IOBuffer { u8 maReserved[4096]; };
}

namespace OverlapGenerationIO
{
    struct alignas(16) OutputBuffer : public CgsModule::IOBuffer { u8 maReserved[4096]; };
}

namespace ContactGenerator
{
    struct alignas(16) QueryAccumulator : public CgsModule::IOBuffer { u8 maReserved[4096]; };
}

}
