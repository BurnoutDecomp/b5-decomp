#pragma once

// Attrib::Gen::crashbinlist — generated AttribSys class (the crash-material bin-list
// attribute schema; drives BrnSound::Logic::Collision::CollisionStateManager's crash-sound
// bin table). Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   Attrib::Gen::crashbinlist::crashbinlist @ 0x82697060
//
// class-sourced (no Feb-2007 partial source / DWARF for this TU) — same generated-ctor
// pattern as surfacelist/debrisparams/propscrashbinlist. The X360 build inlines the
// generated accessor / `using` API away, so the constructor is the only crashbinlist
// function in the ledger (minimal X360-faithful recon). Derives from Attrib::Instance.
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribinstance.h"

namespace Attrib
{
namespace Gen
{
    class crashbinlist : private Instance
    {
    public:
        explicit crashbinlist(Collection* lpCollection = nullptr, void* lpOwner = nullptr);
    };

    // Chain the Instance ctor, assert the collection's class is ClassName::crashbinlist,
    // then give the instance a default data area (0x4D8 bytes) if it has none.
    inline crashbinlist::crashbinlist(Collection* lpCollection, void* lpOwner)
        : Instance(lpCollection, lpOwner)
    {
        static const int KI_CRASHBINLIST_CLASS = static_cast<int>(1439145154u); // Attrib::ClassName::crashbinlist (0x55C79CC2)
        if (GetClass() != KI_CRASHBINLIST_CLASS && GetClass() != 0)
            AssertOnClassCheck(GetClass(), KI_CRASHBINLIST_CLASS, GetCollection());
        if (!mpAttributeData)
            mpAttributeData = DefaultDataArea(0x4D8u);
    }
}
}
