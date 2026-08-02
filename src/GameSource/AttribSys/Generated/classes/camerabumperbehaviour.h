#pragma once

// Attrib::Gen::camerabumperbehaviour — generated AttribSys class (bumper-camera
// behaviour parameters). Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   Attrib::Gen::camerabumperbehaviour::camerabumperbehaviour @ 0x82206550
//
// class-sourced (no Feb-2007 partial source / DWARF for this TU) — same generated-ctor
// pattern as debrisparams / surfacelist / iceanim. The X360 build inlines the generated
// accessor / `using` API away, so the constructor is the only camerabumperbehaviour
// function in the ledger (minimal X360-faithful recon). Derives from Attrib::Instance.
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribinstance.h"

namespace Attrib
{
namespace Gen
{
    class camerabumperbehaviour : private Instance
    {
    public:
        explicit camerabumperbehaviour(Collection* lpCollection = nullptr, void* lpOwner = nullptr);

        // ADDED 2026-08-02 (camera parameter-chain wave) -- see the identical note on the
        // sibling cameraexternalbehaviour.h. The console reads `lwz r11,0(inst)` (IsValid)
        // and `lwz r11,4(inst)` then `lfs f0,0x18(r11)` (the data area, whose +0x18 is the
        // BUMPER cam's mfBoostFOV) at every use site. Instance is a private base here.
        using Instance::IsValid;
        using Instance::GetLayoutPointer;
    };

    // Chain the Instance ctor, assert the collection's class is
    // ClassName::camerabumperbehaviour (skipping the assert when the class is unset/0),
    // then give the instance a default data area (0x2C bytes) if it has none.
    inline camerabumperbehaviour::camerabumperbehaviour(Collection* lpCollection, void* lpOwner)
        : Instance(lpCollection, lpOwner)
    {
        static const int KI_CAMERABUMPERBEHAVIOUR_CLASS = -2057351271; // Attrib::ClassName::camerabumperbehaviour
        if (GetClass() != KI_CAMERABUMPERBEHAVIOUR_CLASS && GetClass() != 0)
            AssertOnClassCheck(GetClass(), KI_CAMERABUMPERBEHAVIOUR_CLASS, GetCollection());
        if (!mpAttributeData)
            mpAttributeData = DefaultDataArea(0x2Cu);
    }
}
}
