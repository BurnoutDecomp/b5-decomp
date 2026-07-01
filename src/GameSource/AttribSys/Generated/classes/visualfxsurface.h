#pragma once

// Attrib::Gen::visualfxsurface — generated AttribSys class (visual-FX surface attribute
// schema; used by BrnEffects for wheel/detached-part/hinged-part surface-contact FX).
// Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   Attrib::Gen::visualfxsurface::visualfxsurface @ 0x8227FC00
//
// class-sourced (no Feb-2007 partial source / DWARF for this TU) — same generated-ctor
// pattern as debrisparams/surfacelist. The X360 build inlines the generated accessor /
// `using` API away, so the constructor is the only visualfxsurface function in the
// ledger (minimal X360-faithful recon). Derives from Attrib::Instance.
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribinstance.h"

namespace Attrib
{
namespace Gen
{
    class visualfxsurface : private Instance
    {
    public:
        explicit visualfxsurface(Collection* lpCollection = nullptr, void* lpOwner = nullptr);
    };

    // Chain the Instance ctor, assert the collection's class is ClassName::visualfxsurface,
    // then give the instance a default data area (0x60 bytes) if it has none.
    inline visualfxsurface::visualfxsurface(Collection* lpCollection, void* lpOwner)
        : Instance(lpCollection, lpOwner)
    {
        static const int KI_VISUALFXSURFACE_CLASS = -509236432; // Attrib::ClassName::visualfxsurface (0xE1A5AB30)
        if (GetClass() != KI_VISUALFXSURFACE_CLASS && GetClass() != 0)
            AssertOnClassCheck(GetClass(), KI_VISUALFXSURFACE_CLASS, GetCollection());
        if (!mpAttributeData)
            mpAttributeData = DefaultDataArea(0x60u);
    }
}
}
