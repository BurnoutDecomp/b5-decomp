#pragma once

// Attrib::Gen::physicsvehicleboostattribs — generated AttribSys class (per-vehicle
// boost tuning attributes). Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   Attrib::Gen::physicsvehicleboostattribs::physicsvehicleboostattribs @ 0x825BDC30
//
// The X360 build inlines the generated accessor / `using Instance::…` API away, so the
// constructor is the only physicsvehicleboostattribs function in the ledger — this is a
// minimal, X360-faithful recon (class identity + ctor), same generated-ctor pattern as
// the sibling classes debrisparams / physicsvehiclebaseattribs / surfacelist. Derives
// from Attrib::Instance. Called by BrnPhysics::Vehicle::VehicleAttribs::SetupAttribs and
// BrnAI::AICar::SetDriver.
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribinstance.h"

namespace Attrib
{
namespace Gen
{
    class physicsvehicleboostattribs : private Instance
    {
    public:
        explicit physicsvehicleboostattribs(Collection* lpCollection = nullptr, void* lpOwner = nullptr);

        // Re-exposed from the private Instance base (attribs-data wave, 2026-08-09): the X360
        // consumer (VehicleAttribs::SetupAttribs @0x825F4CD8) reads the record through
        // `lwz +4` == mpAttributeData, which is what GetLayoutPointer returns.
        using Instance::GetLayoutPointer;
    };

    // Chain the Instance ctor, assert the collection's class is
    // ClassName::physicsvehicleboostattribs (skipping the assert when the class is
    // unset/0), then give the instance a default data area (0x40 bytes) if it has none.
    inline physicsvehicleboostattribs::physicsvehicleboostattribs(Collection* lpCollection, void* lpOwner)
        : Instance(lpCollection, lpOwner)
    {
        static const int KI_PHYSICSVEHICLEBOOSTATTRIBS_CLASS = -1350913250; // Attrib::ClassName::physicsvehicleboostattribs (0xAF7AB31E)
        if (GetClass() != KI_PHYSICSVEHICLEBOOSTATTRIBS_CLASS && GetClass() != 0)
            AssertOnClassCheck(GetClass(), KI_PHYSICSVEHICLEBOOSTATTRIBS_CLASS, GetCollection());
        if (!mpAttributeData)
            mpAttributeData = DefaultDataArea(0x40u);
    }
}
}
