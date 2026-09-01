#pragma once

// Attrib::Gen::proptomaterialmappings — generated AttribSys class (the "prop to
// material mappings" attribute schema). Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   Attrib::Gen::proptomaterialmappings::proptomaterialmappings @ 0x82697358
//
// class-sourced (no Feb-2007 partial source / DWARF for this TU) — same generated-ctor
// pattern as propscrashbinlist/surfacelist, and called from the same TU
// (BrnSound::Logic::Collision::CollisionStateManager). The X360 build inlines the
// generated accessor / `using Instance::…` API away, so the constructor is the only
// proptomaterialmappings function in the ledger (minimal X360-faithful recon). Derives
// from Attrib::Instance.
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribinstance.h"
#include "GameSource/AttribSys/Generated/attrib_findcollection.h"
#include "GameSource/AttribSys/Generated/attrib_private.h"

namespace Attrib
{
namespace Gen
{
    class proptomaterialmappings : private Instance
    {
    public:
        static const u64 KU_CLASS_KEY = 0x4916214D378020A6ull;
        explicit proptomaterialmappings(Collection* lpCollection = nullptr, void* lpOwner = nullptr);

        Collection* ChangeWithDefault(u64 luCollectionKey)
        {
            return Change(FindCollectionWithDefault(KU_CLASS_KEY, luCollectionKey));
        }

        u32 MappingCount() const
        {
            return *reinterpret_cast<const u32*>(
                static_cast<const u8*>(mpAttributeData) + 0x1008u);
        }

        u64 CgsIds(u32 luIndex) const
        {
            const u8* lpData = static_cast<const u8*>(mpAttributeData);
            if (luIndex >= reinterpret_cast<const Private*>(lpData)->GetLength())
                return 0;
            return reinterpret_cast<const u64*>(lpData + 8u)[luIndex];
        }

        u8 MaterialIndices(u32 luIndex) const
        {
            const u8* lpData = static_cast<const u8*>(mpAttributeData);
            if (luIndex >= reinterpret_cast<const Private*>(lpData + 0x1010u)->GetLength())
                return 0;
            return *(lpData + 0x1018u + luIndex);
        }
    };

    // Chain the Instance ctor, assert the collection's class is ClassName::proptomaterialmappings,
    // then give the instance a default data area (0x1218 bytes) if construction left it without one.
    inline proptomaterialmappings::proptomaterialmappings(Collection* lpCollection, void* lpOwner)
        : Instance(lpCollection, lpOwner)
    {
        static const int KI_PROPTOMATERIALMAPPINGS_CLASS = 931143846; // Attrib::ClassName::proptomaterialmappings (0x378020A6)
        if (GetClass() != KI_PROPTOMATERIALMAPPINGS_CLASS && GetClass() != 0)
            AssertOnClassCheck(GetClass(), KI_PROPTOMATERIALMAPPINGS_CLASS, GetCollection());
        if (!mpAttributeData)
            mpAttributeData = DefaultDataArea(0x1218u);
    }
}
}
