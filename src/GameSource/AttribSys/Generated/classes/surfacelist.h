#pragma once

// Attrib::Gen::surfacelist — generated AttribSys class (the "surface list" attribute
// schema). Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   Attrib::Gen::surfacelist::surfacelist @ 0x825C2DF8
//
// The X360 build inlines the generated accessor / `using Instance::…` API away, so the
// constructor is the only surfacelist function in the ledger — this is therefore a
// minimal, X360-faithful recon (class identity + ctor). The full generated accessor
// surface (DefaultSurface/Surfaces + the _LayoutStruct) lives in the Feb-2007 partial source and
// can be added if/when a caller TU needs those symbols. Derives from Attrib::Instance.
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribinstance.h"

namespace Attrib
{
namespace Gen
{
    class surfacelist : private Instance
    {
    public:
        explicit surfacelist(Collection* lpCollection = nullptr, void* lpOwner = nullptr);
    };

    // Chain the Instance ctor, assert the collection's class is ClassName::surfacelist,
    // then give the instance a default data area if construction left it without one.
    inline surfacelist::surfacelist(Collection* lpCollection, void* lpOwner)
        : Instance(lpCollection, lpOwner)
    {
        static const int KI_SURFACELIST_CLASS = static_cast<int>(2243282164u); // Attrib::ClassName::surfacelist
        if (GetClass() != KI_SURFACELIST_CLASS && GetClass() != 0)
            AssertOnClassCheck(GetClass(), KI_SURFACELIST_CLASS, GetCollection());
        if (!mpAttributeData)
            mpAttributeData = DefaultDataArea(0x18u);
    }
}
}
