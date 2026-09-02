#pragma once

// Attrib::Gen::debrisparams — generated AttribSys class (debris-burst parameters).
// Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   Attrib::Gen::debrisparams::debrisparams @ 0x8227E9A0
//
// class-sourced (no Feb-2007 partial source / DWARF for this TU) — same generated-ctor pattern as
// surfacelist. The X360 build inlines the generated accessor / `using` API away, so the
// constructor is the only debrisparams function in the ledger (minimal X360-faithful
// recon). Derives from Attrib::Instance.
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribinstance.h"
#include "GameSource/AttribSys/Generated/attrib_findcollection.h"   // Attrib::FindCollection (the keyed ctor)

namespace Attrib
{
namespace Gen
{
    class debrisparams : private Instance
    {
    public:
        // The 64-bit class key the X360 keyed ctor @0x8227EA48 hands FindCollection:
        // `lis r11,0x26D8 / ori r3,r11,0x1D5F` (the low word, == KI_DEBRISPARAMS_CLASS
        // below) then `lis r11,0x3024 / ori r11,r11,0xA287 / insrdi r3,r11,32,0` (the high
        // word). EffectsModule::Prepare @0x8229E690 calls it with
        // StringToKey("383338"/"595518"/"608203"). Additive (2026-09-02).
        static const u64 KU_DEBRISPARAMS_CLASS_KEY = 0x3024A28726D81D5FULL;

        explicit debrisparams(Collection* lpCollection = nullptr, void* lpOwner = nullptr);
        // @0x8227EA48 -- the keyed form: FindCollection(class, luCollectionKey) then the
        // same Instance chain + 0xE0-byte default data area (no class-check assert there).
        debrisparams(u64 luCollectionKey, void* lpOwner);
    };

    inline debrisparams::debrisparams(u64 luCollectionKey, void* lpOwner)
        : Instance(FindCollection(KU_DEBRISPARAMS_CLASS_KEY, luCollectionKey), lpOwner)
    {
        if (!mpAttributeData)
            mpAttributeData = DefaultDataArea(0xE0u);
    }

    // Chain the Instance ctor, assert the collection's class is ClassName::debrisparams,
    // then give the instance a default data area (0xE0 bytes) if it has none.
    inline debrisparams::debrisparams(Collection* lpCollection, void* lpOwner)
        : Instance(lpCollection, lpOwner)
    {
        static const int KI_DEBRISPARAMS_CLASS = 651697503; // Attrib::ClassName::debrisparams
        if (GetClass() != KI_DEBRISPARAMS_CLASS && GetClass() != 0)
            AssertOnClassCheck(GetClass(), KI_DEBRISPARAMS_CLASS, GetCollection());
        if (!mpAttributeData)
            mpAttributeData = DefaultDataArea(0xE0u);
    }
}
}
