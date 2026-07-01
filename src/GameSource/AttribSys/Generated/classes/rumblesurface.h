#pragma once

// Attrib::Gen::rumblesurface — generated AttribSys class (rumble surface / road-feel
// attribute schema). Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   Attrib::Gen::rumblesurface::rumblesurface @ 0x82364828
//
// class-sourced (no Feb-2007 partial source / DWARF for this TU) — same generated-ctor
// pattern as surfacelist/debrisparams. The X360 build inlines the generated accessor /
// `using Instance::…` API away, so the constructor is the only rumblesurface function in
// the ledger (minimal X360-faithful recon). Derives from Attrib::Instance.
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribinstance.h"

namespace Attrib
{
namespace Gen
{
    class rumblesurface : private Instance
    {
    public:
        explicit rumblesurface(Collection* lpCollection = nullptr, void* lpOwner = nullptr);
    };

    // Chain the Instance ctor, assert the collection's class is ClassName::rumblesurface,
    // then give the instance a default data area (0x3C bytes) if it has none.
    inline rumblesurface::rumblesurface(Collection* lpCollection, void* lpOwner)
        : Instance(lpCollection, lpOwner)
    {
        static const int KI_RUMBLESURFACE_CLASS = static_cast<int>(0x14E37D72u); // Attrib::ClassName::rumblesurface (350453106)
        if (GetClass() != KI_RUMBLESURFACE_CLASS && GetClass() != 0)
            AssertOnClassCheck(GetClass(), KI_RUMBLESURFACE_CLASS, GetCollection());
        if (!mpAttributeData)
            mpAttributeData = DefaultDataArea(0x3Cu);
    }
}
}
