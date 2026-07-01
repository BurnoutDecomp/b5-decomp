#pragma once

// Attrib::Gen::junkyardlocators -- generated AttribSys class (the junkyard "locators"
// attribute schema: the placement markers a junkyard scene's wrecked cars/props spawn
// at). The generated accessor / `using Instance::...` API is inlined away at the call
// sites, so the constructor is the only junkyardlocators function the X360 ledger
// attests -- a minimal, X360-faithful recon. Its ctor body has NO AssertOnClassCheck
// call: it resolves its own collection via FindCollection (like the adjacent sibling
// sparkeffect / shotgroup, NOT like debrisparams/iceanim/surfacelist which take a
// Collection* and assert). Derives from Attrib::Instance.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   Attrib::Gen::junkyardlocators::junkyardlocators @ 0x8227F018
//
//   * the ctor resolves the junkyardlocators class collection via
//     FindCollection(-828779101) and chains Instance(Collection, lpOwner) over it, then
//     gives the instance a default data area (4 bytes) if construction left it without one.
//   * FLAG: the X360 ctor stages the class key as the low 32 bits of a 64-bit immediate
//     (0x6BE263B8_CE99D5A3) via lis/ori + insrdi; FindCollection reads only the low 32
//     bits (0xCE99D5A3). The high word (0x6BE263B8) is a dead/incidental upper half of
//     the 64-bit constant-load idiom, not a second argument -- same staged-but-dead key
//     pattern noted for sparkeffect (0xCAF7F032_BCD68E9C) and surfacelist
//     (0x0ADCE56E_F3DA7F1F).
#include "types.hpp"                                                          // u32
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribinstance.h"
#include "GameSource/AttribSys/Generated/attrib_findcollection.h"   // Attrib::FindCollection (canonical)

namespace Attrib
{
namespace Gen
{
    class junkyardlocators : private Instance
    {
    public:
        // The junkyardlocators class key the ctor resolves its collection against. The
        // X360 ctor builds this as the low 32 bits of a 64-bit immediate
        // (0x6BE263B8_CE99D5A3); Hex-Rays collapses the call to the single int
        // FindCollection(-828779101) -- the high half is a dead/incidental upper word of
        // the 64-bit immediate load, not a second argument.
        static const int KI_JUNKYARDLOCATORS_CLASS = -828779101; // 0xCE99D5A3

        // Construct over the junkyardlocators collection, optionally owned by lpOwner.
        explicit junkyardlocators(void* lpOwner = nullptr);
    };

    // X360 ctor @0x8227F018: Collection = FindCollection(KI_JUNKYARDLOCATORS_CLASS); chain
    // the Instance ctor over it; then give the instance a default data area (4 bytes) if
    // it has none. No class-check assert in this ctor (unlike debrisparams/iceanim).
    inline junkyardlocators::junkyardlocators(void* lpOwner)
        : Instance(FindCollection(KI_JUNKYARDLOCATORS_CLASS), lpOwner)
    {
        if (!mpAttributeData)
            mpAttributeData = DefaultDataArea(4u);
    }
}
}
