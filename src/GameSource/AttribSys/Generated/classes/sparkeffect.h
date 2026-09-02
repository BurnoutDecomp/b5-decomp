#pragma once

// Attrib::Gen::sparkeffect -- generated AttribSys class (spark-effect burst
// parameters, fed to BrnEffects::EffectsModule::Prepare). Reconstructed from
// BURNOUT_X360_ARTIST.XEX:
//   Attrib::Gen::sparkeffect::sparkeffect @ 0x8227EC10
//
// class-sourced (no Feb-2007 partial source / DWARF for this TU) -- same generated-ctor
// pattern as the sibling generated classes shotgroup / debrisparams / iceanim. The X360
// build inlines the generated accessor / `using Instance::...` API away, so the
// constructor is the only sparkeffect function in the ledger (minimal, X360-faithful
// recon). Unlike debrisparams/iceanim/surfacelist, the X360 ctor body has NO
// AssertOnClassCheck call -- it resolves its own collection via FindCollection (like
// shotgroup) and only guards the default data area. Derives from Attrib::Instance.
#include "types.hpp"                                                          // u32
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribinstance.h"
#include "GameSource/AttribSys/Generated/attrib_findcollection.h"   // Attrib::FindCollection (canonical)

namespace Attrib
{
namespace Gen
{
    class sparkeffect : private Instance
    {
    public:
        // The sparkeffect class key the ctor resolves its collection against. The X360
        // ctor builds this as the low 32 bits of a 64-bit immediate (0xCAF7F032_BCD68E9C);
        // Hex-Rays collapses the call to the single int FindCollection(-1126789476) --
        // the high half is a dead/incidental upper word of the 64-bit immediate load, not
        // a second argument (FindCollection reads only a 32-bit key here, matching the
        // sibling shotgroup ctor's shape).
        static const int KI_SPARKEFFECT_CLASS = -1126789476; // 0xBCD68E9C

        // The FULL 64-bit class key Attrib::FindCollection resolves against -- the
        // doubleword the X360 ctor stages in r3 with lis/ori + insrdi. KI_KI_SPARKEFFECT_CLASS
        // above is only its LOW word (which is what Hex-Rays surfaces, and what this
        // header used to pass to the old one-key FindCollection(int)); the class
        // registry is keyed by the whole doubleword, so the low word alone MISSES.
        static const u64 KU_SPARKEFFECT_CLASS_KEY = 0xCAF7F032BCD68E9CULL;

        // Construct over the sparkeffect collection, optionally owned by lpOwner.
        explicit sparkeffect(void* lpOwner = nullptr);
    };

    // X360 ctor @0x8227EC10: Collection = FindCollection(KI_SPARKEFFECT_CLASS); chain the
    // Instance ctor over it; then give the instance a default data area (0x90 bytes) if
    // it has none. No class-check assert in this ctor (unlike debrisparams/iceanim).
    inline sparkeffect::sparkeffect(void* lpOwner)
                // FLAG (collection key): the X360 ctor never writes r4, so the CALLER's key
        // argument passes straight through to FindCollection as the collection key.
        // This ctor does not model that parameter yet (no call site in this repo
        // supplies one), so it resolves the class's collection key 0 -- exactly what
        // the previous `FindCollection(KI_..., nullptr)` form did. Add the parameter
        // when a real call site needs a named collection.
    : Instance(FindCollection(KU_SPARKEFFECT_CLASS_KEY, 0), lpOwner)
    {
        if (!mpAttributeData)
            mpAttributeData = DefaultDataArea(0x90u);
    }
}
}
