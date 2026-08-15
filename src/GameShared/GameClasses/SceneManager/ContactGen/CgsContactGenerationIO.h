#pragma once

// ===========================================================================
// Contact-generation / spatial-partition IO buffer payloads referenced by
// CgsSceneManager::SceneManagerModule's per-frame passes (UpdateContactGeneration,
// ProcessFrustumTestJobResults).
//
// These buffers are pushed/popped on a CgsModule::IOBufferStack via the
// CreateIOBuffer<T>/DestroyIOBuffer<T> member templates, which placement-construct and
// sizeof() the payload -- and, as of 2026-08-15, CreateIOBuffer<T> runs T::Construct and
// DestroyIOBuffer<T> runs T::Destruct, so the template needs a COMPLETE type. None of the
// stand-ins below declares either, so both resolve to the CgsModule::IOBuffer base -- which
// for QueryAccumulator matches the console's bare-Alloc instantiation @0x828AE7F0 except for
// the base's status byte, a store the console never makes (see the base-only-Construct policy
// in CgsIOBufferStack.h). Each payload's
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
#include "GameShared/GameClasses/SceneManager/SpatialPartitionModule/CgsSpatialPartitionManagerIO.h"  // SpatialPartitionIO::OutputBuffer (real home)

namespace CgsSceneManager
{

// (SpatialPartitionIO::OutputBuffer's 65536-byte stand-in RETIRED 2026-07-28, culling
//  wave: the REAL definition -- the query-result VEQ plus the named
//  CoarseQueryResultBuffer<16384> at +0x5014 -- now lives in its own home,
//  SpatialPartitionModule/CgsSpatialPartitionManagerIO.h, included at the top of
//  this header so every consumer keeps seeing the type by name.)

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
