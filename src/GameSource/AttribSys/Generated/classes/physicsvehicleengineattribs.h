#pragma once

// Attrib::Gen::physicsvehicleengineattribs — generated AttribSys class (physics vehicle
// engine tuning attributes). Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   Attrib::Gen::physicsvehicleengineattribs::physicsvehicleengineattribs @ 0x825BDD80
//
// The X360 build inlines the generated accessor / `using Instance::…` API away, so the
// constructor is the only physicsvehicleengineattribs function in the ledger — this is
// therefore a minimal, X360-faithful recon (class identity + ctor), same generated-ctor
// pattern as physicsvehiclebaseattribs/debrisparams/surfacelist. Derives from Attrib::Instance.
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribinstance.h"

namespace Attrib
{
namespace Gen
{
    class physicsvehicleengineattribs : private Instance
    {
    public:
        explicit physicsvehicleengineattribs(Collection* lpCollection = nullptr, void* lpOwner = nullptr);
    };

    // Chain the Instance ctor, assert the collection's class is
    // ClassName::physicsvehicleengineattribs, then give the instance a default data area
    // (0x90 bytes) if construction left it without one.
    inline physicsvehicleengineattribs::physicsvehicleengineattribs(Collection* lpCollection, void* lpOwner)
        : Instance(lpCollection, lpOwner)
    {
        static const int KI_PHYSICSVEHICLEENGINEATTRIBS_CLASS = static_cast<int>(0xA54C9B92u); // Attrib::ClassName::physicsvehicleengineattribs
        if (GetClass() != KI_PHYSICSVEHICLEENGINEATTRIBS_CLASS && GetClass() != 0)
            AssertOnClassCheck(GetClass(), KI_PHYSICSVEHICLEENGINEATTRIBS_CLASS, GetCollection());
        if (!mpAttributeData)
            mpAttributeData = DefaultDataArea(0x90u);
    }
}
}
