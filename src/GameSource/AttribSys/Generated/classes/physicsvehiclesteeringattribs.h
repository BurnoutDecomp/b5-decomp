#pragma once

// Attrib::Gen::physicsvehiclesteeringattribs — generated AttribSys class (vehicle
// steering attributes schema). Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   Attrib::Gen::physicsvehiclesteeringattribs::physicsvehiclesteeringattribs @ 0x825BDE28
//
// The X360 build inlines the generated accessor / `using Instance::…` API away, so the
// constructor is the only physicsvehiclesteeringattribs function in the ledger — this is
// therefore a minimal, X360-faithful recon (class identity + ctor), same generated-ctor
// pattern as physicsvehiclebaseattribs/debrisparams/surfacelist. Derives from Attrib::Instance.
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribinstance.h"

namespace Attrib
{
namespace Gen
{
    class physicsvehiclesteeringattribs : private Instance
    {
    public:
        explicit physicsvehiclesteeringattribs(Collection* lpCollection = nullptr, void* lpOwner = nullptr);

        // Re-exposed from the private Instance base (attribs-data wave, 2026-08-09): the X360
        // consumer (VehicleAttribs::SetupAttribs @0x825F4CD8) reads the record through
        // `lwz +4` == mpAttributeData, which is what GetLayoutPointer returns.
        using Instance::GetLayoutPointer;
    };

    // Chain the Instance ctor, assert the collection's class is
    // ClassName::physicsvehiclesteeringattribs, then give the instance a default data area
    // (0x38 bytes) if construction left it without one.
    inline physicsvehiclesteeringattribs::physicsvehiclesteeringattribs(Collection* lpCollection, void* lpOwner)
        : Instance(lpCollection, lpOwner)
    {
        static const int KI_PHYSICSVEHICLESTEERINGATTRIBS_CLASS = 556409804; // Attrib::ClassName::physicsvehiclesteeringattribs (0x212A23CC)
        if (GetClass() != KI_PHYSICSVEHICLESTEERINGATTRIBS_CLASS && GetClass() != 0)
            AssertOnClassCheck(GetClass(), KI_PHYSICSVEHICLESTEERINGATTRIBS_CLASS, GetCollection());
        if (!mpAttributeData)
            mpAttributeData = DefaultDataArea(0x38u);
    }
}
}
