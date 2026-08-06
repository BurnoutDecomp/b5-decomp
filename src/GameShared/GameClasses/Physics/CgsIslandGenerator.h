#pragma once

// CgsPhysics::IslandGenerator (DWARF CgsIslandGenerator.h:33, declared at CgsRigidBody.h:24)
// -- the per-tick union-find scratch buffer PhysicsSimulationModule::Update carves out of the
// frame's IOBufferStack and hands to ActiveSetClosure.
//
// =====================================================================================
// ⭐⭐ RECONSTRUCTED AS A TYPE ONLY, AND THAT IS THE HONEST UNIT -- 2026-08-06.
//
// The brief that commissioned this file said ActiveSetClosure @0x828A0808 "inlines the
// CgsIslandGenerator union-find". IT DOES NOT. Read off the shipped X360 body:
//   * r4 (this argument) is DEAD ON ENTRY -- the prologue saves r5/r6/r7 (the three limits)
//     and never touches r4; no instruction in the 1,184 dereferences the generator;
//   * the closure's own mechanism is the module's mpiNextIndex chains + the three BitArrays
//     (see the body in CgsPhysicsSimulationModule.cpp) -- no Init/RelateBodies/
//     FlattenIslandTree shape appears anywhere in it;
//   * the DWARF's real consumers of this class, PhysicsSimulationModule::UpdateFreezing
//     (.cpp:2435, takes an IslandGenerator*) and DebugRender (.cpp:3190), are ABSENT from
//     the X360 image -- zero ledger entries, zero references from the module TU's dossier,
//     no address anywhere: dead-stripped, PS3/DecFIGS-only. Update creates the buffer,
//     passes it dead, and destroys it.
// Per the standing DWARF-gating rule (AGENTS.md: "the X360 ledger decides what exists"),
// the DWARF's method set -- Construct/Prepare/Release/Destruct, CreateIslandFromBody,
// RelateBodies, FlattenIslandTree, GetIsland, IsRootIsland, SetParentIsland, FindRootIsland,
// and IslandData's Prepare/Init/accessors -- is therefore deliberately NOT declared: not one
// has an X360 body or caller, and declaring PS3-only methods without bodies would be exactly
// the hollow-shell shape this campaign keeps re-finding. A minimal, layout-true type is the
// correct reconstruction. (If a future wave wants the union-find, the DecFIGS PS3 export set
// carries real bodies to transcribe -- do not invent them from the DWARF hints.)
//
// ⭐ THE LAYOUT IS X360-PINNED ALL THE SAME, by the one witness the image does carry:
// CgsModule::IOBufferStack::CreateIOBuffer<CgsPhysics::IslandGenerator> @0x8289E0D0 is a raw
//     `bl IOBufferStack::Alloc(this, 0x4B2, lpcName)`
// and the Destroy twin @0x8289E190 frees the same 0x4B2. 0x4B2 == 1202 ==
//     sizeof(IOBuffer status byte, padded to align 2) + 200 * 4 (maBodyIslandData)
//                                                     + 200 * 2 (mau16ParentIsland),
// which confirms KI_MAX_NUM_BODIES == 200 and both element widths exactly as the DWARF
// declares them. The type is pointer-free, so the host size is the console size; pinned
// below. ⚠️ The console template does NOT run a constructor over the block (raw Alloc; the
// DWARF design initialises lazily through Prepare()); the committed PC CreateIOBuffer<T>
// placement-news T(), which value-initialises the block to zero -- a benign superset on a
// buffer nothing reads, noted for the record.
// =====================================================================================

#include "types.hpp"
#include "GameShared/GameClasses/Module/CgsIOBuffer.h"   // CgsModule::IOBuffer (1-byte FlagSet status base)

#include <cstddef>   // offsetof (layout pins below)

namespace CgsPhysics
{
    struct IslandGenerator : public CgsModule::IOBuffer
    {
        // DWARF CgsIslandGenerator.h:70. One per body slot: the slot's body index and its
        // union-find rank, u16 each (the invalid sentinels are 65535 -- DWARF :72/:73/:74).
        struct IslandData
        {
            static const u16 KU16_INVALID_BODY_INDEX   = 65535;   // DWARF :72
            static const u16 KU16_INVALID_PARENT_INDEX = 65535;   // DWARF :73
            static const u16 KU16_INVALID_RANK         = 65535;   // DWARF :74

            u16 mu16BodyIndex;   // DWARF :122
            u16 mu16Rank;        // DWARF :124
        };

        static const s32 KI_MAX_NUM_BODIES = 200;   // DWARF :66 -- confirmed by the 0x4B2 Alloc

        IslandData maBodyIslandData[KI_MAX_NUM_BODIES];    // DWARF :127  console @+0x002
        u16        mau16ParentIsland[KI_MAX_NUM_BODIES];   // DWARF :128  console @+0x322
    };

    // The 0x4B2 pin -- the one X360-attested fact about this type (Alloc/Free literal in the
    // CreateIOBuffer/DestroyIOBuffer instantiations @0x8289E0D0/@0x8289E190). Pointer-free,
    // so it holds verbatim on the host.
    static_assert(sizeof(IslandGenerator) == 0x4B2, "IslandGenerator == 1202 (the X360 IOBufferStack::Alloc literal)");
    static_assert(offsetof(IslandGenerator, maBodyIslandData)  == 0x002, "maBodyIslandData after the 1-byte IOBuffer base, align 2");
    static_assert(offsetof(IslandGenerator, mau16ParentIsland) == 0x322, "mau16ParentIsland == 2 + 200*4");
}
