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
        explicit physicsvehiclebaseattribs(Collection* lpCollection = 0, void* lpOwner = 0);

        // Re-exposed from the private Instance base (attribs-setup wave, 2026-08-09): the X360
        // consumers (SimpleVehicleAttribs::SetupAttribs @0x825E6778, the tire scatters) read the
        // record through `lwz +4` == mpAttributeData, which is what GetLayoutPointer returns.
        using Instance::GetLayoutPointer;
    };

    // Chain the Instance ctor, assert the collection's class is
    // ClassName::physicsvehiclebaseattribs, then give the instance a default data area
    // (0x140 bytes) if needed. ARTIST 0x825BD994 constructs the full key 0xF79C545E141DFFA6.
    inline physicsvehiclebaseattribs::physicsvehiclebaseattribs(Collection* lpCollection, void* lpOwner)
        : Instance(lpCollection, lpOwner)
    {
        static const u64 KU_PHYSICSVEHICLEBASEATTRIBS_CLASS = 0xF79C545E141DFFA6ULL; // ARTIST 0x825BD994
        if (GetClass() != KU_PHYSICSVEHICLEBASEATTRIBS_CLASS && GetClass() != 0)
            AssertOnClassCheck(GetClass(), KU_PHYSICSVEHICLEBASEATTRIBS_CLASS, GetCollection());
        if (!mpAttributeData)
            mpAttributeData = DefaultDataArea(0x140u);
    }
}
}
