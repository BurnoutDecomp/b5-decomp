#pragma once

// Attrib::Gen::physicssurface — generated AttribSys class (physics surface-property
// schema, e.g. friction/grip tables consumed by BrnPhysics::Vehicle::VehicleManager::
// ReadSurfaceProperties). Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   Attrib::Gen::physicssurface::physicssurface @ 0x825C0D20
//
// class-sourced (no Feb-2007 partial source / DWARF for this TU) — same generated-ctor
// pattern as surfacelist/debrisparams/propscrashbinlist. The X360 build inlines the
// generated accessor / `using Instance::…` API away, so the constructor is the only
// physicssurface function in the ledger (minimal, X360-faithful recon). Derives from
// Attrib::Instance.
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribinstance.h"

namespace Attrib
{
namespace Gen
{
    class physicssurface : private Instance
    {
    public:
        explicit physicssurface(Collection* lpCollection = nullptr, void* lpOwner = nullptr);
    };

    // Chain the Instance ctor, assert the collection's class is ClassName::physicssurface,
    // then give the instance a default data area (0xC bytes) if it has none.
    inline physicssurface::physicssurface(Collection* lpCollection, void* lpOwner)
        : Instance(lpCollection, lpOwner)
    {
        // Class key is the BASE word of the 64-bit immediate the asm builds
        // (lis/ori 0x2C485337); the insrdi'd high word 0xFD61B26B is the
        // dead/incidental upper half. Mirrors the surfacelist sibling, whose base
        // word 0x85B5C4F4 is the committed class constant.
        static const int KI_PHYSICSSURFACE_CLASS = 742937399; // Attrib::ClassName::physicssurface (0x2C485337)
        if (GetClass() != KI_PHYSICSSURFACE_CLASS && GetClass() != 0)
            AssertOnClassCheck(GetClass(), KI_PHYSICSSURFACE_CLASS, GetCollection());
        if (!mpAttributeData)
            mpAttributeData = DefaultDataArea(0xCu);
    }
}
}
