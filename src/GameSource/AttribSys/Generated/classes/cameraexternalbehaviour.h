#pragma once

// Attrib::Gen::cameraexternalbehaviour — generated AttribSys class (external camera
// behaviour parameters). Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   Attrib::Gen::cameraexternalbehaviour::cameraexternalbehaviour @ 0x822064A8
//
// class-sourced (no Feb-2007 partial source / DWARF for this TU) — same generated-ctor
// pattern as debrisparams/surfacelist. The X360 build inlines the generated accessor /
// `using Instance::…` API away, so the constructor is the only cameraexternalbehaviour
// function in the ledger (minimal X360-faithful recon). Derives from Attrib::Instance.
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribinstance.h"

namespace Attrib
{
namespace Gen
{
    class cameraexternalbehaviour : private Instance
    {
    public:
        explicit cameraexternalbehaviour(Collection* lpCollection = nullptr, void* lpOwner = nullptr);

        // ADDED 2026-08-02 (camera parameter-chain wave). The two base members the console
        // reads straight off the stack instance at every one of this class's four use sites
        // (MainDirector::ProcessNewVehicleEvents @0x8221A6B0 / UpdateAttribSys @0x8221AFD0,
        // BrnDirectorVehicleInputInterface::NewVehicle @0x822CBA90, ReplayDirector::
        // PreSceneQueryUpdate @0x8225BD28):
        //   `lwz r11, 0(inst)` + `cmplwi r11,0`  -- the collection, i.e. IsValid()
        //   `lwz r11, 4(inst)` then `lfs f0, 0x40(r11)` -- the attribute DATA AREA, whose
        //     +0x40 is mfBoostFOV and which BehaviourGameplayExternal::Parameters::Set reads
        //     as its whole f32 source array.
        // Re-exported rather than re-derived because Instance is a PRIVATE base here (same
        // using-declaration precedent as cameradefaults.h:71 / shotgroup.h).
        using Instance::IsValid;
        using Instance::GetLayoutPointer;
    };

    // Chain the Instance ctor, assert the collection's class is ClassName::cameraexternalbehaviour,
    // then give the instance a default data area (0x44 bytes) if it has none.
    inline cameraexternalbehaviour::cameraexternalbehaviour(Collection* lpCollection, void* lpOwner)
        : Instance(lpCollection, lpOwner)
    {
        static const int KI_CAMERAEXTERNALBEHAVIOUR_CLASS = -991282044; // Attrib::ClassName::cameraexternalbehaviour (0xC4EA3C84)
        if (GetClass() != KI_CAMERAEXTERNALBEHAVIOUR_CLASS && GetClass() != 0)
            AssertOnClassCheck(GetClass(), KI_CAMERAEXTERNALBEHAVIOUR_CLASS, GetCollection());
        if (!mpAttributeData)
            mpAttributeData = DefaultDataArea(0x44u);
    }
}
}
