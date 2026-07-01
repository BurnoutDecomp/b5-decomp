#pragma once

// Attrib::Gen::languagestreamconfiguration — generated AttribSys class (language stream
// configuration schema). Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   Attrib::Gen::languagestreamconfiguration::languagestreamconfiguration @ 0x8269C910
//   Attrib::Gen::languagestreamconfiguration::ContentSpecs                @ 0x82686D80
//
// class-sourced (no Feb-2007 partial source for this TU; the AttribSys runtime shapes are
// DWARF-attested in attribsys.h) — same generated-ctor pattern as debrisparams/surfacelist.
// The X360 build inlines the generated accessor / `using` API away, so the constructor and
// the ContentSpecs array-data accessor are the only languagestreamconfiguration functions
// in this ledger slice (minimal X360-faithful recon). Derives from Attrib::Instance.
//
// class key correction (was 0xE0E111DB in a first pass): the asm's low-32-bit compare value
// (and Hex-Rays' -720584208) is 0xD50CC1F0 — insrdi puts r10 in the UPPER half, so r11's
// low word (0xD50CC1F0, first loaded via lis/ori) is what GetClass()'s 32-bit int compares
// against, matching the sibling aftertouchcam/audiosurface low-word rule.
#include "types.hpp"                                                          // u8 / u32
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribinstance.h"
#include "GameSource/AttribSys/Generated/attrib_private.h"   // Attrib::Private (canonical)

namespace Attrib
{
namespace Gen
{
    class languagestreamconfiguration : private Instance
    {
    public:
        explicit languagestreamconfiguration(Collection* lpCollection = nullptr, void* lpOwner = nullptr);

        // Resolve the per-index data pointer for this instance's ContentSpecs array
        // attribute. REAL X360 function @0x82686D80. Bounds-checks luIndex against the
        // array header's Private::GetLength(); out-of-range returns the shared
        // DefaultDataArea(4) sentinel (a single zeroed u32), otherwise returns a byte
        // pointer into the block: 4*(luIndex+2) + base — an 8-byte header skip
        // (count/capacity words) followed by 4-byte-stride elements, i.e.
        // &((u32*)mpAttributeData)[luIndex + 2].
        const void* GetContentSpecsData(u32 luIndex) const;
    };

    // Chain the Instance ctor, assert the collection's class is
    // ClassName::languagestreamconfiguration, then give the instance a default data area
    // (0x20 bytes) if construction left it without one.
    inline languagestreamconfiguration::languagestreamconfiguration(Collection* lpCollection, void* lpOwner)
        : Instance(lpCollection, lpOwner)
    {
        static const int KI_LANGUAGESTREAMCONFIGURATION_CLASS = static_cast<int>(0xD50CC1F0u); // -720584208, Attrib::ClassName::languagestreamconfiguration
        if (GetClass() != KI_LANGUAGESTREAMCONFIGURATION_CLASS && GetClass() != 0)
            AssertOnClassCheck(GetClass(), KI_LANGUAGESTREAMCONFIGURATION_CLASS, GetCollection());
        if (!mpAttributeData)
            mpAttributeData = DefaultDataArea(0x20u);
    }

    // X360 @0x82686D80: v2 = *(this+4) (mpAttributeData, the ContentSpecs array's data
    // block). If luIndex >= Private::GetLength(v2) return DefaultDataArea(4); else return
    // 4*(luIndex+2) + v2.
    inline const void* languagestreamconfiguration::GetContentSpecsData(u32 luIndex) const
    {
        void* lpArrayData = mpAttributeData;
        if (luIndex >= static_cast<const Private*>(lpArrayData)->GetLength())
            return DefaultDataArea(4u);
        return reinterpret_cast<u8*>(lpArrayData) + 4 * (luIndex + 2);
    }
}
}
