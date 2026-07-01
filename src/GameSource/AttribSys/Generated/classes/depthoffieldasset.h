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

        // Construct over the depthoffieldasset collection, optionally owned by lpOwner.
        explicit depthoffieldasset(void* lpOwner = nullptr);
    };

    // X360 ctor @0x82677EF0: Collection = FindCollection(KI_DEPTHOFFIELDASSET_CLASS);
    // chain the Instance ctor over it (passing the owner through), then give the
    // instance a default data area (0x14 bytes) if construction left it without one.
    // No class-check assert in this ctor (unlike debrisparams/iceanim/surfacelist).
    inline depthoffieldasset::depthoffieldasset(void* lpOwner)
        : Instance(FindCollection(KI_DEPTHOFFIELDASSET_CLASS), lpOwner)
    {
        if (!mpAttributeData)
            mpAttributeData = DefaultDataArea(0x14u);
    }
}
}
