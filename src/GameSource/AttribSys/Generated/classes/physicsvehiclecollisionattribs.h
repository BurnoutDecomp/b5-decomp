#pragma once

// Attrib::Gen::physicsvehiclecollisionattribs — generated AttribSys class (physics
// vehicle-collision attribute schema). Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   Attrib::Gen::physicsvehiclecollisionattribs::physicsvehiclecollisionattribs @ 0x825BDCD8
//
// class-sourced (no Feb-2007 partial source / DWARF for this TU) — same generated-ctor
// pattern as the sibling generated classes physicsvehiclebaseattribs / surfacelist /
// debrisparams / iceanim. The X360 build inlines the generated accessor / `using` API
// away, so the constructor is the only physicsvehiclecollisionattribs function in the
// ledger (minimal X360-faithful recon). Derives from Attrib::Instance.
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribinstance.h"

namespace Attrib
{
namespace Gen
{
    class physicsvehiclecollisionattribs : private Instance
    {
    public:
        explicit physicsvehiclecollisionattribs(Collection* lpCollection = nullptr, void* lpOwner = nullptr);

        // Re-exposed from the private Instance base (attribs-data wave, 2026-08-09): the X360
        // consumer (VehicleAttribs::SetupAttribs @0x825F4CD8) reads the record through
        // `lwz +4` == mpAttributeData, which is what GetLayoutPointer returns.
        using Instance::GetLayoutPointer;
    };

    // Chain the Instance ctor, assert the collection's class is
    // ClassName::physicsvehiclecollisionattribs (skipping the assert when the class is
    // unset/0), then give the instance a default data area (0x10 bytes) if it has none.
    //
    // The X360 asm materialises the class-id compare via the 64-bit-immediate idiom
    // (lis/ori r11 = low word 0x568F138C, lis/ori r10 = 0xDF956BC0, insrdi r11,r10,32,0 →
    // r11 = 0xDF956BC0568F138C, cmpld). GetClass() is reconstructed as a 32-bit int
    // (Collection::mpClass is int*, *lpClass), so the comparison operand is the low word
    // 0x568F138C = 1452217228 (matching Hex-Rays). The 0xDF956BC0 half is the compiler's
    // 64-bit-constant materialisation, not a distinct data store.
    inline physicsvehiclecollisionattribs::physicsvehiclecollisionattribs(Collection* lpCollection, void* lpOwner)
        : Instance(lpCollection, lpOwner)
    {
        static const int KI_PHYSICSVEHICLECOLLISIONATTRIBS_CLASS = 1452217228; // 0x568F138C
        if (GetClass() != KI_PHYSICSVEHICLECOLLISIONATTRIBS_CLASS && GetClass() != 0)
            AssertOnClassCheck(GetClass(), KI_PHYSICSVEHICLECOLLISIONATTRIBS_CLASS, GetCollection());
        if (!mpAttributeData)
            mpAttributeData = DefaultDataArea(0x10u);
    }
}
}
