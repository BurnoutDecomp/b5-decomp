#pragma once

// Attrib::Gen::depthoffieldasset -- generated AttribSys class (depth-of-field
// rendering parameters, fed to BrnEffects::DepthOfFieldData::Construct).
// Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   Attrib::Gen::depthoffieldasset::depthoffieldasset @ 0x82677EF0
//
// class-sourced (no Feb-2007 partial source / DWARF for this TU) -- same
// generated-ctor pattern as the sibling generated class sparkeffect: resolves its
// own collection via FindCollection (like shotgroup) and only guards the default
// data area, with NO AssertOnClassCheck class-check call (unlike
// debrisparams/iceanim/surfacelist). The X360 build inlines the generated accessor
// / `using Instance::...` API away, so the constructor is the only depthoffieldasset
// function in the ledger (minimal, X360-faithful recon). Derives from Attrib::Instance.
#include "types.hpp"                                                          // u32
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribinstance.h"
#include "GameSource/AttribSys/Generated/attrib_findcollection.h"   // Attrib::FindCollection (canonical)

namespace Attrib
{
namespace Gen
{
    class depthoffieldasset : private Instance
    {
    public:
        // The depthoffieldasset class key the ctor resolves its collection against.
        // The X360 ctor builds this as the low 32 bits of a 64-bit immediate
        // (0x9F2F63E8_1ECD74BA); Hex-Rays collapses the call to the single literal
        // FindCollection(516781242) -- the high half is a dead/incidental upper word
        // of the 64-bit immediate load (packed via `insrdi r3,r11,32,0`), not a
        // second argument, matching the sibling shotgroup/sparkeffect key-staging.
        static const int KI_DEPTHOFFIELDASSET_CLASS = 516781242; // 0x1ECD74BA

        // The FULL 64-bit class key Attrib::FindCollection resolves against -- the
        // doubleword the X360 ctor stages in r3 with lis/ori + insrdi. KI_KI_DEPTHOFFIELDASSET_CLASS
        // above is only its LOW word (which is what Hex-Rays surfaces, and what this
        // header used to pass to the old one-key FindCollection(int)); the class
        // registry is keyed by the whole doubleword, so the low word alone MISSES.
        static const u64 KU_DEPTHOFFIELDASSET_CLASS_KEY = 0x9F2F63E81ECD74BAULL;

        // Construct over the depthoffieldasset collection, optionally owned by lpOwner.
        explicit depthoffieldasset(void* lpOwner = nullptr);
    };

    // X360 ctor @0x82677EF0: Collection = FindCollection(KI_DEPTHOFFIELDASSET_CLASS);
    // chain the Instance ctor over it (passing the owner through), then give the
    // instance a default data area (0x14 bytes) if construction left it without one.
    // No class-check assert in this ctor (unlike debrisparams/iceanim/surfacelist).
    inline depthoffieldasset::depthoffieldasset(void* lpOwner)
                // FLAG (collection key): the X360 ctor never writes r4, so the CALLER's key
        // argument passes straight through to FindCollection as the collection key.
        // This ctor does not model that parameter yet (no call site in this repo
        // supplies one), so it resolves the class's collection key 0 -- exactly what
        // the previous `FindCollection(KI_..., nullptr)` form did. Add the parameter
        // when a real call site needs a named collection.
    : Instance(FindCollection(KU_DEPTHOFFIELDASSET_CLASS_KEY, 0), lpOwner)
    {
        if (!mpAttributeData)
            mpAttributeData = DefaultDataArea(0x14u);
    }
}
}
