#pragma once

// Attrib::Gen::surface — generated AttribSys class (per-surface-type physical/audio
// attribute schema). Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   Attrib::Gen::surface::surface @ 0x8227FAB0
//
// class-sourced (no Feb-2007 partial source / DWARF for this TU) — same generated-ctor
// pattern as surfacelist / debrisparams. The X360 build inlines the generated accessor /
// `using` API away, so the constructor is the only surface function in the ledger
// (minimal X360-faithful recon). Derives from Attrib::Instance.
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribinstance.h"

namespace Attrib
{
namespace Gen
{
    class surface : private Instance
    {
    public:
        explicit surface(Collection* lpCollection = nullptr, void* lpOwner = nullptr);
    };

    // Chain the Instance ctor, assert the collection's class is ClassName::surface,
    // then give the instance a default data area (0x90 bytes) if it has none.
    inline surface::surface(Collection* lpCollection, void* lpOwner)
        : Instance(lpCollection, lpOwner)
    {
        static const int KI_SURFACE_CLASS = 2016857936; // Attrib::ClassName::surface (0x7836CF50)
        if (GetClass() != KI_SURFACE_CLASS && GetClass() != 0)
            AssertOnClassCheck(GetClass(), KI_SURFACE_CLASS, GetCollection());
        if (!mpAttributeData)
            mpAttributeData = DefaultDataArea(0x90u);
    }
}
}
