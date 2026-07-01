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

namespace Attrib
{
namespace Gen
{
    class proptomaterialmappings : private Instance
    {
    public:
        explicit proptomaterialmappings(Collection* lpCollection = nullptr, void* lpOwner = nullptr);
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
