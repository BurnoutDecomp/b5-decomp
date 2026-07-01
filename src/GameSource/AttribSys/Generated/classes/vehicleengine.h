#pragma once

// Attrib::Gen::vehicleengine — generated AttribSys class (vehicle engine attributes).
// Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   Attrib::Gen::vehicleengine::vehicleengine @ 0x82696F38
//
// class-sourced (no Feb-2007 partial source / DWARF for this TU) — same generated-ctor
// pattern as debrisparams/surfacelist/physicsvehiclebaseattribs. The X360 build inlines
// the generated accessor / `using Instance::…` API away, so the constructor is the only
// vehicleengine function in the ledger (minimal X360-faithful recon). Derives from
// Attrib::Instance.
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribinstance.h"

namespace Attrib
{
namespace Gen
{
    class vehicleengine : private Instance
    {
    public:
        explicit vehicleengine(Collection* lpCollection = nullptr, void* lpOwner = nullptr);
    };

    // Chain the Instance ctor, assert the collection's class is
    // ClassName::vehicleengine, then give the instance a default data area
    // (0x230 bytes) if construction left it without one.
    inline vehicleengine::vehicleengine(Collection* lpCollection, void* lpOwner)
        : Instance(lpCollection, lpOwner)
    {
        static const int KI_VEHICLEENGINE_CLASS = 1210889151; // 0x482CB3BF, Attrib::ClassName::vehicleengine
        if (GetClass() != KI_VEHICLEENGINE_CLASS && GetClass() != 0)
            AssertOnClassCheck(GetClass(), KI_VEHICLEENGINE_CLASS, GetCollection());
        if (!mpAttributeData)
            mpAttributeData = DefaultDataArea(0x230u);
    }
}
}
