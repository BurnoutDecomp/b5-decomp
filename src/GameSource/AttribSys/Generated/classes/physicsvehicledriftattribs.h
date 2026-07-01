#pragma once

// Attrib::Gen::physicsvehicledriftattribs — generated AttribSys class (physics vehicle
// drift attributes). Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   Attrib::Gen::physicsvehicledriftattribs::physicsvehicledriftattribs @ 0x825BE020
//
// The X360 build inlines the generated accessor / `using Instance::…` API away, so the
// constructor is the only physicsvehicledriftattribs function in the ledger — this is
// therefore a minimal, X360-faithful recon (class identity + ctor), same generated-ctor
// pattern as physicsvehiclebaseattribs/debrisparams/surfacelist. Derives from Attrib::Instance.
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribinstance.h"

namespace Attrib
{
namespace Gen
{
    class physicsvehicledriftattribs : private Instance
    {
    public:
        explicit physicsvehicledriftattribs(Collection* lpCollection = nullptr, void* lpOwner = nullptr);
    };

    // Chain the Instance ctor, assert the collection's class is
    // ClassName::physicsvehicledriftattribs, then give the instance a default data area
    // (0xA0 bytes) if construction left it without one.
    inline physicsvehicledriftattribs::physicsvehicledriftattribs(Collection* lpCollection, void* lpOwner)
        : Instance(lpCollection, lpOwner)
    {
        static const int KI_PHYSICSVEHICLEDRIFTATTRIBS_CLASS = static_cast<int>(0xF8D767ACu); // Attrib::ClassName::physicsvehicledriftattribs (== -120100948)
        if (GetClass() != KI_PHYSICSVEHICLEDRIFTATTRIBS_CLASS && GetClass() != 0)
            AssertOnClassCheck(GetClass(), KI_PHYSICSVEHICLEDRIFTATTRIBS_CLASS, GetCollection());
        if (!mpAttributeData)
            mpAttributeData = DefaultDataArea(0xA0u);
    }
}
}
