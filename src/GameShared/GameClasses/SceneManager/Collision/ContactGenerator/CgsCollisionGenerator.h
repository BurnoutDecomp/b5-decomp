#pragma once

// GameShared/GameClasses/SceneManager/Collision/ContactGenerator/CgsCollisionGenerator.h
//
// CgsSceneManager::CgsCollision::BaseCollisionGenerator -- an IOBuffer-derived contact
// generator. It owns a ring of collision "batches" (each an EA::Jobs::Job + descriptor,
// CgsCollisionBatch.h), a set of collision-result lists carved from a LinearMalloc bump
// allocator, and the used-slot bookkeeping that drives the async collide/test family. This
// header is the owning home DecFIGS attributes the class to (CgsCollisionGenerator.h:63); it
// is grown ADDITIVELY -- only the members + the lifecycle/batch methods reconstructed so far
// are declared. The remaining Collide*/Test*/Run* API (declared in the DWARF) lands as its
// TUs are worked; declaring them now would pull in the (as-yet-unrecovered) geometry list /
// spatial-map / result-list parameter types and break the compile gate.
//
// Member NAMES/TYPES: DecFIGS DWARF (CgsCollisionGenerator.h). Member COUNTS/OFFSETS: X360
// ARTIST asm (ground truth, arbitrates over the DWARF where they differ):
//   * KU16_MAX_NUM_BATCHES is 64 on X360 (Finish loops i<0x40; CreateNewBatch masks %64 via
//     an srawi/slwi<<6; Prepare zeroes 64 used-flag bytes and constructs 64 jobs). The DWARF,
//     the older PS3/DecFIGS shape, says 32 -- the X360 build widened the ring to 64.
//   * X360 byte layout (this=r3): maCollisionBatches @ +0x80 (stride 1152), then
//     mapCollisionResultLists[200] (@+0x12080, 4-byte console ptrs), mCollisionResultsAllocator
//     @ +0x123A0, mu16NumUsedResultLists @ +0x123BC, mu16NumUsedBatches @ +0x123BE,
//     mabUsedBatches[64] @ +0x123C0. Per the project rule the PC compile does NOT reproduce
//     byte offsets (pointers widen to 8 bytes) -- this is semantic-parity-by-named-member.
//     The ~0x80 of members between the 1-byte IOBuffer base and maCollisionBatches are not
//     named in the DWARF and are not touched by any reconstructed method here, so they are
//     honestly omitted rather than invented.

#include "types.hpp"
#include <cstddef>

#include "GameShared/GameClasses/Module/CgsIOBuffer.h"                                  // CgsModule::IOBuffer (base)
#include "GameShared/GameClasses/Memory/CgsLinearMalloc.h"                              // CgsMemory::LinearMalloc (by value)
#include "GameShared/GameClasses/SceneManager/Collision/ContactGenerator/CgsCollisionBatch.h" // CollisionBatch (by value)

// Producer factory return type (pointer-only use in this header); full layout in
// Memory/DataStream/CgsSimpleDataStreamProducer.h, included by the .cpp.
namespace CgsMemory { struct SimpleDataStreamProducer; }

namespace EA { namespace Jobs { struct Job; } }

namespace CgsSceneManager
{
namespace CgsCollision
{
    // Result-list pointer array element; only referenced by pointer here (its full layout
    // lives in Collision/Primitives/CgsCollisionResult.h). Forward-declared to avoid a header
    // cascade for a pointer-only member.
    struct CollisionResultList;

    // DWARF CgsCollisionGenerator.h:63.
    struct BaseCollisionGenerator : public CgsModule::IOBuffer
    {
        // --- capacities (DWARF h:314/318/320; batch count corrected to the X360 ring size) ---
        static const u16 KU16_MAX_NUM_RESULTS      = 2000; // h:314
        static const u16 KU16_MAX_NUM_BATCHES      = 64;   // h:318 says 32 (PS3); X360 asm = 64
        static const u16 KU16_MAX_NUM_RESULT_LISTS = 200;  // h:320

        // --- lifecycle -----------------------------------------------------------------
        void Construct();                        // h:67  / X360 0x828105F8
        void Destruct();                         // h:70  / X360 0x8284CB38 (empty; ICF-folded)
        bool Prepare(void* lpResultBuffer, s32 liResultBufferSize); // h:73 / X360 0x82810660
        void Finish();                           // h:80  / X360 0x828128D8

        // Copy of the luIndex'th result list (by value, bounds-checked against
        // mu16NumUsedResultLists). X360 0x825B2AE0. (Incomplete return type is fine
        // for a declaration — CollisionResultList is forward-declared above and the
        // definition in the .cpp includes its owning header.)
        CollisionResultList GetResultList(u16 luIndex) const;   // X360 0x825B2AE0

        // Allocate + construct a SimpleDataStreamProducer for a streamed collision pass out of
        // the result allocator (128-byte aligned, alignment saved/restored around the burst).
        // X360 0x828109F8 (ledger identity "Crea" -- the IDA symbol is truncated; this is the
        // producer-factory it names). Sizes both the command and result buffers via
        // SimpleDataStreamProducer::GetRequiredBufferSizes before constructing.
        CgsMemory::SimpleDataStreamProducer* CreateStreamProducer(s32 liMaxCommands);

    private:
        u16  CreateNewBatch();                   // h:350 / X360 0x82810960
        void FinishBatch(u16 lu16BatchIndex);    // h:353 / X360 0x82810718

        // Allocate + placement-construct one standalone (empty) EA::Jobs::Job out of the result
        // allocator (128-byte aligned, alignment saved/restored). Returns null if the bump
        // allocator overflows. X360 0x82810588. The Run* dispatch family uses the returned job
        // as the parent/root the per-batch jobs depend on.
        EA::Jobs::Job* AllocateJob();

    private:
        // Layout in DWARF declaration order (see file header for X360 offsets).
        CollisionBatch          maCollisionBatches[KU16_MAX_NUM_BATCHES];        // h:322  @+0x80
        CollisionResultList*    mapCollisionResultLists[KU16_MAX_NUM_RESULT_LISTS]; // h:323 @+0x12080
        CgsMemory::LinearMalloc mCollisionResultsAllocator;                      // h:324  @+0x123A0
        u16                     mu16NumUsedResultLists;                          // h:325  @+0x123BC
        u16                     mu16NumUsedBatches;                              // h:326  @+0x123BE
        bool                    mabUsedBatches[KU16_MAX_NUM_BATCHES];            // h:327  @+0x123C0

        // Static "register StartJobs perfmon once" latch (DWARF h:329/330; X360 statics
        // byte_83011D70 / dword_82F310B4).
        static bool _mbInitializedPerfMons;      // h:329
        static s32  _miStartJobsPerfMon;         // h:330
    };

    // ⭐ ADDED 2026-08-06 (PhysicsModule::Update leaves wave). The FULL-SIZE derived generator --
    // the type VehicleManager owns through mpContactGenerator and destroys in FreeAllocations
    // @0x8261BAE0 (whose bl target's mangled name pins the exact qualified type:
    // ??$DestroyIOBuffer@VCollisionGenerator@CgsCollision@CgsSceneManager@@@...). Verbatim from the
    // DecFIGS DWARF (CgsCollisionGenerator.h:382..:391): the base plus one 2 MB results arena.
    // CROSS-CHECK: the console DestroyIOBuffer<CollisionGenerator> instantiation @0x8259DE50 frees
    // exactly 2171904 bytes == 0x12400 (base span) + 0x200000 (this arena) -- the two sources agree.
    // Prepare (DWARF :386) is the no-arg override that feeds the arena to the base's two-arg
    // Prepare; declare-only until its own TU lands (X360 body not yet identified).
    struct CollisionGenerator : public BaseCollisionGenerator
    {
        bool Prepare();                                            // DWARF :386

    private:
        static const s32 KI_RESULTS_MEMORY_SIZE = 2097152;         // DWARF :390
        u8 mau8CollisionResultsMemory[KI_RESULTS_MEMORY_SIZE];     // DWARF :391
    };
}
}
