#pragma once

// Attrib::Gen::vignetteasset -- generated AttribSys class (the vignette post-effect
// attribute schema; called by BrnEffects::VignetteData::Construct). Reconstructed from
// BURNOUT_X360_ARTIST.XEX:
//   Attrib::Gen::vignetteasset::vignetteasset @ 0x82677F70
//
// class-sourced (no Feb-2007 partial source / DWARF for this TU) -- same generated-ctor
// pattern as the sibling sparkeffect: resolves its own collection via FindCollection
// (instead of taking a Collection* in like debrisparams/surfacelist), and asserts NO
// class check (matches the X360 body exactly: no GetClass()/AssertOnClassCheck in the
// pseudocode or asm). The X360 build inlines the generated accessor / `using Instance::...`
// API away, so the constructor is the only vignetteasset function in the ledger. Derives
// from Attrib::Instance.
//
// FLAG: the ctor's middle argument (a2 / r4 in the ledger's 3-arg signature) is never
// referenced in the body -- r4 is immediately clobbered by the FindCollection result
// (`mr r4,r3`) before any use. Modelled as an unused param for provenance. The owner
// arrives in r5 (a3) -> so the C++ shape must keep a middle param, otherwise owner would
// land in r4 and diverge from the asm's `mr r30,r5`.
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribinstance.h"
#include "GameSource/AttribSys/Generated/attrib_findcollection.h"   // Attrib::FindCollection (canonical)

namespace Attrib
{
namespace Gen
{
    class vignetteasset : private Instance
    {
    public:
        // The vignetteasset class key the ctor resolves its collection against. The X360
        // ctor builds a 64-bit immediate in r3 and passes it to FindCollection: the LOW 32
        // bits are 0x9C02B73F (`ori r3,r11,0xB73F` over `lis r11,-0x63FE`), and `insrdi
        // r3,r11,32,0` writes 0x92F96F62 into the HIGH 32 bits (full r3 = 0x92F96F62_9C02B73F).
        // FindCollection reads only a 32-bit int key, so the value that actually reaches it
        // is the low word 0x9C02B73F -- exactly what Hex-Rays surfaces as
        // FindCollection(-1677543617). The high half is a dead/incidental upper word of the
        // 64-bit load (same key-staging pattern as sparkeffect/shotgroup/surfacelist).
        static const int KI_VIGNETTEASSET_CLASS = -1677543617; // 0x9C02B73F

        // luUnusedKey mirrors the ctor's dead middle argument (r4, clobbered before use --
        // same provenance pattern as shotgroup's luGroupNameKey). lpOwner is the optional
        // owning object the AttribSys collection resolve threads through (arrives in r5).
        explicit vignetteasset(int luUnusedKey = 0, void* lpOwner = nullptr);
    };

    // X360 ctor @0x82677F70: Collection = FindCollection(0x9C02B73F); chain the Instance
    // ctor over it; give the instance a default data area (0x50 bytes) if construction left
    // it without one. No class-check assert in this ctor (unlike debrisparams/surfacelist).
    inline vignetteasset::vignetteasset(int /*luUnusedKey*/, void* lpOwner)
        : Instance(FindCollection(KI_VIGNETTEASSET_CLASS), lpOwner)
    {
        if (!mpAttributeData)
            mpAttributeData = DefaultDataArea(0x50u);
    }
}
}
