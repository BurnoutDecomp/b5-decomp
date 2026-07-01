#pragma once

// Attrib::Gen::propscrashbinlist — generated AttribSys class (per-prop crash-sound bin
// list, used by BrnSound::Logic::Collision::CollisionStateManager). Reconstructed from
// BURNOUT_X360_ARTIST.XEX:
//   Attrib::Gen::propscrashbinlist::propscrashbinlist @ 0x82697230
//
// class-sourced (no Feb-2007 partial source / DWARF for this TU) — same generated-ctor
// pattern as surfacelist/debrisparams. The X360 build inlines the generated accessor /
// `using Instance::…` API away, so the constructor is the only propscrashbinlist
// function in the ledger (minimal X360-faithful recon). Derives from Attrib::Instance.
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribinstance.h"

namespace Attrib
{
namespace Gen
{
    class propscrashbinlist : private Instance
    {
    public:
        explicit propscrashbinlist(Collection* lpCollection = nullptr, void* lpOwner = nullptr);
    };

    // Chain the Instance ctor, assert the collection's class is ClassName::propscrashbinlist,
    // then give the instance a default data area (0x4D8 bytes) if it has none.
    inline propscrashbinlist::propscrashbinlist(Collection* lpCollection, void* lpOwner)
        : Instance(lpCollection, lpOwner)
    {
        static const int KI_PROPSCRASHBINLIST_CLASS = 1326501919; // Attrib::ClassName::propscrashbinlist
        if (GetClass() != KI_PROPSCRASHBINLIST_CLASS && GetClass() != 0)
            AssertOnClassCheck(GetClass(), KI_PROPSCRASHBINLIST_CLASS, GetCollection());
        if (!mpAttributeData)
            mpAttributeData = DefaultDataArea(0x4D8u);
    }
}
}
