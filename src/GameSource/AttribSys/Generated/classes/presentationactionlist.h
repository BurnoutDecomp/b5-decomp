#pragma once

// Attrib::Gen::presentationactionlist -- generated AttribSys class (a presentation
// effect's action list). The generated accessor / `using Instance::...` API is inlined
// away at the call sites, so the constructor is the only presentationactionlist entry
// point the X360 ledger attests (same minimal-recon convention as the sibling generated
// classes iceanim / surfacelist / debrisparams / physicsvehiclebaseattribs). Derives from
// Attrib::Instance.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   Attrib::Gen::presentationactionlist::presentationactionlist @ 0x8269E208
//
// called by BrnSound::Logic::PresentationEffect::PresentationEffect.
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribinstance.h"

namespace Attrib
{
namespace Gen
{
    class presentationactionlist : private Instance
    {
    public:
        explicit presentationactionlist(Collection* lpCollection = nullptr, void* lpOwner = nullptr);
    };

    // X360 @0x8269E208: chain the Instance ctor, assert the collection's class is
    // ClassName::presentationactionlist (skipping the assert when the class is unset/0),
    // then give the instance a default data area (0x1D48 bytes) if it has none.
    inline presentationactionlist::presentationactionlist(Collection* lpCollection, void* lpOwner)
        : Instance(lpCollection, lpOwner)
    {
        static const int KI_PRESENTATIONACTIONLIST_CLASS = -1959977425; // Attrib::ClassName::presentationactionlist
        if (GetClass() != KI_PRESENTATIONACTIONLIST_CLASS && GetClass() != 0)
            AssertOnClassCheck(GetClass(), KI_PRESENTATIONACTIONLIST_CLASS, GetCollection());
        if (!mpAttributeData)
            mpAttributeData = DefaultDataArea(0x1D48u);
    }
}
}
