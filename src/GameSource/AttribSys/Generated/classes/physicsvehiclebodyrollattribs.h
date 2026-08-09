#pragma once

// Attrib::Gen::physicsvehiclebodyrollattribs — generated AttribSys class (vehicle body-roll physics attributes).
// Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   Attrib::Gen::physicsvehiclebodyrollattribs::physicsvehiclebodyrollattribs @ 0x825BDF78
//
// class-sourced (no Feb-2007 partial source / DWARF for this TU) — same generated-ctor pattern as
// debrisparams / physicsvehiclebaseattribs / surfacelist. The X360 build inlines the generated
// accessor / `using Instance::…` API away, so the constructor is the only
// physicsvehiclebodyrollattribs function in the ledger (minimal X360-faithful recon).
// Derives from Attrib::Instance.
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribinstance.h"

namespace Attrib
{
namespace Gen
{
    class physicsvehiclebodyrollattribs : private Instance
    {
    public:
        explicit physicsvehiclebodyrollattribs(Collection* lpCollection = nullptr, void* lpOwner = nullptr);

        // Re-exposed from the private Instance base (attribs-data wave, 2026-08-09): the X360
        // consumer (VehicleAttribs::SetupAttribs @0x825F4CD8) reads the record through
        // `lwz +4` == mpAttributeData, which is what GetLayoutPointer returns.
        using Instance::GetLayoutPointer;
    };

    // Chain the Instance ctor, assert the collection's class is
    // ClassName::physicsvehiclebodyrollattribs, then give the instance a default data area
    // (0x28 bytes) if construction left it without one.
    inline physicsvehiclebodyrollattribs::physicsvehiclebodyrollattribs(Collection* lpCollection, void* lpOwner)
        : Instance(lpCollection, lpOwner)
    {
        static const int KI_PHYSICSVEHICLEBODYROLLATTRIBS_CLASS = -767015842; // Attrib::ClassName::physicsvehiclebodyrollattribs (0xD248445E)
        if (GetClass() != KI_PHYSICSVEHICLEBODYROLLATTRIBS_CLASS && GetClass() != 0)
            AssertOnClassCheck(GetClass(), KI_PHYSICSVEHICLEBODYROLLATTRIBS_CLASS, GetCollection());
        if (!mpAttributeData)
            mpAttributeData = DefaultDataArea(0x28u);
    }
}
}
