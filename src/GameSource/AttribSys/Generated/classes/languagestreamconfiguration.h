#pragma once

// Attrib::Gen::languagestreamconfiguration — generated AttribSys class (language stream
// configuration schema). Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   Attrib::Gen::languagestreamconfiguration::languagestreamconfiguration @ 0x8269C910
//   Attrib::Gen::languagestreamconfiguration::ContentSpecs                @ 0x82686D80
//
// class-sourced (no LegacySrc partial source for this TU; the AttribSys runtime shapes are
// DWARF-attested in attribsys.h) — same generated-ctor pattern as debrisparams/surfacelist.
// The X360 build inlines the generated accessor / `using` API away, so the constructor and
// the ContentSpecs array-data accessor are the only languagestreamconfiguration functions
// in this ledger slice (minimal X360-faithful recon). Derives from Attrib::Instance.
//
// ARTIST 0x8269C93C constructs the full class key 0xE0E111DBD50CC1F0.
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
        explicit languagestreamconfiguration(Collection* lpCollection = 0, void* lpOwner = 0);
        explicit languagestreamconfiguration(const RefSpec& lrRefSpec, void* lpOwner = 0)
            : Instance(lrRefSpec, lpOwner) {}

        // Resolve the per-index data pointer for this instance's ContentSpecs array
        // attribute. REAL X360 function @0x82686D80. Bounds-checks luIndex against the
        // array header's Private::GetLength(); out-of-range returns the shared
        // DefaultDataArea(4) sentinel (a single zeroed u32), otherwise returns a byte
        // pointer into the block: 4*(luIndex+2) + base — an 8-byte header skip
        // (count/capacity words) followed by 4-byte-stride elements, i.e.
        // &((u32*)mpAttributeData)[luIndex + 2].
        bool IsValid() const { return Instance::IsValid(); }
        const void* GetContentSpecsData(u32 luIndex) const;
        u32 ContentSpec(u32 luIndex) const
        {
            return *static_cast<const u32*>(GetContentSpecsData(luIndex));
        }
    };

    // Chain the Instance ctor, assert the collection's class is
    // ClassName::languagestreamconfiguration, then give the instance a default data area
    // (0x20 bytes) if construction left it without one.
    inline languagestreamconfiguration::languagestreamconfiguration(Collection* lpCollection, void* lpOwner)
        : Instance(lpCollection, lpOwner)
    {
        static const u64 KU_LANGUAGESTREAMCONFIGURATION_CLASS = 0xE0E111DBD50CC1F0ULL; // ARTIST 0x8269C93C
        if (GetClass() != KU_LANGUAGESTREAMCONFIGURATION_CLASS && GetClass() != 0)
            AssertOnClassCheck(GetClass(), KU_LANGUAGESTREAMCONFIGURATION_CLASS, GetCollection());
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
