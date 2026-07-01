#pragma once

// Attrib::Gen::audiosurface — generated AttribSys class (audio surface-type attribute
// schema). Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   Attrib::Gen::audiosurface::audiosurface @ 0x8269AD10
//
// The X360 build inlines the generated accessor / `using Instance::…` API away, so the
// constructor is the only audiosurface function in the ledger — this is therefore a
// minimal, X360-faithful recon (class identity + ctor), matching the sibling
// debrisparams/surfacelist Attrib::Gen::* generated classes. Derives from Attrib::Instance.
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribinstance.h"

namespace Attrib
{
namespace Gen
{
    class audiosurface : private Instance
    {
    public:
        explicit audiosurface(Collection* lpCollection = nullptr, void* lpOwner = nullptr);
    };

    // Chain the Instance ctor, assert the collection's class is ClassName::audiosurface,
    // then give the instance a default data area (0x20 bytes) if it has none.
    inline audiosurface::audiosurface(Collection* lpCollection, void* lpOwner)
        : Instance(lpCollection, lpOwner)
    {
        static const int KI_AUDIOSURFACE_CLASS = 594563281; // Attrib::ClassName::audiosurface
        if (GetClass() != KI_AUDIOSURFACE_CLASS && GetClass() != 0)
            AssertOnClassCheck(GetClass(), KI_AUDIOSURFACE_CLASS, GetCollection());
        if (!mpAttributeData)
            mpAttributeData = DefaultDataArea(0x20u);
    }
}
}
