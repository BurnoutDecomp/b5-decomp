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
