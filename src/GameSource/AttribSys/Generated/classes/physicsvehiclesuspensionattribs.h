#pragma once

// Attrib::Gen::physicsvehiclesuspensionattribs — generated AttribSys class (vehicle
// suspension attributes schema). Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   Attrib::Gen::physicsvehiclesuspensionattribs::physicsvehiclesuspensionattribs @ 0x825BDED0
//
// The X360 build inlines the generated accessor / `using Instance::…` API away, so the
// constructor is the only physicsvehiclesuspensionattribs function in the ledger — this is
// therefore a minimal, X360-faithful recon (class identity + ctor), same generated-ctor
// pattern as physicsvehiclebaseattribs/debrisparams/surfacelist. Derives from Attrib::Instance.
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribinstance.h"

namespace Attrib
{
namespace Gen
{
    class physicsvehiclesuspensionattribs : private Instance
    {
    public:
        explicit physicsvehiclesuspensionattribs(Collection* lpCollection = nullptr, void* lpOwner = nullptr);

        // Re-exposed from the private Instance base (attribs-setup wave, 2026-08-09): the X360
        // consumer (SimpleVehicleAttribs::SetupAttribs @0x825E6778) reads the record through
        // `lwz +4` == mpAttributeData, which is what GetLayoutPointer returns.
        using Instance::GetLayoutPointer;
    };

    // Chain the Instance ctor, assert the collection's class is
    // ClassName::physicsvehiclesuspensionattribs, then give the instance a default data
    // area (0x34 bytes) if construction left it without one.
    inline physicsvehiclesuspensionattribs::physicsvehiclesuspensionattribs(Collection* lpCollection, void* lpOwner)
        : Instance(lpCollection, lpOwner)
    {
        static const int KI_PHYSICSVEHICLESUSPENSIONATTRIBS_CLASS = static_cast<int>(525480399u); // Attrib::ClassName::physicsvehiclesuspensionattribs (0x1F5231CF)
        if (GetClass() != KI_PHYSICSVEHICLESUSPENSIONATTRIBS_CLASS && GetClass() != 0)
            AssertOnClassCheck(GetClass(), KI_PHYSICSVEHICLESUSPENSIONATTRIBS_CLASS, GetCollection());
        if (!mpAttributeData)
            mpAttributeData = DefaultDataArea(0x34u);
    }
}
}
