#pragma once

// The 1202-byte scratch buffer allocated by PhysicsSimulationModule::Update.
// ARTIST CreateIOBuffer<IslandGenerator> at 0x8289E0D0 and its Destroy twin
// at 0x8289E190 pass 0x4B2 to Alloc and Free. ActiveSetClosure does not
// dereference its IslandGenerator argument in the Xenon build.

#include "types.hpp"
#include "GameShared/GameClasses/Module/CgsIOBuffer.h"   // CgsModule::IOBuffer (1-byte FlagSet status base)

#include <cstddef>   // offsetof (layout pins below)

namespace CgsPhysics
{
    // The original CreateIOBuffer/DestroyIOBuffer symbols use the class type key.
    class IslandGenerator : public CgsModule::IOBuffer
    {
    public:
        // DecFIGS CgsIslandGenerator.cpp:38 and :54. The ARTIST allocation
        // and free instantiations leave the inherited status byte untouched.
        void Construct() {}
        void Destruct() {}

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
    CGS_STATIC_ASSERT((sizeof(IslandGenerator) == 0x4B2), "IslandGenerator == 1202 (the X360 IOBufferStack::Alloc literal)");
    CGS_STATIC_ASSERT((offsetof(IslandGenerator, maBodyIslandData)  == 0x002), "maBodyIslandData after the 1-byte IOBuffer base, align 2");
    CGS_STATIC_ASSERT((offsetof(IslandGenerator, mau16ParentIsland) == 0x322), "mau16ParentIsland == 2 + 200*4");
}
