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
#include "GameShared/GameClasses/SceneManager/ContactGen/CgsOverlapGenerationModuleIO.h"  // OverlapGenerationIO::{InputBuffer,OutputBuffer} (real home)
#include "GameShared/GameClasses/SceneManager/ContactGen/CgsOverlapCullingModuleIO.h"     // OverlapCullingIO::{InputBuffer,OutputBuffer} (real home, wave Q5)

namespace CgsSceneManager
{

// (SpatialPartitionIO::OutputBuffer's 65536-byte stand-in RETIRED 2026-07-28, culling
//  wave: the REAL definition -- the query-result VEQ plus the named
//  CoarseQueryResultBuffer<16384> at +0x5014 -- now lives in its own home,
//  SpatialPartitionModule/CgsSpatialPartitionManagerIO.h, included at the top of
//  this header so every consumer keeps seeing the type by name.)

// (OverlapCullingIO's two 4096-byte stand-ins RETIRED 2026-08-18, wave Q5: the real
//  InputBuffer / OutputBuffer live in ContactGen/CgsOverlapCullingModuleIO.h, included at the top.)

// (OverlapGenerationIO's 4096-byte OutputBuffer stand-in RETIRED 2026-08-18, wave Q5
//  cluster D1: the REAL OutputBuffer -- CgsModule::IOBuffer plus the single
//  EventQueue<OverlappingPair,16384> the console allocates 262,160 bytes for -- now lives
//  in its own console-named home, ContactGen/CgsOverlapGenerationModuleIO.h, included at
//  the top of this header so every consumer keeps seeing the type by name. That header is
//  also the real home of OverlapGenerationIO::InputBuffer and its four event types, which
//  used to be a partial stand-in inside CgsOverlapGenerationModule.h.)

namespace ContactGenerator
{
    struct alignas(16) QueryAccumulator : public CgsModule::IOBuffer { u8 maReserved[4096]; };
}

}
