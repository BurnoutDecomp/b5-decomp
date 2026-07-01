#pragma once

// Attrib::Gen::physicsvehiclebaseattribs — generated AttribSys class (base physics
// vehicle attributes schema). Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   Attrib::Gen::physicsvehiclebaseattribs::physicsvehiclebaseattribs @ 0x825BD968
//
// The X360 build inlines the generated accessor / `using Instance::…` API away, so the
// constructor is the only physicsvehiclebaseattribs function in the ledger — this is
// therefore a minimal, X360-faithful recon (class identity + ctor), same generated-ctor
// pattern as debrisparams/surfacelist. Derives from Attrib::Instance.
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribinstance.h"

namespace Attrib
{
namespace Gen
{
    class physicsvehiclebaseattribs : private Instance
    {
    public:
        explicit physicsvehiclebaseattribs(Collection* lpCollection = nullptr, void* lpOwner = nullptr);
    };

    // Chain the Instance ctor, assert the collection's class is
    // ClassName::physicsvehiclebaseattribs, then give the instance a default data area
    // (0x140 bytes) if construction left it without one. The class id is the LOW word of
    // the 64-bit insrdi compare doubleword (0x141DFFA6); the incidental high word
    // (0xF79C545E) is dead — GetClass() is a 32-bit int occupying the low word.
    inline physicsvehiclebaseattribs::physicsvehiclebaseattribs(Collection* lpCollection, void* lpOwner)
        : Instance(lpCollection, lpOwner)
    {
        static const int KI_PHYSICSVEHICLEBASEATTRIBS_CLASS = 337510310; // 0x141DFFA6 — Attrib::ClassName::physicsvehiclebaseattribs
        if (GetClass() != KI_PHYSICSVEHICLEBASEATTRIBS_CLASS && GetClass() != 0)
            AssertOnClassCheck(GetClass(), KI_PHYSICSVEHICLEBASEATTRIBS_CLASS, GetCollection());
        if (!mpAttributeData)
            mpAttributeData = DefaultDataArea(0x140u);
    }
}
}
